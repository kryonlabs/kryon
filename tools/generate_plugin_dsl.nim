## Kryon Plugin DSL Generator
## Automatically generates Nim DSL macros from plugin binding schemas
##
## Usage:
##   generate_plugin_dsl <bindings.json> <output_dir>
##
## Example:
##   generate_plugin_dsl plugin/bindings/bindings.json plugin/bindings/nim/

import json, os, strutils, strformat, tables

type
  PropertyType = enum
    ptString, ptInt, ptFloat, ptBool, ptEnum, ptColor, ptDimension

  Property = object
    name: string
    propType: PropertyType
    required: bool
    default: string
    description: string
    irFunction: string
    irParamTransform: string
    handler: string
    enumValues: seq[string]

  Component = object
    name: string
    componentType: int
    irType: string
    description: string
    properties: seq[Property]

  Helper = object
    name: string
    description: string
    signature: string
    implementation: string
    cases: Table[string, JsonNode]
    default: JsonNode

proc parsePropertyType(typeStr: string): PropertyType =
  case typeStr.toLowerAscii()
  of "string": ptString
  of "int", "integer": ptInt
  of "float", "number": ptFloat
  of "bool", "boolean": ptBool
  of "enum": ptEnum
  of "color": ptColor
  of "dimension": ptDimension
  else: ptString

proc parseProperty(propJson: JsonNode): Property =
  result.name = propJson["name"].getStr()
  result.propType = parsePropertyType(propJson["type"].getStr())
  result.required = propJson.getOrDefault("required").getBool(false)
  result.description = propJson.getOrDefault("description").getStr("")
  result.irFunction = propJson.getOrDefault("ir_function").getStr("")
  result.irParamTransform = propJson.getOrDefault("ir_param_transform").getStr("")
  result.handler = propJson.getOrDefault("handler").getStr("")

  if propJson.hasKey("default"):
    let defaultNode = propJson["default"]
    if defaultNode.kind == JString:
      result.default = defaultNode.getStr()
    elif defaultNode.kind == JInt:
      result.default = $defaultNode.getInt()
    elif defaultNode.kind == JFloat:
      result.default = $defaultNode.getFloat()
    elif defaultNode.kind == JBool:
      result.default = $defaultNode.getBool()

  if propJson.hasKey("values"):
    for val in propJson["values"]:
      result.enumValues.add(val.getStr())

proc parseComponent(compJson: JsonNode): Component =
  result.name = compJson["name"].getStr()
  result.componentType = compJson["type"].getInt()
  result.irType = compJson["ir_type"].getStr()
  result.description = compJson.getOrDefault("description").getStr("")

  for propJson in compJson["properties"]:
    result.properties.add(parseProperty(propJson))

proc parseHelper(name: string, helperJson: JsonNode): Helper =
  result.name = name
  result.description = helperJson.getOrDefault("description").getStr("")
  result.signature = helperJson.getOrDefault("signature").getStr("")
  result.implementation = helperJson.getOrDefault("implementation").getStr("")

  if helperJson.hasKey("cases"):
    let casesJson = helperJson["cases"]
    for key, val in casesJson.pairs:
      result.cases[key] = val

  if helperJson.hasKey("default"):
    result.default = helperJson["default"]

proc generateHelperFunction(helper: Helper): string =
  ## Generate Nim code for a helper function
  result = &"proc {helper.name}(value: string): IRColor =\n"
  result &= &"  ## {helper.description}\n"

  if helper.implementation == "case_mapping":
    # Add hex color parsing support
    result &= "  # Try hex color first (#RGB, #RRGGBB, #RRGGBBAA)\n"
    result &= "  if value.len > 0 and value[0] == '#':\n"
    result &= "    let hex = value[1..^1]\n"
    result &= "    try:\n"
    result &= "      case hex.len\n"
    result &= "      of 3:  # #RGB -> #RRGGBB\n"
    result &= "        let r = uint8(parseHexInt(hex[0..0] & hex[0..0]))\n"
    result &= "        let g = uint8(parseHexInt(hex[1..1] & hex[1..1]))\n"
    result &= "        let b = uint8(parseHexInt(hex[2..2] & hex[2..2]))\n"
    result &= "        return IRColor(r: r, g: g, b: b, a: 255)\n"
    result &= "      of 6:  # #RRGGBB\n"
    result &= "        let r = uint8(parseHexInt(hex[0..1]))\n"
    result &= "        let g = uint8(parseHexInt(hex[2..3]))\n"
    result &= "        let b = uint8(parseHexInt(hex[4..5]))\n"
    result &= "        return IRColor(r: r, g: g, b: b, a: 255)\n"
    result &= "      of 8:  # #RRGGBBAA\n"
    result &= "        let r = uint8(parseHexInt(hex[0..1]))\n"
    result &= "        let g = uint8(parseHexInt(hex[2..3]))\n"
    result &= "        let b = uint8(parseHexInt(hex[4..5]))\n"
    result &= "        let a = uint8(parseHexInt(hex[6..7]))\n"
    result &= "        return IRColor(r: r, g: g, b: b, a: a)\n"
    result &= "      else: discard\n"
    result &= "    except: discard\n\n"
    result &= "  # Named colors\n"
    result &= "  case value.toLowerAscii()\n"
    for key, val in helper.cases:
      let r = val["r"].getInt()
      let g = val["g"].getInt()
      let b = val["b"].getInt()
      let a = val["a"].getInt()
      result &= &"  of \"{key}\": IRColor(r: {r}, g: {g}, b: {b}, a: {a})\n"

    if helper.default != nil:
      let r = helper.default["r"].getInt()
      let g = helper.default["g"].getInt()
      let b = helper.default["b"].getInt()
      let a = helper.default["a"].getInt()
      result &= &"  else: IRColor(r: {r}, g: {g}, b: {b}, a: {a})\n"

proc generatePropertyParsing(prop: Property): string =
  ## Generate Nim code to parse a property from the DSL
  let propName = prop.name
  let caseBranch = &"of \"{propName}\":"

  case prop.propType
  of ptString:
    if prop.required:
      result = &"{caseBranch}\n          if propValue.kind in {{nnkStrLit, nnkTripleStrLit, nnkRStrLit}}:\n            {propName} = propValue.strVal"
    else:
      result = &"{caseBranch}\n          if propValue.kind in {{nnkStrLit, nnkTripleStrLit, nnkRStrLit}}:\n            {propName} = propValue.strVal\n            has{propName.capitalizeAscii()} = true"

  of ptInt:
    result = &"{caseBranch}\n          if propValue.kind == nnkIntLit:\n            {propName} = propValue.intVal\n            has{propName.capitalizeAscii()} = true"

  of ptFloat:
    result = &"{caseBranch}\n          if propValue.kind in {{nnkFloatLit, nnkIntLit}}:\n            {propName} = propValue.floatVal\n            has{propName.capitalizeAscii()} = true"

  of ptBool:
    result = &"{caseBranch}\n          if propValue.kind in {{nnkIdent, nnkSym}}:\n            {propName} = propValue.strVal == \"true\"\n            has{propName.capitalizeAscii()} = true"

  of ptEnum:
    result = &"{caseBranch}\n          if propValue.kind in {{nnkStrLit, nnkTripleStrLit, nnkRStrLit}}:\n            {propName} = propValue.strVal\n            has{propName.capitalizeAscii()} = true"

  of ptColor:
    result = &"{caseBranch}\n          if propValue.kind in {{nnkStrLit, nnkTripleStrLit, nnkRStrLit}}:\n            {propName} = propValue.strVal\n            has{propName.capitalizeAscii()} = true"

  of ptDimension:
    result = &"{caseBranch}\n          if propValue.kind in {{nnkStrLit, nnkTripleStrLit, nnkRStrLit}}:\n            {propName} = propValue.strVal\n            has{propName.capitalizeAscii()} = true"

proc needsStyle(irFunction: string): bool =
  ## Check if an IR function operates on style rather than component
  irFunction in ["ir_set_background_color", "ir_set_padding", "ir_set_width", "ir_set_height",
                 "ir_set_margin", "ir_set_border", "ir_set_font", "ir_set_opacity"]

proc generatePropertyApplication(prop: Property, componentVar: string): string =
  ## Generate Nim code to apply a property to the IR component (inside quote block)
  if prop.irFunction.len == 0:
    return ""

  let target = if needsStyle(prop.irFunction): &"{componentVar}Style" else: componentVar
  result = ""

  case prop.irParamTransform
  of "cstring":
    result &= &"    {prop.irFunction}({target}, cstring(`{prop.name}`))"

  of "rgba_components":
    result &= &"    block:\n"
    result &= &"      let color{prop.name.capitalizeAscii()} = parseColor(`{prop.name}`)\n"
    result &= &"      {prop.irFunction}({target}, color{prop.name.capitalizeAscii()}.r, color{prop.name.capitalizeAscii()}.g, color{prop.name.capitalizeAscii()}.b, color{prop.name.capitalizeAscii()}.a)"

  of "uniform_edges":
    result &= &"    {prop.irFunction}({target}, float32(`{prop.name}`), float32(`{prop.name}`), float32(`{prop.name}`), float32(`{prop.name}`))"

  of "dimension_value_unit":
    result &= &"    block:\n"
    result &= &"      if `{prop.name}`.endsWith(\"%\"):\n"
    result &= &"        let val = parseFloat(`{prop.name}`[0..^2])\n"
    result &= &"        {prop.irFunction}({target}, IR_DIMENSION_PERCENT, cfloat(val))\n"
    result &= &"      elif `{prop.name}` == \"auto\":\n"
    result &= &"        {prop.irFunction}({target}, IR_DIMENSION_AUTO, 0)\n"
    result &= &"      else:\n"
    result &= &"        let val = parseFloat(`{prop.name}`)\n"
    result &= &"        {prop.irFunction}({target}, IR_DIMENSION_PX, cfloat(val))"

  of "dimension_int_pixels":
    result &= &"    {prop.irFunction}({target}, IR_DIMENSION_PX, cfloat(`{prop.name}`))"

  else:
    result &= &"    # Unknown transform: {prop.irParamTransform}"

proc generateDSLMacro(component: Component): string =
  ## Generate the complete DSL macro for a component
  let macroName = component.name
  let compVar = component.name.toLowerAscii() & "Comp"

  result = &"\nmacro {macroName}*(props: untyped): untyped =\n"
  result &= &"  ## Auto-generated DSL macro for {component.name} component\n"
  result &= &"  ## {component.description}\n\n"

  # Declare variables for all properties
  for prop in component.properties:
    case prop.propType
    of ptString:
      if prop.default.len > 0:
        result &= &"  var {prop.name} = \"{prop.default}\"\n"
      else:
        result &= &"  var {prop.name} = \"\"\n"
    of ptInt:
      if prop.default.len > 0:
        result &= &"  var {prop.name} = {prop.default}\n"
      else:
        result &= &"  var {prop.name} = 0\n"
    of ptFloat:
      if prop.default.len > 0:
        result &= &"  var {prop.name} = {prop.default}\n"
      else:
        result &= &"  var {prop.name} = 0.0\n"
    of ptBool:
      if prop.default.len > 0:
        result &= &"  var {prop.name} = {prop.default}\n"
      else:
        result &= &"  var {prop.name} = false\n"
    of ptEnum, ptColor, ptDimension:
      if prop.default.len > 0:
        result &= &"  var {prop.name} = \"{prop.default}\"\n"
      else:
        result &= &"  var {prop.name} = \"\"\n"

    if not prop.required:
      result &= &"  var has{prop.name.capitalizeAscii()} = false\n"

  result &= "\n  # Parse properties from DSL block\n"
  result &= "  if props.kind == nnkStmtList:\n"
  result &= "    for stmt in props:\n"
  result &= "      if stmt.kind == nnkAsgn:\n"
  result &= "        let propName = stmt[0].strVal\n"
  result &= "        let propValue = stmt[1]\n"
  result &= "        case propName\n"

  for prop in component.properties:
    result &= "        " & generatePropertyParsing(prop) & "\n"

  result &= "        else:\n"
  result &= &"          warning(\"Unknown {macroName} property: \" & propName)\n\n"

  # Check if any properties need style
  var needsStyleObj = false
  for prop in component.properties:
    if prop.irFunction.len > 0 and needsStyle(prop.irFunction):
      needsStyleObj = true
      break

  # Generate component creation code
  result &= "  # Generate component creation code\n"
  result &= "  result = quote do:\n"
  result &= &"    let {compVar} = ir_create_component(IRComponentType({component.componentType}))\n"

  # Create style if needed
  if needsStyleObj:
    result &= &"    let {compVar}Style = ir_create_style()\n"

  # Apply properties
  for prop in component.properties:
    let appCode = generatePropertyApplication(prop, compVar)
    if appCode.len > 0:
      # Wrap optional properties in conditional
      if not prop.required:
        let hasFlag = &"has{prop.name.capitalizeAscii()}"
        result &= &"    if `{hasFlag}`:\n"
        result &= appCode.replace("    ", "      ") & "\n"
      else:
        result &= appCode & "\n"

  # Set style on component if we created one
  if needsStyleObj:
    result &= &"    ir_set_style({compVar}, {compVar}Style)\n"

  # Return component
  result &= &"    {compVar}\n\n"

proc generateBindingFile(schema: JsonNode, outputPath: string) =
  ## Generate the complete Nim binding file from schema
  var output = "## AUTO-GENERATED from bindings.json - DO NOT EDIT\n"
  output &= "## Kryon Plugin DSL Bindings\n\n"

  output &= "import macros, strutils, os\n\n"

  # Import Kryon IR types from the core bindings
  output &= "# Import Kryon IR core types and functions\n"
  output &= "# Use KRYON_ROOT environment variable to locate Kryon installation\n"
  output &= "const KRYON_ROOT = getEnv(\"HOME\") & \"/Projects/kryon\"\n"
  output &= "{.passC: \"-I\" & KRYON_ROOT & \"/ir\".}\n"
  output &= "{.passL: \"-L\" & KRYON_ROOT & \"/build -lkryon_ir\".}\n\n"

  output &= "# Import IR types from Kryon's bindings\n"
  output &= "import ir_core\n\n"

  # Define color helper type
  output &= "# Color helper type for parseColor functions\n"
  output &= "type IRColor* = object\n"
  output &= "  r*, g*, b*, a*: uint8\n\n"

  # Generate IR constants (these are local to the plugin)
  output &= "# IR Size Units\n"
  output &= "const\n"
  output &= "  IR_SIZE_UNIT_PIXELS* = 0'u32\n"
  output &= "  IR_SIZE_UNIT_PERCENT* = 1'u32\n"
  output &= "  IR_SIZE_UNIT_AUTO* = 2'u32\n\n"

  # Generate helper functions
  if schema.hasKey("helpers"):
    output &= "# Helper Functions\n"
    for name, helperJson in schema["helpers"].pairs:
      let helper = parseHelper(name, helperJson)
      output &= generateHelperFunction(helper) & "\n"

  # Generate DSL macros for each component
  if schema.hasKey("components"):
    output &= "# DSL Macros\n"
    for compJson in schema["components"]:
      let component = parseComponent(compJson)
      output &= generateDSLMacro(component)
      output &= &"export {component.name}\n\n"

  # Write output file
  writeFile(outputPath, output)
  echo &"✓ Generated DSL bindings: {outputPath}"

proc main() =
  if paramCount() < 2:
    echo "Usage: generate_plugin_dsl <bindings.json> <output_dir>"
    echo "Example: generate_plugin_dsl plugin/bindings/bindings.json plugin/bindings/nim/"
    quit(1)

  let schemaPath = paramStr(1)
  let outputDir = paramStr(2)

  if not fileExists(schemaPath):
    echo &"Error: Schema file not found: {schemaPath}"
    quit(1)

  let schema = parseFile(schemaPath)
  let pluginName = schema["plugin"]["name"].getStr()
  let outputPath = outputDir / &"{pluginName}.nim"

  echo &"Generating DSL bindings for {pluginName}..."
  generateBindingFile(schema, outputPath)
  echo "✓ DSL generation complete"

when isMainModule:
  main()
