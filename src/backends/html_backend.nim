## HTML Backend for Kryon
##
## This backend generates HTML/CSS/JS files instead of a native executable.
## DSL elements are converted to web standards with CSS handling layout and styling.

import ../kryon/core
import options, tables, strutils, os, re

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
  let cssClass = "kryon-" & ($elem.kind).toLowerAscii().sanitizeCSSClass()

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

  of ekHeader, ekBody, ekConditional, ekForLoop, ekText, ekButton, ekInput, ekCheckbox, ekDropdown, ekGrid, ekImage, ekScrollView, ekTabGroup, ekTabBar, ekTab, ekTabContent, ekTabPanel:
    # These elements either don't need special CSS layout or are handled elsewhere
    discard

  # Text styling
  if elem.kind == ekText:
    let fontSize = elem.getProp("fontSize").get(val(16)).getInt()
    css.add("  font-size: " & $fontSize & "px;\n")
    css.add("  font-family: system-ui, -apple-system, sans-serif;\n")

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
    css.add("  font-family: system-ui, -apple-system, sans-serif;\n")
    css.add("  user-select: none;\n")
    css.add("  text-decoration: none;\n")  # For navigation links
    css.add("  color: inherit;\n")           # For navigation links

    let fontSize = elem.getProp("fontSize").get(val(16)).getInt()
    css.add("  font-size: " & $fontSize & "px;\n")

    # Button padding
    if not elem.getProp("width").isSome:
      css.add("  padding: 8px 16px;\n")

  # Input styling
  if elem.kind == ekInput:
    css.add("  display: inline-block;\n")
    css.add("  border: 1px solid #ccc;\n")
    css.add("  padding: 8px;\n")
    css.add("  font-family: system-ui, -apple-system, sans-serif;\n")
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
    css.add("  font-family: system-ui, -apple-system, sans-serif;\n")
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
  for child in elem.children:
    css.add(backend.generateCSS(child, elementId))

  result = css

# ============================================================================
# JavaScript Event Handler Generation
# ============================================================================

proc generateJavaScriptForElement*(backend: var HTMLBackend, elem: Element, elementId: string) =
  ## Generate JavaScript event handlers for an element
  if elem.eventHandlers.len > 0:
    for event, handler in elem.eventHandlers:
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
        if event == "onChange":
          backend.jsContent.add("// Input change handler for element " & elementId & "\n")
          backend.jsContent.add("document.querySelector('[data-element-id=\"" & elementId & "\"]').addEventListener('input', function(e) {\n")
          backend.jsContent.add("  // Kryon event handler: " & event & "\n")
          backend.jsContent.add("  console.log('Input changed! New value:', e.target.value);\n")
          backend.jsContent.add("  console.log('Event: " & event & "');\n")
          backend.jsContent.add("  // Original Nim procedure would be called here with the new value\n")
          backend.jsContent.add("  // For now, logging the change to console\n")
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
  let cssClass = "kryon-" & ($elem.kind).toLowerAscii().sanitizeCSSClass()

  var html = ""

  case elem.kind:
  of ekHeader, ekBody, ekConditional, ekForLoop:
    # These are structural elements - don't render themselves
    for child in elem.children:
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

  # Generate shared CSS
  backend.sharedCSS = """
    * {
      margin: 0;
      padding: 0;
      box-sizing: border-box;
    }

    body {
      font-family: system-ui, -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
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
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
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
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>""" & backend.windowTitle & """</title>
    <style>
"""

  # Add base styles
  html.add("""
    * {
      margin: 0;
      padding: 0;
      box-sizing: border-box;
    }

    body {
      font-family: system-ui, -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
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

  # Add generated HTML
  html.add(backend.generateHTML(root))

  html.add("""    </div>
    <script>
""")

  # Add generated JavaScript
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

  echo "HTML web app generated successfully!"
  echo "Open " & outputDir / "index.html" & " in your browser to view the app."