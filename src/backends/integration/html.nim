## HTML Backend for Kryon
##
## This backend generates HTML/CSS/JS files instead of a native executable.
## DSL elements are converted to web standards with CSS handling layout and styling.

when defined(kryonDevMode):
  import ../../kryon/core
  import ../../kryon/fonts
  import ../../kryon/components/canvas
  import ../../kryon/rendering/renderingContext
  import ../../kryon/pipeline/renderCommands
  import ../../kryon/js_codegen
else:
  import kryon/core
  import kryon/fonts
  import kryon/components/canvas
  import kryon/rendering/renderingContext
  import kryon/pipeline/renderCommands
  import kryon/js_codegen
import options, tables, strutils, os, sets, sequtils

# ============================================================================
# Backend Type
# ============================================================================

type
  RouteInfo* = object
    name*: string          # Route name (e.g., "HomeScreen")
    fileName*: string     # HTML filename (e.g., "index.html")
    title*: string        # Page title
    element*: Element     # The UI element for this route
    isDefault*: bool      # Whether this is the default/index route

  HTMLBackend* = object
    windowWidth*: int
    windowHeight*: int
    windowTitle*: string
    backgroundColor*: Color
    htmlContent*: string
    cssContent*: string
    jsContent*: string
    elementCounter*: int  # For generating unique IDs
    cssClasses*: seq[string]  # Track generated CSS classes
    routes*: seq[RouteInfo]  # Detected routes for multi-file generation
    sharedCSS*: string      # Shared CSS content across all routes
    sharedJS*: string       # Shared JavaScript content across all routes

proc newHTMLBackend*(width, height: int, title: string): HTMLBackend =
  ## Create a new HTML backend
  result = HTMLBackend(
    windowWidth: width,
    windowHeight: height,
    windowTitle: title,
    backgroundColor: rgba(30, 30, 30, 255),
    htmlContent: "",
    cssContent: "",
    jsContent: "",
    elementCounter: 0,
    cssClasses: @[],
    routes: @[],
    sharedCSS: "",
    sharedJS: ""
  )


proc newHTMLBackendFromApp*(app: Element): HTMLBackend =
  ## Create backend from app element (extracts config from Header and Body)
  ## App structure: Body -> [Header, Body]

  var width = 800
  var height = 600
  var title = "Kryon App"
  var bgColor: Option[Value] = none(Value)

  # Look for Header and Body children in app
  for child in app.children:
    if child.kind == ekHeader:
      # Extract window config from Header
      var widthProp = child.getProp("windowWidth")
      if widthProp.isNone:
        widthProp = child.getProp("width")

      var heightProp = child.getProp("windowHeight")
      if heightProp.isNone:
        heightProp = child.getProp("height")

      var titleProp = child.getProp("windowTitle")
      if titleProp.isNone:
        titleProp = child.getProp("title")

      width = widthProp.get(val(800)).getInt()
      height = heightProp.get(val(600)).getInt()
      title = titleProp.get(val("Kryon App")).getString()
    elif child.kind == ekBody:
      # Extract window background from Body
      bgColor = child.getProp("backgroundColor")

  result = HTMLBackend(
    windowWidth: width,
    windowHeight: height,
    windowTitle: title,
    backgroundColor: if bgColor.isSome: bgColor.get.getColor() else: rgba(30, 30, 30, 255),
    htmlContent: "",
    cssContent: "",
    jsContent: "",
    elementCounter: 0,
    cssClasses: @[],
    routes: @[],
    sharedCSS: "",
    sharedJS: ""
  )

# ============================================================================
# Utility Functions
# ============================================================================

proc generateElementId*(backend: var HTMLBackend): string =
  ## Generate a unique element ID
  result = "kryon_elem_" & $backend.elementCounter
  inc backend.elementCounter

proc toCSSColor*(c: Color): string =
  ## Convert Kryon Color to CSS color string
  result = "rgba(" & $c.r & ", " & $c.g & ", " & $c.b & ", " & ($(c.a.float / 255.0)) & ")"

proc sanitizeCSSClass*(name: string): string =
  ## Sanitize string for use as CSS class name
  result = ""
  for ch in name:
    if ch.isAlphaNumeric() or ch == '_' or ch == '-':
      result.add(ch)
    else:
      result.add('_')

proc escapeHTML*(text: string): string =
  ## Escape text for HTML output
  result = text.multiReplace([
    ("&", "&amp;"),
    ("<", "&lt;"),
    (">", "&gt;"),
    ("\"", "&quot;"),
    ("'", "&#39;")
  ])

# ============================================================================
# Route Detection System
# ============================================================================

proc detectRouteVariable*(elem: Element): Option[string] =
  ## Detect if this element contains a case statement for routing
  ## Returns the variable name if found
  if elem.children.len > 0:
    for child in elem.children:
      # Look for patterns that suggest routing (case statements)
      # This is a simplified detection - in a real implementation we'd
      # need more sophisticated AST analysis
      if child.kind == ekContainer and child.children.len > 0:
        # Check if this looks like a case statement container
        # For now, we'll look for navigation patterns in the structure
        for grandChild in child.children:
          if grandChild.kind == ekButton or grandChild.kind == ekText:
            # If we find buttons with navigateTo calls, this is likely a routing app
            if grandChild.eventHandlers.len > 0:
              for event, _ in grandChild.eventHandlers:
                if event == "onClick":
                  # Assume this is a routing app and look for currentRoute pattern
                  return some("currentRoute")
  result = none(string)

proc extractRoutes*(backend: var HTMLBackend, root: Element): bool =
  ## Extract routes from a routing application
  ## Returns true if routes were detected, false otherwise

  # First, detect if this is a routing app
  let routeVar = detectRouteVariable(root)
  if routeVar.isNone:
    return false

  # For now, we'll use a simple heuristic based on the navigation example
  # In a full implementation, we'd parse the actual case statement

  # Look for the main body and check if it has multiple screens
  var bodyElement: Element = nil
  for child in root.children:
    if child.kind == ekBody:
      bodyElement = child
      break

  if bodyElement == nil or bodyElement.children.len == 0:
    return false

  # Look for screen components (Container with multiple child screens)
  var screensContainer: Element = nil
  for child in bodyElement.children:
    if child.kind == ekContainer and child.children.len >= 2:
      # Check if children look like different screens
      var hasTextContent = false
      var hasButtonContent = false
      for grandChild in child.children:
        if grandChild.kind == ekText:
          hasTextContent = true
        elif grandChild.kind == ekButton:
          hasButtonContent = true

      if hasTextContent or hasButtonContent:
        screensContainer = child
        break

  if screensContainer == nil:
    return false

  # Extract routes based on common patterns
  # For the navigation example, we'll create routes based on detected screens

  # Check for Home screen pattern
  var homeElement: Element = nil
  for child in screensContainer.children:
    if child.kind == ekColumn or child.kind == ekContainer:
      # Look for home screen characteristics
      var hasWelcomeText = false
      var hasNavigationButtons = false

      for grandChild in child.children:
        if grandChild.kind == ekText:
          let text = grandChild.getProp("text").get(val("")).getString()
          if "welcome" in text.toLowerAscii() or "home" in text.toLowerAscii():
            hasWelcomeText = true
        elif grandChild.kind == ekButton:
          let buttonText = grandChild.getProp("text").get(val("")).getString()
          if "about" in buttonText.toLowerAscii() or "settings" in buttonText.toLowerAscii():
            hasNavigationButtons = true

      if hasWelcomeText and hasNavigationButtons:
        homeElement = child
        break

  # Create Home route if found
  if homeElement != nil:
    backend.routes.add(RouteInfo(
      name: "HomeScreen",
      fileName: "index.html",
      title: backend.windowTitle,
      element: homeElement,
      isDefault: true
    ))

  # For now, we'll create placeholder routes for About and Settings
  # In a full implementation, we'd extract these from the actual case statement
  backend.routes.add(RouteInfo(
    name: "AboutScreen",
    fileName: "about.html",
    title: "About - " & backend.windowTitle,
    element: screensContainer,  # Placeholder - would extract actual About screen
    isDefault: false
  ))

  backend.routes.add(RouteInfo(
    name: "SettingsScreen",
    fileName: "settings.html",
    title: "Settings - " & backend.windowTitle,
    element: screensContainer,  # Placeholder - would extract actual Settings screen
    isDefault: false
  ))

  return backend.routes.len > 0

# ============================================================================
# CSS Generation
# ============================================================================

proc generateCSS*(backend: var HTMLBackend, elem: Element, parentId: string = ""): string =
  ## Generate CSS for an element and its children
  let elementId = if elem.id.len > 0: elem.id else: "elem_" & $backend.elementCounter
  # Only increment counter for elements that don't have a custom ID
  if elem.id.len == 0:
    inc backend.elementCounter
  # Use unique CSS class per element to avoid style collision
  let cssClass = "kryon-" & ($elem.kind).toLowerAscii().sanitizeCSSClass() & "-" & elementId.replace("_", "-")

  var css = ""

  # Generate CSS for this element
  css.add("." & cssClass & " {\n")

  # Position and size
  if elem.getProp("posX").isSome or elem.getProp("posY").isSome:
    css.add("  position: absolute;\n")
    if elem.getProp("posX").isSome:
      css.add("  left: " & $elem.getProp("posX").get.getFloat() & "px;\n")
    if elem.getProp("posY").isSome:
      css.add("  top: " & $elem.getProp("posY").get.getFloat() & "px;\n")

  if elem.getProp("width").isSome:
    css.add("  width: " & $elem.getProp("width").get.getFloat() & "px;\n")
  if elem.getProp("height").isSome:
    css.add("  height: " & $elem.getProp("height").get.getFloat() & "px;\n")

  # Colors and styling
  if elem.getProp("backgroundColor").isSome:
    css.add("  background-color: " & elem.getProp("backgroundColor").get.getColor().toCSSColor() & ";\n")
  elif elem.getProp("background").isSome:
    css.add("  background-color: " & elem.getProp("background").get.getColor().toCSSColor() & ";\n")

  if elem.getProp("color").isSome:
    css.add("  color: " & elem.getProp("color").get.getColor().toCSSColor() & ";\n")

  # Border
  if elem.getProp("borderWidth").isSome:
    css.add("  border-width: " & $elem.getProp("borderWidth").get.getFloat() & "px;\n")
  if elem.getProp("borderColor").isSome:
    css.add("  border-color: " & elem.getProp("borderColor").get.getColor().toCSSColor() & ";\n")
    css.add("  border-style: solid;\n")

  # Layout properties for containers
  case elem.kind:
  of ekColumn:
    css.add("  display: flex;\n")
    css.add("  flex-direction: column;\n")

    # Alignment
    let mainAxisAlignment = elem.getProp("mainAxisAlignment")
    if mainAxisAlignment.isSome:
      let align = mainAxisAlignment.get.getString()
      case align:
      of "center": css.add("  justify-content: center;\n")
      of "end", "flex-end": css.add("  justify-content: flex-end;\n")
      of "spaceBetween": css.add("  justify-content: space-between;\n")
      of "spaceAround": css.add("  justify-content: space-around;\n")
      of "spaceEvenly": css.add("  justify-content: space-evenly;\n")
      else: discard

    let crossAxisAlignment = elem.getProp("crossAxisAlignment")
    if crossAxisAlignment.isSome:
      let align = crossAxisAlignment.get.getString()
      case align:
      of "center": css.add("  align-items: center;\n")
      of "end", "flex-end": css.add("  align-items: flex-end;\n")
      of "stretch": css.add("  align-items: stretch;\n")
      else: discard

    # Gap
    let gap = elem.getProp("gap")
    if gap.isSome:
      css.add("  gap: " & $gap.get.getFloat() & "px;\n")

    # Flex property - allows column to expand in parent flex container
    let flexProp = elem.getProp("flex")
    if flexProp.isSome:
      css.add("  flex: " & $flexProp.get.getFloat() & ";\n")

  of ekRow:
    css.add("  display: flex;\n")
    css.add("  flex-direction: row;\n")

    # Alignment
    let mainAxisAlignment = elem.getProp("mainAxisAlignment")
    if mainAxisAlignment.isSome:
      let align = mainAxisAlignment.get.getString()
      case align:
      of "center": css.add("  justify-content: center;\n")
      of "end", "flex-end": css.add("  justify-content: flex-end;\n")
      of "spaceBetween": css.add("  justify-content: space-between;\n")
      of "spaceAround": css.add("  justify-content: space-around;\n")
      of "spaceEvenly": css.add("  justify-content: space-evenly;\n")
      else: discard

    let crossAxisAlignment = elem.getProp("crossAxisAlignment")
    if crossAxisAlignment.isSome:
      let align = crossAxisAlignment.get.getString()
      case align:
      of "center": css.add("  align-items: center;\n")
      of "end", "flex-end": css.add("  align-items: flex-end;\n")
      of "stretch": css.add("  align-items: stretch;\n")
      else: discard

    # Gap
    let gap = elem.getProp("gap")
    if gap.isSome:
      css.add("  gap: " & $gap.get.getFloat() & "px;\n")

    # Flex property - allows row to expand in parent flex container
    let flexProp = elem.getProp("flex")
    if flexProp.isSome:
      css.add("  flex: " & $flexProp.get.getFloat() & ";\n")

  of ekCenter:
    css.add("  display: flex;\n")
    css.add("  justify-content: center;\n")
    css.add("  align-items: center;\n")
    css.add("  text-align: center;\n")

  of ekContainer:
    let alignment = elem.getProp("contentAlignment")
    if alignment.isSome and alignment.get.getString() == "center":
      css.add("  display: flex;\n")
      css.add("  justify-content: center;\n")
      css.add("  align-items: center;\n")

    # Flex property - allows container to expand in parent flex container
    let flexProp = elem.getProp("flex")
    if flexProp.isSome:
      css.add("  flex: " & $flexProp.get.getFloat() & ";\n")

  of ekCanvas:
    css.add("  display: block;\n")
    css.add("  border: 1px solid #ccc;\n")

  of ekHeader, ekBody, ekConditional, ekForLoop, ekText, ekButton, ekInput, ekCheckbox, ekDropdown, ekGrid, ekImage, ekScrollView, ekTabGroup, ekTabBar, ekTab, ekTabContent, ekTabPanel, ekResources, ekFont, ekLink, ekSpacer, ekDraggable, ekDropTarget, ekH1, ekH2, ekH3:
    # These elements either don't need special CSS layout or are handled elsewhere
    discard

  # Text styling
  if elem.kind == ekText:
    let fontSize = elem.getProp("fontSize").get(val(16)).getInt()
    css.add("  font-size: " & $fontSize & "px;\n")

    # Text alignment
    let textAlign = elem.getProp("textAlign")
    if textAlign.isSome:
      css.add("  text-align: " & textAlign.get.getString() & ";\n")

  # Button styling
  if elem.kind == ekButton:
    css.add("  display: inline-flex;\n")
    css.add("  align-items: center;\n")
    css.add("  justify-content: center;\n")
    css.add("  cursor: pointer;\n")
    css.add("  border: none;\n")
    css.add("  user-select: none;\n")
    css.add("  text-decoration: none;\n")  # For navigation links
    css.add("  color: inherit;\n")           # For navigation links
    css.add("  box-sizing: border-box;\n")   # Ensure padding doesn't affect dimensions

    # Font size - default 20 to match layout engine
    let fontSize = elem.getProp("fontSize").get(val(20)).getInt()
    css.add("  font-size: " & $fontSize & "px;\n")

    # Button padding - removed conditional, always add some padding for text
    # This ensures text doesn't touch button edges
    css.add("  padding: 4px 8px;\n")

  # Input styling
  if elem.kind == ekInput:
    css.add("  display: inline-block;\n")
    # Border styling will be handled by the generic border properties above
    css.add("  padding: 8px;\n")
    css.add("  font-size: 16px;\n")
    css.add("  border-radius: 4px;\n")
    css.add("  outline: none;\n")
    css.add("  transition: border-color 0.2s;\n")
    css.add("  box-sizing: border-box;\n")

  # Checkbox styling
  if elem.kind == ekCheckbox:
    css.add("  display: flex;\n")
    css.add("  align-items: center;\n")
    css.add("  gap: 8px;\n")
    css.add("  cursor: pointer;\n")
    css.add("  font-size: 16px;\n")

  css.add("}\n")

  # Add hover and focus states
  if elem.kind == ekButton:
    css.add("." & cssClass & ":hover {\n")
    css.add("  opacity: 0.8;\n")
    css.add("}\n")
  elif elem.kind == ekInput:
    css.add("." & cssClass & ":focus {\n")
    css.add("  border-color: #4A90E2;\n")
    css.add("}\n")

  css.add("\n")

  # Generate CSS for children
  if elem.kind == ekForLoop:
    # For loops need special handling - expand to get actual children
    let expandedChildren = expandForLoopChildren(elem)
    for child in expandedChildren:
      css.add(backend.generateCSS(child, elementId))
  else:
    for child in elem.children:
      css.add(backend.generateCSS(child, elementId))

  result = css

# ============================================================================
# JavaScript State Management System
# ============================================================================

type
  StateVariable = object
    name: string
    varType: string  # "string", "number", "boolean", "array", "object"
    initialValue: string
    elementId: string  # Element that owns this state

  DependencyInfo = object
    stateVar: string      # Name of the state variable
    elementId: string     # ID of the dependent element
    propertyName: string  # Which property depends on it (text, src, backgroundColor, etc.)

# Forward declarations
proc generateDOMUpdateFunctions*(backend: var HTMLBackend, root: Element, dependencies: seq[DependencyInfo]): string

proc addStateVariable(states: var seq[StateVariable], name: string, varType: string, initialValue: string, elementId: string) =
  ## Helper to add a state variable if it doesn't already exist
  for state in states:
    if state.name == name:
      return  # Already exists
  states.add(StateVariable(
    name: name,
    varType: varType,
    initialValue: initialValue,
    elementId: elementId
  ))

proc evaluateInitialValue(val: Value): string =
  ## Evaluate a Value to get its JavaScript representation
  case val.kind:
  of vkString:
    return "\"" & val.strVal.replace("\"", "\\\"") & "\""
  of vkInt:
    return $val.intVal
  of vkFloat:
    return $val.floatVal
  of vkBool:
    return if val.boolVal: "true" else: "false"
  of vkColor:
    let c = val.colorVal
    return "\"rgba(" & $c.r & "," & $c.g & "," & $c.b & "," & $(c.a.float / 255.0) & ")\""
  of vkGetter:
    # Execute getter to get current value
    try:
      let currentVal = val.getter()
      return evaluateInitialValue(currentVal)
    except:
      return "null"
  else:
    return "null"

proc extractStateVarsFromJS(jsCode: string): seq[string] =
  ## Parse JavaScript code for updateState('varName', ...) calls
  ## Returns list of unique state variable names found
  result = @[]
  var idx = 0
  while idx < jsCode.len:
    idx = jsCode.find("updateState('", idx)
    if idx == -1:
      break
    let startIdx = idx + 13  # len("updateState('")
    let endIdx = jsCode.find("'", startIdx)
    if endIdx > startIdx:
      let varName = jsCode[startIdx ..< endIdx]
      if varName notin result:
        result.add(varName)
    idx = endIdx + 1

proc extractDependencies(elem: Element, elementId: string, dependencies: var seq[DependencyInfo]) =
  ## Extract which state variables this element depends on
  ## We do this by temporarily hooking into registerDependency

  # Store the original currentElementBeingProcessed
  let originalCurrent = currentElementBeingProcessed
  currentElementBeingProcessed = elem

  # Track which dependencies are registered
  var registeredDeps: seq[string] = @[]

  # Evaluate each property that might have a getter
  for propName, propValue in elem.properties:
    if propValue.kind == vkGetter:
      try:
        # Clear reactive dependencies for this element
        if propName in reactiveDependencies:
          reactiveDependencies.del(propName)

        # Evaluate the getter - this will call registerDependency
        discard propValue.getter()

        # Check what dependencies were registered
        for varName in reactiveDependencies.keys:
          if elem in reactiveDependencies[varName]:
            if varName notin registeredDeps:
              registeredDeps.add(varName)
              dependencies.add(DependencyInfo(
                stateVar: varName,
                elementId: elementId,
                propertyName: propName
              ))
      except:
        # Ignore errors during dependency extraction
        discard

  # Restore original
  currentElementBeingProcessed = originalCurrent

proc extractStateVariables(elem: Element, states: var seq[StateVariable], dependencies: var seq[DependencyInfo], elementId: string = "") =
  ## Recursively extract state variables and dependencies from element tree
  ## State variables include:
  ## - Bound input values (Input with value property)
  ## - Reactive variables referenced in event handlers
  ## - For loop iterables

  # Generate element ID if not provided
  let currentElementId = if elementId.len > 0: elementId else: "elem_" & $elem.id

  # Check for bound variables (Input elements with value binding)
  if elem.kind == ekInput and elem.boundVarName.isSome:
    let varName = elem.boundVarName.get()
    let initialValue = elem.getProp("value").get(val("")).getString()
    addStateVariable(states, varName, "string", "\"" & initialValue & "\"", "input_" & varName)

  # Extract state variables from event handlers
  for event, jsCode in elem.jsHandlerCode:
    for varName in extractStateVarsFromJS(jsCode):
      # Add with default initial value 0 (will be smarter in the future)
      addStateVariable(states, varName, "number", "0", "handler_" & currentElementId)

  # Extract dependencies for this element
  extractDependencies(elem, currentElementId, dependencies)

  # Process children recursively
  for i, child in elem.children:
    let childId = currentElementId & "_" & $i
    extractStateVariables(child, states, dependencies, childId)

proc collectReactiveExprs(elem: Element, elementId: string, exprs: var Table[string, seq[ReactiveExpression]]) =
  ## Recursively collect reactive expressions from element tree
  ## Groups expressions by element ID

  # Add this element's reactive expressions
  if elem.reactiveExpressions.len > 0:
    if elementId notin exprs:
      exprs[elementId] = @[]

    for propName, expr in elem.reactiveExpressions:
      exprs[elementId].add(expr)

  # Recurse into children
  for i, child in elem.children:
    let childId = elementId & "_" & $i
    collectReactiveExprs(child, childId, exprs)

proc generateStateManagementJS*(backend: var HTMLBackend, root: Element): string =
  ## Generate JavaScript state management code
  var states: seq[StateVariable] = @[]
  var dependencies: seq[DependencyInfo] = @[]
  extractStateVariables(root, states, dependencies)

  var js = """
// ============================================================================
// Kryon State Management System
// ============================================================================

// Application state object
const AppState = {
"""

  # Add state variables
  if states.len > 0:
    for i, state in states:
      js.add("  " & state.name & ": " & state.initialValue)
      if i < states.len - 1:
        js.add(",\n")
      else:
        js.add("\n")
  else:
    js.add("  // No state variables detected\n")

  js.add("""};

// Theme data for reactive styling
const themes = [
  {name: "Dark", background: "#1a1a1a", primary: "#2d2d2d", secondary: "#3d3d3d", text: "#ffffff", accent: "#4a90e2"},
  {name: "Light", background: "#f5f5f5", primary: "#ffffff", secondary: "#e0e0e0", text: "#333333", accent: "#2196f3"},
  {name: "Ocean", background: "#0d1b2a", primary: "#1b263b", secondary: "#415a77", text: "#e0e1dd", accent: "#778da9"}
];

function getCurrentTheme() {
  return themes[AppState.currentThemeIndex || 0];
}

// State update function with reactive invalidation
function updateState(key, value) {
  if (AppState[key] !== value) {
    AppState[key] = value;
    invalidateState(key);
  }
}

// Get state value
function getState(key) {
  return AppState[key];
}

// Dependency tracking for reactive updates
const StateDependencies = {
""")

  # Build dependency mappings from reactive expressions
  # Map: state variable -> list of element IDs that depend on it
  var depMap: Table[string, seq[string]] = initTable[string, seq[string]]()

  # Collect dependencies from reactive expressions in element tree
  var elemExprs: Table[string, seq[ReactiveExpression]] = initTable[string, seq[ReactiveExpression]]()
  collectReactiveExprs(root, "elem_0", elemExprs)

  for elementId, exprs in elemExprs:
    for expr in exprs:
      # Each reactive expression has a list of state variables it depends on
      for stateVar in expr.dependencies:
        if stateVar notin depMap:
          depMap[stateVar] = @[]
        if elementId notin depMap[stateVar]:
          depMap[stateVar].add(elementId)

  # Generate JavaScript dependency object
  var firstDep = true
  for stateVar, elemIds in depMap:
    if not firstDep:
      js.add(",\n")
    firstDep = false
    js.add("  '" & stateVar & "': [")
    for i, elemId in elemIds:
      js.add("'" & elemId & "'")
      if i < elemIds.len - 1:
        js.add(", ")
    js.add("]")

  js.add("""
};

// Register a dependency between state variable and DOM element
function registerDependency(stateKey, elementId) {
  if (!StateDependencies[stateKey]) {
    StateDependencies[stateKey] = [];
  }
  if (!StateDependencies[stateKey].includes(elementId)) {
    StateDependencies[stateKey].push(elementId);
  }
}

// Invalidate state and trigger re-render of dependent elements
function invalidateState(stateKey) {
  const dependents = StateDependencies[stateKey] || [];
  dependents.forEach(elementId => {
    reRenderElement(elementId);
  });
}

// Log state changes (for debugging)
function logState() {
  console.log('Current AppState:', AppState);
}

""")

  # Now generate element-specific update functions based on dependencies
  js.add(backend.generateDOMUpdateFunctions(root, dependencies))

  result = js

# ============================================================================
# DOM Update Functions Generation
# ============================================================================

proc generateDOMUpdateFunctions*(backend: var HTMLBackend, root: Element, dependencies: seq[DependencyInfo]): string =
  ## Generate JavaScript functions that update specific DOM elements when state changes
  var js = """
// ============================================================================
// DOM Update Functions
// ============================================================================

// Map of element IDs to their update functions
const UpdateFunctions = {};

"""

  # Collect reactive expressions from element tree
  var elemExprs: Table[string, seq[ReactiveExpression]] = initTable[string, seq[ReactiveExpression]]()
  collectReactiveExprs(root, "elem_0", elemExprs)

  # Generate update function for each element with reactive expressions
  for elementId, exprs in elemExprs:
    js.add("// Update function for element " & elementId & "\n")
    js.add("UpdateFunctions['" & elementId & "'] = function() {\n")
    js.add("  const elem = document.querySelector('[data-element-id=\"" & elementId & "\"]');\n")
    js.add("  if (!elem) return;\n")
    js.add("  \n")

    # For each reactive expression, generate update code using the stored JavaScript expression
    for expr in exprs:
      js.add("  // Update " & expr.propertyName & " (depends on: " & expr.dependencies.join(", ") & ")\n")

      case expr.propertyName:
      of "text":
        js.add("  elem.textContent = String(" & expr.jsExpression & ");\n")
      of "src":
        js.add("  elem.src = String(" & expr.jsExpression & ");\n")
      of "backgroundColor", "background":
        js.add("  elem.style.backgroundColor = " & expr.jsExpression & ";\n")
      of "color":
        js.add("  elem.style.color = " & expr.jsExpression & ";\n")
      of "value":
        js.add("  elem.value = String(" & expr.jsExpression & ");\n")
      of "disabled":
        js.add("  elem.disabled = Boolean(" & expr.jsExpression & ");\n")
      of "width":
        js.add("  elem.style.width = " & expr.jsExpression & ";\n")
      of "height":
        js.add("  elem.style.height = " & expr.jsExpression & ";\n")
      else:
        # Generic property update
        js.add("  elem.setAttribute('" & expr.propertyName & "', String(" & expr.jsExpression & "));\n")

    js.add("};\n\n")

  js.add("""
// Re-render a specific element using its update function
function reRenderElement(elementId) {
  if (UpdateFunctions[elementId]) {
    UpdateFunctions[elementId]();
  } else {
    console.log('No update function for element:', elementId);
  }
}

""")

  result = js

# ============================================================================
# Event Command Recording System
# ============================================================================

type
  EventCommandKind = enum
    ecStateAssignment    # updateState(varName, value)
    ecArrayAdd           # arr.push(value)
    ecArrayDelete        # arr.splice(index, 1)
    ecFunctionCall       # call a named function
    ecConsoleLog         # console.log for debugging

  EventCommand = object
    case kind: EventCommandKind
    of ecStateAssignment:
      varName: string
      valueExpr: string  # JavaScript expression for the value
    of ecArrayAdd:
      arrayName: string
      addValueExpr: string
    of ecArrayDelete:
      delArrayName: string
      delIndex: int
    of ecFunctionCall:
      funcName: string
      funcArgs: seq[string]
    of ecConsoleLog:
      logMessage: string

var eventCommands: seq[EventCommand] = @[]
var recordingEvents: bool = false

proc startRecordingEvents() =
  ## Start recording event commands
  eventCommands = @[]
  recordingEvents = true

proc stopRecordingEvents(): seq[EventCommand] =
  ## Stop recording and return captured commands
  recordingEvents = false
  result = eventCommands
  eventCommands = @[]

proc recordStateAssignment(varName: string, valueExpr: string) =
  ## Record a state variable assignment
  if recordingEvents:
    eventCommands.add(EventCommand(
      kind: ecStateAssignment,
      varName: varName,
      valueExpr: valueExpr
    ))

proc recordArrayAdd(arrayName: string, valueExpr: string) =
  ## Record an array append operation
  if recordingEvents:
    eventCommands.add(EventCommand(
      kind: ecArrayAdd,
      arrayName: arrayName,
      addValueExpr: valueExpr
    ))

proc recordFunctionCall(funcName: string, args: seq[string] = @[]) =
  ## Record a function call
  if recordingEvents:
    eventCommands.add(EventCommand(
      kind: ecFunctionCall,
      funcName: funcName,
      funcArgs: args
    ))

proc generateEventCommandJS(cmd: EventCommand): string =
  ## Convert an event command to JavaScript code
  case cmd.kind:
  of ecStateAssignment:
    return "  updateState('" & cmd.varName & "', " & cmd.valueExpr & ");\n"
  of ecArrayAdd:
    return "  " & cmd.arrayName & ".push(" & cmd.addValueExpr & ");\n  updateState('" & cmd.arrayName & "', " & cmd.arrayName & ");\n"
  of ecArrayDelete:
    return "  " & cmd.delArrayName & ".splice(" & $cmd.delIndex & ", 1);\n  updateState('" & cmd.delArrayName & "', " & cmd.delArrayName & ");\n"
  of ecFunctionCall:
    let argsStr = cmd.funcArgs.join(", ")
    return "  " & cmd.funcName & "(" & argsStr & ");\n"
  of ecConsoleLog:
    return "  console.log(" & cmd.logMessage & ");\n"

# ============================================================================
# JavaScript Event Handler Generation
# ============================================================================

proc generateJavaScriptForElement*(backend: var HTMLBackend, elem: Element, elementId: string) =
  ## Generate JavaScript event handlers for an element
  if elem.eventHandlers.len > 0:
    for event, handler in elem.eventHandlers:
      # Check if we have JavaScript code for this handler
      if event in elem.jsHandlerCode:
        # Use the provided JavaScript code
        var jsCode = elem.jsHandlerCode[event]

        # Check if this element has a loop index (from being inside a for loop)
        let loopIndexProp = elem.getProp("__loopIndex")
        if loopIndexProp.isSome:
          let loopIdx = loopIndexProp.get().getInt()
          # Substitute common loop variable names with the actual index
          jsCode = substituteLoopVariables(jsCode, [("i", $loopIdx), ("idx", $loopIdx), ("index", $loopIdx)])

        backend.jsContent.add("// " & $elem.kind & " handler for element " & elementId & "\n")
        backend.jsContent.add("document.querySelector('[data-element-id=\"" & elementId & "\"]').addEventListener('" & event.replace("on", "").toLowerAscii() & "', function(e) {\n")
        backend.jsContent.add(jsCode)
        backend.jsContent.add("});\n\n")
        continue

      # Otherwise, generate default handler based on element type
      case elem.kind:
      of ekButton:
        if event == "onClick":
          backend.jsContent.add("// Button click handler for element " & elementId & "\n")
          backend.jsContent.add("document.querySelector('[data-element-id=\"" & elementId & "\"]').addEventListener('click', function() {\n")
          backend.jsContent.add("  // Kryon event handler: " & event & "\n")
          backend.jsContent.add("  console.log('Button clicked! Event: " & event & "');\n")
          backend.jsContent.add("  // Original Nim procedure would be called here\n")
          backend.jsContent.add("  // For now, showing a browser alert for demonstration\n")
          backend.jsContent.add("  alert('Button clicked! 🎯\\n\\nThis would normally call your Nim event handler procedure.\\n\\nElement ID: ' + '" & elementId & "' + '\\nEvent: ' + '" & event & "');\n")
          backend.jsContent.add("});\n\n")

      of ekInput:
        if event == "onChange" or event == "onTextChange":
          backend.jsContent.add("// Input change handler for element " & elementId & "\n")
          backend.jsContent.add("document.querySelector('[data-element-id=\"" & elementId & "\"]').addEventListener('input', function(e) {\n")

          # Check if this input has a bound variable
          if elem.boundVarName.isSome:
            let varName = elem.boundVarName.get()
            backend.jsContent.add("  // Update bound state variable: " & varName & "\n")
            backend.jsContent.add("  updateState('" & varName & "', e.target.value);\n")
          else:
            backend.jsContent.add("  // No bound variable, just log the change\n")
            backend.jsContent.add("  console.log('Input changed! New value:', e.target.value);\n")

          backend.jsContent.add("});\n\n")

      of ekCheckbox:
        if event == "onClick" or event == "onChange":
          backend.jsContent.add("// Checkbox handler for element " & elementId & "\n")
          backend.jsContent.add("document.querySelector('[data-element-id=\"" & elementId & "\"]').addEventListener('change', function(e) {\n")
          backend.jsContent.add("  // Kryon event handler: " & event & "\n")
          backend.jsContent.add("  const checkbox = e.target.querySelector('input[type=\"checkbox\"]');\n")
          backend.jsContent.add("  console.log('Checkbox ' + (checkbox.checked ? 'checked' : 'unchecked') + '!');\n")
          backend.jsContent.add("  console.log('Event: " & event & "');\n")
          backend.jsContent.add("  // Original Nim procedure would be called here with the checked state\n")
          backend.jsContent.add("  // For now, logging the change to console\n")
          backend.jsContent.add("});\n\n")

      of ekDropdown:
        if event == "onChange":
          backend.jsContent.add("// Dropdown change handler for element " & elementId & "\n")
          backend.jsContent.add("document.querySelector('[data-element-id=\"" & elementId & "\"]').addEventListener('change', function(e) {\n")
          backend.jsContent.add("  // Kryon event handler: " & event & "\n")
          backend.jsContent.add("  console.log('Dropdown selection changed!');\n")
          backend.jsContent.add("  console.log('Selected value:', e.target.value);\n")
          backend.jsContent.add("  console.log('Selected index:', e.target.selectedIndex);\n")
          backend.jsContent.add("  console.log('Event: " & event & "');\n")
          backend.jsContent.add("  // Original Nim procedure would be called here with the selected value\n")
          backend.jsContent.add("  // For now, logging the change to console\n")
          backend.jsContent.add("});\n\n")

      else:
        # For other element types, generate generic event handlers
        backend.jsContent.add("// Generic event handler for element " & elementId & "\n")
        backend.jsContent.add("document.querySelector('[data-element-id=\"" & elementId & "\"]').addEventListener('" & event & "', function(e) {\n")
        backend.jsContent.add("  // Kryon event handler: " & event & "\n")
        backend.jsContent.add("  console.log('Event triggered: " & event & "');\n")
        backend.jsContent.add("  console.log('Element ID: ' + '" & elementId & "' + '\nEvent: ' + '" & event & "');\n")
        backend.jsContent.add("  // Original Nim procedure would be called here\n")
        backend.jsContent.add("  // For now, logging to console\n")
        backend.jsContent.add("});\n\n")

# ============================================================================
# Canvas JavaScript Generation
# ============================================================================

proc generateCanvasJavaScript*(backend: var HTMLBackend, elem: Element, canvasId: string, canvasData: CanvasData) =
  ## Generate JavaScript code to render canvas content
  ## This procedure extracts drawing commands from the Nim canvas procedure
  ## and converts them to HTML5 Canvas API calls

  backend.jsContent.add("// Canvas drawing for element " & canvasId & "\n")
  backend.jsContent.add("document.addEventListener('DOMContentLoaded', function() {\n")
  backend.jsContent.add("  const canvas_" & canvasId & " = document.getElementById('" & canvasId & "');\n")
  backend.jsContent.add("  if (!canvas_" & canvasId & ") return;\n")
  backend.jsContent.add("  const ctx_" & canvasId & " = canvas_" & canvasId & ".getContext('2d');\n")
  backend.jsContent.add("  \n")

  # Clear canvas background if specified
  if canvasData.hasBackground:
    let bg = canvasData.backgroundColor
    backend.jsContent.add("  // Clear background\n")
    backend.jsContent.add("  ctx_" & canvasId & ".fillStyle = 'rgba(" & $bg.r & ", " & $bg.g & ", " & $bg.b & ", " & $(bg.a.float / 255.0) & ")';\n")
    backend.jsContent.add("  ctx_" & canvasId & ".fillRect(0, 0, " & $canvasData.width & ", " & $canvasData.height & ");\n")
    backend.jsContent.add("  \n")

  # Execute the drawing procedure and capture render commands
  if canvasData.drawProc != nil:
    var drawCtx = newDrawingContext()
    var commands: seq[renderCommands.RenderCommand] = @[]
    drawCtx.renderCommands = addr commands

    # Execute the user's drawing procedure
    canvasData.drawProc(drawCtx, canvasData.width, canvasData.height)

    # Convert render commands to JavaScript
    backend.jsContent.add("  // User drawing commands\n")
    for cmd in commands:
      case cmd.kind:
      of rcClearCanvas:
        let c = cmd.clearColor
        backend.jsContent.add("  ctx_" & canvasId & ".fillStyle = 'rgba(" & $c.r & ", " & $c.g & ", " & $c.b & ", " & $(c.a.float / 255.0) & ")';\n")
        backend.jsContent.add("  ctx_" & canvasId & ".fillRect(" & $cmd.clearX & ", " & $cmd.clearY & ", " & $cmd.clearWidth & ", " & $cmd.clearHeight & ");\n")

      of rcDrawPath:
        # Begin path
        backend.jsContent.add("  ctx_" & canvasId & ".beginPath();\n")

        # Translate path commands
        for pathCmd in cmd.pathCommands:
          case pathCmd.kind:
          of PathCommandKind.MoveTo:
            backend.jsContent.add("  ctx_" & canvasId & ".moveTo(" & $pathCmd.moveToX & ", " & $pathCmd.moveToY & ");\n")
          of PathCommandKind.LineTo:
            backend.jsContent.add("  ctx_" & canvasId & ".lineTo(" & $pathCmd.lineToX & ", " & $pathCmd.lineToY & ");\n")
          of PathCommandKind.Arc:
            backend.jsContent.add("  ctx_" & canvasId & ".arc(" & $pathCmd.arcX & ", " & $pathCmd.arcY & ", " & $pathCmd.arcRadius & ", " & $pathCmd.arcStartAngle & ", " & $pathCmd.arcEndAngle & ");\n")
          of PathCommandKind.BezierCurveTo:
            backend.jsContent.add("  ctx_" & canvasId & ".bezierCurveTo(" & $pathCmd.cp1x & ", " & $pathCmd.cp1y & ", " & $pathCmd.cp2x & ", " & $pathCmd.cp2y & ", " & $pathCmd.bezierX & ", " & $pathCmd.bezierY & ");\n")
          of PathCommandKind.ClosePath:
            backend.jsContent.add("  ctx_" & canvasId & ".closePath();\n")

        # Fill or stroke based on command flags
        if cmd.pathShouldFill:
          let c = cmd.pathFillStyle
          backend.jsContent.add("  ctx_" & canvasId & ".fillStyle = 'rgba(" & $c.r & ", " & $c.g & ", " & $c.b & ", " & $(c.a.float / 255.0) & ")';\n")
          backend.jsContent.add("  ctx_" & canvasId & ".fill();\n")

        if cmd.pathShouldStroke:
          let c = cmd.pathStrokeStyle
          backend.jsContent.add("  ctx_" & canvasId & ".strokeStyle = 'rgba(" & $c.r & ", " & $c.g & ", " & $c.b & ", " & $(c.a.float / 255.0) & ")';\n")
          backend.jsContent.add("  ctx_" & canvasId & ".lineWidth = " & $cmd.pathLineWidth & ";\n")
          backend.jsContent.add("  ctx_" & canvasId & ".stroke();\n")

      of rcDrawText:
        let c = cmd.textColor
        backend.jsContent.add("  ctx_" & canvasId & ".fillStyle = 'rgba(" & $c.r & ", " & $c.g & ", " & $c.b & ", " & $(c.a.float / 255.0) & ")';\n")
        backend.jsContent.add("  ctx_" & canvasId & ".font = '" & $cmd.textSize & "px Arial';\n")
        backend.jsContent.add("  ctx_" & canvasId & ".fillText('" & cmd.textContent.replace("'", "\\'") & "', " & $cmd.textX & ", " & $cmd.textY & ");\n")

      else:
        # Other render commands not commonly used in canvas (like rcDrawRectangle, rcDrawLine)
        # Can be implemented as needed
        discard

  backend.jsContent.add("});\n\n")

# ============================================================================
# HTML Generation (Clean Version)
# ============================================================================

proc generateNavigationLink*(backend: var HTMLBackend, elem: Element, text: string): string =
  ## Generate navigation link instead of button for routing apps
  let elementId = if elem.id.len > 0: elem.id else: "elem_" & $backend.elementCounter
  let cssClass = "kryon-" & ($elem.kind).toLowerAscii().sanitizeCSSClass()

  # Determine target route based on button text
  var targetFile = "index.html"
  let lowerText = text.toLowerAscii()

  if "about" in lowerText:
    targetFile = "about.html"
  elif "settings" in lowerText:
    targetFile = "settings.html"
  elif "back" in lowerText or "home" in lowerText:
    targetFile = "index.html"

  result = "<a href=\"" & targetFile & "\" class=\"" & cssClass & "\" data-element-id=\"" & elementId & "\">" & text.escapeHTML() & "</a>\n"

proc generateHTML*(backend: var HTMLBackend, elem: Element): string =
  ## Generate HTML for an element and its children
  let elementId = if elem.id.len > 0: elem.id else: "elem_" & $backend.elementCounter
  # Only increment counter for elements that don't have a custom ID
  if elem.id.len == 0:
    inc backend.elementCounter
  # Use unique CSS class per element to match CSS generation
  let cssClass = "kryon-" & ($elem.kind).toLowerAscii().sanitizeCSSClass() & "-" & elementId.replace("_", "-")

  var html = ""

  case elem.kind:
  of ekHeader, ekBody, ekConditional:
    # These are structural elements - don't render themselves
    for child in elem.children:
      html.add(backend.generateHTML(child))

  of ekForLoop:
    # For loops need special handling - execute the builder to get actual children
    let expandedChildren = expandForLoopChildren(elem)
    for idx, child in expandedChildren:
      # Store loop index in child for potential JS substitution
      # This helps with loop variable capture in event handlers
      child.setProp("__loopIndex", val(idx))
      html.add(backend.generateHTML(child))

  of ekContainer:
    html.add("<div class=\"" & cssClass & "\">\n")
    for child in elem.children:
      html.add(backend.generateHTML(child))
    html.add("</div>\n")

  of ekText:
    let text = elem.getProp("text").get(val("")).getString()
    html.add("<span class=\"" & cssClass & "\">" & text.escapeHTML() & "</span>\n")

  of ekButton:
    let text = elem.getProp("text").get(val("Button")).getString()

    # Check if this is a routing app and generate navigation link instead
    if backend.routes.len > 0 and elem.eventHandlers.hasKey("onClick"):
      html.add(backend.generateNavigationLink(elem, text))
    else:
      html.add("<button class=\"" & cssClass & "\" data-element-id=\"" & elementId & "\">" & text.escapeHTML() & "</button>\n")

      # Generate JavaScript for element if it has event handlers
      if elem.eventHandlers.len > 0:
        backend.generateJavaScriptForElement(elem, elementId)

  of ekInput:
    let placeholder = elem.getProp("placeholder").get(val("")).getString()
    let value = elem.getProp("value").get(val("")).getString()
    html.add("<input type=\"text\" class=\"" & cssClass & "\" data-element-id=\"" & elementId & "\" placeholder=\"" & placeholder & "\" value=\"" & value & "\">\n")

    # Generate JavaScript for element if it has event handlers
    if elem.eventHandlers.len > 0:
      backend.generateJavaScriptForElement(elem, elementId)

  of ekCheckbox:
    let label = elem.getProp("label").get(val("")).getString()
    let checked = elem.getProp("checked").get(val(false)).getBool()
    let checkedAttr = if checked: "checked" else: ""

    html.add("<label class=\"" & cssClass & "\" data-element-id=\"" & elementId & "\">\n")
    html.add("  <input type=\"checkbox\" " & checkedAttr & ">\n")
    html.add("  <span>" & label.escapeHTML() & "</span>\n")
    html.add("</label>\n")

    # Generate JavaScript for element if it has event handlers
    if elem.eventHandlers.len > 0:
      backend.generateJavaScriptForElement(elem, elementId)

  of ekDropdown:
    html.add("<select class=\"" & cssClass & "\" data-element-id=\"" & elementId & "\">\n")

    for i, option in elem.dropdownOptions:
      let selectedAttr = if i == elem.dropdownSelectedIndex: "selected" else: ""
      html.add("  <option value=\"" & option & "\" " & selectedAttr & ">" & option.escapeHTML() & "</option>\n")

    html.add("</select>\n")

    # Generate JavaScript for element if it has event handlers
    if elem.eventHandlers.len > 0:
      backend.generateJavaScriptForElement(elem, elementId)

  of ekColumn, ekRow, ekCenter:
    html.add("<div class=\"" & cssClass & "\"\">\n")
    for child in elem.children:
      html.add(backend.generateHTML(child))
    html.add("</div>\n")

  of ekTabGroup:
    html.add("<div class=\"" & cssClass & "\" data-element-id=\"" & elementId & "\">\n")
    for child in elem.children:
      html.add(backend.generateHTML(child))
    html.add("</div>\n")

  of ekTabBar:
    html.add("<div class=\"" & cssClass & "\">\n")
    for child in elem.children:
      html.add(backend.generateHTML(child))
    html.add("</div>\n")

  of ekTab:
    let title = elem.getProp("title").get(val("Tab")).getString()
    html.add("<button class=\"" & cssClass & "\" data-element-id=\"" & elementId & "\">" & title.escapeHTML() & "</button>\n")

  of ekTabContent:
    html.add("<div class=\"" & cssClass & "\">\n")
    for child in elem.children:
      html.add(backend.generateHTML(child))
    html.add("</div>\n")

  of ekTabPanel:
    html.add("<div class=\"" & cssClass & "\">\n")
    for child in elem.children:
      html.add(backend.generateHTML(child))
    html.add("</div>\n")

  of ekCanvas:
    # Generate canvas element with unique ID for JavaScript access
    # Get dimensions from properties (layout hasn't run yet in HTML backend)
    let width = elem.getProp("width").get(val(300)).getFloat()
    let height = elem.getProp("height").get(val(150)).getFloat()

    html.add("<canvas id=\"" & elementId & "\" class=\"" & cssClass & "\" width=\"" & $width.int & "\" height=\"" & $height.int & "\" data-element-id=\"" & elementId & "\"></canvas>\n")

    # Generate JavaScript to render canvas content
    if elem.canvasDrawProc != nil:
      # Create canvas data manually with correct dimensions
      var canvasData = CanvasData()
      canvasData.x = 0
      canvasData.y = 0
      canvasData.width = width
      canvasData.height = height
      canvasData.drawProc = cast[proc(ctx: DrawingContext, width, height: float) {.closure.}](elem.canvasDrawProc)

      let bgColor = elem.getProp("backgroundColor")
      if bgColor.isSome:
        canvasData.hasBackground = true
        canvasData.backgroundColor = bgColor.get.getColor()

      backend.generateCanvasJavaScript(elem, elementId, canvasData)

  else:
    # Default case - render as div
    html.add("<div class=\"" & cssClass & "\" data-element-id=\"" & elementId & "\">\n")
    for child in elem.children:
      html.add(backend.generateHTML(child))
    html.add("</div>\n")

  result = html

# ============================================================================
# Multi-File Generation System
# ============================================================================

proc generateSharedAssets*(backend: var HTMLBackend, root: Element, outputDir: string) =
  ## Generate shared CSS and JavaScript files
  let sharedDir = outputDir / "shared"
  createDir(sharedDir)

  # Determine default font family from Body (if specified)
  let bodyFontFamily = resolvePreferredFont(root)

  # Copy font files to shared directory (custom + resources)
  var copiedFontFiles = initHashSet[string]()
  var fontFaceSegments: seq[string] = @[]

  let fontInfo = getDefaultFontInfo()
  if fontInfo.path.len > 0:
    let fontSplit = fontInfo.path.splitFile()
    let fontFileName = fontSplit.name & fontSplit.ext
    let destFontPath = sharedDir / fontFileName
    if fontFileName notin copiedFontFiles:
      copyFile(fontInfo.path, destFontPath)
      copiedFontFiles.incl(fontFileName)
      echo "Copied font to shared assets: " & destFontPath
    fontFaceSegments.add(generateFontFaceCSS(fontInfo.name, "shared/" & fontFileName))

  for fontName in getRegisteredFontNames():
    let resourceInfo = getResourceFontInfo(fontName)
    if resourceInfo.path.len > 0:
      let split = resourceInfo.path.splitFile()
      let fontFileName = split.name & split.ext
      if fontFileName notin copiedFontFiles:
        let destFontPath = sharedDir / fontFileName
        copyFile(resourceInfo.path, destFontPath)
        copiedFontFiles.incl(fontFileName)
        echo "Copied resource font to shared assets: " & destFontPath
      fontFaceSegments.add(generateFontFaceCSS(resourceInfo.name, "shared/" & fontFileName))

  let fontFaceCSS = if fontFaceSegments.len > 0:
    fontFaceSegments.join("\n")
  else:
    "/* No custom fonts configured */"

  backend.sharedCSS = """
    * {
      margin: 0;
      padding: 0;
      box-sizing: border-box;
    }

    """ & fontFaceCSS & """

    body {
      font-family: """ & (if bodyFontFamily.len > 0: getFontStackFor(bodyFontFamily) else: getFontStack()) & """;
      background-color: """ & backend.backgroundColor.toCSSColor() & """;
      width: """ & $backend.windowWidth & """px;
      height: """ & $backend.windowHeight & """px;
      overflow: hidden;
      margin: 0;
      padding: 20px;
    }

    .kryon-app {
      width: 100%;
      height: 100%;
    }

    /* Navigation styling */
    .kryon-navigation {
      position: fixed;
      top: 10px;
      right: 10px;
      background: rgba(0, 0, 0, 0.8);
      padding: 10px;
      border-radius: 5px;
      color: white;
      font-size: 12px;
      z-index: 1000;
    }

    .kryon-navigation a {
      color: #3498DB;
      text-decoration: none;
      margin: 0 5px;
    }

    .kryon-navigation a:hover {
      color: #2980B9;
      text-decoration: underline;
    }

    .kryon-navigation a.active {
      color: #2ECC71;
      font-weight: bold;
    }
  """

  # Add generated CSS from the root element
  backend.sharedCSS.add(backend.generateCSS(root))

  let sharedCSSFile = sharedDir / "styles.css"
  writeFile(sharedCSSFile, backend.sharedCSS)
  echo "Generated shared CSS: " & sharedCSSFile

  # Generate shared JavaScript
  backend.sharedJS = """
// Kryon Navigation System
document.addEventListener('DOMContentLoaded', function() {
  // Highlight active navigation link
  const currentPage = window.location.pathname.split('/').pop() || 'index.html';
  const navLinks = document.querySelectorAll('.kryon-navigation a');

  navLinks.forEach(link => {
    const href = link.getAttribute('href');
    if (href === currentPage || (currentPage === '' && href === 'index.html')) {
      link.classList.add('active');
    }
  });

  // Add smooth transitions
  document.querySelectorAll('.kryon-button, a.kryon-button').forEach(element => {
    element.style.transition = 'opacity 0.2s';
    element.addEventListener('mouseenter', function() {
      this.style.opacity = '0.8';
    });
    element.addEventListener('mouseleave', function() {
      this.style.opacity = '1';
    });
  });

  // Show current page in navigation
  console.log('Kryon Navigation: Loaded page - ' + currentPage);
});
"""

  let sharedJSFile = sharedDir / "app.js"
  writeFile(sharedJSFile, backend.sharedJS)
  echo "Generated shared JavaScript: " & sharedJSFile

proc generateRouteHTMLFile*(backend: var HTMLBackend, route: RouteInfo): string =
  ## Generate HTML file for a specific route
  var html = """<!DOCTYPE html>
<!--
  Generated by Kryon Web Backend
  Multi-page application with client-side routing
-->
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <meta name="generator" content="Kryon Framework">
    <title>""" & route.title & """</title>
    <link rel="stylesheet" href="shared/styles.css">
</head>
<body>
    <div class="kryon-app">
"""

  # Add navigation widget
  html.add("""
    <div class="kryon-navigation">
      Navigation:
      <a href="index.html">Home</a>
      <a href="about.html">About</a>
      <a href="settings.html">Settings</a>
    </div>
""")

  # Add generated HTML for this route
  html.add(backend.generateHTML(route.element))

  html.add("""    </div>
    <script src="shared/app.js"></script>
</body>
</html>""")

  result = html

proc generateMultiFileHTML*(backend: var HTMLBackend, root: Element, outputDir: string) =
  ## Generate multiple HTML files for a routing application
  createDir(outputDir)

  # Generate shared assets
  backend.generateSharedAssets(root, outputDir)

  # Generate HTML file for each route
  for route in backend.routes:
    let htmlContent = backend.generateRouteHTMLFile(route)
    let htmlFile = outputDir / route.fileName
    writeFile(htmlFile, htmlContent)
    echo "Generated route: " & route.name & " -> " & htmlFile

  # Generate 404 page
  let notFoundHTML = """<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Page Not Found - """ & backend.windowTitle & """</title>
    <link rel="stylesheet" href="shared/styles.css">
</head>
<body>
    <div class="kryon-app">
        <div class="kryon-center">
            <div style="text-align: center;">
                <h1 style="color: #E74C3C; margin-bottom: 20px;">404 - Page Not Found</h1>
                <p style="color: #7F8C8D; margin-bottom: 20px;">The page you're looking for doesn't exist.</p>
                <a href="index.html" class="kryon-button" style="background-color: #3498DB;">← Back to Home</a>
            </div>
        </div>
    </div>
    <script src="shared/app.js"></script>
</body>
</html>"""

  let notFoundFile = outputDir / "404.html"
  writeFile(notFoundFile, notFoundHTML)
  echo "Generated 404 page: " & notFoundFile

# ============================================================================
# Single-File Generation (Legacy)
# ============================================================================

proc generateHTMLFile*(backend: var HTMLBackend, root: Element): string =
  ## Generate complete HTML file (for non-routing apps)
  var html = """<!DOCTYPE html>
<!--
  Generated by Kryon Web Backend
  https://github.com/kryon-framework

  This is a static HTML/CSS/JS export of a Kryon application.
  The framework is similar to Flutter but targets multiple backends.
-->
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <meta name="generator" content="Kryon Framework">
    <title>""" & backend.windowTitle & """</title>
    <style>
"""

  # Add base styles
  let bodyFontFamily = resolvePreferredFont(root)
  let fontInfo = getDefaultFontInfo()

  var fontFaceSegments: seq[string] = @[]
  var seenFonts = initHashSet[string]()

  if fontInfo.path.len > 0:
    fontFaceSegments.add(generateFontFaceCSS(fontInfo.name, fontInfo.path))
    seenFonts.incl(fontInfo.name)

  for fontName in getRegisteredFontNames():
    if fontName in seenFonts:
      continue
    let resourceInfo = getResourceFontInfo(fontName)
    if resourceInfo.path.len > 0:
      fontFaceSegments.add(generateFontFaceCSS(resourceInfo.name, resourceInfo.path))
      seenFonts.incl(resourceInfo.name)

  let fontFaceCSS = if fontFaceSegments.len > 0:
    fontFaceSegments.join("\n")
  else:
    "/* No custom fonts configured */"

  html.add("""
    * {
      margin: 0;
      padding: 0;
      box-sizing: border-box;
    }

    """ & fontFaceCSS & """

    body {
      font-family: """ & (if bodyFontFamily.len > 0: getFontStackFor(bodyFontFamily) else: getFontStack()) & """;
      background-color: """ & backend.backgroundColor.toCSSColor() & """;
      width: """ & $backend.windowWidth & """px;
      height: """ & $backend.windowHeight & """px;
      overflow: hidden;
      margin: 0;
      padding: 20px;
    }

    .kryon-app {
      width: 100%;
      height: 100%;
    }
""")

  # Add generated CSS
  html.add(backend.generateCSS(root))

  html.add("""    </style>
</head>
<body>
    <div class="kryon-app">
""")

  # Reset element counter so HTML generation uses same IDs as CSS
  backend.elementCounter = 0

  # Add generated HTML
  html.add(backend.generateHTML(root))

  html.add("""    </div>
    <script>
""")

  # Add state management system
  html.add(backend.generateStateManagementJS(root))

  # Add generated JavaScript (event handlers, canvas, etc.)
  html.add(backend.jsContent)

  html.add("""    </script>
</body>
</html>""")

  result = html

proc generateFiles*(backend: var HTMLBackend, root: Element, outputDir: string) =
  ## Generate HTML files - detects routing and generates appropriate files
  createDir(outputDir)

  # Check if this is a routing application
  let hasRoutes = backend.extractRoutes(root)

  if hasRoutes:
    echo "Detected routing application - generating multi-file HTML..."
    backend.generateMultiFileHTML(root, outputDir)
  else:
    echo "Generating single-file HTML..."
    # Generate single HTML file
    let htmlContent = backend.generateHTMLFile(root)
    let htmlFile = outputDir / "index.html"
    writeFile(htmlFile, htmlContent)
    echo "Generated HTML: " & htmlFile

# ============================================================================
# Main Entry Point
# ============================================================================

proc run*(backend: var HTMLBackend, root: Element, outputDir: string = ".") =
  ## Generate HTML files from the app root element
  echo "Generating HTML web app..."
  echo "Title: " & backend.windowTitle
  echo "Size: " & $backend.windowWidth & "x" & $backend.windowHeight

  backend.generateFiles(root, outputDir)

  # Show summary of generated files
  echo ""
  echo "Generated files:"
  var totalSize = 0
  for file in walkFiles(outputDir / "*.*"):
    let size = getFileSize(file)
    totalSize += size
    let sizeStr = if size < 1024: $size & " B"
                  elif size < 1024*1024: $(size div 1024) & " KB"
                  else: $(size div (1024*1024)) & " MB"
    echo "  " & file.extractFilename & " (" & sizeStr & ")"

  if dirExists(outputDir / "shared"):
    for file in walkFiles(outputDir / "shared" / "*.*"):
      let size = getFileSize(file)
      totalSize += size
      let sizeStr = if size < 1024: $size & " B"
                    elif size < 1024*1024: $(size div 1024) & " KB"
                    else: $(size div (1024*1024)) & " MB"
      echo "  shared/" & file.extractFilename & " (" & sizeStr & ")"

  echo ""
  echo "Total size: " & $(totalSize div 1024) & " KB"
  echo "Output directory: " & outputDir
  echo ""
  echo "HTML web app generated successfully!"
  echo "Open " & outputDir / "index.html" & " in your browser to view the app."
