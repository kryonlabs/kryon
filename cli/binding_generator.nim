## Binding Generator
##
## Automatically generates language bindings from bindings.json specification

import std/[os, json, strformat, strutils, sequtils, tables]

type
  BindingSpec = object
    pluginName: string
    version: string
    description: string
    functions: seq[FunctionSpec]
    dslMacros: Table[string, MacroSpec]
    types: Table[string, TypeSpec]

  FunctionSpec = object
    name: string
    cName: string
    description: string
    returnType: TypeInfo
    parameters: seq[ParameterSpec]

  ParameterSpec = object
    name: string
    cType: string
    nimType: string
    description: string
    defaultValue: string

  TypeInfo = object
    cType: string
    nimType: string

  MacroSpec = object
    properties: seq[PropertySpec]
    example: string

  PropertySpec = object
    name: string
    propType: string
    required: bool
    defaultValue: string
    values: seq[string]
    description: string

  TypeSpec = object
    kind: string  # "enum", "struct", etc.
    values: seq[string]

# =============================================================================
# JSON Parsing
# =============================================================================

proc parseBindingSpec(path: string): BindingSpec =
  ## Parse bindings.json file
  if not fileExists(path):
    raise newException(IOError, &"Bindings file not found: {path}")

  let jsonNode = parseFile(path)
  result = BindingSpec()

  # Parse plugin metadata
  if jsonNode.hasKey("plugin"):
    let pluginNode = jsonNode["plugin"]
    result.pluginName = pluginNode["name"].getStr()
    result.version = pluginNode["version"].getStr()
    result.description = pluginNode["description"].getStr()

  # Parse functions
  if jsonNode.hasKey("functions"):
    result.functions = @[]
    for funcNode in jsonNode["functions"]:
      var funcSpec = FunctionSpec(
        name: funcNode["name"].getStr(),
        cName: funcNode["c_name"].getStr(),
        description: funcNode["description"].getStr()
      )

      # Parse return type
      if funcNode.hasKey("returns"):
        let retNode = funcNode["returns"]
        funcSpec.returnType = TypeInfo(
          cType: retNode["type"].getStr(),
          nimType: retNode["nim_type"].getStr()
        )

      # Parse parameters
      if funcNode.hasKey("parameters"):
        funcSpec.parameters = @[]
        for paramNode in funcNode["parameters"]:
          var param = ParameterSpec(
            name: paramNode["name"].getStr(),
            cType: paramNode["type"].getStr(),
            nimType: paramNode["nim_type"].getStr(),
            description: paramNode["description"].getStr()
          )
          if paramNode.hasKey("default"):
            param.defaultValue = paramNode["default"].getStr()
          funcSpec.parameters.add(param)

      result.functions.add(funcSpec)

  # Parse DSL macros
  result.dslMacros = initTable[string, MacroSpec]()
  if jsonNode.hasKey("dsl_macros") and jsonNode["dsl_macros"].hasKey("nim"):
    let nimMacros = jsonNode["dsl_macros"]["nim"]
    for macroName, macroNode in nimMacros.pairs:
      var macroSpec = MacroSpec()

      if macroNode.hasKey("properties"):
        macroSpec.properties = @[]
        for propNode in macroNode["properties"]:
          var prop = PropertySpec(
            name: propNode["name"].getStr(),
            propType: propNode["type"].getStr(),
            required: propNode["required"].getBool(),
            description: propNode["description"].getStr()
          )

          if propNode.hasKey("default"):
            prop.defaultValue = propNode["default"].getStr()

          if propNode.hasKey("values"):
            prop.values = @[]
            for val in propNode["values"]:
              prop.values.add(val.getStr())

          macroSpec.properties.add(prop)

      if macroNode.hasKey("example"):
        macroSpec.example = macroNode["example"].getStr()

      result.dslMacros[macroName] = macroSpec

  # Parse types
  result.types = initTable[string, TypeSpec]()
  if jsonNode.hasKey("types"):
    for typeName, typeNode in jsonNode["types"].pairs:
      var typeSpec = TypeSpec(
        kind: typeNode["kind"].getStr()
      )

      if typeNode.hasKey("values"):
        typeSpec.values = @[]
        for val in typeNode["values"]:
          typeSpec.values.add(val.getStr())

      result.types[typeName] = typeSpec

# =============================================================================
# Nim Code Generation
# =============================================================================

proc generateNimBindings(spec: BindingSpec, pluginDir: string): string =
  ## Generate Nim bindings from specification
  result = &"""## {spec.pluginName} Plugin - Nim Bindings
##
## {spec.description}
## Version: {spec.version}
##
## This file is auto-generated from bindings.json

import std/[strformat]
import ../../../bindings/nim/ir_core
import ../../../bindings/nim/runtime

# =============================================================================
# Dynamic Library Loading
# =============================================================================

const pluginLib = "{spec.pluginName}"

when defined(windows):
  const libName = pluginLib & ".dll"
elif defined(macosx):
  const libName = "lib" & pluginLib & ".dylib"
else:
  const libName = "lib" & pluginLib & ".so"

# Use relative path to find plugin library in plugin's build directory
const pluginPath = currentSourcePath().parentDir() / ".." / ".." / "build" / libName

{{.pragma: pluginImport, importc, dynlib: pluginPath.}}

"""

  # Generate type definitions
  if spec.types.len > 0:
    result.add("\n# =============================================================================\n")
    result.add("# Type Definitions\n")
    result.add("# =============================================================================\n\n")

    for typeName, typeSpec in spec.types:
      if typeSpec.kind == "enum":
        result.add(&"type {typeName}* {{.pure.}} = enum\n")
        for val in typeSpec.values:
          result.add(&"  {val}\n")
        result.add("\n")

  # Generate function imports
  if spec.functions.len > 0:
    result.add("# =============================================================================\n")
    result.add("# C Function Imports\n")
    result.add("# =============================================================================\n\n")

    for funcSpec in spec.functions:
      # Build parameter list
      var params: seq[string] = @[]
      for param in funcSpec.parameters:
        params.add(&"{param.name}: {param.nimType}")

      let paramList = params.join(", ")
      let returnType = funcSpec.returnType.nimType

      result.add(&"proc {funcSpec.cName}({paramList}): {returnType} {{.pluginImport.}}\n")

    result.add("\n")

    # Generate high-level wrappers
    result.add("# =============================================================================\n")
    result.add("# High-Level Wrappers\n")
    result.add("# =============================================================================\n\n")

    for funcSpec in spec.functions:
      var params: seq[string] = @[]
      var args: seq[string] = @[]

      for param in funcSpec.parameters:
        if param.defaultValue.len > 0:
          params.add(&"{param.name}: {param.nimType} = {param.defaultValue}")
        else:
          params.add(&"{param.name}: {param.nimType}")
        args.add(param.name)

      let paramList = params.join(", ")
      let argList = args.join(", ")
      let returnType = funcSpec.returnType.nimType

      result.add(&"proc {funcSpec.name}*({paramList}): {returnType} =\n")
      result.add(&"  ## {funcSpec.description}\n")
      result.add(&"  result = {funcSpec.cName}({argList})\n\n")

  # Generate DSL macros
  if spec.dslMacros.len > 0:
    result.add("# =============================================================================\n")
    result.add("# DSL Macros\n")
    result.add("# =============================================================================\n\n")

    for macroName, macroSpec in spec.dslMacros:
      result.add(&"macro {macroName}*(props: untyped): untyped =\n")
      result.add(&"  ## Auto-generated DSL macro for {macroName}\n")

      # Add example in comments
      if macroSpec.example.len > 0:
        result.add("  ##\n")
        result.add("  ## Example:\n")
        for line in macroSpec.example.split("\n"):
          result.add(&"  ## {line}\n")

      result.add("  var propValues = newTree(nnkStmtList)\n\n")

      # Generate property extraction
      for prop in macroSpec.properties:
        let defaultVal = if prop.defaultValue.len > 0: &"\"{prop.defaultValue}\"" else: "\"\""
        result.add(&"  var {prop.name}Val = {defaultVal}\n")

      result.add("\n")
      result.add("  # Extract properties from DSL block\n")
      result.add("  for child in props:\n")
      result.add("    if child.kind == nnkAsgn:\n")
      result.add("      let propName = $child[0]\n")
      result.add("      let propValue = child[1]\n")
      result.add("      case propName\n")

      for prop in macroSpec.properties:
        result.add(&"      of \"{prop.name}\":\n")
        result.add(&"        {prop.name}Val = propValue.strVal\n")

      result.add("      else:\n")
      result.add("        discard\n\n")

      result.add("  # TODO: Generate component creation code\n")
      result.add("  result = newNilLit()\n\n")

  return result

proc generateNimBindingsForPlugin*(pluginDir: string): bool =
  ## Generate Nim bindings for a plugin
  let bindingsJsonPath = pluginDir / "bindings" / "bindings.json"
  let outputDir = pluginDir / "bindings" / "nim"

  if not fileExists(bindingsJsonPath):
    echo "  ⚠ No bindings.json found - skipping binding generation"
    return true  # Not an error

  try:
    # Parse binding specification
    let spec = parseBindingSpec(bindingsJsonPath)

    # Generate Nim bindings
    let nimCode = generateNimBindings(spec, pluginDir)

    # Ensure output directory exists
    if not dirExists(outputDir):
      createDir(outputDir)

    # Write bindings file
    let outputPath = outputDir / (spec.pluginName & ".nim")
    writeFile(outputPath, nimCode)

    echo &"  ✓ Generated Nim bindings: {outputPath}"
    return true

  except CatchableError as e:
    echo &"  ✗ Failed to generate Nim bindings: {e.msg}"
    return false

# =============================================================================
# CLI Interface
# =============================================================================

when isMainModule:
  import std/parseopt

  var pluginDir = ""

  for kind, key, val in getopt():
    case kind
    of cmdArgument:
      if pluginDir.len == 0:
        pluginDir = key
    of cmdLongOption, cmdShortOption:
      discard
    of cmdEnd:
      break

  if pluginDir.len == 0:
    echo "Usage: binding_generator <plugin_directory>"
    quit(1)

  if not dirExists(pluginDir):
    echo &"Error: Directory not found: {pluginDir}"
    quit(1)

  echo &"Generating bindings for plugin in: {pluginDir}"

  if generateNimBindingsForPlugin(pluginDir):
    echo "✓ Binding generation complete"
    quit(0)
  else:
    echo "✗ Binding generation failed"
    quit(1)
