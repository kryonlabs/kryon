## .kir v2.1 → Lua Code Generator
## Generates Lua code from Kryon IR v2.1 format (with reactive manifest)

import std/[json, strformat, strutils, tables]

type
  LuaCodeGenerator* = object
    indent: int
    output: string

proc newLuaCodeGenerator*(): LuaCodeGenerator =
  result.indent = 0
  result.output = ""

proc addLine(gen: var LuaCodeGenerator, line: string) =
  gen.output.add(repeat("  ", gen.indent) & line & "\n")

proc increaseIndent(gen: var LuaCodeGenerator) =
  inc gen.indent

proc decreaseIndent(gen: var LuaCodeGenerator) =
  dec gen.indent

proc generateReactiveVars(gen: var LuaCodeGenerator, manifest: JsonNode) =
  ## Generate reactive variable declarations
  if not manifest.hasKey("variables"):
    return

  let vars = manifest["variables"]
  if vars.len == 0:
    return

  gen.addLine("")
  gen.addLine("-- Reactive State")

  for varNode in vars:
    let name = varNode["name"].getStr()
    let varType = varNode["type"].getStr()
    let initialValue = varNode["initial_value"].getStr()

    # Parse and format initial value
    let luaValue = case varType
      of "int": initialValue
      of "float": initialValue
      of "string": initialValue  # Already quoted
      of "bool": initialValue
      else: "nil"

    gen.addLine(fmt"local {name} = Reactive.state({luaValue})")

  gen.addLine("")

proc generateComponentTree(gen: var LuaCodeGenerator, comp: JsonNode, varName: string = "root"): string =
  ## Recursively generate component tree code using smart DSL syntax
  let compType = comp["type"].getStr()

  # Map IR type to Lua Smart DSL component
  let dslFunc = case compType
    of "Row": "Row"
    of "Column": "Column"
    of "Text": "Text"
    of "Button": "Button"
    of "Checkbox": "Checkbox"
    of "Input": "Input"
    of "TabGroup": "TabGroup"
    of "TabBar": "TabBar"
    of "TabContent": "TabContent"
    of "TabPanel": "TabPanel"
    else: "Container"

  gen.addLine(fmt"local {varName} = UI.{dslFunc} {{")
  gen.increaseIndent()

  # Generate properties (string keys)
  if comp.hasKey("width"):
    let width = comp["width"].getStr()
    if width != "0.0px":
      gen.addLine(fmt"width = \"{width}\",")

  if comp.hasKey("height"):
    let height = comp["height"].getStr()
    if height != "0.0px":
      gen.addLine(fmt"height = \"{height}\",")

  if comp.hasKey("padding") and comp["padding"].getInt() > 0:
    gen.addLine(fmt"padding = {comp[\"padding\"].getInt()},")

  if comp.hasKey("fontSize") and comp["fontSize"].getInt() > 0:
    gen.addLine(fmt"fontSize = {comp[\"fontSize\"].getInt()},")

  if comp.hasKey("fontWeight") and comp["fontWeight"].getInt() != 400:
    gen.addLine(fmt"fontWeight = {comp[\"fontWeight\"].getInt()},")

  if comp.hasKey("text"):
    let text = comp["text"].getStr()
    gen.addLine(fmt"text = \"{text}\",")

  if comp.hasKey("backgroundColor"):
    let bg = comp["backgroundColor"].getStr()
    gen.addLine(fmt"backgroundColor = \"{bg}\",")

  if comp.hasKey("color"):
    let color = comp["color"].getStr()
    gen.addLine(fmt"color = \"{color}\",")

  if comp.hasKey("flexDirection"):
    let flexDir = comp["flexDirection"].getStr()
    if flexDir == "column":
      gen.addLine("layoutDirection = \"column\",")

  # Generate children using Nim-like DSL (children as array indices, NOT children = {...})
  if comp.hasKey("children") and comp["children"].len > 0:
    # Add blank line before children for readability
    gen.addLine("")

    for i, child in comp["children"]:
      let childVar = fmt"{varName}_child{i}"
      discard gen.generateComponentTree(child, childVar)
      gen.addLine(fmt"{childVar},")

  gen.decreaseIndent()
  gen.addLine("}")

  return varName

proc generateLuaCode*(kirJson: JsonNode): string =
  ## Main entry point - generates complete Lua file from .kir v2.1 JSON
  var gen = newLuaCodeGenerator()

  # Header
  gen.addLine("-- Generated from .kir v2.1 by Kryon Code Generator")
  gen.addLine("-- Uses Smart DSL syntax for clean, readable code")
  gen.addLine("-- Do not edit manually - regenerate from source")
  gen.addLine("")
  gen.addLine("local Reactive = require(\"kryon.reactive\")")
  gen.addLine("local UI = require(\"kryon.dsl\")")

  # Generate reactive variables if manifest exists
  if kirJson.hasKey("reactive_manifest"):
    gen.generateReactiveVars(kirJson["reactive_manifest"])

  # Generate component tree
  gen.addLine("-- UI Component Tree")
  if kirJson.hasKey("component"):
    discard gen.generateComponentTree(kirJson["component"], "root")

  # Footer
  gen.addLine("")
  gen.addLine("return root")

  return gen.output

proc generateLuaFromFile*(kirPath: string): string =
  ## Read .kir file and generate Lua code
  let content = readFile(kirPath)
  let kirJson = parseJson(content)

  # Verify format version
  if not kirJson.hasKey("format_version"):
    raise newException(ValueError, "Not a valid .kir file (missing format_version)")

  let version = kirJson["format_version"].getStr()
  if version != "2.1":
    raise newException(ValueError, fmt"Unsupported .kir version: {version} (expected 2.1)")

  return generateLuaCode(kirJson)
