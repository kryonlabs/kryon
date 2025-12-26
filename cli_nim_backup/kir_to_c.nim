## Kryon IR to C Code Generator
##
## Transpiles .kir (JSON IR) files to C source code using Kryon C bindings.
## Generated C code can be compiled and run to produce the same .kir (round-trip).

import json, strutils, tables, sets, sequtils, strformat

type
  CCodegen = object
    output: string
    indent: int
    varCounter: int
    componentVars: Table[uint32, string]  # component ID -> variable name
    handlerFunctions: seq[string]
    usedVarNames: HashSet[string]

proc indent(gen: var CCodegen, delta: int = 0) =
  gen.indent += delta

proc write(gen: var CCodegen, text: string) =
  gen.output.add("    ".repeat(gen.indent) & text)

proc writeln(gen: var CCodegen, text: string = "") =
  if text.len > 0:
    gen.write(text & "\n")
  else:
    gen.output.add("\n")

proc newVar(gen: var CCodegen, prefix: string = "comp"): string =
  ## Generate a unique variable name
  result = fmt"{prefix}_{gen.varCounter}"
  gen.varCounter += 1
  gen.usedVarNames.incl(result)

proc escapeString(s: string): string =
  ## Escape string for C
  result = s
  result = result.replace("\\", "\\\\")
  result = result.replace("\"", "\\\"")
  result = result.replace("\n", "\\n")
  result = result.replace("\t", "\\t")

proc parseDimension(value: JsonNode): tuple[val: float, unit: string] =
  ## Parse dimension from JSON (supports "100px", "50%", "auto", etc.)
  if value.kind == JString:
    let str = value.getStr()
    if str == "auto":
      return (0.0, "auto")
    elif str.endsWith("px"):
      return (parseFloat(str[0..^3]), "px")
    elif str.endsWith("%"):
      return (parseFloat(str[0..^2]), "%")
    elif str.endsWith("fr"):
      return (parseFloat(str[0..^3]), "fr")
    else:
      return (parseFloat(str), "px")
  elif value.kind == JInt:
    return (value.getInt().float, "px")
  elif value.kind == JFloat:
    return (value.getFloat(), "px")
  else:
    return (0.0, "auto")

proc parseColor(color: JsonNode): string =
  ## Parse color to C hex format
  if color.kind == JString:
    let colorStr = color.getStr()
    if colorStr.startsWith("#"):
      # Hex color: #RRGGBB or #RRGGBBAA
      let hex = colorStr[1..^1]
      if hex.len == 6:
        return fmt"0x{hex}"
      elif hex.len == 8:
        # RGBA: reorder to AARRGGBB for C macro
        return fmt"0x{hex[6..7]}{hex[0..5]}"
    # Named colors - map to C constants
    case colorStr.toLowerAscii()
    of "black": return "KRYON_COLOR_BLACK"
    of "white": return "KRYON_COLOR_WHITE"
    of "red": return "KRYON_COLOR_RED"
    of "green": return "KRYON_COLOR_GREEN"
    of "blue": return "KRYON_COLOR_BLUE"
    of "yellow": return "KRYON_COLOR_YELLOW"
    else: return "KRYON_COLOR_BLACK"
  return "KRYON_COLOR_BLACK"

proc generateComponent(gen: var CCodegen, node: JsonNode, parentVar: string = ""): string =
  ## Recursively generate C code for a component tree
  ## Returns the variable name of the created component

  let componentType = node["type"].getStr()
  let varName = gen.newVar(componentType.toLowerAscii())

  # Store component ID -> var name mapping
  if node.hasKey("id"):
    gen.componentVars[node["id"].getInt().uint32] = varName

  # Create component
  case componentType
  of "Container":
    gen.writeln(fmt"IRComponent* {varName} = kryon_container();")
  of "Row":
    gen.writeln(fmt"IRComponent* {varName} = kryon_row();")
  of "Column":
    gen.writeln(fmt"IRComponent* {varName} = kryon_column();")
  of "Center":
    gen.writeln(fmt"IRComponent* {varName} = kryon_center();")
  of "Text":
    let text = if node.hasKey("text"): node["text"].getStr().escapeString() else: ""
    gen.writeln("IRComponent* " & varName & " = kryon_text(\"" & text & "\");")
  of "Button":
    let text = if node.hasKey("text"): node["text"].getStr().escapeString() else: "Button"
    gen.writeln("IRComponent* " & varName & " = kryon_button(\"" & text & "\");")
  of "Input":
    let placeholder = if node.hasKey("placeholder"): node["placeholder"].getStr().escapeString() else: ""
    gen.writeln("IRComponent* " & varName & " = kryon_input(\"" & placeholder & "\");")
  of "Checkbox":
    let label = if node.hasKey("text"): node["text"].getStr().escapeString() else: ""
    let checked = if node.hasKey("checked"): node["checked"].getBool() else: false
    gen.writeln("IRComponent* " & varName & " = kryon_checkbox(\"" & label & "\", " & $checked & ");")
  of "TabGroup":
    gen.writeln(fmt"IRComponent* {varName} = kryon_tab_group();")
  of "TabBar":
    gen.writeln(fmt"IRComponent* {varName} = kryon_tab_bar();")
  of "Tab":
    let title = if node.hasKey("text"): node["text"].getStr().escapeString() else: "Tab"
    gen.writeln("IRComponent* " & varName & " = kryon_tab(\"" & title & "\");")
  of "TabContent":
    gen.writeln(fmt"IRComponent* {varName} = kryon_tab_content();")
  of "TabPanel":
    gen.writeln(fmt"IRComponent* {varName} = kryon_tab_panel();")
  else:
    gen.writeln(fmt"IRComponent* {varName} = kryon_container(); // Unknown type: {componentType}")

  gen.writeln()

  # Set properties
  if node.hasKey("width"):
    let (val, unit) = parseDimension(node["width"])
    gen.writeln("kryon_set_width(" & varName & ", " & $val & ", \"" & unit & "\");")

  if node.hasKey("height"):
    let (val, unit) = parseDimension(node["height"])
    gen.writeln("kryon_set_height(" & varName & ", " & $val & ", \"" & unit & "\");")

  if node.hasKey("background"):
    let color = parseColor(node["background"])
    gen.writeln(fmt"kryon_set_background({varName}, {color});")

  if node.hasKey("color"):
    let color = parseColor(node["color"])
    gen.writeln(fmt"kryon_set_color({varName}, {color});")

  if node.hasKey("padding"):
    let padding = node["padding"]
    if padding.kind == JObject:
      let top = padding["top"].getFloat()
      let right = padding["right"].getFloat()
      let bottom = padding["bottom"].getFloat()
      let left = padding["left"].getFloat()
      gen.writeln(fmt"kryon_set_padding_sides({varName}, {top}, {right}, {bottom}, {left});")
    else:
      let val = padding.getFloat()
      gen.writeln(fmt"kryon_set_padding({varName}, {val});")

  if node.hasKey("margin"):
    let margin = node["margin"]
    if margin.kind == JObject:
      let top = margin["top"].getFloat()
      let right = margin["right"].getFloat()
      let bottom = margin["bottom"].getFloat()
      let left = margin["left"].getFloat()
      gen.writeln(fmt"kryon_set_margin_sides({varName}, {top}, {right}, {bottom}, {left});")
    else:
      let val = margin.getFloat()
      gen.writeln(fmt"kryon_set_margin({varName}, {val});")

  if node.hasKey("gap"):
    gen.writeln("kryon_set_gap(" & varName & ", " & $node["gap"].getFloat() & ");")

  if node.hasKey("fontSize"):
    gen.writeln("kryon_set_font_size(" & varName & ", " & $node["fontSize"].getFloat() & ");")

  if node.hasKey("justifyContent"):
    let align = node["justifyContent"].getStr()
    let alignConst = case align
      of "center": "IR_ALIGNMENT_CENTER"
      of "flex-start", "start": "IR_ALIGNMENT_START"
      of "flex-end", "end": "IR_ALIGNMENT_END"
      of "space-between": "IR_ALIGNMENT_SPACE_BETWEEN"
      of "space-around": "IR_ALIGNMENT_SPACE_AROUND"
      of "space-evenly": "IR_ALIGNMENT_SPACE_EVENLY"
      else: "IR_ALIGNMENT_START"
    gen.writeln(fmt"kryon_set_justify_content({varName}, {alignConst});")

  if node.hasKey("alignItems"):
    let align = node["alignItems"].getStr()
    let alignConst = case align
      of "center": "IR_ALIGNMENT_CENTER"
      of "flex-start", "start": "IR_ALIGNMENT_START"
      of "flex-end", "end": "IR_ALIGNMENT_END"
      of "stretch": "IR_ALIGNMENT_STRETCH"
      else: "IR_ALIGNMENT_START"
    gen.writeln(fmt"kryon_set_align_items({varName}, {alignConst});")

  if node.hasKey("opacity"):
    let opacity = node["opacity"].getFloat()
    gen.writeln(fmt"kryon_set_opacity({varName}, {opacity});")

  if node.hasKey("borderRadius"):
    let radius = node["borderRadius"].getFloat()
    gen.writeln(fmt"kryon_set_border_radius({varName}, {radius});")

  gen.writeln()

  # Handle children
  if node.hasKey("children") and node["children"].len > 0:
    for child in node["children"]:
      let childVar = gen.generateComponent(child, varName)
      gen.writeln(fmt"kryon_add_child({varName}, {childVar});")
      gen.writeln()

  return varName

proc generateCFromKir*(kirFile: string, outputFile: string = ""): string =
  ## Generate C source code from .kir file
  var gen = CCodegen(
    output: "",
    indent: 0,
    varCounter: 0,
    componentVars: initTable[uint32, string](),
    handlerFunctions: @[],
    usedVarNames: initHashSet[string]()
  )

  # Parse KIR file
  let kirJson = parseFile(kirFile)
  let root = kirJson["root"]

  # Generate header
  gen.writeln("/**")
  gen.writeln(" * Auto-generated C code from .kir file")
  gen.writeln(" * DO NOT EDIT MANUALLY - regenerate with: kryon codegen")
  gen.writeln(" */")
  gen.writeln()
  gen.writeln("#include <kryon.h>")
  gen.writeln("#include <stdio.h>")
  gen.writeln()

  # Generate event handler functions (if needed)
  # TODO: Extract from events and generate handler functions

  # Generate main function
  gen.writeln("int main(void) {")
  gen.indent(1)

  # Initialize Kryon
  let title = if kirJson.hasKey("title"): kirJson["title"].getStr().escapeString() else: "Kryon App"
  let width = if kirJson.hasKey("width"): kirJson["width"].getInt() else: 800
  let height = if kirJson.hasKey("height"): kirJson["height"].getInt() else: 600

  gen.writeln("kryon_init(\"" & title & "\", " & $width & ", " & $height & ");")
  gen.writeln()

  # Get the C API root
  gen.writeln("IRComponent* root = kryon_get_root();")
  gen.writeln()

  # Generate component tree
  let rootComponentVar = gen.generateComponent(root, "")

  # Add the generated root component as a child of the C API root
  gen.writeln("kryon_add_child(root, " & rootComponentVar & ");")
  gen.writeln()

  # Finalize and serialize
  let outputPath = if outputFile.len > 0: outputFile else: "output.kir"
  gen.writeln("kryon_finalize(\"" & outputPath & "\");")
  gen.writeln("kryon_cleanup();")
  gen.writeln()
  gen.writeln("return 0;")

  gen.indent(-1)
  gen.writeln("}")

  return gen.output

when isMainModule:
  import os

  if paramCount() < 1:
    echo "Usage: kir_to_c <input.kir> [output.c]"
    quit(1)

  let inputFile = paramStr(1)
  let outputFile = if paramCount() >= 2: paramStr(2) else: changeFileExt(inputFile, ".c")

  let cCode = generateCFromKir(inputFile, outputFile)
  writeFile(outputFile, cCode)

  echo "Generated C code: ", outputFile
