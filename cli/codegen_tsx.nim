## TSX/JSX Code Generator for Kryon
## Generates React-Hooks style TSX code from .kir files

import json, strformat, os, strutils, tables, sets

# =============================================================================
# Helper Types
# =============================================================================

type
  ComponentDef = object
    name: string
    props: seq[tuple[name: string, typ: string, default: string]]
    state: seq[tuple[name: string, typ: string, initial: string]]
    templateNode: JsonNode

  TsxContext = object
    componentDefs: Table[string, ComponentDef]
    logicFunctions: JsonNode
    eventBindings: JsonNode
    indentLevel: int

# =============================================================================
# Property Mapping
# =============================================================================

proc mapKirPropertyToTsx(key: string): string =
  ## Map .kir property names to TSX property names
  case key
  of "backgroundColor", "background": "background"
  of "windowTitle", "title": "title"
  of "flexDirection": "flexDirection"
  of "justifyContent": "justifyContent"
  of "alignItems": "alignItems"
  of "borderRadius": "borderRadius"
  of "borderWidth": "borderWidth"
  of "borderColor": "borderColor"
  of "fontSize": "fontSize"
  of "fontWeight": "fontWeight"
  of "fontFamily": "fontFamily"
  of "color": "color"
  of "opacity": "opacity"
  of "gap": "gap"
  of "padding": "padding"
  of "paddingTop": "paddingTop"
  of "paddingBottom": "paddingBottom"
  of "paddingLeft": "paddingLeft"
  of "paddingRight": "paddingRight"
  of "margin": "margin"
  of "width": "width"
  of "height": "height"
  of "text": "text"
  of "placeholder": "placeholder"
  of "selectedIndex": "selectedIndex"
  of "options": "options"
  of "initialValue": "initialValue"
  else: key

proc formatTsxValue(key: string, val: JsonNode): string =
  ## Format a JSON value as TSX property value
  case val.kind
  of JString:
    var s = val.getStr()
    # Check for dimension values
    if s.endsWith("px"):
      let num = s[0..^3]
      try:
        let f = parseFloat(num)
        if f == float(int(f)):
          return "{" & $int(f) & "}"
        else:
          return "{" & num & "}"
      except:
        return "\"" & s & "\""
    elif s.endsWith("%"):
      # Clean up "100.0%" -> "100%"
      let num = s[0..^2]
      try:
        let f = parseFloat(num)
        if f == float(int(f)):
          s = $int(f) & "%"
      except:
        discard
      return "\"" & s & "\""
    elif s.startsWith("#"):
      return "\"" & s & "\""
    elif s.startsWith("{{") and s.endsWith("}}"):
      # Template expression: {{value}} -> {String(value)}
      let varName = s[2..^3]
      return "{String(" & varName & ")}"
    else:
      return "\"" & s & "\""
  of JInt:
    return "{" & $val.getInt() & "}"
  of JFloat:
    return "{" & $val.getFloat() & "}"
  of JBool:
    return "{" & (if val.getBool(): "true" else: "false") & "}"
  of JArray:
    # Array of strings for options
    var items: seq[string] = @[]
    for item in val:
      if item.kind == JString:
        items.add("\"" & item.getStr() & "\"")
      else:
        items.add($item)
    return "{[" & items.join(", ") & "]}"
  else:
    return "\"\""

proc isDefaultValue(key: string, val: JsonNode): bool =
  ## Check if a value is a default that should be omitted
  case key
  of "minWidth", "minHeight", "maxWidth", "maxHeight":
    return val.kind == JString and (val.getStr() == "0.0px" or val.getStr() == "0px")
  of "direction":
    return val.kind == JString and val.getStr() == "auto"
  of "flexShrink", "flexGrow":
    return val.kind == JInt and val.getInt() == 0
  of "color", "background":
    return val.kind == JString and val.getStr() == "#00000000"
  of "fontFamily":
    return val.kind == JString and val.getStr() == "sans-serif"
  of "alignItems":
    return val.kind == JString and val.getStr() == "stretch"
  else:
    return false

# =============================================================================
# Logic Expression to TSX
# =============================================================================

proc generateTsxExpression(expr: JsonNode): string =
  ## Convert a universal expression AST to TSX/JavaScript code
  if expr.kind == JInt:
    return $expr.getInt()
  if expr.kind == JFloat:
    return $expr.getFloat()
  if expr.kind == JString:
    return "\"" & expr.getStr() & "\""
  if expr.kind == JBool:
    return (if expr.getBool(): "true" else: "false")

  if expr.kind != JObject:
    return "null"

  # Variable reference: {"var": "name"}
  if expr.hasKey("var"):
    return expr["var"].getStr()

  # Binary operations
  let op = expr{"op"}.getStr("")
  case op
  of "add":
    let left = generateTsxExpression(expr["left"])
    let right = generateTsxExpression(expr["right"])
    return &"({left} + {right})"
  of "sub":
    let left = generateTsxExpression(expr["left"])
    let right = generateTsxExpression(expr["right"])
    return &"({left} - {right})"
  of "mul":
    let left = generateTsxExpression(expr["left"])
    let right = generateTsxExpression(expr["right"])
    return &"({left} * {right})"
  of "div":
    let left = generateTsxExpression(expr["left"])
    let right = generateTsxExpression(expr["right"])
    return &"({left} / {right})"
  of "not":
    let operand = if expr.hasKey("operand"): expr["operand"] else: expr["expr"]
    return &"(!{generateTsxExpression(operand)})"
  else:
    return "null"

proc generateHandlerBody(functions: JsonNode, handlerId: string, stateVarName: string): string =
  ## Generate the body of a click handler
  ## Note: functions is the "logic.functions" object, not the full logic object
  if functions.isNil or functions.kind != JObject:
    return ""

  if not functions.hasKey(handlerId):
    return ""

  let funcDef = functions[handlerId]
  if not funcDef.hasKey("universal"):
    return ""

  let universal = funcDef["universal"]
  if not universal.hasKey("statements") or universal["statements"].len == 0:
    return ""

  # Convert statements to JS
  for stmt in universal["statements"]:
    if stmt{"op"}.getStr() == "assign":
      let target = stmt["target"].getStr()
      let expr = stmt["expr"]

      # Check for increment/decrement patterns
      if expr{"op"}.getStr() == "add":
        let left = expr{"left"}
        let right = expr{"right"}
        if not left.isNil and left.hasKey("var") and left["var"].getStr() == target:
          if right.kind == JInt:
            let setterName = "set" & target[0].toUpperAscii & target[1..^1]
            return &"{setterName}({target} + {right.getInt()})"
      elif expr{"op"}.getStr() == "sub":
        let left = expr{"left"}
        let right = expr{"right"}
        if not left.isNil and left.hasKey("var") and left["var"].getStr() == target:
          if right.kind == JInt:
            let setterName = "set" & target[0].toUpperAscii & target[1..^1]
            return &"{setterName}({target} - {right.getInt()})"

      # Generic assignment
      let setterName = "set" & target[0].toUpperAscii & target[1..^1]
      return &"{setterName}({generateTsxExpression(expr)})"

  return ""

# =============================================================================
# Component Generation
# =============================================================================

proc generateTsxElement(node: JsonNode, ctx: var TsxContext, indent: int): string

proc generateTsxProps(node: JsonNode, ctx: var TsxContext, hasTextExpression: bool = false): seq[string] =
  ## Generate TSX properties for a component
  result = @[]

  var skipProps = @["id", "type", "children", "events", "template", "loop_var",
                    "iterable", "minWidth", "minHeight", "maxWidth", "maxHeight",
                    "direction", "flexShrink", "flexGrow", "component_ref",
                    "text_expression", "windowTitle", "dropdown_state"]

  # Skip text if we have a text_expression (will be added separately)
  if hasTextExpression:
    skipProps.add("text")

  for key, val in node.pairs:
    if key in skipProps:
      continue
    if isDefaultValue(key, val):
      continue

    let tsxKey = mapKirPropertyToTsx(key)
    let tsxVal = formatTsxValue(key, val)

    # Skip empty or null values
    if tsxVal in ["\"\"", "null", "{0}", "\"\""]:
      if key != "selectedIndex":  # selectedIndex=0 is valid
        continue

    result.add(&"{tsxKey}={tsxVal}")

proc generateTsxChildren(node: JsonNode, ctx: var TsxContext, indent: int): string =
  ## Generate TSX children elements
  if not node.hasKey("children") or node["children"].len == 0:
    return ""

  result = ""
  for child in node["children"]:
    result.add("\n")
    result.add(generateTsxElement(child, ctx, indent))

proc generateTsxElement(node: JsonNode, ctx: var TsxContext, indent: int): string =
  ## Generate a single TSX element
  if node.kind != JObject:
    return ""

  let spaces = "  ".repeat(indent)
  let compType = node{"type"}.getStr("div")

  # Check if this is a custom component (unused for now)
  discard compType in ctx.componentDefs

  # Check if we have a text_expression
  let hasTextExpression = node.hasKey("text_expression")

  # Get properties
  var props = generateTsxProps(node, ctx, hasTextExpression)

  # Handle events
  if node.hasKey("events"):
    for event in node["events"]:
      if event["type"].getStr() == "click":
        let logicId = event{"logic_id"}.getStr("")
        if logicId != "":
          let handlerBody = generateHandlerBody(ctx.logicFunctions, logicId, "value")
          if handlerBody != "":
            props.add(&"onClick={{() => {handlerBody}}}")
          else:
            props.add("onClick={() => console.log('click')}")

  # Handle text_expression (reactive text)
  if hasTextExpression:
    let expr = node["text_expression"].getStr()
    let varName = expr[2..^3]  # Remove {{ and }}
    props.add(&"text={{String({varName})}}")

  # Generate children
  let children = generateTsxChildren(node, ctx, indent + 1)

  # Build element
  if children.len == 0:
    # Self-closing element
    if props.len == 0:
      result = &"{spaces}<{compType} />"
    elif props.len <= 3:
      result = &"{spaces}<{compType} {props.join(\" \")} />"
    else:
      result = &"{spaces}<{compType}\n"
      for i, prop in props:
        result.add(&"{spaces}  {prop}")
        if i < props.len - 1:
          result.add("\n")
      result.add(&"\n{spaces}/>"  )
  else:
    # Element with children
    if props.len == 0:
      result = &"{spaces}<{compType}>{children}\n{spaces}</{compType}>"
    elif props.len <= 3:
      result = &"{spaces}<{compType} {props.join(\" \")}>{children}\n{spaces}</{compType}>"
    else:
      result = &"{spaces}<{compType}\n"
      for i, prop in props:
        result.add(&"{spaces}  {prop}")
        if i < props.len - 1:
          result.add("\n")
      result.add(&"\n{spaces}>{children}\n{spaces}</{compType}>")

# =============================================================================
# Component Definition Generation
# =============================================================================

proc generateComponentFunction(def: ComponentDef, ctx: var TsxContext): string =
  ## Generate a React function component from a component definition
  let name = def.name

  # Generate interface for props
  result = &"interface {name}Props {{\n"
  for prop in def.props:
    let tsType = case prop.typ
      of "int", "float": "number"
      of "string": "string"
      of "bool": "boolean"
      else: "any"
    if prop.default != "":
      result.add(&"  {prop.name}?: {tsType};\n")
    else:
      result.add(&"  {prop.name}: {tsType};\n")
  result.add("}\n\n")

  # Generate function signature
  result.add(&"function {name}({{ ")
  var propParams: seq[string] = @[]
  for prop in def.props:
    if prop.default != "":
      propParams.add(&"{prop.name} = {prop.default}")
    else:
      propParams.add(prop.name)
  result.add(propParams.join(", "))
  result.add(&" }}: {name}Props) {{\n")

  # Generate state hooks
  for stateVar in def.state:
    let setterName = "set" & stateVar.name[0].toUpperAscii & stateVar.name[1..^1]
    var initialVal = stateVar.initial
    # Handle {"var": "propName"} references
    if initialVal.startsWith("{\"var\":\""):
      initialVal = initialVal[8..^3]  # Extract prop name
    result.add(&"  const [{stateVar.name}, {setterName}] = useState({initialVal});\n")

  result.add("\n  return (\n")

  # Generate template
  if not def.templateNode.isNil:
    result.add(generateTsxElement(def.templateNode, ctx, 2))

  result.add("\n  );\n}\n")

# =============================================================================
# Main Generation
# =============================================================================

proc extractWindowConfig(component: JsonNode): tuple[width: int, height: int, title: string, background: string] =
  ## Extract window configuration from root component
  var width = 800
  var height = 600
  var title = "Kryon App"
  var background = "#1E1E1E"

  if component.hasKey("width"):
    let w = component["width"].getStr("800")
    if w.endsWith("px"):
      try:
        width = int(parseFloat(w[0..^3]))
      except:
        discard

  if component.hasKey("height"):
    let h = component["height"].getStr("600")
    if h.endsWith("px"):
      try:
        height = int(parseFloat(h[0..^3]))
      except:
        discard

  if component.hasKey("windowTitle"):
    title = component["windowTitle"].getStr("Kryon App")
  elif component.hasKey("title"):
    title = component["title"].getStr("Kryon App")

  if component.hasKey("background"):
    background = component["background"].getStr("#1E1E1E")

  return (width, height, title, background)

proc generateTsxFromKir*(kirPath: string): string =
  ## Generate TSX code from .kir file (React-Hooks style)
  if not fileExists(kirPath):
    raise newException(IOError, "File not found: " & kirPath)

  let kirJson = parseFile(kirPath)

  # Build context
  var ctx = TsxContext(
    componentDefs: initTable[string, ComponentDef](),
    logicFunctions: if kirJson.hasKey("logic") and kirJson["logic"].hasKey("functions"):
                      kirJson["logic"]["functions"]
                    else:
                      newJObject(),
    eventBindings: if kirJson.hasKey("logic") and kirJson["logic"].hasKey("event_bindings"):
                     kirJson["logic"]["event_bindings"]
                   else:
                     newJArray(),
    indentLevel: 0
  )

  # Parse component definitions
  if kirJson.hasKey("component_definitions"):
    for compDef in kirJson["component_definitions"]:
      var def = ComponentDef(
        name: compDef["name"].getStr(),
        props: @[],
        state: @[],
        templateNode: if compDef.hasKey("template"): compDef["template"] else: nil
      )

      if compDef.hasKey("props"):
        for prop in compDef["props"]:
          var defaultVal = ""
          if prop.hasKey("default"):
            let defaultNode = prop["default"]
            case defaultNode.kind
            of JInt: defaultVal = $defaultNode.getInt()
            of JFloat: defaultVal = $defaultNode.getFloat()
            of JString: defaultVal = defaultNode.getStr()
            of JBool: defaultVal = if defaultNode.getBool(): "true" else: "false"
            else: discard
          def.props.add((
            name: prop["name"].getStr(),
            typ: prop{"type"}.getStr("any"),
            default: defaultVal
          ))

      if compDef.hasKey("state"):
        for stateVar in compDef["state"]:
          def.state.add((
            name: stateVar["name"].getStr(),
            typ: stateVar{"type"}.getStr("any"),
            initial: stateVar{"initial"}.getStr("0")
          ))

      ctx.componentDefs[def.name] = def

  # Get root and window config
  let root = if kirJson.hasKey("root"): kirJson["root"]
             elif kirJson.hasKey("component"): kirJson["component"]
             else: kirJson

  let (width, height, title, background) = extractWindowConfig(root)

  # Generate TSX file
  result = &"// Generated from {extractFilename(kirPath)}\n"
  result.add("// Generated by Kryon Code Generator (React-Hooks Style)\n\n")
  result.add("import { kryonApp, Column, Row, Button, Text, useState } from '@kryon/react';\n\n")

  # Generate component definitions
  for name, def in ctx.componentDefs:
    result.add(generateComponentFunction(def, ctx))
    result.add("\n")

  # Generate main app export
  result.add("export default kryonApp({\n")
  result.add(&"  width: {width},\n")
  result.add(&"  height: {height},\n")
  result.add(&"  title: \"{title}\",\n")
  result.add(&"  background: \"{background}\",\n")
  result.add("\n  render: () => (\n")

  # Generate root children (skip the outer container, just render children)
  if root.hasKey("children") and root["children"].len > 0:
    for child in root["children"]:
      result.add(generateTsxElement(child, ctx, 2))
      result.add("\n")

  result.add("  )\n")
  result.add("});\n")

# CLI entry point for standalone use
when isMainModule:
  import os

  if paramCount() < 1:
    echo "Usage: codegen_tsx <input.kir> [output.tsx]"
    quit(1)

  let inputPath = paramStr(1)
  let outputPath = if paramCount() >= 2: paramStr(2)
                   else: inputPath.changeFileExt("tsx")

  try:
    let tsxCode = generateTsxFromKir(inputPath)
    writeFile(outputPath, tsxCode)
    echo &"Generated: {outputPath}"
  except IOError as e:
    echo &"Error: {e.msg}"
    quit(1)
