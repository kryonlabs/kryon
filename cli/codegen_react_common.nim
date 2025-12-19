## Common React/JSX Code Generation Module
## Shared logic for generating both TypeScript (.tsx) and JavaScript (.jsx)
## from Kryon IR (.kir) files

import json, strformat, os, strutils, tables

# =============================================================================
# Types
# =============================================================================

type
  ReactOutputMode* = enum
    ## Output mode for React code generation
    TypeScript,    # Generate TypeScript with types (.tsx)
    JavaScript     # Generate JavaScript without types (.jsx)

  PropDef* = object
    ## Component property definition
    name*: string
    typ*: string      # Used only in TypeScript mode
    default*: string

  StateDef* = object
    ## Component state definition
    name*: string
    typ*: string      # Used only in TypeScript mode
    initial*: string

  ComponentDef* = object
    ## React component definition
    name*: string
    props*: seq[PropDef]
    state*: seq[StateDef]
    templateNode*: JsonNode

  ReactContext* = object
    ## Generation context for React code
    mode*: ReactOutputMode
    componentDefs*: Table[string, ComponentDef]
    logicFunctions*: JsonNode
    eventBindings*: JsonNode
    indentLevel*: int

# =============================================================================
# Mode-Aware Helper Functions
# =============================================================================

proc generateTypeAnnotation*(mode: ReactOutputMode, typ: string): string =
  ## Generate type annotation for TypeScript, empty for JavaScript
  if mode == TypeScript:
    case typ
    of "int", "float": ": number"
    of "string": ": string"
    of "bool": ": boolean"
    else: ": any"
  else:
    ""

proc shouldGenerateInterface*(mode: ReactOutputMode): bool =
  ## Check if interfaces should be generated
  mode == TypeScript

proc getFileExtension*(mode: ReactOutputMode): string =
  ## Get file extension based on mode
  if mode == TypeScript: ".tsx" else: ".jsx"

proc generateReactImports*(mode: ReactOutputMode): string =
  ## Generate React imports (same for both JS and TS - Bun-compatible)
  "import { kryonApp, Column, Row, Button, Text, useState } from '@kryon/react';"

# =============================================================================
# Property Mapping
# =============================================================================

proc mapKirPropertyToReact*(key: string): string =
  ## Map .kir property names to React property names
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

proc formatReactValue*(key: string, val: JsonNode): string =
  ## Format a JSON value as React property value
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

proc isDefaultValue*(key: string, val: JsonNode): bool =
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
# Logic Expression to React/JavaScript
# =============================================================================

proc generateReactExpression*(expr: JsonNode): string =
  ## Convert a universal expression AST to React/JavaScript code
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
    let left = generateReactExpression(expr["left"])
    let right = generateReactExpression(expr["right"])
    return &"({left} + {right})"
  of "sub":
    let left = generateReactExpression(expr["left"])
    let right = generateReactExpression(expr["right"])
    return &"({left} - {right})"
  of "mul":
    let left = generateReactExpression(expr["left"])
    let right = generateReactExpression(expr["right"])
    return &"({left} * {right})"
  of "div":
    let left = generateReactExpression(expr["left"])
    let right = generateReactExpression(expr["right"])
    return &"({left} / {right})"
  of "not":
    let operand = if expr.hasKey("operand"): expr["operand"] else: expr["expr"]
    return &"(!{generateReactExpression(operand)})"
  else:
    return "null"

proc generateHandlerBody*(functions: JsonNode, handlerId: string, stateVarName: string): string =
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
      return &"{setterName}({generateReactExpression(expr)})"

  return ""

# =============================================================================
# Element Generation
# =============================================================================

proc generateReactElement*(node: JsonNode, ctx: var ReactContext, indent: int): string

proc generateReactProps*(node: JsonNode, ctx: var ReactContext, hasTextExpression: bool = false): seq[string] =
  ## Generate React properties for a component
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

    let reactKey = mapKirPropertyToReact(key)
    let reactVal = formatReactValue(key, val)

    # Skip empty or null values
    if reactVal in ["\"\"", "null", "{0}", "\"\""]:
      if key != "selectedIndex":  # selectedIndex=0 is valid
        continue

    result.add(&"{reactKey}={reactVal}")

proc generateReactChildren*(node: JsonNode, ctx: var ReactContext, indent: int): string =
  ## Generate React children elements
  if not node.hasKey("children") or node["children"].len == 0:
    return ""

  result = ""
  for child in node["children"]:
    result.add("\n")
    result.add(generateReactElement(child, ctx, indent))

proc generateReactElement*(node: JsonNode, ctx: var ReactContext, indent: int): string =
  ## Generate a single React/JSX element
  if node.kind != JObject:
    return ""

  let spaces = "  ".repeat(indent)
  let compType = node{"type"}.getStr("div")

  # Check if this is a custom component (unused for now)
  discard compType in ctx.componentDefs

  # Check if we have a text_expression
  let hasTextExpression = node.hasKey("text_expression")

  # Get properties
  var props = generateReactProps(node, ctx, hasTextExpression)

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
  let children = generateReactChildren(node, ctx, indent + 1)

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
      result.add(&"\n{spaces}/>")
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
# Window Configuration
# =============================================================================

proc extractWindowConfig*(component: JsonNode): tuple[width: int, height: int, title: string, background: string] =
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

# =============================================================================
# State Hooks Generation
# =============================================================================

proc generateStateHooks*(state: seq[StateDef], ctx: ReactContext): string =
  ## Generate useState hooks for component state
  result = ""
  for stateVar in state:
    let setterName = "set" & stateVar.name[0].toUpperAscii & stateVar.name[1..^1]
    var initialVal = stateVar.initial
    # Handle {"var": "propName"} references
    if initialVal.startsWith("{\"var\":\""):
      initialVal = initialVal[8..^3]  # Extract prop name
    result.add(&"  const [{stateVar.name}, {setterName}] = useState({initialVal});\n")
