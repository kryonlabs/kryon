## Nim Frontend to IR Compiler
##
## Compiles existing Kryon DSL to IR format for universal backend targeting

import macros, strutils, sequtils
import ../../ir/ir_core

# C bindings for IR library
{.pragma: ir_import, cdecl, importc.}
type
  IRComponentC* {.ir_import.} = object
  IRStyleC* {.ir_import.} = object
  IREventC* {.ir_import.} = object
  IRLogicC* {.ir_import.} = object
  IRContextC* {.ir_import.} = object

# C function declarations
proc ir_create_context*(): IRContextC {.ir_import.}
proc ir_destroy_context*(context: IRContextC) {.ir_import.}
proc ir_set_context*(context: IRContextC) {.ir_import.}

proc ir_create_component*(component_type: IRComponentType): IRComponentC {.ir_import.}
proc ir_create_component_with_id*(component_type: IRComponentType, id: uint32): IRComponentC {.ir_import.}
proc ir_destroy_component*(component: IRComponentC) {.ir_import.}

proc ir_add_child*(parent: IRComponentC, child: IRComponentC) {.ir_import.}
proc ir_remove_child*(parent: IRComponentC, child: IRComponentC) {.ir_import.}

proc ir_create_style*(): IRStyleC {.ir_import.}
proc ir_destroy_style*(style: IRStyleC) {.ir_import.}
proc ir_set_style*(component: IRComponentC, style: IRStyleC) {.ir_import.}

proc ir_set_width*(style: IRStyleC, dim_type: IRDimensionType, value: float32) {.ir_import.}
proc ir_set_height*(style: IRStyleC, dim_type: IRDimensionType, value: float32) {.ir_import.}
proc ir_set_background_color*(style: IRStyleC, r, g, b, a: uint8) {.ir_import.}
proc ir_set_border*(style: IRStyleC, width: float32, r, g, b, a: uint8, radius: uint8) {.ir_import.}
proc ir_set_margin*(style: IRStyleC, top, right, bottom, left: float32) {.ir_import.}
proc ir_set_padding*(style: IRStyleC, top, right, bottom, left: float32) {.ir_import.}
proc ir_set_font*(style: IRStyleC, size: float32, family: cstring, r, g, b, a: uint8, bold, italic: bool) {.ir_import.}

proc ir_create_layout*(): pointer {.ir_import.}
proc ir_destroy_layout*(layout: pointer) {.ir_import.}
proc ir_set_layout*(component: IRComponentC, layout: pointer) {.ir_import.}
proc ir_set_flexbox*(layout: pointer, wrap: bool, gap: uint32, main_axis, cross_axis: IRAlignment) {.ir_import.}
proc ir_set_justify_content*(layout: pointer, justify: IRAlignment) {.ir_import.}

proc ir_create_event*(event_type: IREventType, logic_id: cstring, handler_data: cstring): IREventC {.ir_import.}
proc ir_destroy_event*(event: IREventC) {.ir_import.}
proc ir_add_event*(component: IRComponentC, event: IREventC) {.ir_import.}

proc ir_create_logic*(id: cstring, logic_type: LogicSourceType, source_code: cstring): IRLogicC {.ir_import.}
proc ir_destroy_logic*(logic: IRLogicC) {.ir_import.}
proc ir_add_logic*(component: IRComponentC, logic: IRLogicC) {.ir_import.}

proc ir_set_text_content*(component: IRComponentC, text: cstring) {.ir_import.}
proc ir_set_tag*(component: IRComponentC, tag: cstring) {.ir_import.}

# Helper conversion functions
proc mapKryonTypeToIR*(kryon_type: string): IRComponentType =
  ## Maps Kryon component types to IR component types
  case kryon_type.toLowerAscii():
    of "container": result = IR_COMPONENT_CONTAINER
    of "text": result = IR_COMPONENT_TEXT
    of "button": result = IR_COMPONENT_BUTTON
    of "input": result = IR_COMPONENT_INPUT
    of "checkbox": result = IR_COMPONENT_CHECKBOX
    of "row": result = IR_COMPONENT_ROW
    of "column": result = IR_COMPONENT_COLUMN
    of "center": result = IR_COMPONENT_CENTER
    of "image": result = IR_COMPONENT_IMAGE
    of "canvas": result = IR_COMPONENT_CANVAS
    else: result = IR_COMPONENT_CONTAINER  # Default to container

proc mapAlignmentToIR*(alignment: string): IRAlignment =
  ## Maps alignment strings to IR alignment types
  case alignment.toLowerAscii():
    of "start", "left", "top": result = IR_ALIGNMENT_START
    of "center", "middle": result = IR_ALIGNMENT_CENTER
    of "end", "right", "bottom": result = IR_ALIGNMENT_END
    of "stretch": result = IR_ALIGNMENT_STRETCH
    else: result = IR_ALIGNMENT_START  # Default

proc mapDimensionToIR*(value: string): tuple[`type`: IRDimensionType, value: float32] =
  ## Parses dimension values (e.g., "100px", "50%", "auto", "1.0flex")
  if value.endsWith("px"):
    result.type = IR_DIMENSION_PX
    result.value = parseFloat(value[0..<value.len-2])
  elif value.endsWith("%"):
    result.type = IR_DIMENSION_PERCENT
    result.value = parseFloat(value[0..<value.len-1])
  elif value == "auto":
    result.type = IR_DIMENSION_AUTO
    result.value = 0.0
  elif value.endsWith("flex"):
    result.type = IR_DIMENSION_FLEX
    result.value = parseFloat(value[0..<value.len-4])
  else:
    # Assume pixels if no unit specified
    result.type = IR_DIMENSION_PX
    result.value = parseFloat(value)

proc mapColorToIR*(color: string): tuple[r, g, b, a: uint8] =
  ## Parses color strings (e.g., "#RRGGBB", "#AARRGGBB", "rgba(r,g,b,a)")
  if color.startsWith("#"):
    let hex = if color.len == 7: color[1..6] & "FF"  # Add alpha if missing
              elif color.len == 9: color[1..8]
              else: "FFFFFFFF"

    result.r = parseHexInt(hex[0..1]).uint8
    result.g = parseHexInt(hex[2..3]).uint8
    result.b = parseHexInt(hex[4..5]).uint8
    result.a = parseHexInt(hex[6..7]).uint8
  elif color.startsWith("rgba(") and color.endsWith(")"):
    let values = color[5..<color.len-1].split(",")
    if values.len >= 3:
      result.r = parseInt(values[0]).uint8
      result.g = parseInt(values[1]).uint8
      result.b = parseInt(values[2]).uint8
      result.a = if values.len >= 4: parseInt(values[3]).uint8 else: 255.uint8
  else:
    # Default to black
    result = (0'u8, 0'u8, 0'u8, 255'u8)

# Main compilation functions
proc createIRComponent*(component_type: string, properties: Table[string, string]): IRComponentC =
  ## Creates an IR component from type and properties
  let ir_type = mapKryonTypeToIR(component_type)
  result = ir_create_component(ir_type)

  # Set tag if specified
  if properties.contains("tag"):
    ir_set_tag(result, properties["tag"])

  # Set text content for text-based components
  if component_type.toLowerAscii() in ["text", "button"] and properties.contains("text"):
    ir_set_text_content(result, properties["text"])

proc applyStyleToComponent*(component: IRComponentC, properties: Table[string, string]) =
  ## Applies style properties to an IR component
  var style = ir_create_style()

  # Apply dimensions
  if properties.contains("width"):
    let dim = mapDimensionToIR(properties["width"])
    ir_set_width(style, dim.`type`, dim.value)

  if properties.contains("height"):
    let dim = mapDimensionToIR(properties["height"])
    ir_set_height(style, dim.`type`, dim.value)

  # Apply background color
  if properties.contains("backgroundColor"):
    let color = mapColorToIR(properties["backgroundColor"])
    ir_set_background_color(style, color.r, color.g, color.b, color.a)
  elif properties.contains("color"):
    let color = mapColorToIR(properties["color"])
    ir_set_background_color(style, color.r, color.g, color.b, color.a)

  # Apply border
  if properties.contains("borderWidth") or properties.contains("borderColor"):
    let width = if properties.contains("borderWidth"): parseFloat(properties["borderWidth"]) else: 1.0
    let color = if properties.contains("borderColor"): mapColorToIR(properties["borderColor"]) else: (0,0,0,255)
    let radius = if properties.contains("borderRadius"): parseInt(properties["borderRadius"]).uint8 else: 0'u8
    ir_set_border(style, width, color.r, color.g, color.b, color.a, radius)

  # Apply margin
  if properties.contains("margin"):
    let margin_str = properties["margin"]
    let margins = margin_str.split(",")
    case margins.len:
      of 1:
        let value = parseFloat(margins[0])
        ir_set_margin(style, value, value, value, value)
      of 2:
        let v = parseFloat(margins[0])
        let h = parseFloat(margins[1])
        ir_set_margin(style, v, h, v, h)
      of 4:
        let t = parseFloat(margins[0])
        let r = parseFloat(margins[1])
        let b = parseFloat(margins[2])
        let l = parseFloat(margins[3])
        ir_set_margin(style, t, r, b, l)

  # Apply padding
  if properties.contains("padding"):
    let padding_str = properties["padding"]
    let paddings = padding_str.split(",")
    case paddings.len:
      of 1:
        let value = parseFloat(paddings[0])
        ir_set_padding(style, value, value, value, value)
      of 2:
        let v = parseFloat(paddings[0])
        let h = parseFloat(paddings[1])
        ir_set_padding(style, v, h, v, h)
      of 4:
        let t = parseFloat(paddings[0])
        let r = parseFloat(paddings[1])
        let b = parseFloat(paddings[2])
        let l = parseFloat(paddings[3])
        ir_set_padding(style, t, r, b, l)

  # Apply font
  if properties.contains("fontSize") or properties.contains("fontColor") or properties.contains("fontFamily"):
    let size = if properties.contains("fontSize"): parseFloat(properties["fontSize"]) else: 16.0
    let color = if properties.contains("fontColor"): mapColorToIR(properties["fontColor"]) else: (0,0,0,255)
    let family = if properties.contains("fontFamily"): properties["fontFamily"] else: nil
    let bold = properties.getOrDefault("fontWeight", "normal").toLowerAscii() == "bold"
    let italic = properties.getOrDefault("fontStyle", "normal").toLowerAscii() == "italic"

    ir_set_font(style, size, family, color.r, color.g, color.b, color.a, bold, italic)

  ir_set_style(component, style)

proc applyLayoutToComponent*(component: IRComponentC, properties: Table[string, string]) =
  ## Applies layout properties to an IR component
  var layout = ir_create_layout()

  # Apply flexbox properties
  let wrap = properties.getOrDefault("flexWrap", "nowrap") == "wrap"
  let gap = if properties.contains("gap"): parseInt(properties["gap"]).uint32 else: 0
  let main_axis = mapAlignmentToIR(properties.getOrDefault("mainAxisAlignment", "start"))
  let cross_axis = mapAlignmentToIR(properties.getOrDefault("crossAxisAlignment", "start"))

  ir_set_flexbox(layout, wrap, gap, main_axis, cross_axis)
  ir_set_justify_content(layout, main_axis)
  ir_set_layout(component, layout)

proc applyEventsToComponent*(component: IRComponentC, properties: Table[string, string]) =
  ## Applies event handlers to an IR component
  if properties.contains("onClick"):
    let event = ir_create_event(IR_EVENT_CLICK, "click_handler", nil)
    ir_add_event(component, event)

  if properties.contains("onHover"):
    let event = ir_create_event(IR_EVENT_HOVER, "hover_handler", nil)
    ir_add_event(component, event)

  # TODO: Handle other event types and actual handler compilation

# Macro-based DSL compilation
macro compileKryonToIR*(dsl_node: untyped): untyped =
  ## Main macro that compiles Kryon DSL to IR components
  result = newStmtList()

  # Extract component type from first identifier
  let component_type = $dsl_node[0]

  # Extract properties from named parameters
  var properties = nnkTableConstr.newTree()

  # Process DSL node properties
  for child in dsl_node.children:
    if child.kind == nnkExprEqExpr:
      # Property assignment: property = value
      let prop_name = child[0]
      let prop_value = child[1]

      # Convert Nim AST to string representation
      var value_str: string
      case prop_value.kind:
        of nnkStrLit:
          value_str = prop_value.strVal
        of nnkIntLit:
          value_str = $prop_value.intVal
        of nnkFloatLit:
          value_str = $prop_value.floatVal
        else:
          value_str = "complex_expression"  # TODO: Handle complex expressions

      properties.add nnkExprColonExpr.newTree(
        newLit($prop_name),
        newLit(value_str)
      )

  # Generate IR creation code
  let create_call = newCall(
    ident"createIRComponent",
    newLit(component_type),
    properties
  )

  let apply_style = newCall(
    ident"applyStyleToComponent",
    create_call,
    properties
  )

  let apply_layout = newCall(
    ident"applyLayoutToComponent",
    create_call,
    properties
  )

  let apply_events = newCall(
    ident"applyEventsToComponent",
    create_call,
    properties
  )

  result.add(create_call)
  result.add(apply_style)
  result.add(apply_layout)
  result.add(apply_events)

# High-level compilation functions
proc compileToIR*(kryon_ast: NimNode): IRComponentC =
  ## Compiles a complete Kryon AST to IR
  var context = ir_create_context()
  ir_set_context(context)

  # Process the root component
  let ir_node = compileKryonToIR(kryon_ast)

  result = ir_node
  # Note: context cleanup should be handled by the caller

proc writeIRToFile*(component: IRComponentC, filename: string) =
  ## Serializes IR component to binary file
  # This would call ir_write_binary_file when C bindings are complete
  echo "Writing IR to file: ", filename

proc readIRFromFile*(filename: string): IRComponentC =
  ## Deserializes IR component from binary file
  # This would call ir_read_binary_file when C bindings are complete
  echo "Reading IR from file: ", filename
  result = ir_create_component(IR_COMPONENT_CONTAINER)

# Utility functions for debugging
proc printIRComponent*(component: IRComponentC) =
  ## Prints information about an IR component for debugging
  echo "IR Component Info:"
  # This would call ir_print_component_info when C bindings are complete
  echo "  Type: Container"
  echo "  ID: 1"

proc validateIRComponent*(component: IRComponentC): bool =
  ## Validates an IR component structure
  # This would call ir_validate_component when C bindings are complete
  result = true

# Compilation example
when isMainModule:
  echo "Kryon IR Compiler for Nim"
  echo "This module compiles Kryon DSL to IR format"

  # Example compilation (would be done through the macro system in practice)
  echo "Example: Button with styling"

  # Simulated DSL compilation
  var button_props = {
    "width": "100px",
    "height": "40px",
    "backgroundColor": "#007BFF",
    "text": "Click Me",
    "fontSize": "16px",
    "onClick": "handleClick"
  }.toTable

  let ir_button = createIRComponent("button", button_props)
  applyStyleToComponent(ir_button, button_props)
  applyEventsToComponent(ir_button, button_props)

  echo "Successfully compiled button to IR component"
  printIRComponent(ir_button)