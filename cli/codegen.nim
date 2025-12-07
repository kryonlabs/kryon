import json, strformat, os, strutils, tables, sets

# =============================================================================
# Helper: Extract window configuration from root component
# =============================================================================

proc parseDimensionValue(value: string): int =
  ## Parse "600.0px" or "600" to integer
  var s = value.strip()
  if s.endsWith("px"):
    s = s[0 ..< s.len - 2]
  try:
    result = int(parseFloat(s))
  except:
    result = 800  # default

proc extractWindowConfig(component: JsonNode): tuple[width: int, height: int, title: string] =
  ## Extract window dimensions and title from root component
  var width = 800
  var height = 600
  var title = "Kryon App"

  if component.hasKey("width"):
    width = parseDimensionValue(component["width"].getStr("800"))
  if component.hasKey("height"):
    height = parseDimensionValue(component["height"].getStr("600"))
  if component.hasKey("title"):
    title = component["title"].getStr("Kryon App")
  elif component.hasKey("windowTitle"):
    title = component["windowTitle"].getStr("Kryon App")

  return (width, height, title)

# =============================================================================
# Conditional Context for code generation
# =============================================================================

type
  ConditionalInfo = object
    condition: string
    thenChildrenIds: seq[int]
    elseChildrenIds: seq[int]

  CodegenContext = object
    # Map: parent_id -> conditionals that apply to it
    conditionalsByParent: Table[int, seq[ConditionalInfo]]
    # Set of all conditional child IDs (for quick lookup)
    allConditionalChildIds: HashSet[int]
    # Logic functions for resolving handler names
    logicFunctions: JsonNode

proc buildCodegenContext(kirJson: JsonNode): CodegenContext =
  ## Build context from reactive_manifest for conditional generation
  result = CodegenContext(
    conditionalsByParent: initTable[int, seq[ConditionalInfo]](),
    allConditionalChildIds: initHashSet[int](),
    logicFunctions: if kirJson.hasKey("logic") and kirJson["logic"].hasKey("functions"):
                      kirJson["logic"]["functions"]
                    else:
                      newJObject()
  )

  if not kirJson.hasKey("reactive_manifest"):
    return

  let manifest = kirJson["reactive_manifest"]
  if not manifest.hasKey("conditionals"):
    return

  for cond in manifest["conditionals"]:
    # Support both "component_id" and "parent_id" field names
    let parentId = if cond.hasKey("component_id"): cond["component_id"].getInt()
                   elif cond.hasKey("parent_id"): cond["parent_id"].getInt()
                   else: 0

    # Handle condition as string or object {"var": "name"}
    var condition = ""
    if cond.hasKey("condition"):
      let condNode = cond["condition"]
      if condNode.kind == JString:
        condition = condNode.getStr()
      elif condNode.kind == JObject and condNode.hasKey("var"):
        condition = condNode["var"].getStr()

    var info = ConditionalInfo(condition: condition)

    # Parse then_children_ids
    if cond.hasKey("then_children_ids"):
      for childId in cond["then_children_ids"]:
        let id = childId.getInt()
        info.thenChildrenIds.add(id)
        result.allConditionalChildIds.incl(id)

    # Parse else_children_ids
    if cond.hasKey("else_children_ids"):
      for childId in cond["else_children_ids"]:
        let id = childId.getInt()
        info.elseChildrenIds.add(id)
        result.allConditionalChildIds.incl(id)

    # Only add if there's actual conditional children
    if info.thenChildrenIds.len > 0 or info.elseChildrenIds.len > 0:
      if parentId notin result.conditionalsByParent:
        result.conditionalsByParent[parentId] = @[]
      result.conditionalsByParent[parentId].add(info)

proc indent(s: string, level: int): string =
  let spaces = "  ".repeat(level)
  result = ""
  for line in s.splitLines():
    if line.len > 0:
      result.add spaces & line & "\n"
    else:
      result.add "\n"

proc toLuaValue(val: JsonNode): string =
  case val.kind:
  of JString:
    return "\"" & val.getStr & "\""
  of JInt:
    return $val.getInt
  of JFloat:
    return $val.getFloat
  of JBool:
    return (if val.getBool: "true" else: "false")
  of JNull:
    return "nil"
  else:
    return "nil"

proc toNimValue(key: string, val: JsonNode): string =
  ## Convert JSON value to Nim DSL value, handling dimension strings
  case val.kind:
  of JString:
    let s = val.getStr
    # Convert dimension strings like "50.0px" or "100%" for width/height properties
    if key in ["width", "height", "minWidth", "minHeight", "maxWidth", "maxHeight",
               "gap", "padding", "margin", "borderRadius", "borderWidth",
               "paddingTop", "paddingBottom", "paddingLeft", "paddingRight",
               "marginTop", "marginBottom", "marginLeft", "marginRight"]:
      if s.endsWith("px"):
        # "50.0px" -> 50
        let numStr = s[0 ..< s.len - 2]
        try:
          return $int(parseFloat(numStr))
        except:
          return "\"" & s & "\""
      elif s.endsWith("%"):
        # "100%" -> 100.percent
        let numStr = s[0 ..< s.len - 1]
        try:
          return $int(parseFloat(numStr)) & ".percent"
        except:
          return "\"" & s & "\""
      elif s == "auto":
        return "auto"
    return "\"" & s & "\""
  of JInt:
    return $val.getInt
  of JFloat:
    return $val.getFloat
  of JBool:
    return (if val.getBool: "true" else: "false")
  of JNull:
    return "nil"
  else:
    return "nil"

proc generateLuaComponent(node: JsonNode, level: int = 0): string =
  if node.kind != JObject:
    return ""

  let compType = node{"type"}.getStr("Container")
  var props: seq[string] = @[]

  # Collect properties (skip structural fields)
  for key, val in node.pairs:
    if key notin ["id", "type", "children", "events", "minWidth", "minHeight",
                   "maxWidth", "maxHeight", "direction", "flexShrink", "flexGrow"]:
      # Map property names to Lua style
      let luaKey = case key:
        of "background": "background"
        of "color": "color"
        of "width": "width"
        of "height": "height"
        of "text": "text"
        of "fontSize": "fontSize"
        of "fontFamily": "fontFamily"
        of "flexDirection": "flexDirection"
        of "justifyContent": "justifyContent"
        of "alignItems": "alignItems"
        else: key

      let luaVal = toLuaValue(val)
      if luaVal != "nil":
        props.add &"{luaKey} = {luaVal}"

  # Handle children
  if node.hasKey("children"):
    var childrenCode = "children = {\n"
    for child in node["children"]:
      let childCode = generateLuaComponent(child, level + 2)
      childrenCode.add indent(childCode, 1)
    childrenCode.add indent("}", 0)
    props.add childrenCode

  # Handle events (generate placeholder handlers)
  if node.hasKey("events"):
    for event in node["events"]:
      if event["type"].getStr == "click":
        let handlerCode = """onClick = function()
  -- TODO: Implement click handler
  print("Button clicked!")
end"""
        props.add handlerCode

  # Build component constructor
  result = &"kryon.{compType}({{\n"
  result.add indent(props.join(",\n"), 1)
  result.add "})"

  if level == 0:
    result.add "\n"

proc generateLuaFromKir*(kirPath: string): string =
  ## Generate Lua code from .kir file (v2.0 and v2.1 support)
  if not fileExists(kirPath):
    raise newException(IOError, "File not found: " & kirPath)

  let kirJson = parseFile(kirPath)

  var luaCode = "-- Generated from " & extractFilename(kirPath) & "\n"
  luaCode.add "-- Generated by Kryon Code Generator\n"
  luaCode.add "local kryon = require(\"kryon\")\n\n"

  # Check for v2.1 format (with reactive manifest)
  if kirJson.hasKey("format_version") and kirJson["format_version"].getStr() == "2.1":
    luaCode.add "local Reactive = require(\"kryon.reactive\")\n\n"

    # Generate reactive variables if manifest exists
    if kirJson.hasKey("reactive_manifest"):
      let manifest = kirJson["reactive_manifest"]
      if manifest.hasKey("variables") and manifest["variables"].len > 0:
        luaCode.add "-- Reactive State\n"
        for varNode in manifest["variables"]:
          let varName = varNode["name"].getStr()
          let initialValue = varNode["initial_value"].getStr()
          luaCode.add &"local {varName} = Reactive.state({initialValue})\n"
        luaCode.add "\n"

    # Generate UI from component field
    if kirJson.hasKey("component"):
      luaCode.add "-- UI Component Tree\n"
      luaCode.add "return "
      luaCode.add generateLuaComponent(kirJson["component"])
    else:
      luaCode.add "return nil -- No component found in .kir file\n"
  else:
    # Legacy v2.0 format or no version
    luaCode.add "return "
    luaCode.add generateLuaComponent(kirJson)

  return luaCode

proc isDefaultValue(key: string, val: JsonNode): bool =
  ## Check if a value is a default that should be omitted from output
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

# Forward declaration
proc generateNimComponentWithContext(node: JsonNode, ctx: CodegenContext, indent: int = 0): string

# Generate Nim code for a static_for template (replacing {{var.prop}} with var.prop)
proc generateStaticTemplate(node: JsonNode, ctx: CodegenContext, loopVar: string, indent: int): string =
  if node.kind != JObject:
    return ""

  let spaces = "  ".repeat(indent)
  let compType = node{"type"}.getStr("Container")

  result = &"{spaces}discard {compType}:\n"

  let skipProps = ["id", "type", "children", "template", "loop_var", "iterable"]

  for key, val in node.pairs:
    if key in skipProps:
      continue

    var nimVal: string
    if val.kind == JString:
      let s = val.getStr()
      # Check for template reference: {{item.prop}} or {{item.colors[0]}}
      if s.startsWith("{{") and s.endsWith("}}"):
        let inner = s[2..^3]  # Remove {{ and }}
        nimVal = inner  # Use directly as Nim expression
      else:
        nimVal = "\"" & s & "\""
    else:
      nimVal = toNimValue(key, val)

    if nimVal != "nil" and nimVal != "\"auto\"" and nimVal != "0":
      result.add &"{spaces}  {key} = {nimVal}\n"

  # Generate children
  if node.hasKey("children") and node["children"].len > 0:
    for child in node["children"]:
      result.add "\n"
      result.add generateStaticTemplate(child, ctx, loopVar, indent + 1)

proc generateNimComponentWithContext(node: JsonNode, ctx: CodegenContext, indent: int = 0): string =
  ## Generate Nim DSL code from component JSON with conditional support
  if node.kind != JObject:
    return ""

  let spaces = "  ".repeat(indent)
  let compType = node{"type"}.getStr("Container")
  let nodeId = node{"id"}.getInt(0)

  # Handle static_for nodes
  if compType == "static_for":
    let loopVar = node{"loop_var"}.getStr("item")
    let iterable = node{"iterable"}.getStr("items")
    let tmpl = node{"template"}

    result = &"{spaces}static:\n"
    result.add &"{spaces}  for {loopVar} in {iterable}:\n"
    if tmpl.kind == JObject:
      result.add generateStaticTemplate(tmpl, ctx, loopVar, indent + 2)
    elif tmpl.kind == JArray:
      for t in tmpl:
        result.add generateStaticTemplate(t, ctx, loopVar, indent + 2)
    return result

  # Use component type directly (Row, Column, Container, etc.)
  let nimType = compType

  result = &"{spaces}{nimType}:\n"

  # Skip internal/structural properties
  let skipProps = ["id", "type", "children", "events"]

  # Generate properties (skip defaults and internal properties)
  for key, val in node.pairs:
    if key in skipProps:
      continue
    # Handle border object specially - extract borderRadius
    if key == "border" and val.kind == JObject:
      if val.hasKey("radius"):
        let radius = val["radius"].getInt
        if radius > 0:
          result.add &"{spaces}  borderRadius = {radius}\n"
      # TODO: Add borderWidth, borderColor if needed
      continue
    if isDefaultValue(key, val):
      continue
    let nimVal = toNimValue(key, val)
    if nimVal != "nil" and nimVal != "\"auto\"" and nimVal != "0":
      result.add &"{spaces}  {key} = {nimVal}\n"

  # Generate children with conditional handling
  if node.hasKey("children") and node["children"].len > 0:
    # Check if this node has conditionals
    let hasConditionals = nodeId in ctx.conditionalsByParent

    if hasConditionals:
      let conditionals = ctx.conditionalsByParent[nodeId]

      # Build set of conditional child IDs for this parent
      var thenChildIds: HashSet[int]
      var elseChildIds: HashSet[int]
      for cond in conditionals:
        for id in cond.thenChildrenIds:
          thenChildIds.incl(id)
        for id in cond.elseChildrenIds:
          elseChildIds.incl(id)

      # Generate regular children first (not in any conditional)
      for child in node["children"]:
        let childId = child{"id"}.getInt(0)
        if childId notin thenChildIds and childId notin elseChildIds:
          result.add "\n"
          result.add generateNimComponentWithContext(child, ctx, indent + 1)

      # Generate conditional blocks
      for cond in conditionals:
        if cond.thenChildrenIds.len > 0:
          result.add &"\n{spaces}  if {cond.condition}:\n"
          # Find and generate then-branch children
          for child in node["children"]:
            let childId = child{"id"}.getInt(0)
            if childId in cond.thenChildrenIds:
              result.add generateNimComponentWithContext(child, ctx, indent + 2)

        if cond.elseChildrenIds.len > 0:
          if cond.thenChildrenIds.len > 0:
            result.add &"{spaces}  else:\n"
          else:
            result.add &"\n{spaces}  if not {cond.condition}:\n"
          # Find and generate else-branch children
          for child in node["children"]:
            let childId = child{"id"}.getInt(0)
            if childId in cond.elseChildrenIds:
              result.add generateNimComponentWithContext(child, ctx, indent + 2)
    else:
      # No conditionals - generate all children normally
      for child in node["children"]:
        result.add "\n"
        result.add generateNimComponentWithContext(child, ctx, indent + 1)

  # Handle events
  if node.hasKey("events"):
    for event in node["events"]:
      if event["type"].getStr == "click":
        let logicId = event{"logic_id"}.getStr("")
        if logicId != "" and ctx.logicFunctions.hasKey(logicId):
          # Look up the handler function to get the actual handler name
          let fn = ctx.logicFunctions[logicId]
          var handlerName = ""
          if fn.hasKey("universal") and fn["universal"].hasKey("statements"):
            let stmts = fn["universal"]["statements"]
            if stmts.len > 0:
              let stmt = stmts[0]
              # Check for direct var reference: {"var": "handlerName"}
              if stmt.hasKey("var"):
                handlerName = stmt["var"].getStr()
              # Check for call pattern: {"op": "assign", "target": "_call", "expr": {"var": "handlerName"}}
              elif stmt.hasKey("op") and stmt["op"].getStr() == "assign":
                if stmt.hasKey("expr") and stmt["expr"].hasKey("var"):
                  handlerName = stmt["expr"]["var"].getStr()
          if handlerName != "":
            result.add &"{spaces}  onClick = {handlerName}\n"
            # Register handler name for round-trip serialization
            result.add &"{spaces}  onClickName = \"{handlerName}\"\n"
          else:
            result.add &"{spaces}  onClick = proc() =\n"
            result.add &"{spaces}    # Handler: {logicId}\n"
            result.add &"{spaces}    discard\n"
        else:
          result.add &"{spaces}  onClick = proc() =\n"
          result.add &"{spaces}    # TODO: Implement click handler\n"
          result.add &"{spaces}    echo \"Button clicked!\"\n"

proc generateKryonAppCode(root: JsonNode, ctx: CodegenContext): string =
  ## Generate kryonApp DSL code with Header and Body from root component
  let (width, height, title) = extractWindowConfig(root)

  # Get background color from root
  let bg = root{"background"}.getStr("#1e1e1e")

  result = "discard kryonApp:\n"
  result.add "  Header:\n"
  result.add &"    windowWidth = {width}\n"
  result.add &"    windowHeight = {height}\n"
  result.add &"    windowTitle = \"{title}\"\n"
  result.add "\n"
  result.add "  Body:\n"
  result.add &"    backgroundColor = \"{bg}\"\n"

  # Generate children inside Body
  if root.hasKey("children"):
    for child in root["children"]:
      result.add "\n"
      result.add generateNimComponentWithContext(child, ctx, 2)

  result.add "\n"

proc generateNimComponent(node: JsonNode, indent: int = 0): string =
  ## Generate Nim DSL code from component JSON (no conditionals)
  # Create empty context for backwards compatibility
  var ctx = CodegenContext(
    conditionalsByParent: initTable[int, seq[ConditionalInfo]](),
    allConditionalChildIds: initHashSet[int]()
  )
  return generateNimComponentWithContext(node, ctx, indent)

# Generate Nim const value from JSON (handles arrays, objects, primitives)
proc generateNimConstValue(value: JsonNode, indent: int): string =
  let spaces = "  ".repeat(indent)
  case value.kind
  of JString:
    return "\"" & value.getStr() & "\""
  of JInt:
    return $value.getInt()
  of JFloat:
    return $value.getFloat()
  of JBool:
    return if value.getBool(): "true" else: "false"
  of JNull:
    return "nil"
  of JArray:
    if value.len == 0:
      return "@[]"
    # Check if it's an array of objects (for named tuples)
    if value[0].kind == JObject:
      result = "[\n"
      var idx = 0
      for elem in value:
        result.add spaces & "  (\n"
        for key, val in elem.pairs:
          result.add spaces & "    " & key & ": " & generateNimConstValue(val, 0)
          result.add ",\n"
        result.add spaces & "  )"
        if idx < value.len - 1:
          result.add ","
        result.add "\n"
        idx.inc
      result.add spaces & "]"
    else:
      # Simple array
      result = "["
      var first = true
      for elem in value:
        if not first: result.add ", "
        first = false
        result.add generateNimConstValue(elem, 0)
      result.add "]"
  of JObject:
    # Generate named tuple
    result = "(\n"
    var first = true
    for key, val in value.pairs:
      if not first:
        result.add ",\n"
      first = false
      result.add spaces & "  " & key & ": " & generateNimConstValue(val, indent + 1)
    result.add "\n" & spaces & ")"

proc generateNimFromKir*(kirPath: string): string =
  ## Generate Nim code from .kir file (v2.0 and v2.1 support)
  if not fileExists(kirPath):
    raise newException(IOError, "File not found: " & kirPath)

  let kirJson = parseFile(kirPath)

  # Build codegen context from reactive_manifest
  let ctx = buildCodegenContext(kirJson)

  var nimCode = "## Generated from " & extractFilename(kirPath) & "\n"
  nimCode.add "## Generated by Kryon Code Generator\n\n"
  nimCode.add "import kryon_dsl\n"

  # Check for v2.1 format (with reactive manifest)
  if kirJson.hasKey("format_version") and kirJson["format_version"].getStr() == "2.1":
    nimCode.add "import reactive_system\n\n"

    # Generate reactive variables if manifest exists
    if kirJson.hasKey("reactive_manifest"):
      let manifest = kirJson["reactive_manifest"]
      if manifest.hasKey("variables") and manifest["variables"].len > 0:
        nimCode.add "# Reactive State\n"
        for varNode in manifest["variables"]:
          let varName = varNode["name"].getStr()
          let varType = varNode["type"].getStr()
          let initialValue = varNode["initial_value"].getStr()

          # Map type to Nim type annotation
          let nimInitVal = case varType
            of "string": initialValue
            of "int", "float": initialValue
            of "bool": initialValue
            else: initialValue

          nimCode.add &"var {varName} = namedReactiveVar(\"{varName}\", {nimInitVal})\n"
        nimCode.add "\n"

    # Generate UI using kryonApp pattern
    if kirJson.hasKey("component"):
      nimCode.add generateKryonAppCode(kirJson["component"], ctx)
    elif kirJson.hasKey("root"):
      # v3.0 format without component_definitions
      nimCode.add generateKryonAppCode(kirJson["root"], ctx)
  elif kirJson.hasKey("root"):
    # v3.0 format (without component_definitions, falls back to v2 codegen)

    # Generate const declarations if static blocks are present and constants exist
    if kirJson.hasKey("constants") and kirJson["constants"].len > 0:
      # Check if there are static_for nodes in the tree (meaning we should generate consts)
      var hasStaticFor = false
      proc checkForStaticFor(node: JsonNode) =
        if node.kind == JObject:
          if node{"type"}.getStr() == "static_for":
            hasStaticFor = true
            return
          if node.hasKey("children"):
            for child in node["children"]:
              checkForStaticFor(child)
              if hasStaticFor: return
      checkForStaticFor(kirJson["root"])

      if hasStaticFor:
        nimCode.add "# Constants for static loops\n"
        for constNode in kirJson["constants"]:
          let name = constNode["name"].getStr()
          let value = constNode["value"]

          # Generate Nim const with appropriate structure
          nimCode.add &"const {name} = "
          nimCode.add generateNimConstValue(value, 0)
          nimCode.add "\n"
        nimCode.add "\n"

    # Include Nim source handlers from sources section
    # The source is preserved in .kir via the onClickName property and DSL auto-registration
    if kirJson.hasKey("sources") and kirJson["sources"].hasKey("nim"):
      let nimSource = kirJson["sources"]["nim"].getStr().replace("\t", "  ")
      nimCode.add "\n# Event Handlers (from sources.nim)\n"
      nimCode.add nimSource
      nimCode.add "\n\n"  # Extra blank line before kryonApp

    nimCode.add generateKryonAppCode(kirJson["root"], ctx)
  else:
    # Legacy v2.0 format (root component is the JSON itself)
    nimCode.add generateKryonAppCode(kirJson, ctx)

  return nimCode

# =============================================================================
# v3.0 Logic Block Codegen - Universal Expressions to Nim
# =============================================================================

proc generateNimExpression*(expr: JsonNode): string =
  ## Convert a universal expression AST to Nim code
  if expr.kind == JInt:
    return $expr.getInt()
  if expr.kind == JFloat:
    return $expr.getFloat()
  if expr.kind == JString:
    return "\"" & expr.getStr() & "\""
  if expr.kind == JBool:
    return (if expr.getBool(): "true" else: "false")

  if expr.kind != JObject:
    return "nil"

  # Variable reference: {"var": "name"}
  if expr.hasKey("var"):
    let varName = expr["var"].getStr()
    return varName & ".value"

  # Binary operations: {"op": "add", "left": ..., "right": ...}
  let op = expr{"op"}.getStr("")
  case op
  of "add":
    let left = generateNimExpression(expr["left"])
    let right = generateNimExpression(expr["right"])
    return &"({left} + {right})"
  of "sub":
    let left = generateNimExpression(expr["left"])
    let right = generateNimExpression(expr["right"])
    return &"({left} - {right})"
  of "mul":
    let left = generateNimExpression(expr["left"])
    let right = generateNimExpression(expr["right"])
    return &"({left} * {right})"
  of "div":
    let left = generateNimExpression(expr["left"])
    let right = generateNimExpression(expr["right"])
    return &"({left} div {right})"
  of "not":
    let operand = generateNimExpression(expr["operand"])
    return &"(not {operand})"
  else:
    return "nil"

proc generateNimStatement*(stmt: JsonNode): string =
  ## Convert a universal statement to Nim code
  let op = stmt{"op"}.getStr("")
  case op
  of "assign":
    let target = stmt["target"].getStr()
    let expr = generateNimExpression(stmt["expr"])
    return &"{target}.value = {expr}"
  else:
    return "# Unknown statement: " & $stmt

proc generateNimHandler*(name: string, fn: JsonNode): string =
  ## Generate Nim proc from a logic function definition
  result = &"proc {name.replace(\":\", \"_\")}*() =\n"

  if fn.hasKey("universal") and fn["universal"].kind == JObject:
    let universal = fn["universal"]
    if universal.hasKey("statements"):
      for stmt in universal["statements"]:
        result.add "  " & generateNimStatement(stmt) & "\n"
  elif fn.hasKey("sources") and fn["sources"].hasKey("nim"):
    # Embedded Nim source
    let nimSource = fn["sources"]["nim"].getStr().replace("\t", "  ")
    result.add "  " & nimSource.replace("\n", "\n  ") & "\n"
  else:
    result.add "  discard # No implementation\n"

# =============================================================================
# Helper functions for enhanced v3.0 codegen
# =============================================================================

proc mapKirTypeToNim(kirType: string): string =
  ## Map .kir types to Nim types
  case kirType
  of "int": "int"
  of "float": "float"
  of "string": "string"
  of "bool": "bool"
  of "any": "int"  # Default to int for generic
  else: "int"

proc formatNimValue(val: JsonNode): string =
  ## Format a JSON value as Nim literal
  case val.kind
  of JString:
    let s = val.getStr()
    # Check if it's a dimension value like "60.0px"
    if s.endsWith("px"):
      return s.replace(".0px", "").replace("px", "")
    elif s.startsWith("#"):
      return "\"" & s & "\""
    else:
      return "\"" & s & "\""
  of JInt: $val.getInt()
  of JFloat: $val.getFloat()
  of JBool: (if val.getBool(): "true" else: "false")
  else: "nil"

proc lookupHandlerForEvent(bindings: JsonNode, componentId: int): string =
  ## Look up handler name for a component ID from event_bindings
  for binding in bindings:
    if binding["component_id"].getInt() == componentId:
      let handler = binding["handler"].getStr()
      # Extract local name: "Button:value_decrement" -> "decrement"
      let parts = handler.split(":")
      if parts.len > 1:
        let operation = parts[^1]  # "value_decrement"
        if "_" in operation:
          return operation.split("_")[^1]  # "decrement"
        return operation
      return handler.replace(":", "_")
  return ""

proc generateNimDslTree(node: JsonNode, bindings: JsonNode, indentLevel: int): string =
  ## Generate Nim DSL tree from template node
  if node.kind != JObject:
    return ""

  let spaces = "  ".repeat(indentLevel)
  let compType = node{"type"}.getStr("")

  if compType == "":
    return ""

  result = &"{spaces}{compType}:\n"

  # Track which properties to output (skip internal/structural ones)
  let skipProps = ["id", "type", "children", "events", "component_ref",
                    "minWidth", "minHeight", "maxWidth", "maxHeight",
                    "flexShrink", "direction", "flexDirection", "border"]

  # Generate properties
  for key, val in node.pairs:
    if key in skipProps:
      continue

    # Map property names to DSL names
    let dslKey = case key
      of "background": "backgroundColor"
      of "text_expression": continue  # Handle separately
      of "fontFamily": continue  # Skip for cleaner output
      of "text":
        # Skip static text if we have a text_expression
        if node.hasKey("text_expression"):
          continue
        else:
          key
      else: key

    # Skip transparent backgrounds and zero values
    if key == "background" and val.getStr("") == "#00000000":
      continue
    if key == "color" and val.getStr("") == "#00000000":
      continue

    let nimVal = formatNimValue(val)
    if nimVal != "nil" and nimVal != "\"\"":
      result.add &"{spaces}  {dslKey} = {nimVal}\n"

  # Handle text expressions (reactive text)
  if node.hasKey("text_expression"):
    let expr = node["text_expression"].getStr()
    # Convert {{value}} to $value (plain int, not reactive)
    let nimExpr = expr.replace("{{", "$").replace("}}", "")
    result.add &"{spaces}  text = {nimExpr}\n"

  # Handle events
  if node.hasKey("events"):
    let nodeId = node{"id"}.getInt(0)
    let handler = lookupHandlerForEvent(bindings, nodeId)
    if handler != "":
      result.add &"{spaces}  onClick = {handler}\n"

  # Generate children
  if node.hasKey("children"):
    for child in node["children"]:
      result.add generateNimDslTree(child, bindings, indentLevel + 1)

proc generateComponentProc(compDef: JsonNode, logic: JsonNode): string =
  ## Generate a component proc definition
  let name = compDef["name"].getStr()
  let props = compDef{"props"}
  let state = compDef{"state"}
  let tmpl = compDef{"template"}

  # Generate proc signature
  result = &"proc {name}*(initialValue: int = 0): Element {{.kryonComponent.}} =\n"

  # Generate state variables
  if not state.isNil and state.kind == JArray:
    for stateVar in state:
      let sName = stateVar["name"].getStr()
      let initial = stateVar{"initial"}.getStr("0")
      result.add &"  var {sName} = initialValue\n"

  # Generate nested handlers based on component's logic functions
  # Look for functions that match this component's operations
  if not logic.isNil and logic.hasKey("functions"):
    let functions = logic["functions"]

    # Check for decrement operation
    if functions.hasKey("Button:value_decrement"):
      result.add "\n  proc decrement() =\n"
      result.add "    value -= 1\n"

    # Check for increment operation
    if functions.hasKey("Button:value_increment"):
      result.add "\n  proc increment() =\n"
      result.add "    value += 1\n"

  # Generate UI tree
  result.add "\n  result = "
  if not tmpl.isNil:
    let bindings = if not logic.isNil and logic.hasKey("event_bindings"):
                     logic["event_bindings"]
                   else:
                     newJArray()
    let tree = generateNimDslTree(tmpl, bindings, 2)
    # Remove leading indentation from first line since we have "result = "
    let lines = tree.split("\n")
    if lines.len > 0:
      result.add lines[0].strip() & "\n"
      for i in 1..<lines.len:
        if lines[i].len > 0:
          result.add lines[i] & "\n"
  else:
    result.add "Row:\n    discard\n"

proc generateKryonApp(root: JsonNode): string =
  ## Generate the kryonApp block
  let width = root{"width"}.getStr("800px").replace(".0px", "").replace("px", "")
  let height = root{"height"}.getStr("600px").replace(".0px", "").replace("px", "")
  let bg = root{"background"}.getStr("#1e1e1e")

  result = "let app = kryonApp:\n"
  result.add "  Header:\n"
  result.add &"    windowWidth = {width}\n"
  result.add &"    windowHeight = {height}\n"
  result.add "    windowTitle = \"Generated App\"\n"
  result.add "\n"
  result.add "  Body:\n"
  result.add &"    backgroundColor = \"{bg}\"\n"
  result.add "\n"

  # Find the inner Column that contains component instances
  if root.hasKey("children"):
    for child in root["children"]:
      if child.hasKey("children"):
        for inner in child["children"]:
          if inner.hasKey("component"):
            let compName = inner["component"].getStr()
            let props = inner{"props"}
            if not props.isNil and props.hasKey("initialValue"):
              let initVal = props["initialValue"].getInt(0)
              result.add &"    {compName}({initVal})\n"
            else:
              result.add &"    {compName}()\n"
          else:
            # Regular element - generate DSL
            result.add "    Column:\n"
            result.add "      discard\n"

proc generateNimFromKirV3*(kirPath: string): string =
  ## Generate Nim code from .kir v2.1/v3.0 file with component definitions
  if not fileExists(kirPath):
    raise newException(IOError, "File not found: " & kirPath)

  let kirJson = parseFile(kirPath)

  # Check version - accept v2.1 and v3.0, or any file with component_definitions
  let version = kirJson{"format_version"}.getStr("2.0")
  let hasComponentDefs = kirJson.hasKey("component_definitions")
  if version notin ["2.1", "3.0"] and not hasComponentDefs:
    raise newException(ValueError, &"Expected v2.1/v3.0 or component_definitions, got v{version}")

  var nimCode = "## Generated from " & extractFilename(kirPath) & "\n"
  nimCode.add "## Kryon Code Generator - v3.0 Component-Based Output\n\n"
  nimCode.add "import kryon_dsl\n\n"

  # Generate component definitions
  if kirJson.hasKey("component_definitions"):
    nimCode.add "# =============================================================================\n"
    nimCode.add "# Component Definitions\n"
    nimCode.add "# =============================================================================\n\n"

    let logic = if kirJson.hasKey("logic"): kirJson["logic"] else: newJObject()

    for compDef in kirJson["component_definitions"]:
      nimCode.add generateComponentProc(compDef, logic)
      nimCode.add "\n"

  # Generate kryonApp
  nimCode.add "# =============================================================================\n"
  nimCode.add "# Application\n"
  nimCode.add "# =============================================================================\n\n"

  if kirJson.hasKey("root"):
    nimCode.add generateKryonApp(kirJson["root"])

  return nimCode

proc generateLuaFromKirV3*(kirPath: string): string =
  ## Generate Lua code from .kir v3.0 file with logic block
  if not fileExists(kirPath):
    raise newException(IOError, "File not found: " & kirPath)

  let kirJson = parseFile(kirPath)

  # Check version
  let version = kirJson{"format_version"}.getStr("2.0")
  if version != "3.0":
    raise newException(ValueError, &"Expected v3.0, got {version}")

  var luaCode = "-- Generated from " & extractFilename(kirPath) & "\n"
  luaCode.add "-- Generated by Kryon Code Generator (v3.0 format)\n\n"
  luaCode.add "local Reactive = require(\"kryon.reactive\")\n"
  luaCode.add "local UI = require(\"kryon.dsl\")\n\n"

  # Generate reactive variables from manifest
  if kirJson.hasKey("reactive_manifest"):
    let manifest = kirJson["reactive_manifest"]
    if manifest.hasKey("variables") and manifest["variables"].len > 0:
      luaCode.add "-- Reactive State\n"
      for varNode in manifest["variables"]:
        let varName = varNode["name"].getStr().split(":")[^1]
        let initialValue = varNode{"initial_value"}.getStr("0")
        luaCode.add &"local {varName} = Reactive.state({initialValue})\n"
      luaCode.add "\n"

  # Generate handlers from logic block
  if kirJson.hasKey("logic"):
    let logic = kirJson["logic"]
    if logic.hasKey("functions") and logic["functions"].len > 0:
      luaCode.add "-- Event Handlers\n"
      for fnName, fnDef in logic["functions"].pairs:
        let safeName = fnName.replace(":", "_")
        luaCode.add &"local function {safeName}()\n"

        if fnDef.hasKey("universal") and fnDef["universal"].kind == JObject:
          let universal = fnDef["universal"]
          if universal.hasKey("statements"):
            for stmt in universal["statements"]:
              let op = stmt{"op"}.getStr("")
              if op == "assign":
                let target = stmt["target"].getStr()
                let expr = stmt["expr"]
                let exprOp = expr{"op"}.getStr("")
                if exprOp == "add":
                  let right = expr{"right"}.getInt(1)
                  luaCode.add &"  {target}:set({target}:get() + {right})\n"
                elif exprOp == "sub":
                  let right = expr{"right"}.getInt(1)
                  luaCode.add &"  {target}:set({target}:get() - {right})\n"
                elif expr.kind == JInt:
                  luaCode.add &"  {target}:set({expr.getInt()})\n"
        elif fnDef.hasKey("sources") and fnDef["sources"].hasKey("lua"):
          let luaSource = fnDef["sources"]["lua"].getStr()
          luaCode.add "  " & luaSource.replace("\n", "\n  ") & "\n"

        luaCode.add "end\n\n"

  luaCode.add "return {\n"
  if kirJson.hasKey("logic") and kirJson["logic"].hasKey("functions"):
    for fnName, _ in kirJson["logic"]["functions"].pairs:
      let safeName = fnName.replace(":", "_")
      luaCode.add &"  {safeName} = {safeName},\n"
  luaCode.add "}\n"

  return luaCode
