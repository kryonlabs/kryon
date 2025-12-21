import json, strformat, os, strutils, tables, sets, re

# Forward declarations
proc generateNimExpression*(expr: JsonNode): string
proc generateNimStatement*(stmt: JsonNode): string

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

proc shouldIncludePropertyV3(key: string): bool =
  ## Filter out KIR metadata that shouldn't appear in Lua props
  const skipProps = ["id", "type", "children", "events", "component_ref",
                     "text_expression", "cell_data", "template",
                     "loop_var", "iterable"]
  return key notin skipProps

proc mapKirTypeToLuaDsl(componentType: string): string =
  ## Map KIR component types to Lua DSL constructor names
  case componentType:
  of "Column": "UI.Column"
  of "Row": "UI.Row"
  of "Text": "UI.Text"
  of "Button": "UI.Button"
  of "Input": "UI.Input"
  of "Checkbox": "UI.Checkbox"
  of "Container": "UI.Container"
  of "Center": "UI.Center"
  of "Markdown": "UI.Markdown"
  of "Canvas": "UI.Canvas"
  of "Image": "UI.Image"
  of "Dropdown": "UI.Dropdown"
  else: componentType  # Custom component - use name as-is

proc reconstructExpression(val: JsonNode): string =
  ## Recursively reconstruct Nim expression from JSON expression object
  ## Handles binary operators (add, sub, mul, div) and variable references
  if val.kind == JObject:
    # Handle variable references: {var: "name"} -> name
    if val.hasKey("var"):
      return val["var"].getStr

    # Handle operators (both unary and binary)
    if val.hasKey("op"):
      let op = val["op"].getStr

      # Unary operators: {op: "neg", expr: N} -> -N
      if val.hasKey("expr") and not val.hasKey("left"):
        let expr = val["expr"]
        if op == "neg":
          let innerExpr = reconstructExpression(expr)
          return "(-" & innerExpr & ")"
        return "nil"

      # Binary operators: {op: "add", left: L, right: R}
      if val.hasKey("left") and val.hasKey("right"):
        let left = val["left"]
        let right = val["right"]
        let leftExpr = reconstructExpression(left)
        let rightExpr = reconstructExpression(right)

        case op:
        of "add":
          # For string concatenation, use & operator
          return leftExpr & " & " & rightExpr
        of "sub":
          return "(" & leftExpr & " - " & rightExpr & ")"
        of "mul":
          return "(" & leftExpr & " * " & rightExpr & ")"
        of "div":
          return "(" & leftExpr & " / " & rightExpr & ")"
        else:
          return "nil"

    return "nil"
  elif val.kind == JString:
    # String literals need to be quoted
    return "\"" & val.getStr & "\""
  elif val.kind == JInt:
    return $val.getInt
  elif val.kind == JFloat:
    return $val.getFloat
  elif val.kind == JBool:
    return (if val.getBool: "true" else: "false")
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
  of JArray:
    # Handle arrays like options = ["Red", "Green", "Blue"]
    if val.len == 0:
      return "@[]"
    var items: seq[string] = @[]
    for item in val:
      if item.kind == JString:
        items.add("\"" & item.getStr() & "\"")
      else:
        items.add($item)
    return "@[" & items.join(", ") & "]"
  of JObject:
    # Try to reconstruct expression objects
    let expr = reconstructExpression(val)
    if expr != "nil":
      return expr
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
        of "background": "backgroundColor"
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

proc generateLuaComponentV3Internal(node: JsonNode, indentLevel: int, customComponents: HashSet[string]): string =
  ## Internal version that supports custom components

  # Get component type
  let componentType = node{"type"}.getStr("Container")

  # Check if this is a custom component
  if componentType in customComponents:
    # Generate function call: Counter(5) or Counter(initialValue = 10)
    var args: seq[string] = @[]
    for key, val in node.pairs:
      if key notin ["id", "type", "children", "events"]:
        args.add toLuaValue(val)

    result = &"{componentType}("
    if args.len > 0:
      result.add args.join(", ")
    result.add ")"
  else:
    # Generate built-in UI component (original logic)
    let luaDslName = mapKirTypeToLuaDsl(componentType)

    # Collect properties
    var props: seq[string] = @[]
    let baseIndent = "  ".repeat(indentLevel + 1)

    # Check if there's a text_expression field
    let hasTextExpression = node.hasKey("text_expression")

    for key, val in node.pairs:
      # Skip text_expression field (we handle it separately)
      if key == "text_expression":
        continue

      if shouldIncludePropertyV3(key):
        # Special handling for text field when text_expression exists
        if key == "text" and hasTextExpression:
          let textExpr = node["text_expression"].getStr()
          # Convert {{value}} to tostring(value)
          var luaExpr = textExpr.replace("{{", "tostring(").replace("}}", ")")
          props.add baseIndent & &"{key} = {luaExpr}"
        else:
          let luaVal = toLuaValue(val)
          props.add baseIndent & &"{key} = {luaVal}"

    # Handle children recursively - PASS customComponents for nested support
    if node.hasKey("children") and node["children"].len > 0:
      var childrenCode = baseIndent & "children = {\n"
      let children = node["children"]
      for i in 0 ..< children.len:
        let child = children[i]
        childrenCode.add "  ".repeat(indentLevel + 2)
        childrenCode.add generateLuaComponentV3Internal(child, indentLevel + 2, customComponents)
        if i < children.len - 1:
          childrenCode.add ","
        childrenCode.add "\n"
      childrenCode.add baseIndent & "}"
      props.add childrenCode

    # Build component constructor
    result = &"{luaDslName}({{\n"
    for i, prop in props:
      result.add prop
      if i < props.len - 1:
        result.add ","
      result.add "\n"
    result.add "  ".repeat(indentLevel) & "})"

proc generateLuaComponentV3(node: JsonNode, indentLevel: int = 0): string =
  ## Public wrapper for backward compatibility
  return generateLuaComponentV3Internal(node, indentLevel, initHashSet[string]())

proc generateLuaComponentFunction(compDef: JsonNode, logic: JsonNode, customComponents: HashSet[string]): string =
  ## Generate a Lua function definition for a custom component
  let name = compDef["name"].getStr()
  let props = compDef{"props"}
  let state = compDef{"state"}
  let tmpl = compDef{"template"}

  # 1. Function signature with props
  var paramList: seq[string] = @[]
  if not props.isNil and props.kind == JArray:
    for prop in props:
      paramList.add prop["name"].getStr()

  result = &"local function {name}("
  if paramList.len > 0:
    result.add paramList.join(", ")
  result.add ")\n"

  # 2. State variables with initial values from props
  if not state.isNil and state.kind == JArray:
    for stateVar in state:
      let sName = stateVar["name"].getStr()
      let initial = stateVar{"initial"}.getStr("0")

      # Parse prop reference: {"var":"initialValue"} -> initialValue
      var initExpr = initial
      if initial.startsWith("{\"var\":"):
        initExpr = initial.replace("{\"var\":\"", "").replace("\"}", "")

      result.add &"  local {sName} = {initExpr}\n"

  # 3. Event handlers from logic block
  if not logic.isNil and logic.hasKey("functions"):
    for fnName, fnDef in logic["functions"].pairs:
      if fnDef.hasKey("universal"):
        let universal = fnDef["universal"]
        if universal.hasKey("statements"):
          # Check if this handler uses component state
          var usesComponentState = false
          for stmt in universal["statements"]:
            if stmt.hasKey("target") and not state.isNil:
              for stateVar in state:
                if stmt["target"].getStr() == stateVar["name"].getStr():
                  usesComponentState = true
                  break

          if usesComponentState:
            let handlerName = fnName.split("_")[^2] & fnName.split("_")[^1]  # handler_3_click -> 3click
            result.add &"\n  local function handler{handlerName}()\n"

            for stmt in universal["statements"]:
              let op = stmt{"op"}.getStr("")
              if op == "assign":
                let target = stmt["target"].getStr()
                let expr = stmt["expr"]
                let exprOp = expr{"op"}.getStr("")

                if exprOp == "sub":
                  let right = expr{"right"}.getInt(1)
                  result.add &"    {target} = {target} - {right}\n"
                elif exprOp == "add":
                  let right = expr{"right"}.getInt(1)
                  result.add &"    {target} = {target} + {right}\n"

            result.add "  end\n"

  # 4. Return template tree - use Internal version for nested custom component support
  result.add "\n  return "
  if not tmpl.isNil:
    result.add generateLuaComponentV3Internal(tmpl, 2, customComponents)
  else:
    result.add "UI.Row({})"
  result.add "\n"

  result.add "end\n"

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

  result = &"{spaces}{compType}:\n"

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
    # Handle border object specially - extract borderRadius, borderWidth, borderColor
    if key == "border" and val.kind == JObject:
      if val.hasKey("radius"):
        let radius = val["radius"].getInt
        if radius > 0:
          result.add &"{spaces}  borderRadius = {radius}\n"
      if val.hasKey("width"):
        let bwidth = val["width"].getInt
        if bwidth > 0:
          result.add &"{spaces}  borderWidth = {bwidth}\n"
      if val.hasKey("color"):
        let bcolor = val["color"].getStr
        if bcolor != "" and bcolor != "#00000000":
          result.add &"{spaces}  borderColor = \"{bcolor}\"\n"
      continue
    if isDefaultValue(key, val):
      continue

    # Map KIR property names to Nim DSL names
    let nimKey = case key
      of "left": "posX"
      of "top": "posY"
      else: key

    let nimVal = toNimValue(key, val)
    # selectedIndex=0 is a valid value (means first option selected), don't skip it
    let allowZero = nimKey == "selectedIndex"
    if nimVal != "nil" and nimVal != "\"auto\"" and (nimVal != "0" or allowZero):
      result.add &"{spaces}  {nimKey} = {nimVal}\n"

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
            # Generate inline handler from universal statements
            result.add &"{spaces}  onClick = proc() =\n"
            result.add &"{spaces}    # Handler: {logicId}\n"
            var hasStatements = false
            for stmt in fn["universal"]["statements"]:
              let nimStmt = generateNimStatement(stmt)
              result.add &"{spaces}    {nimStmt}\n"
              hasStatements = true
            if not hasStatements:
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

  result = "let app = kryonApp:\n"
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

    # Generate reactive variables from manifest if present
    if kirJson.hasKey("reactive_manifest"):
      let manifest = kirJson["reactive_manifest"]
      if manifest.hasKey("variables") and manifest["variables"].len > 0:
        nimCode.add "# Reactive State\n"
        for varNode in manifest["variables"]:
          let varName = varNode["name"].getStr()
          let varType = varNode{"type"}.getStr("any")
          let initialValue = varNode{"initial_value"}.getStr("0")

          # Map type to appropriate Nim literal
          let nimInitVal = case varType
            of "string": "\"" & initialValue & "\""
            of "int", "float": initialValue
            of "bool": initialValue
            else: initialValue

          nimCode.add &"var {varName} = {nimInitVal}\n"
        nimCode.add "\n"

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
    return varName

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
    # Check both "operand" and "expr" field names
    let operand = if expr.hasKey("operand"): expr["operand"] else: expr["expr"]
    return &"(not {generateNimExpression(operand)})"
  else:
    return "nil"

proc generateNimStatement*(stmt: JsonNode): string =
  ## Convert a universal statement to Nim code
  let op = stmt{"op"}.getStr("")
  case op
  of "assign":
    let target = stmt["target"].getStr()
    let expr = generateNimExpression(stmt["expr"])
    return &"{target} = {expr}"
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
      return binding["handler"].getStr()
  return ""

proc universalLogicToNimProc(logic: JsonNode, handlerName: string): string =
  ## Convert universal logic back to inline Nim proc expression
  ## e.g., {"op": "assign", "target": "value", "expr": {"op": "sub", "left": {"var": "value"}, "right": 1}}
  ## -> "proc() = value -= 1"
  if logic.isNil or not logic.hasKey("functions"):
    return ""

  let functions = logic["functions"]
  if not functions.hasKey(handlerName):
    return ""

  let funcDef = functions[handlerName]
  if not funcDef.hasKey("universal"):
    return ""

  let universal = funcDef["universal"]
  if not universal.hasKey("statements") or universal["statements"].len == 0:
    return ""

  # Convert statements to Nim code
  var stmts: seq[string] = @[]
  for stmt in universal["statements"]:
    if stmt{"op"}.getStr() == "assign":
      let target = stmt["target"].getStr()
      let expr = stmt["expr"]

      # Check if it's a simple increment/decrement
      if expr{"op"}.getStr() == "add":
        let left = expr{"left"}
        let right = expr{"right"}
        if not left.isNil and left.hasKey("var") and left["var"].getStr() == target:
          if right.kind == JInt and right.getInt() == 1:
            stmts.add &"{target} += 1"
            continue
          elif right.kind == JInt:
            stmts.add &"{target} += {right.getInt()}"
            continue
      elif expr{"op"}.getStr() == "sub":
        let left = expr{"left"}
        let right = expr{"right"}
        if not left.isNil and left.hasKey("var") and left["var"].getStr() == target:
          if right.kind == JInt and right.getInt() == 1:
            stmts.add &"{target} -= 1"
            continue
          elif right.kind == JInt:
            stmts.add &"{target} -= {right.getInt()}"
            continue

      # Generic assignment
      stmts.add &"{target} = ..."  # Fallback

  if stmts.len == 0:
    return ""
  elif stmts.len == 1:
    return "proc() = " & stmts[0]
  else:
    return "proc() =\n      " & stmts.join("\n      ")

proc generateNimDslTree(node: JsonNode, bindings: JsonNode, logic: JsonNode, indentLevel: int): string =
  ## Generate Nim DSL tree from template node
  if node.kind != JObject:
    return ""

  let spaces = "  ".repeat(indentLevel)
  let compType = node{"type"}.getStr("")

  if compType == "":
    return ""

  result = &"{spaces}{compType}:\n"

  # Track which properties to output (skip internal/structural ones)
  var skipProps = @["id", "type", "children", "events", "component_ref",
                     "minWidth", "minHeight", "maxWidth", "maxHeight",
                     "flexShrink", "direction", "flexDirection", "border",
                     "dropdown_state"]  # Handle dropdown_state specially below

  # For Dropdown components, also skip top-level dropdown properties (handled specially)
  if compType == "Dropdown":
    skipProps.add("options")
    skipProps.add("placeholder")
    skipProps.add("selectedIndex")

  # Handle dropdown properties (can be nested in dropdown_state or at top level)
  let isDropdown = compType == "Dropdown"
  if isDropdown:
    # Try nested dropdown_state first, then fall back to top-level
    let dropdownState = if node.hasKey("dropdown_state"): node["dropdown_state"] else: node

    # Placeholder
    if dropdownState.hasKey("placeholder"):
      let placeholder = dropdownState["placeholder"].getStr()
      result.add &"{spaces}  placeholder = \"{placeholder}\"\n"

    # Options
    if dropdownState.hasKey("options") and dropdownState["options"].kind == JArray:
      var optionStrings: seq[string] = @[]
      for opt in dropdownState["options"]:
        optionStrings.add "\"" & opt.getStr() & "\""
      result.add &"{spaces}  options = @[{optionStrings.join(\", \")}]\n"

    # Selected index
    if dropdownState.hasKey("selectedIndex"):
      let selectedIndex = dropdownState["selectedIndex"].getInt()
      if selectedIndex >= 0:  # Only emit if a selection is made
        result.add &"{spaces}  selectedIndex = {selectedIndex}\n"

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

  # Handle events - convert universal logic to inline proc
  if node.hasKey("events"):
    let nodeId = node{"id"}.getInt(0)
    let handlerName = lookupHandlerForEvent(bindings, nodeId)
    if handlerName != "":
      let nimProc = universalLogicToNimProc(logic, handlerName)
      if nimProc != "":
        result.add &"{spaces}  onClick = {nimProc}\n"

  # Generate children
  if node.hasKey("children"):
    for child in node["children"]:
      result.add generateNimDslTree(child, bindings, logic, indentLevel + 1)

proc generateComponentProc(compDef: JsonNode, logic: JsonNode): string =
  ## Generate a component proc definition
  let name = compDef["name"].getStr()
  let props = compDef{"props"}
  let state = compDef{"state"}
  let tmpl = compDef{"template"}

  # Generate proc signature with actual props from component definition
  var paramList: seq[string] = @[]
  if not props.isNil and props.kind == JArray:
    for prop in props:
      let propName = prop["name"].getStr()
      let propType = prop{"type"}.getStr("int")
      let defaultVal = prop{"default"}

      # Map .kry types to Nim types
      let nimType = case propType
        of "int": "int"
        of "float": "float"
        of "string": "string"
        of "bool": "bool"
        else: "int"

      # Format default value
      var defaultStr = ""
      if not defaultVal.isNil:
        defaultStr = case defaultVal.kind
          of JInt: " = " & $defaultVal.getInt()
          of JFloat: " = " & $defaultVal.getFloat()
          of JString: " = \"" & defaultVal.getStr() & "\""
          of JBool: " = " & $defaultVal.getBool()
          else: " = 0"

      paramList.add &"{propName}: {nimType}{defaultStr}"

  let params = if paramList.len > 0: paramList.join(", ") else: ""
  result = &"proc {name}*({params}): Element =\n"

  # Generate state variables using namedReactiveVar for reactive state
  if not state.isNil and state.kind == JArray:
    for stateVar in state:
      let sName = stateVar["name"].getStr()
      let initial = stateVar{"initial"}.getStr("0")

      # Parse initial value - it may reference a prop like {"var":"initialValue"}
      var initExpr = initial
      if initial.startsWith("{\"var\":"):
        # Extract the variable name reference
        let varRef = initial.replace("{\"var\":\"", "").replace("\"}", "")
        initExpr = varRef

      result.add &"  var {sName} = {initExpr}\n"

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
    let tree = generateNimDslTree(tmpl, bindings, logic, 2)
    # Remove leading indentation from first line since we have "result = "
    let lines = tree.split("\n")
    if lines.len > 0:
      result.add lines[0].strip() & "\n"
      for i in 1..<lines.len:
        if lines[i].len > 0:
          result.add lines[i] & "\n"
  else:
    result.add "Row:\n    discard\n"

proc generateKryonApp(root: JsonNode, componentDefs: JsonNode = nil): string =
  ## Generate the kryonApp block with support for custom components
  let width = root{"width"}.getStr("800px").replace(".0px", "").replace("px", "")
  let height = root{"height"}.getStr("600px").replace(".0px", "").replace("px", "")
  let bg = root{"background"}.getStr("#1e1e1e")

  # Build set of custom component names
  var customComponents: HashSet[string]
  if not componentDefs.isNil:
    for def in componentDefs:
      customComponents.incl(def["name"].getStr())

  # Helper to generate body content recursively
  proc generateBodyContent(node: JsonNode, indent: int): string =
    let nodeType = node{"type"}.getStr("Container")
    let spaces = "  ".repeat(indent)

    # Check if this is a custom component
    if nodeType in customComponents:
      # Generate function call: Counter(initialValue = 10)
      var args: seq[string]
      if node.hasKey("initialValue"):
        let val = node["initialValue"]
        if val.kind == JInt:
          args.add(&"initialValue = {val.getInt()}")
        elif val.kind == JString:
          args.add(&"initialValue = {val.getStr()}")
      if args.len > 0:
        result = &"{spaces}{nodeType}({args.join(\", \")})\n"
      else:
        result = &"{spaces}{nodeType}()\n"
    else:
      # Generate DSL block for built-in component
      result = &"{spaces}{nodeType}:\n"

      # Skip internal/structural properties
      let skipProps = ["id", "type", "children", "events"]

      # Generate properties
      for key, val in node.pairs:
        if key in skipProps:
          continue
        if isDefaultValue(key, val):
          continue
        let nimVal = toNimValue(key, val)
        # selectedIndex=0 is a valid value, don't skip it
        let allowZero = key == "selectedIndex"
        if nimVal != "nil" and nimVal != "\"auto\"" and (nimVal != "0" or allowZero):
          result.add &"{spaces}  {key} = {nimVal}\n"

      # Generate children recursively
      if node.hasKey("children") and node["children"].len > 0:
        for child in node["children"]:
          result.add "\n"
          result.add generateBodyContent(child, indent + 1)

  let title = root{"windowTitle"}.getStr("Untitled")

  result = "let app = kryonApp:\n"
  result.add "  Header:\n"
  result.add &"    windowWidth = {width}\n"
  result.add &"    windowHeight = {height}\n"
  result.add &"    windowTitle = \"{title}\"\n"
  result.add "\n"
  result.add "  Body:\n"
  result.add &"    backgroundColor = \"{bg}\"\n"

  # Generate body content
  if root.hasKey("children"):
    for child in root["children"]:
      result.add "\n"
      result.add generateBodyContent(child, 2)

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
    let componentDefs = if kirJson.hasKey("component_definitions"): kirJson["component_definitions"] else: nil
    nimCode.add generateKryonApp(kirJson["root"], componentDefs)

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

  # Build set of custom component names
  var customComponents: HashSet[string]
  if kirJson.hasKey("component_definitions"):
    for def in kirJson["component_definitions"]:
      customComponents.incl(def["name"].getStr())

  # Generate reactive variables from manifest (skip component-scoped variables)
  if kirJson.hasKey("reactive_manifest"):
    let manifest = kirJson["reactive_manifest"]
    if manifest.hasKey("variables") and manifest["variables"].len > 0:
      var hasGlobalVars = false
      for varNode in manifest["variables"]:
        let scope = varNode{"scope"}.getStr("global")
        if scope != "component":
          if not hasGlobalVars:
            luaCode.add "-- Reactive State\n"
            hasGlobalVars = true
          let varName = varNode["name"].getStr().split(":")[^1]
          let initialValue = varNode{"initial_value"}.getStr("0")
          luaCode.add &"local {varName} = Reactive.state({initialValue})\n"
      if hasGlobalVars:
        luaCode.add "\n"

  # Generate component definitions and track component-scoped handlers
  var componentHandlers: HashSet[string]
  if kirJson.hasKey("component_definitions"):
    luaCode.add "-- Component Definitions\n"
    let logic = kirJson{"logic"}

    # Build set of component state variables to identify component handlers
    for compDef in kirJson["component_definitions"]:
      let state = compDef{"state"}
      if not state.isNil and state.kind == JArray:
        for stateVar in state:
          let sName = stateVar["name"].getStr()
          # Find handlers that use this state variable
          if not logic.isNil and logic.hasKey("functions"):
            for fnName, fnDef in logic["functions"].pairs:
              if fnDef.hasKey("universal"):
                let universal = fnDef["universal"]
                if universal.hasKey("statements"):
                  for stmt in universal["statements"]:
                    if stmt.hasKey("target") and stmt["target"].getStr() == sName:
                      componentHandlers.incl(fnName)

      luaCode.add generateLuaComponentFunction(compDef, logic, customComponents)
      luaCode.add "\n"

  # Generate handlers from logic block (skip component-scoped handlers)
  if kirJson.hasKey("logic"):
    let logic = kirJson["logic"]
    if logic.hasKey("functions") and logic["functions"].len > 0:
      var hasGlobalHandlers = false
      for fnName, fnDef in logic["functions"].pairs:
        # Skip handlers that are component-scoped
        if fnName in componentHandlers:
          continue

        if not hasGlobalHandlers:
          luaCode.add "-- Event Handlers\n"
          hasGlobalHandlers = true
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

  # Generate root component tree
  luaCode.add "-- UI Tree\n"
  luaCode.add "local root = "
  if kirJson.hasKey("root"):
    luaCode.add generateLuaComponentV3Internal(kirJson["root"], 0, customComponents)
    luaCode.add "\n\n"

    # Extract window properties from root component
    let root = kirJson["root"]
    let windowWidth = root{"width"}.getStr("800.0px").replace("px", "")
    let windowHeight = root{"height"}.getStr("600.0px").replace("px", "")
    let windowTitle = root{"windowTitle"}.getStr("Kryon App")

    # Return app object instead of just root
    luaCode.add "-- Return app object\n"
    luaCode.add "return {\n"
    luaCode.add "  root = root,\n"
    luaCode.add "  window = {\n"
    luaCode.add &"    width = {windowWidth},\n"
    luaCode.add &"    height = {windowHeight},\n"
    luaCode.add &"    title = \"{windowTitle}\"\n"
    luaCode.add "  }\n"
    luaCode.add "}\n"
  else:
    # Fallback for files without root
    luaCode.add "UI.Container({ format_version = \"3.0\" })\n\n"
    luaCode.add "return {\n"
    luaCode.add "  root = root,\n"
    luaCode.add "  window = { width = 800, height = 600, title = \"Kryon App\" }\n"
    luaCode.add "}\n"

  return luaCode

# =============================================================================
# Markdown Code Generation (Semantic Transpilation)
# =============================================================================

# Helper for static_for expansion - substitute template variables
proc substituteTemplateVarsMarkdown(tmpl: JsonNode, varName: string, value: JsonNode): JsonNode =
  ## Recursively substitute {{varName.prop}} with actual values
  ## Supports: {{var.prop}}, {{var[0]}}, {{var}}

  # Helper to resolve a template pattern like "{{item.name}}"
  proc resolvePattern(pattern: string): string =
    if not pattern.contains("{{"):
      return pattern

    var output = pattern
    let varPattern = "{{" & varName

    # Try each replacement type in order
    # Direct reference {{var}}
    let directPattern = varPattern & "}}"
    if output.contains(directPattern):
      let replValue = if value.kind == JString: value.getStr()
                      else: $value
      output = output.replace(directPattern, replValue)

    # Property access {{var.prop}}
    if value.kind == JObject:
      for key in value.keys():
        let propPattern = varPattern & "." & key & "}}"
        if output.contains(propPattern):
          let propValue = if value[key].kind == JString: value[key].getStr()
                          else: $value[key]
          output = output.replace(propPattern, propValue)

    # Array access {{var[index]}}
    if value.kind == JArray:
      for i in 0..<value.len:
        let indexPattern = varPattern & "[" & $i & "]}}"
        if output.contains(indexPattern):
          let arrayValue = if value[i].kind == JString: value[i].getStr()
                           else: $value[i]
          output = output.replace(indexPattern, arrayValue)

    return output

  # Recursively process JSON node
  case tmpl.kind
  of JString:
    return %resolvePattern(tmpl.getStr())
  of JInt, JFloat, JBool, JNull:
    return tmpl
  of JObject:
    result = newJObject()
    for key, val in tmpl.pairs():
      result[key] = substituteTemplateVarsMarkdown(val, varName, value)
  of JArray:
    result = newJArray()
    for item in tmpl:
      result.add substituteTemplateVarsMarkdown(item, varName, value)

proc generateMarkdownFromKir*(kirFile: string): string =
  ## Generate proper markdown from .kir file
  ## Supports semantic transpilation: IR components -> markdown syntax

  # ===== Helper Functions =====

  # Forward declaration for recursive markdown generation
  proc componentToMarkdown(comp: JsonNode, depth: int, constants: Table[string, JsonNode]): string

  proc escapeMarkdownText(text: string): string =
    ## Escape special markdown characters in plain text
    result = text
    result = result.replace("\\", "\\\\")  # Backslash first
    result = result.replace("*", "\\*")
    result = result.replace("_", "\\_")
    result = result.replace("[", "\\[")
    result = result.replace("]", "\\]")
    result = result.replace("`", "\\`")
    result = result.replace("#", "\\#")

  proc extractText(comp: JsonNode): string =
    ## Extract text content from component
    if comp.hasKey("text"):
      let textNode = comp["text"]
      if textNode.kind == JString:
        return textNode.getStr()
      elif textNode.kind == JObject and textNode.hasKey("value"):
        return textNode["value"].getStr()
    return ""

  proc extractHeadingData(comp: JsonNode): tuple[level: int, text: string] =
    ## Extract heading level and text from custom_data
    var level = 1
    var text = ""

    # Try custom_data first (IRHeadingData)
    if comp.hasKey("heading_data"):
      let data = comp["heading_data"]
      level = data{"level"}.getInt(1)
      text = data{"text"}.getStr("")

    # Fallback to text field
    if text.len == 0:
      text = extractText(comp)

    return (level, text)

  proc extractListData(comp: JsonNode): tuple[isOrdered: bool, start: int] =
    ## Extract list type from custom_data
    if comp.hasKey("list_data"):
      let data = comp["list_data"]
      let listType = data{"type"}.getInt(0)  # 0 = unordered, 1 = ordered
      let start = data{"start"}.getInt(1)
      return (listType == 1, start)
    return (false, 1)

  proc extractCodeBlockData(comp: JsonNode): tuple[language: string, code: string] =
    ## Extract code block language and code from custom_data
    if comp.hasKey("code_block_data"):
      let data = comp["code_block_data"]
      return (data{"language"}.getStr(""), data{"code"}.getStr(""))
    return ("", "")

  proc extractLinkData(comp: JsonNode): tuple[url: string, title: string] =
    ## Extract link URL and title from custom_data
    if comp.hasKey("link_data"):
      let data = comp["link_data"]
      return (data{"url"}.getStr(""), data{"title"}.getStr(""))
    return ("", "")

  proc extractCellAlignment(cell: JsonNode): string =
    ## Extract alignment from table cell metadata
    if cell.hasKey("cell_data"):
      let alignment = cell["cell_data"]{"alignment"}.getInt(0)
      case alignment
      of 1: return "center"  # IR_ALIGNMENT_CENTER
      of 2: return "right"   # IR_ALIGNMENT_END
      else: return "left"
    return "left"

  proc collectTextContent(comp: JsonNode): string =
    ## Recursively collect all text content (handles inline formatting)
    result = ""

    # Direct text field
    let text = extractText(comp)
    if text.len > 0:
      # Check for bold/italic styling
      if comp.hasKey("style"):
        let style = comp["style"]
        let bold = style{"font"}{"bold"}.getBool(false)
        let italic = style{"font"}{"italic"}.getBool(false)

        if bold and italic:
          return "***" & escapeMarkdownText(text) & "***"
        elif bold:
          return "**" & escapeMarkdownText(text) & "**"
        elif italic:
          return "*" & escapeMarkdownText(text) & "*"

      return escapeMarkdownText(text)

    # Collect from children
    if comp.hasKey("children"):
      for child in comp["children"]:
        let childType = child{"type"}.getStr("Text")

        case childType
        of "Link":
          let (url, title) = extractLinkData(child)
          let linkText = collectTextContent(child)
          if title.len > 0:
            result.add &"[{linkText}]({url} \"{title}\")"
          else:
            result.add &"[{linkText}]({url})"
        else:
          result.add collectTextContent(child)

    return result

  # ===== HTML Fallback Helpers =====

  proc generateInlineStyles(comp: JsonNode): string =
    ## Convert IR styles to inline CSS for HTML fallback
    ## Supports both v3.0 format (flat properties) and nested style format
    var styles: seq[string] = @[]

    # Background color (v3.0: flat "background", nested: "style.background")
    if comp.hasKey("background"):
      let bg = comp["background"]
      if bg.kind == JString:
        # Hex color or named color
        styles.add "background-color: " & bg.getStr()
      elif bg.kind == JObject and bg.hasKey("r"):
        let r = bg["r"].getInt()
        let g = bg["g"].getInt()
        let b = bg["b"].getInt()
        let a = bg{"a"}.getInt(255)
        if a < 255:
          styles.add "background-color: rgba(" & $r & ", " & $g & ", " & $b & ", " & $(float(a) / 255.0) & ")"
        else:
          styles.add "background-color: rgb(" & $r & ", " & $g & ", " & $b & ")"

    # Dimensions (v3.0: flat "width"/"height")
    if comp.hasKey("width"):
      let width = comp["width"]
      if width.kind == JString:
        styles.add "width: " & width.getStr()
      elif width.kind == JObject and width.hasKey("value"):
        styles.add "width: " & $width["value"].getFloat() & "px"
      elif width.kind == JFloat or width.kind == JInt:
        styles.add "width: " & $width.getFloat() & "px"

    if comp.hasKey("height"):
      let height = comp["height"]
      if height.kind == JString:
        styles.add "height: " & height.getStr()
      elif height.kind == JObject and height.hasKey("value"):
        styles.add "height: " & $height["value"].getFloat() & "px"
      elif height.kind == JFloat or height.kind == JInt:
        styles.add "height: " & $height.getFloat() & "px"

    # Text color (v3.0: flat "color")
    if comp.hasKey("color"):
      let color = comp["color"]
      if color.kind == JString:
        styles.add "color: " & color.getStr()
      elif color.kind == JObject and color.hasKey("r"):
        let r = color["r"].getInt()
        let g = color["g"].getInt()
        let b = color["b"].getInt()
        styles.add "color: rgb(" & $r & ", " & $g & ", " & $b & ")"

    # Handle nested style format (if exists)
    if comp.hasKey("style"):
      let style = comp["style"]

      if style.hasKey("padding"):
        let p = style["padding"]
        if p.kind == JObject:
          let top = p{"top"}.getFloat(0)
          let right = p{"right"}.getFloat(0)
          let bottom = p{"bottom"}.getFloat(0)
          let left = p{"left"}.getFloat(0)
          if top != 0 or right != 0 or bottom != 0 or left != 0:
            styles.add "padding: " & $top & "px " & $right & "px " & $bottom & "px " & $left & "px"

      if style.hasKey("margin"):
        let m = style["margin"]
        if m.kind == JObject:
          let top = m{"top"}.getFloat(0)
          let right = m{"right"}.getFloat(0)
          let bottom = m{"bottom"}.getFloat(0)
          let left = m{"left"}.getFloat(0)
          if top != 0 or right != 0 or bottom != 0 or left != 0:
            styles.add "margin: " & $top & "px " & $right & "px " & $bottom & "px " & $left & "px"

      if style.hasKey("border"):
        let border = style["border"]
        if border.kind == JObject and border.hasKey("radius"):
          let radius = border["radius"].getInt(0)
          if radius > 0:
            styles.add "border-radius: " & $radius & "px"

      if style.hasKey("font"):
        let font = style["font"]
        if font.kind == JObject:
          if font.hasKey("size"):
            styles.add "font-size: " & $font["size"].getFloat() & "px"
          if font.hasKey("weight"):
            styles.add "font-weight: " & $font["weight"].getInt()
          elif font{"bold"}.getBool(false):
            styles.add "font-weight: bold"
          if font{"italic"}.getBool(false):
            styles.add "font-style: italic"

    if styles.len > 0:
      return " style=\"" & styles.join("; ") & "\""
    return ""

  proc escapeHTMLText(text: string): string =
    ## Escape HTML special characters
    result = text
    result = result.replace("&", "&amp;")
    result = result.replace("<", "&lt;")
    result = result.replace(">", "&gt;")
    result = result.replace("\"", "&quot;")
    result = result.replace("'", "&#x27;")

  proc generateComponentAsHTML(comp: JsonNode): string =
    ## Generate inline HTML for components that can't be represented in markdown
    let compType = comp{"type"}.getStr()
    let styleAttr = generateInlineStyles(comp)

    case compType
    of "Button":
      let text = escapeHTMLText(extractText(comp))
      return "<button class=\"kryon-button\"" & styleAttr & ">" & text & "</button>\n\n"

    of "Input":
      let placeholder = comp{"custom_data"}.getStr("")
      let escapedPlaceholder = escapeHTMLText(placeholder)
      return "<input type=\"text\" class=\"kryon-input\" placeholder=\"" & escapedPlaceholder & "\"" & styleAttr & " />\n\n"

    of "Checkbox":
      let label = escapeHTMLText(extractText(comp))
      return "<label><input type=\"checkbox\" class=\"kryon-checkbox\"" & styleAttr & " /> " & label & "</label>\n\n"

    of "Dropdown":
      # For dropdowns, we'd need to extract options from custom_data
      # For now, just render a basic select
      return "<select class=\"kryon-dropdown\"" & styleAttr & "></select>\n\n"

    of "Canvas":
      let width = comp{"style"}{"width"}{"value"}.getFloat(300)
      let height = comp{"style"}{"height"}{"value"}.getFloat(150)
      return "<canvas class=\"kryon-canvas\" width=\"" & $width & "\" height=\"" & $height & "\"" & styleAttr & "></canvas>\n\n"

    else:
      # Fallback for truly unsupported components
      return "<!-- Unsupported component: " & compType & " -->\n\n"

  # ===== Component Generators =====

  proc generateHeading(comp: JsonNode): string =
    let (level, text) = extractHeadingData(comp)
    let headingText = if text.len > 0: text else: collectTextContent(comp)
    return "#".repeat(level) & " " & headingText & "\n\n"

  proc generateParagraph(comp: JsonNode): string =
    let text = collectTextContent(comp)
    if text.len > 0:
      return text & "\n\n"
    return ""

  proc generateCodeBlock(comp: JsonNode): string =
    let (language, code) = extractCodeBlockData(comp)
    result.add "```"
    if language.len > 0:
      result.add language
    result.add "\n"
    result.add code
    if not code.endsWith("\n"):
      result.add "\n"
    result.add "```\n\n"

  proc generateBlockquote(comp: JsonNode): string =
    let text = collectTextContent(comp)
    for line in text.splitLines():
      result.add "> " & line & "\n"
    result.add "\n"

  proc generateLink(comp: JsonNode): string =
    let (url, title) = extractLinkData(comp)
    let linkText = collectTextContent(comp)
    if title.len > 0:
      return "[" & linkText & "](" & url & " \"" & title & "\")"
    else:
      return "[" & linkText & "](" & url & ")"

  proc generateImage(comp: JsonNode): string =
    let src = comp{"custom_data"}.getStr("")
    let alt = extractText(comp)
    return fmt"![{alt}]({src})\n\n"

  proc generateList(comp: JsonNode, depth: int): string =
    let (isOrdered, start) = extractListData(comp)

    if comp.hasKey("children"):
      var itemNum = start
      for child in comp["children"]:
        if child{"type"}.getStr() == "ListItem":
          let indent = "  ".repeat(depth)
          let text = collectTextContent(child)

          if isOrdered:
            result.add fmt"{indent}{itemNum}. {text}" & "\n"
            itemNum.inc
          else:
            result.add fmt"{indent}- {text}" & "\n"

          # Handle nested lists
          if child.hasKey("children"):
            for nestedChild in child["children"]:
              let nestedType = nestedChild{"type"}.getStr()
              if nestedType == "List":
                # Forward declare componentToMarkdown for recursion
                result.add generateList(nestedChild, depth + 1)

    result.add "\n"

  proc generateTable(comp: JsonNode): string =
    ## Generate markdown table from IR Table component
    var headers: seq[string] = @[]
    var rows: seq[seq[string]] = @[]
    var alignments: seq[string] = @[]

    if not comp.hasKey("children"):
      return ""

    # Walk table structure
    for section in comp["children"]:
      let sectionType = section{"type"}.getStr()

      case sectionType
      of "TableHead":
        # Extract header row
        if section.hasKey("children"):
          for row in section["children"]:
            if row{"type"}.getStr() == "Tr" and row.hasKey("children"):
              for cell in row["children"]:
                let cellType = cell{"type"}.getStr()
                if cellType == "Th" or cellType == "TableHeaderCell":
                  headers.add collectTextContent(cell)
                  alignments.add extractCellAlignment(cell)

      of "TableBody":
        # Extract data rows
        if section.hasKey("children"):
          for row in section["children"]:
            if row{"type"}.getStr() == "Tr" and row.hasKey("children"):
              var rowData: seq[string] = @[]
              for cell in row["children"]:
                let cellType = cell{"type"}.getStr()
                if cellType == "Td" or cellType == "TableCell":
                  rowData.add collectTextContent(cell)
              if rowData.len > 0:
                rows.add rowData

    # Generate markdown table
    if headers.len == 0:
      return ""

    # Header row
    result.add "| " & headers.join(" | ") & " |\n"

    # Separator row with alignment
    result.add "|"
    for i in 0..<headers.len:
      let align = if i < alignments.len: alignments[i] else: "left"
      case align
      of "center":
        result.add ":---:|"
      of "right":
        result.add "---:|"
      else:
        result.add "---|"
    result.add "\n"

    # Data rows
    for row in rows:
      # Pad row to match header count
      var paddedRow = row
      while paddedRow.len < headers.len:
        paddedRow.add ""
      result.add "| " & paddedRow.join(" | ") & " |\n"

    result.add "\n"

  # ===== Flowchart/Mermaid Helpers =====

  proc flowchartShapeToMermaid(shape: string, label: string): string =
    ## Convert IR shape to Mermaid bracket notation
    ## Returns the label wrapped in appropriate brackets
    case shape.toLower()
    of "rectangle": return "[" & label & "]"
    of "rounded": return "(" & label & ")"
    of "stadium": return "([" & label & "])"
    of "diamond": return "{" & label & "}"
    of "circle": return "((" & label & "))"
    of "hexagon": return "{{" & label & "}}"
    of "parallelogram": return "[/" & label & "/]"
    of "cylinder": return "[(" & label & ")]"
    of "subroutine": return "[[" & label & "]]"
    of "asymmetric": return ">" & label & "]"
    of "trapezoid": return "[/" & label & "\\]"
    else: return "[" & label & "]"  # Default to rectangle

  proc flowchartEdgeToMermaid(edgeType: string, hasLabel: bool): tuple[connector: string, labelPrefix: string, labelSuffix: string] =
    ## Convert IR edge type to Mermaid arrow syntax
    ## Returns (connector, labelPrefix, labelSuffix) for formatting
    case edgeType.toLower()
    of "arrow":
      if hasLabel: return ("-->", "|", "|")
      else: return ("-->", "", "")
    of "open":
      if hasLabel: return ("---", "|", "|")
      else: return ("---", "", "")
    of "dotted":
      if hasLabel: return ("-.->", "|", "|")
      else: return ("-.->", "", "")
    of "thick":
      if hasLabel: return ("==>", "|", "|")
      else: return ("==>", "", "")
    of "bidirectional":
      if hasLabel: return ("<-->", "|", "|")
      else: return ("<-->", "", "")
    else:
      if hasLabel: return ("-->", "|", "|")
      else: return ("-->", "", "")

  proc generateFlowchart(comp: JsonNode): string =
    ## Generate Mermaid flowchart from IR Flowchart component
    var mermaid = "```mermaid\n"

    # Get direction (default to TB)
    var direction = "TB"
    if comp.hasKey("flowchart_config"):
      let config = comp["flowchart_config"]
      if config.hasKey("direction"):
        direction = config["direction"].getStr("TB")

    mermaid.add "flowchart " & direction & "\n"

    # Extract nodes and edges from children
    if not comp.hasKey("children"):
      mermaid.add "```\n\n"
      return mermaid

    var nodes: seq[tuple[id: string, shape: string, label: string]] = @[]
    var edges: seq[tuple[fromId: string, toId: string, edgeType: string, label: string]] = @[]

    # First pass: collect all nodes and edges
    for child in comp["children"]:
      let childType = child{"type"}.getStr()

      if childType == "FlowchartNode":
        if child.hasKey("flowchart_node"):
          let node = child["flowchart_node"]
          let nodeId = node{"id"}.getStr("")
          let shape = node{"shape"}.getStr("rectangle")
          let label = node{"label"}.getStr("")

          if nodeId.len > 0:
            nodes.add((nodeId, shape, label))

      elif childType == "FlowchartEdge":
        if child.hasKey("flowchart_edge"):
          let edge = child["flowchart_edge"]
          let fromId = edge{"from"}.getStr("")
          let toId = edge{"to"}.getStr("")
          let edgeType = edge{"type"}.getStr("arrow")
          let label = edge{"label"}.getStr("")

          if fromId.len > 0 and toId.len > 0:
            edges.add((fromId, toId, edgeType, label))

    # Generate node definitions
    for node in nodes:
      let shapedLabel = flowchartShapeToMermaid(node.shape, node.label)
      mermaid.add "    " & node.id & shapedLabel & "\n"

    # Generate edge definitions
    for edge in edges:
      let hasLabel = edge.label.len > 0
      let (connector, labelPrefix, labelSuffix) = flowchartEdgeToMermaid(edge.edgeType, hasLabel)

      mermaid.add "    " & edge.fromId & " " & connector
      if hasLabel:
        mermaid.add labelPrefix & edge.label & labelSuffix & " "
      else:
        mermaid.add " "
      mermaid.add edge.toId & "\n"

    mermaid.add "```\n\n"
    return mermaid

  # ===== Main Dispatcher =====

  proc componentToMarkdown(comp: JsonNode, depth: int, constants: Table[string, JsonNode]): string =
    ## Convert IR component to markdown syntax (constants needed for static_for expansion)
    if comp.isNil or comp.kind != JObject:
      return ""

    if not comp.hasKey("type"):
      return ""

    let compType = comp{"type"}.getStr("Container")

    # Handle static_for expansion inline
    if compType == "static_for":
      let loopVar = comp{"loop_var"}.getStr("item")
      let iterableName = comp{"iterable"}.getStr()
      let loopTemplate = comp["template"]

      # Resolve iterable from constants
      if not constants.hasKey(iterableName):
        return "<!-- Error: constant '" & iterableName & "' not found -->\n\n"

      let iterableValues = constants[iterableName]
      if iterableValues.kind != JArray:
        return "<!-- Error: iterable '" & iterableName & "' is not an array -->\n\n"

      # Expand template for each item
      result = ""
      for item in iterableValues:
        # Substitute template variables using module-level function
        let expandedTemplate = substituteTemplateVarsMarkdown(loopTemplate, loopVar, item)
        # Recursively generate markdown for this iteration
        result.add componentToMarkdown(expandedTemplate, depth, constants)
      return result

    case compType
    of "Flowchart":
      return generateFlowchart(comp)
    of "Heading":
      return generateHeading(comp)
    of "Paragraph":
      return generateParagraph(comp)
    of "List":
      return generateList(comp, depth)
    of "CodeBlock":
      return generateCodeBlock(comp)
    of "Blockquote":
      return generateBlockquote(comp)
    of "HorizontalRule":
      return "---\n\n"
    of "Table":
      return generateTable(comp)
    of "Link":
      return generateLink(comp)
    of "Image":
      return generateImage(comp)
    of "Text":
      # Standalone text (not inside paragraph)
      let text = collectTextContent(comp)
      if text.len > 0:
        return text & "\n\n"
      return ""
    of "Container", "Column", "Row", "Center":
      # Structural components - recurse into children
      if comp.hasKey("children"):
        for child in comp["children"]:
          result.add componentToMarkdown(child, depth, constants)
    of "Button", "Input", "Checkbox", "Dropdown", "Canvas":
      # UI components - generate inline HTML
      return generateComponentAsHTML(comp)
    of "FlowchartNode", "FlowchartEdge":
      # These should only appear inside Flowchart component
      # If encountered standalone, skip them
      return ""
    else:
      # Try HTML fallback for other components, or comment if not supported
      return generateComponentAsHTML(comp)

  # ===== Main Entry Point =====

  let kirJson = parseFile(kirFile)
  var mdOutput = ""

  # Extract constants for static_for expansion
  var constants = initTable[string, JsonNode]()
  if kirJson.hasKey("constants"):
    for constDef in kirJson["constants"]:
      let name = constDef["name"].getStr()
      constants[name] = constDef["value"]

  # Get root component
  let root = if kirJson.hasKey("root"): kirJson["root"]
             elif kirJson.hasKey("component"): kirJson["component"]
             else: kirJson

  # Skip root container wrapper, process children directly
  if root{"type"}.getStr() == "Column" and root.hasKey("children"):
    for child in root["children"]:
      mdOutput.add componentToMarkdown(child, 0, constants)
  else:
    mdOutput.add componentToMarkdown(root, 0, constants)

  return mdOutput.strip()
