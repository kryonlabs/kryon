0BSD# 0BSD
## Kryon Plugin Bindings Generator
## Automatically generates Nim bindings from JSON specifications

import std/[json, os, strutils, strformat, tables]

type
  PluginSpec = object
    name: string
    version: string
    description: string
    header: string
    library: string

  TypeMapping = Table[string, string]

  Parameter = object
    name: string
    paramType: string
    description: string
    hasDefault: bool
    defaultValue: string

  FunctionSpec = object
    cName: string
    nimName: string
    description: string
    parameters: seq[Parameter]
    returnType: string

  BindingSpec = object
    plugin: PluginSpec
    types: TypeMapping
    functions: seq[FunctionSpec]

proc parseBindingSpec(jsonPath: string): BindingSpec =
  ## Parse JSON binding specification
  let jsonStr = readFile(jsonPath)
  let root = parseJson(jsonStr)

  # Parse plugin metadata
  let pluginNode = root["plugin"]
  result.plugin = PluginSpec(
    name: pluginNode["name"].getStr(),
    version: pluginNode["version"].getStr(),
    description: pluginNode["description"].getStr(),
    header: pluginNode["header"].getStr(),
    library: pluginNode["library"].getStr()
  )

  # Parse type mappings
  result.types = initTable[string, string]()
  let typesNode = root["types"]
  for key, val in typesNode.pairs:
    result.types[key] = val["nim"].getStr()

  # Parse functions
  result.functions = @[]
  let functionsNode = root["functions"]
  for funcNode in functionsNode:
    var funcSpec = FunctionSpec(
      cName: funcNode["c_name"].getStr(),
      nimName: funcNode["nim_name"].getStr(),
      description: funcNode["description"].getStr(),
      returnType: funcNode["return_type"].getStr(),
      parameters: @[]
    )

    # Parse parameters
    for paramNode in funcNode["parameters"]:
      let paramType = paramNode["type"].getStr()
      var param = Parameter(
        name: paramNode["name"].getStr(),
        paramType: paramType,
        description: paramNode["description"].getStr(),
        hasDefault: paramNode.hasKey("default"),
        defaultValue: if paramNode.hasKey("default"): paramNode["default"].getStr() else: ""
      )
      funcSpec.parameters.add(param)

    result.functions.add(funcSpec)

proc nimType(spec: BindingSpec, typeName: string): string =
  ## Get Nim type for a given type name
  if spec.types.hasKey(typeName):
    return spec.types[typeName]
  elif typeName == "bool":
    return "bool"
  elif typeName == "void":
    return "void"
  else:
    return typeName

proc generateFileHeader(spec: BindingSpec): string =
  ## Generate file header with metadata
  result = &"""# 0BSD
## {spec.plugin.name} Plugin Bindings for Nim
## Auto-generated from bindings.json - DO NOT EDIT MANUALLY
##
## {spec.plugin.description}
## Version: {spec.plugin.version}

import strutils

# Link plugin libraries
{{.passL: "{spec.plugin.library}".}}

"""

proc generateCImports(spec: BindingSpec): string =
  ## Generate importc declarations for C functions
  result = "# ============================================================================\n"
  result &= "# C Function Imports\n"
  result &= "# ============================================================================\n\n"

  for funcSpec in spec.functions:
    # Generate function signature
    var params: seq[string] = @[]
    for param in funcSpec.parameters:
      let nimT = spec.nimType(param.paramType)
      params.add(&"{param.name}: {nimT}")

    let paramStr = params.join(", ")
    let returnType = if funcSpec.returnType == "void": "" else: ": " & funcSpec.returnType

    result &= &"""proc {funcSpec.cName}*({paramStr}){returnType} {{.
  importc: "{funcSpec.cName}",
  header: "{spec.plugin.header}".}}

"""

proc generateColorParser(spec: BindingSpec): string =
  ## Generate color parsing helper
  result = """# ============================================================================
# Helper Functions
# ============================================================================

proc parseColor*(hex: string): uint32 =
  ## Convert hex color string (#RRGGBB or #RRGGBBAA) to uint32 RGBA
  var h = hex
  if h.startsWith("#"):
    h = h[1..^1]

  if h.len == 6:
    # RGB - add full alpha
    let r = parseHexInt(h[0..1])
    let g = parseHexInt(h[2..3])
    let b = parseHexInt(h[4..5])
    result = (r.uint32 shl 24) or (g.uint32 shl 16) or (b.uint32 shl 8) or 0xFF'u32
  elif h.len == 8:
    # RGBA
    let r = parseHexInt(h[0..1])
    let g = parseHexInt(h[2..3])
    let b = parseHexInt(h[4..5])
    let a = parseHexInt(h[6..7])
    result = (r.uint32 shl 24) or (g.uint32 shl 16) or (b.uint32 shl 8) or a.uint32
  else:
    # Default to white
    result = 0xFFFFFFFF'u32

"""

proc generateHighLevelWrappers(spec: BindingSpec): string =
  ## Generate high-level Nim wrapper functions
  result = "# ============================================================================\n"
  result &= "# High-Level Wrappers\n"
  result &= "# ============================================================================\n\n"

  for funcSpec in spec.functions:
    # Generate documentation comment
    result &= &"proc {funcSpec.nimName}*("

    # Generate parameters with type conversion
    var paramParts: seq[string] = @[]
    var callArgs: seq[string] = @[]

    for param in funcSpec.parameters:
      let nimT = spec.nimType(param.paramType)

      # Special handling for color type (string -> uint32)
      let wrapperType = if param.paramType == "color": "string" else: nimT
      let wrapperParam = if param.hasDefault:
        &"{param.name}: {wrapperType} = {param.defaultValue}"
      else:
        &"{param.name}: {wrapperType}"

      paramParts.add(wrapperParam)

      # Generate call argument with type conversion
      if param.paramType == "color":
        callArgs.add(&"parseColor({param.name})")
      elif param.paramType == "kryon_fp_t":
        callArgs.add(&"{param.name}.int32")
      else:
        callArgs.add(param.name)

    result &= paramParts.join(", ")
    result &= ") =\n"
    result &= &"  ## {funcSpec.description}\n"

    # Add parameter documentation
    for param in funcSpec.parameters:
      result &= &"  ## - {param.name}: {param.description}\n"

    result &= &"  {funcSpec.cName}({callArgs.join(\", \")})\n\n"

proc generateBindings(spec: BindingSpec): string =
  ## Generate complete Nim bindings file
  result = generateFileHeader(spec)
  result &= generateCImports(spec)
  result &= generateColorParser(spec)
  result &= generateHighLevelWrappers(spec)

proc main() =
  if paramCount() < 1:
    echo "Usage: generate_nim_bindings <bindings.json> [output.nim]"
    echo ""
    echo "Generates Nim bindings from a JSON plugin specification."
    echo ""
    echo "Example:"
    echo "  nim c -r tools/generate_nim_bindings.nim plugins/canvas/bindings.json"
    quit(1)

  let jsonPath = paramStr(1)
  if not fileExists(jsonPath):
    echo &"Error: File not found: {jsonPath}"
    quit(1)

  echo &"Parsing binding specification: {jsonPath}"
  let spec = parseBindingSpec(jsonPath)

  echo &"Generating Nim bindings for: {spec.plugin.name} v{spec.plugin.version}"
  echo &"  Description: {spec.plugin.description}"
  echo &"  Functions: {spec.functions.len}"

  let bindings = generateBindings(spec)

  # Determine output path
  let outputPath = if paramCount() >= 2:
    paramStr(2)
  else:
    # Default: bindings/nim/{plugin}_generated.nim
    &"bindings/nim/{spec.plugin.name}_generated.nim"

  echo &"Writing bindings to: {outputPath}"

  # Ensure output directory exists
  let outputDir = parentDir(outputPath)
  if not dirExists(outputDir):
    createDir(outputDir)

  writeFile(outputPath, bindings)

  echo ""
  echo "✓ Bindings generated successfully!"
  echo ""
  echo "Usage in Nim code:"
  echo &"  import {spec.plugin.name}_generated"
  echo ""

  # Print example
  if spec.functions.len > 0:
    echo "Example:"
    let firstFunc = spec.functions[0]
    echo &"  {firstFunc.nimName}(...)"

when isMainModule:
  main()
