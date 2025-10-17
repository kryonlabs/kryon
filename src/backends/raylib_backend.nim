## Raylib Backend for Kryon
##
## This backend renders Kryon UI elements using Raylib for native desktop applications.

import ../kryon/core
import ../kryon/fonts
import ../kryon/layout/layoutEngine
import ../kryon/layout/zindexSort
import ../kryon/state/backendState
import ../kryon/components/button
import ../kryon/components/text
import ../kryon/components/input
import ../kryon/components/checkbox
import ../kryon/components/dropdown
import ../kryon/components/container
import ../kryon/components/containers
import ../kryon/components/tabs
import ../kryon/rendering/renderingContext
import options, tables, math, os
import raylib_ffi

# ============================================================================
# Backend Type
# ============================================================================

type
  RaylibBackend* = object
    windowWidth*: int
    windowHeight*: int
    windowTitle*: string
    backgroundColor*: Color
    running*: bool
    rootElement*: Element
    font*: RFont           # Custom font for consistent rendering
    state*: BackendState   # Shared state for all interactive elements

proc newRaylibBackend*(width, height: int, title: string): RaylibBackend =
  ## Create a new Raylib backend
  result = RaylibBackend(
    windowWidth: width,
    windowHeight: height,
    windowTitle: title,
    backgroundColor: rgba(30, 30, 30, 255),
    running: false,
    state: BackendState(
      focusedInput: nil,
      cursorBlink: 0.0,
      inputValues: initTable[Element, string](),
      inputScrollOffsets: initTable[Element, float](),
      backspaceHoldTimer: 0.0,
      backspaceRepeatDelay: 0.5,
      backspaceRepeatRate: 0.05,
      focusedDropdown: nil,
      checkboxStates: initTable[Element, bool]()
    )
  )

proc newRaylibBackendFromApp*(app: Element): RaylibBackend =
  ## Create backend from app element (extracts config from Header and Body)
  ## App structure: Body -> [Header, Body]

  var width = 800
  var height = 600
  var title = "Kryon App"
  var bgColor: Option[Value] = none(Value)

  # Look for Header and Body children in app
  for child in app.children:
    if child.kind == ekHeader:
      # Extract window config from Header (support both full names and aliases)
      # Try windowWidth first, then width
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

  result = RaylibBackend(
    windowWidth: width,
    windowHeight: height,
    windowTitle: title,
    backgroundColor: if bgColor.isSome: bgColor.get.getColor() else: rgba(30, 30, 30, 255),
    running: false,
    state: BackendState(
      focusedInput: nil,
      cursorBlink: 0.0,
      inputValues: initTable[Element, string](),
      inputScrollOffsets: initTable[Element, float](),
      backspaceHoldTimer: 0.0,
      backspaceRepeatDelay: 0.5,
      backspaceRepeatRate: 0.05,
      focusedDropdown: nil,
      checkboxStates: initTable[Element, bool]()
    )
  )

var
  layoutFont: RFont
  layoutFontInitialized = false

proc setLayoutFont*(font: RFont) =
  ## Cache the active font for layout measurements.
  layoutFont = font
  layoutFontInitialized = true

proc measureLayoutTextWidth(text: string, fontSize: int): float =
  ## Measure text width for layout calculations using the cached font.
  if text.len == 0:
    return 0.0
  if layoutFontInitialized:
    return MeasureTextEx(layoutFont, text.cstring, fontSize.cfloat, 0.0).x
  else:
    return MeasureText(text.cstring, fontSize.cint).float

proc measureTextWidth*(backend: RaylibBackend, text: string, fontSize: int): float =
  ## Measure text width using the backend's active font.
  if text.len == 0:
    return 0.0
  return MeasureTextEx(backend.font, text.cstring, fontSize.cfloat, 0.0).x

# ============================================================================
# TextMeasurer Interface Implementation
# ============================================================================

proc measureText*(backend: RaylibBackend, text: string, fontSize: int): tuple[width: float, height: float] =
  ## Implement TextMeasurer interface for layout engine
  if text.len == 0:
    return (width: 0.0, height: fontSize.float)
  let size = MeasureTextEx(backend.font, text.cstring, fontSize.cfloat, 0.0)
  return (width: size.x, height: size.y)

# ============================================================================
# Color Conversion
# ============================================================================

proc toRaylibColor*(c: Color): RColor {.inline.} =
  ## Convert Kryon Color to Raylib RColor
  rcolor(c.r, c.g, c.b, c.a)

# ============================================================================
# Renderer Interface Implementation
# ============================================================================

proc drawRectangle*(backend: var RaylibBackend, x, y, width, height: float, color: Color) =
  ## Draw a filled rectangle
  DrawRectangleRec(rrect(x, y, width, height), color.toRaylibColor())

proc drawRectangleBorder*(backend: var RaylibBackend, x, y, width, height, borderWidth: float, color: Color) =
  ## Draw a rectangle border
  DrawRectangleLinesEx(rrect(x, y, width, height), borderWidth, color.toRaylibColor())

proc drawText*(backend: var RaylibBackend, text: string, x, y: float, fontSize: int, color: Color) =
  ## Draw text using the backend's font
  DrawTextEx(backend.font, text.cstring, rvec2(x, y), fontSize.cfloat, 0.0, color.toRaylibColor())

proc drawLine*(backend: var RaylibBackend, x1, y1, x2, y2, thickness: float, color: Color) =
  ## Draw a line
  if thickness <= 1.0:
    # Use rectangle for thin lines since DrawLine might not be available
    DrawRectangle(x1.cint, y1.cint, (x2-x1).abs.cint + 1, (y2-y1).abs.cint + 1, color.toRaylibColor())
  else:
    # For thicker lines, draw a rectangle rotated
    # Simple implementation: just draw a thick horizontal or vertical line
    if abs(x2 - x1) > abs(y2 - y1):
      # More horizontal
      DrawRectangle(min(x1, x2).cint, (y1 - thickness/2).cint, abs(x2-x1).cint, thickness.cint, color.toRaylibColor())
    else:
      # More vertical
      DrawRectangle((x1 - thickness/2).cint, min(y1, y2).cint, thickness.cint, abs(y2-y1).cint, color.toRaylibColor())

proc beginClipping*(backend: var RaylibBackend, x, y, width, height: float) =
  ## Begin clipping region (scissor mode)
  BeginScissorMode(x.cint, y.cint, width.cint, height.cint)

proc endClipping*(backend: var RaylibBackend) =
  ## End clipping region
  EndScissorMode()

# ============================================================================
# Layout Engine Wrapper
# ============================================================================

# Text measurer for layout - uses cached font or active backend font
type LayoutTextMeasurer = object
  backend: ptr RaylibBackend

proc measureText(m: LayoutTextMeasurer, text: string, fontSize: int): tuple[width: float, height: float] =
  ## Measure text for layout using cached font or backend font
  if text.len == 0:
    return (width: 0.0, height: fontSize.float)

  # Use cached layout font if available, otherwise use backend font
  if layoutFontInitialized:
    let size = MeasureTextEx(layoutFont, text.cstring, fontSize.cfloat, 0.0)
    return (width: size.x, height: size.y)
  elif m.backend != nil:
    let size = MeasureTextEx(m.backend.font, text.cstring, fontSize.cfloat, 0.0)
    return (width: size.x, height: size.y)
  else:
    # Fallback to basic measurement
    return (width: MeasureText(text.cstring, fontSize.cint).float, height: fontSize.float)

proc measureTextWidth(m: LayoutTextMeasurer, text: string, fontSize: int): float =
  ## Measure text width for layout - optimization when height not needed
  let (w, _) = measureText(m, text, fontSize)
  return w

proc calculateLayout*(backend: var RaylibBackend, elem: Element, x, y, parentWidth, parentHeight: float) =
  ## Wrapper that calls the shared layout engine
  var measurer = LayoutTextMeasurer(backend: addr backend)
  layoutEngine.calculateLayout(measurer, elem, x, y, parentWidth, parentHeight)

proc calculateLayout*(elem: Element, x, y, parentWidth, parentHeight: float) =
  ## Convenience wrapper for layout calculation without backend reference
  ## Uses the cached layout font
  var measurer = LayoutTextMeasurer(backend: nil)
  layoutEngine.calculateLayout(measurer, elem, x, y, parentWidth, parentHeight)

# ============================================================================
# Rendering with Real Raylib
# ============================================================================

proc renderElement*(backend: var RaylibBackend, elem: Element, inheritedColor: Option[Color] = none(Color)) =
  ## Render an element using Raylib
  ## inheritedColor: color to use for text if not explicitly set

  case elem.kind:
  of ekHeader:
    # Header is metadata only - don't render it
    discard

  of ekResources, ekFont:
    # Resource declarations are non-visual
    discard

  of ekConditional:
    # Conditional elements don't render themselves, only their active branch
    if elem.condition != nil:
      let conditionResult = elem.condition()
      let activeBranch = if conditionResult: elem.trueBranch else: elem.falseBranch


      # Render the active branch if it exists
      if activeBranch != nil:
        backend.renderElement(activeBranch, inheritedColor)

  of ekForLoop:
    # For loop elements don't render themselves, only their generated children
    # The actual rendering and layout is handled in the layout calculation
    # So this just needs to render the children
    
    for child in elem.children:
      backend.renderElement(child, inheritedColor)

  of ekBody:
    # Extract body data and render children with inherited color
    let data = extractBodyData(elem, inheritedColor)
    # Render children with inherited color (sorted by z-index)
    for child in data.sortedChildren:
      backend.renderElement(child, data.colorToInherit)

  of ekContainer:
    # Extract container data and render it
    let data = extractContainerData(elem, inheritedColor)

    # Draw background
    if data.hasBackground:
      backend.drawRectangle(data.x, data.y, data.width, data.height, data.backgroundColor)

    # Draw border
    if data.hasBorder:
      backend.drawRectangleBorder(data.x, data.y, data.width, data.height, data.borderWidth, data.borderColor)

    # Render children (sorted by z-index)
    for child in data.sortedChildren:
      backend.renderElement(child, inheritedColor)

  of ekText:
    # Extract text data and render it
    let data = extractTextData(elem, inheritedColor)
    backend.drawText(data.text, data.x, data.y, data.fontSize, data.color)

  of ekButton:
    # Extract button data and render it
    let data = extractButtonData(elem, inheritedColor)

    # Apply disabled styling if button is disabled
    let bgColor = if data.disabled: getDisabledColor(data.backgroundColor) else: data.backgroundColor
    let textColor = if data.disabled: getDisabledColor(data.textColor) else: data.textColor
    let borderColor = if data.disabled: getDisabledColor(data.borderColor) else: data.borderColor

    # Draw background
    backend.drawRectangle(data.x, data.y, data.width, data.height, bgColor)
    # Draw border
    backend.drawRectangleBorder(data.x, data.y, data.width, data.height, data.borderWidth, borderColor)
    # Draw centered text
    let textMeasurements = backend.measureText(data.text, data.fontSize)
    let textX = data.x + (data.width - textMeasurements.width) / 2.0
    let textY = data.y + (data.height - textMeasurements.height) / 2.0
    backend.drawText(data.text, textX, textY, data.fontSize, textColor)

  of ekInput:
    # Extract input data
    let data = extractInputData(elem, backend.state, inheritedColor)

    # Draw background
    backend.drawRectangle(data.x, data.y, data.width, data.height, data.backgroundColor)

    # Draw border
    backend.drawRectangleBorder(data.x, data.y, data.width, data.height, data.borderWidth, data.borderColor)

    # Calculate cursor position and scroll offset (backend-specific, requires text measurement)
    let cursorTextPos = if data.value.len > 0 and data.isFocused:
      measureTextWidth(backend, data.value, data.fontSize)
    else:
      0.0

    var scrollOffset = data.scrollOffset

    # Adjust scroll offset to keep cursor visible
    if data.isFocused:
      let totalTextWidth = if data.value.len > 0:
        measureTextWidth(backend, data.value, data.fontSize)
      else:
        0.0

      # If cursor is approaching right edge, scroll to keep it visible
      if cursorTextPos - scrollOffset > data.visibleWidth - 20.0:
        scrollOffset = cursorTextPos - data.visibleWidth + 20.0

      # If cursor is approaching left edge, scroll left
      if cursorTextPos < scrollOffset + 20.0:
        scrollOffset = max(0.0, cursorTextPos - 20.0)

      # Don't scroll past the beginning
      scrollOffset = max(0.0, scrollOffset)

      # If text is shorter than visible area, reset scroll to 0
      if totalTextWidth <= data.visibleWidth:
        scrollOffset = 0.0
      else:
        let maxScrollOffset = max(0.0, totalTextWidth - data.visibleWidth)
        scrollOffset = min(scrollOffset, maxScrollOffset)

      # Store updated scroll offset
      backend.state.inputScrollOffsets[elem] = scrollOffset

    # Set up clipping for text
    backend.beginClipping(data.x + data.padding, data.y + 2.0, data.visibleWidth, data.height - 4.0)

    # Draw text with scroll offset
    if data.displayText.len > 0:
      let textX = data.x + data.padding - scrollOffset
      let textY = data.y + (data.height - data.fontSize.float) / 2.0
      backend.drawText(data.displayText, textX, textY, data.fontSize, data.displayColor)

    backend.endClipping()

    # Draw cursor if focused and visible
    if data.showCursor:
      let cursorX = data.x + data.padding + cursorTextPos - scrollOffset
      let cursorY = data.cursorY

      # Only draw cursor if within visible area
      if cursorX >= data.x + data.padding and cursorX <= data.x + data.width - data.padding:
        DrawRectangle(cursorX.cint, cursorY.cint, 1, data.fontSize.cint - 2, data.textColor.toRaylibColor())

  of ekDropdown:
    # Extract dropdown button data
    let data = extractDropdownButtonData(elem, backend.state, inheritedColor)

    # Draw background
    backend.drawRectangle(data.x, data.y, data.width, data.height, data.backgroundColor)

    # Draw border
    backend.drawRectangleBorder(data.x, data.y, data.width, data.height, data.borderWidth, data.borderColor)

    # Draw arrow
    backend.drawText(data.arrowChar, data.arrowX, data.arrowY, data.fontSize, data.textColor)

    # Draw selected text or placeholder with clipping
    backend.beginClipping(data.textX, data.textY, data.maxTextWidth, data.fontSize.float)
    backend.drawText(data.displayText, data.textX, data.textY, data.fontSize, data.displayColor)
    backend.endClipping()

    # NOTE: Dropdown menus are rendered separately in renderDropdownMenus() to ensure they appear on top

  of ekCheckbox:
    # Extract checkbox data
    let data = extractCheckboxData(elem, backend.state, inheritedColor)

    # Draw checkbox background
    backend.drawRectangle(data.checkboxX, data.checkboxY, data.checkboxSize, data.checkboxSize, data.backgroundColor)

    # Draw checkbox border
    if data.borderWidth > 0:
      backend.drawRectangleBorder(data.checkboxX, data.checkboxY, data.checkboxSize, data.checkboxSize, data.borderWidth, data.borderColor)

    # Draw checkmark if checked (using pre-calculated segments)
    if data.checked:
      for segment in data.checkmarkSegments1:
        DrawRectangle(segment.x.cint, segment.y.cint, segment.thickness.cint, segment.thickness.cint, data.checkColor.toRaylibColor())
      for segment in data.checkmarkSegments2:
        DrawRectangle(segment.x.cint, segment.y.cint, segment.thickness.cint, segment.thickness.cint, data.checkColor.toRaylibColor())

    # Draw label if provided
    if data.hasLabel:
      backend.drawText(data.label, data.labelX, data.labelY, data.fontSize, data.labelColor)

  of ekColumn:
    # Extract column data and render children
    let data = extractColumnData(elem, inheritedColor)
    for child in data.sortedChildren:
      backend.renderElement(child, inheritedColor)

  of ekRow:
    # Extract row data and render children
    let data = extractRowData(elem, inheritedColor)
    for child in data.sortedChildren:
      backend.renderElement(child, inheritedColor)

  of ekCenter:
    # Extract center data and render children
    let data = extractCenterData(elem, inheritedColor)
    for child in data.sortedChildren:
      backend.renderElement(child, inheritedColor)

  of ekTabGroup:
    # Extract tab group data and render children
    let data = extractTabGroupData(elem, inheritedColor)
    # Draw background if present
    if data.hasBackground:
      backend.drawRectangle(data.x, data.y, data.width, data.height, data.backgroundColor)
    # Render children (TabBar and TabContent) sorted by z-index
    for child in data.sortedChildren:
      if child.kind != ekConditional and child.kind != ekForLoop:
        backend.renderElement(child, inheritedColor)

  of ekTabBar:
    # Extract tab bar data and render it
    let data = extractTabBarData(elem, inheritedColor)
    # Draw background if present
    if data.hasBackground:
      backend.drawRectangle(data.x, data.y, data.width, data.height, data.backgroundColor)
    # Draw bottom border if present
    if data.hasBorder:
      # Draw a horizontal line for the bottom border
      backend.drawRectangle(data.x, data.borderY, data.width, data.borderWidth, data.borderColor)
    # Render children (Tab elements) sorted by z-index
    for child in data.sortedChildren:
      backend.renderElement(child, inheritedColor)

  of ekTab:
    # Extract tab data and render it
    let data = extractTabData(elem, inheritedColor)

    # Draw tab background
    backend.drawRectangle(data.x, data.y, data.width, data.height, data.backgroundColor)

    # Draw tab border
    backend.drawRectangleBorder(data.x, data.y, data.width, data.height, data.borderWidth, data.borderColor)

    # Draw bottom border (active or inactive) as a horizontal line
    backend.drawRectangle(data.x, data.bottomBorderY, data.width, data.bottomBorderWidth, data.bottomBorderColor)

    # Center text in tab
    if data.label.len > 0:
      let textMeasurements = backend.measureText(data.label, data.fontSize)
      let textX = data.x + (data.width - textMeasurements.width) / 2.0
      let textY = data.y + (data.height - textMeasurements.height) / 2.0
      backend.drawText(data.label, textX, textY, data.fontSize, data.labelColor)

  of ekTabContent:
    # Extract tab content data and render it
    let data = extractTabContentData(elem, inheritedColor)

    # Draw background if present
    if data.hasBackground:
      backend.drawRectangle(data.x, data.y, data.width, data.height, data.backgroundColor)

    # Only render the active TabPanel
    if data.hasActivePanel:
      backend.renderElement(data.activePanel, inheritedColor)

  of ekTabPanel:
    # Extract tab panel data and render it
    let data = extractTabPanelData(elem, inheritedColor)

    # Draw background if present
    if data.hasBackground:
      backend.drawRectangle(data.x, data.y, data.width, data.height, data.backgroundColor)

    # Render children sorted by z-index
    for child in data.sortedChildren:
      backend.renderElement(child, inheritedColor)

  else:
    # Unsupported element - skip
    discard

  # Render children for other elements (like Container) sorted by z-index
  if elem.kind != ekColumn and elem.kind != ekRow and elem.kind != ekCenter and elem.kind != ekBody and
     elem.kind != ekTabGroup and elem.kind != ekTabBar and elem.kind != ekTabContent and elem.kind != ekTabPanel:
    let sortedChildren = sortChildrenByZIndex(elem.children)
    for child in sortedChildren:
      backend.renderElement(child, inheritedColor)

proc renderDropdownMenus*(backend: var RaylibBackend, elem: Element) =
  ## Render all open dropdown menus on top of everything else
  ## This is called AFTER renderElement to ensure dropdowns appear above all other content

  case elem.kind:
  of ekDropdown:
    # Extract dropdown menu data and render if open
    let data = extractDropdownMenuData(elem, backend.state, none(Color))
    if not data.isOpen:
      return

    # Draw dropdown menu background
    backend.drawRectangle(data.x, data.y, data.width, data.height, data.backgroundColor)
    backend.drawRectangleBorder(data.x, data.y, data.width, data.height, data.borderWidth, data.borderColor)

    # Draw visible options
    for opt in data.options:
      # Highlight hovered option
      if opt.isHovered:
        backend.drawRectangle(opt.x, opt.y, opt.width, opt.height, data.hoveredHighlightColor)

      # Highlight selected option
      if opt.isSelected:
        backend.drawRectangle(opt.x, opt.y, opt.width, opt.height, data.selectedHighlightColor)

      # Draw option text with clipping
      backend.beginClipping(opt.textX, opt.textY, opt.maxTextWidth, data.fontSize.float)
      backend.drawText(opt.text, opt.textX, opt.textY, data.fontSize, data.textColor)
      backend.endClipping()

  of ekConditional:
    # Only check the active branch
    if elem.condition != nil:
      let conditionResult = elem.condition()
      let activeBranch = if conditionResult: elem.trueBranch else: elem.falseBranch

      if activeBranch != nil:
        backend.renderDropdownMenus(activeBranch)

  else:
    # Recursively check all children for open dropdowns
    for child in elem.children:
      backend.renderDropdownMenus(child)

# ============================================================================
# Input Handling
# ============================================================================

proc closeOtherDropdowns*(backend: var RaylibBackend, keepOpen: Element) =
  ## Close all dropdowns except the specified one
  proc closeDropdownsRecursive(elem: Element) =
    case elem.kind:
    of ekDropdown:
      if elem != keepOpen:
        elem.dropdownIsOpen = false
        elem.dropdownHoveredIndex = -1
    else:
      for child in elem.children:
        closeDropdownsRecursive(child)

  closeDropdownsRecursive(backend.rootElement)

proc checkHoverCursor*(elem: Element): bool =
  ## Check if mouse is hovering over any interactive element that needs pointer cursor
  ## Returns true if pointer cursor should be shown

  case elem.kind:
  of ekConditional:
    # Only check hover cursor for the active branch
    if elem.condition != nil:
      let conditionResult = elem.condition()
      let activeBranch = if conditionResult: elem.trueBranch else: elem.falseBranch

      # Check hover cursor for the active branch if it exists
      if activeBranch != nil:
        if checkHoverCursor(activeBranch):
          return true

  of ekButton:
    # Check if button is disabled - if so, don't show pointer cursor
    if isDisabled(elem):
      return false
    let mousePos = GetMousePosition()
    let rect = rrect(elem.x, elem.y, elem.width, elem.height)
    if CheckCollisionPointRec(mousePos, rect):
      return true

  of ekTab:
    let mousePos = GetMousePosition()
    let rect = rrect(elem.x, elem.y, elem.width, elem.height)
    if CheckCollisionPointRec(mousePos, rect):
      return true

  of ekDropdown:
    let mousePos = GetMousePosition()
    let mainRect = rrect(elem.x, elem.y, elem.width, elem.height)

    # Check if hovering over main dropdown button
    if CheckCollisionPointRec(mousePos, mainRect):
      return true

    # Check if hovering over dropdown options (when open)
    if elem.dropdownIsOpen and elem.dropdownOptions.len > 0:
      let fontSize = elem.getProp("fontSize").get(val(16)).getInt()
      let itemHeight = fontSize.float + 10.0  # Match rendering code
      let dropdownHeight = min(elem.dropdownOptions.len.float * itemHeight, 200.0)
      let dropdownRect = rrect(elem.x, elem.y + elem.height, elem.width, dropdownHeight)
      if CheckCollisionPointRec(mousePos, dropdownRect):
        return true

  of ekCheckbox:
    let mousePos = GetMousePosition()

    # Calculate checkbox clickable area (including label)
    let fontSize = elem.getProp("fontSize").get(val(16)).getInt()
    let checkboxSize = min(elem.height, fontSize.float + 8.0)
    let checkboxRect = rrect(elem.x, elem.y + (elem.height - checkboxSize) / 2.0, checkboxSize, checkboxSize)

    # Check if hovering over checkbox or label
    let label = elem.getProp("label").get(val("")).getString()
    var hoverArea = checkboxRect

    if label.len > 0:
      # Extend hover area to include the entire element bounds (checkbox + label)
      hoverArea.x = elem.x
      hoverArea.y = elem.y
      hoverArea.width = elem.width
      hoverArea.height = elem.height

    if CheckCollisionPointRec(mousePos, hoverArea):
      return true

  else:
    discard

  # Check children recursively
  for child in elem.children:
    if checkHoverCursor(child):
      return true

  return false

proc handleInput*(backend: var RaylibBackend, elem: Element) =
  ## Handle mouse input for interactive elements

  case elem.kind:
  of ekHeader:
    # Header is metadata only - no input handling
    discard

  of ekConditional:
    # Conditional elements only handle input for their active branch
    if elem.condition != nil:
      let conditionResult = elem.condition()
      let activeBranch = if conditionResult: elem.trueBranch else: elem.falseBranch

      # Handle input for the active branch if it exists
      if activeBranch != nil:
        backend.handleInput(activeBranch)

  of ekBody:
    # Body is a wrapper - handle children's input (reverse z-index order, highest first)
    let sortedChildren = sortChildrenByZIndexReverse(elem.children)
    for child in sortedChildren:
      backend.handleInput(child)

  of ekButton:
    # Check if button is disabled - if so, don't handle clicks
    if not isDisabled(elem):
      if IsMouseButtonPressed(MOUSE_BUTTON_LEFT):
        let mousePos = GetMousePosition()
        let rect = rrect(elem.x, elem.y, elem.width, elem.height)

        if CheckCollisionPointRec(mousePos, rect):
          # Button was clicked - trigger onClick handler
          if elem.eventHandlers.hasKey("onClick"):
            elem.eventHandlers["onClick"]()

  of ekInput:
    if IsMouseButtonPressed(MOUSE_BUTTON_LEFT):
      let mousePos = GetMousePosition()
      let rect = rrect(elem.x, elem.y, elem.width, elem.height)

      if CheckCollisionPointRec(mousePos, rect):
        # Input was clicked - focus it
        backend.state.focusedInput = elem
        backend.state.cursorBlink = 0.0  # Reset cursor blink

        # Initialize input value if not already present
        if not backend.state.inputValues.hasKey(elem):
          let initialValue = elem.getProp("value").get(val("")).getString()
          backend.state.inputValues[elem] = initialValue
      else:
        # Clicked outside - unfocus if this was focused
        if backend.state.focusedInput == elem:
          backend.state.focusedInput = nil

  of ekCheckbox:
    if IsMouseButtonPressed(MOUSE_BUTTON_LEFT):
      let mousePos = GetMousePosition()

      # Calculate checkbox clickable area (including label)
      let fontSize = elem.getProp("fontSize").get(val(16)).getInt()
      let checkboxSize = min(elem.height, fontSize.float + 8.0)
      let checkboxRect = rrect(elem.x, elem.y + (elem.height - checkboxSize) / 2.0, checkboxSize, checkboxSize)

      # Check if click is on checkbox or label
      let label = elem.getProp("label").get(val("")).getString()
      var clickArea = checkboxRect

      if label.len > 0:
        # Extend clickable area to include the full element bounds
        clickArea.x = elem.x
        clickArea.y = elem.y
        clickArea.width = elem.width
        clickArea.height = elem.height

      if CheckCollisionPointRec(mousePos, clickArea):
        # Checkbox was clicked - toggle state
        var currentState = backend.state.checkboxStates.getOrDefault(elem, false)
        currentState = not currentState
        backend.state.checkboxStates[elem] = currentState

        # Trigger onClick handler if present
        if elem.eventHandlers.hasKey("onClick"):
          elem.eventHandlers["onClick"]()

        # Trigger onChange handler if present (pass the new state as data)
        if elem.eventHandlers.hasKey("onChange"):
          let handler = elem.eventHandlers["onChange"]
          handler($currentState)  # Pass boolean state as string

  of ekTabGroup, ekTabBar, ekTabContent:
    # Tab containers - explicitly handle structural children's input (reverse z-index order)
    let sortedChildren = sortChildrenByZIndexReverse(elem.children)
    for child in sortedChildren:
      if child.kind == ekConditional:
        # Conditionals handle their own input (see ekConditional case)
        backend.handleInput(child)
      elif child.kind == ekForLoop:
        # For loops contain dynamic children - recurse into them
        for grandchild in child.children:
          backend.handleInput(grandchild)
      else:
        backend.handleInput(child)

  of ekTab:
    let mousePos = GetMousePosition()
    let rect = rrect(elem.x, elem.y, elem.width, elem.height)
    let isPressed = IsMouseButtonPressed(MOUSE_BUTTON_LEFT)
    let isDown = IsMouseButtonDown(MOUSE_BUTTON_LEFT)

    if isPressed:
      if CheckCollisionPointRec(mousePos, rect):
        # Tab was clicked - find and update parent TabGroup's selectedIndex
        var parent = elem.parent
        var tabGroup: Element = nil

        # Find the TabGroup (not just TabBar)
        while parent != nil:
          if parent.kind == ekTabGroup:
            tabGroup = parent
            break
          parent = parent.parent

        if tabGroup != nil:
          # Update the selected index
          let oldSelectedIndex = tabGroup.tabSelectedIndex
          tabGroup.tabSelectedIndex = elem.tabIndex

          # Enhanced reactivity: invalidate reactive values that depend on tab selection
          # This triggers automatic updates for any element that depends on tab state
          if oldSelectedIndex != tabGroup.tabSelectedIndex:
            # Log the tab change for debugging
            echo "TAB CHANGED: from index ", oldSelectedIndex, " to index ", tabGroup.tabSelectedIndex

            # Trigger the onSelectedIndexChanged event for two-way binding
            if tabGroup.eventHandlers.hasKey("onSelectedIndexChanged"):
              echo "TRIGGERING: onSelectedIndexChanged event for TabGroup"
              tabGroup.eventHandlers["onSelectedIndexChanged"]()

            # Use the improved reactive system to invalidate related values
            # This automatically handles cross-element dependencies
            invalidateRelatedValues("tabSelectedIndex")

            # Also mark TabGroup and related elements as dirty for immediate visual updates
            markDirty(tabGroup)
            for child in tabGroup.children:
              markDirty(child)
              # Also mark TabContent's children (TabPanels) for complete re-render
              if child.kind == ekTabContent:
                echo "Marking TabContent children as dirty for tab change"
                for panel in child.children:
                  markDirty(panel)
                  # Recursively mark all descendants as dirty to ensure calendar updates
                  markAllDescendantsDirty(panel)

        # Trigger onClick handler if present
        if elem.eventHandlers.hasKey("onClick"):
          elem.eventHandlers["onClick"]()

  of ekDropdown:
    if IsMouseButtonPressed(MOUSE_BUTTON_LEFT):
      let mousePos = GetMousePosition()
      let mainRect = rrect(elem.x, elem.y, elem.width, elem.height)

      if CheckCollisionPointRec(mousePos, mainRect):
        # Main dropdown button was clicked
        if elem.dropdownIsOpen:
          # Close the dropdown
          elem.dropdownIsOpen = false
          elem.dropdownHoveredIndex = -1
          backend.state.focusedDropdown = nil
        else:
          # Open the dropdown
          elem.dropdownIsOpen = true
          elem.dropdownHoveredIndex = elem.dropdownSelectedIndex  # Start with current selection
          backend.state.focusedDropdown = elem

          # Close any other dropdowns
          backend.closeOtherDropdowns(elem)

        # Early return to prevent click-through to underlying elements
        return

      elif elem.dropdownIsOpen:
        # Check if clicked on dropdown options
        if elem.dropdownOptions.len > 0:
          let fontSize = elem.getProp("fontSize").get(val(16)).getInt()
          let itemHeight = fontSize.float + 10.0  # Match rendering code
          let dropdownHeight = min(elem.dropdownOptions.len.float * itemHeight, 200.0)
          let dropdownRect = rrect(elem.x, elem.y + elem.height, elem.width, dropdownHeight)

          if CheckCollisionPointRec(mousePos, dropdownRect):
            # Calculate which option was clicked
            let relativeY = mousePos.y - dropdownRect.y
            let clickedIndex = int(relativeY / itemHeight)

            if clickedIndex >= 0 and clickedIndex < elem.dropdownOptions.len:
              # Select the clicked option
              elem.dropdownSelectedIndex = clickedIndex
              elem.dropdownIsOpen = false
              elem.dropdownHoveredIndex = -1
              backend.state.focusedDropdown = nil

              # Trigger onChange handler if present
              if elem.eventHandlers.hasKey("onChange"):
                let handler = elem.eventHandlers["onChange"]
                handler(elem.dropdownOptions[clickedIndex])

              # For onSelectionChange, we pass the selected value as data (index can be parsed if needed)
              if elem.eventHandlers.hasKey("onSelectionChange"):
                let handler = elem.eventHandlers["onSelectionChange"]
                handler(elem.dropdownOptions[clickedIndex])

              # Early return to prevent click-through to underlying elements
              return
          else:
            # Clicked outside dropdown - close it
            elem.dropdownIsOpen = false
            elem.dropdownHoveredIndex = -1
            backend.state.focusedDropdown = nil
        else:
          # No options - close dropdown
          elem.dropdownIsOpen = false
          elem.dropdownHoveredIndex = -1
          backend.state.focusedDropdown = nil
      else:
        # Clicked outside while closed - just ensure it's not focused
        if backend.state.focusedDropdown == elem:
          backend.state.focusedDropdown = nil

    # Handle hover state for dropdown options
    if elem.dropdownIsOpen and elem.dropdownOptions.len > 0:
      let mousePos = GetMousePosition()
      let fontSize = elem.getProp("fontSize").get(val(16)).getInt()
      let itemHeight = fontSize.float + 10.0  # Match rendering code
      let dropdownHeight = min(elem.dropdownOptions.len.float * itemHeight, 200.0)
      let dropdownRect = rrect(elem.x, elem.y + elem.height, elem.width, dropdownHeight)

      if CheckCollisionPointRec(mousePos, dropdownRect):
        let relativeY = mousePos.y - dropdownRect.y
        let hoveredIndex = int(relativeY / itemHeight)

        if hoveredIndex >= 0 and hoveredIndex < elem.dropdownOptions.len:
          elem.dropdownHoveredIndex = hoveredIndex
        else:
          elem.dropdownHoveredIndex = -1
      else:
        elem.dropdownHoveredIndex = -1

  of ekColumn, ekRow, ekCenter, ekContainer:
    # Layout containers - only handle children's input (reverse z-index order, highest first)
    let sortedChildren = sortChildrenByZIndexReverse(elem.children)
    for child in sortedChildren:
      backend.handleInput(child)

  else:
    # Other element types - check children (reverse z-index order, highest first)
    let sortedChildren = sortChildrenByZIndexReverse(elem.children)
    for child in sortedChildren:
      backend.handleInput(child)

proc handleKeyboardInput*(backend: var RaylibBackend, root: Element) =
  ## Handle keyboard input for focused input and dropdown elements

  # Handle checkbox keyboard input
  # For simplicity, we'll handle basic checkbox toggling with a focused element state
  # Note: This is a basic implementation. A full implementation would need checkbox focus management
  let mousePos = GetMousePosition()
  proc findCheckboxUnderMouse(elem: Element): Element =
    if elem.kind == ekCheckbox:
      let fontSize = elem.getProp("fontSize").get(val(16)).getInt()
      let checkboxSize = min(elem.height, fontSize.float + 8.0)
      let checkboxRect = rrect(elem.x, elem.y + (elem.height - checkboxSize) / 2.0, checkboxSize, checkboxSize)

      let label = elem.getProp("label").get(val("")).getString()
      var clickArea = checkboxRect

      if label.len > 0:
        clickArea.x = elem.x
        clickArea.y = elem.y
        clickArea.width = elem.width
        clickArea.height = elem.height

      if CheckCollisionPointRec(mousePos, clickArea):
        return elem

    # Check children
    for child in elem.children:
      let found = findCheckboxUnderMouse(child)
      if found != nil:
        return found

    return nil

  let hoveredCheckbox = findCheckboxUnderMouse(root)
  if hoveredCheckbox != nil:
    if IsKeyPressed(KEY_ENTER):
      # Toggle checkbox state
      var currentState = backend.state.checkboxStates.getOrDefault(hoveredCheckbox, false)
      currentState = not currentState
      backend.state.checkboxStates[hoveredCheckbox] = currentState

      # Trigger onClick handler if present
      if hoveredCheckbox.eventHandlers.hasKey("onClick"):
        hoveredCheckbox.eventHandlers["onClick"]()

      # Trigger onChange handler if present (pass the new state as data)
      if hoveredCheckbox.eventHandlers.hasKey("onChange"):
        let handler = hoveredCheckbox.eventHandlers["onChange"]
        handler($currentState)  # Pass boolean state as string

  # Handle dropdown keyboard input
  if backend.state.focusedDropdown != nil:
    let dropdown = backend.state.focusedDropdown

    if IsKeyPressed(KEY_ESCAPE):
      # Close dropdown on ESC
      dropdown.dropdownIsOpen = false
      dropdown.dropdownHoveredIndex = -1
      backend.state.focusedDropdown = nil

    elif dropdown.dropdownIsOpen:
      if dropdown.dropdownOptions.len > 0:
        var handled = false

        if IsKeyPressed(KEY_UP):
          # Move selection up
          dropdown.dropdownHoveredIndex = if dropdown.dropdownHoveredIndex <= 0:
            dropdown.dropdownOptions.len - 1  # Wrap to bottom
          else:
            dropdown.dropdownHoveredIndex - 1
          handled = true

        elif IsKeyPressed(KEY_DOWN):
          # Move selection down
          dropdown.dropdownHoveredIndex = if dropdown.dropdownHoveredIndex >= dropdown.dropdownOptions.len - 1:
            0  # Wrap to top
          else:
            dropdown.dropdownHoveredIndex + 1
          handled = true

        elif IsKeyPressed(KEY_ENTER):
          # Select current hover item
          if dropdown.dropdownHoveredIndex >= 0 and dropdown.dropdownHoveredIndex < dropdown.dropdownOptions.len:
            dropdown.dropdownSelectedIndex = dropdown.dropdownHoveredIndex
            dropdown.dropdownIsOpen = false
            dropdown.dropdownHoveredIndex = -1
            backend.state.focusedDropdown = nil

            # Trigger onChange handler if present
            if dropdown.eventHandlers.hasKey("onChange"):
              let handler = dropdown.eventHandlers["onChange"]
              handler(dropdown.dropdownOptions[dropdown.dropdownSelectedIndex])

            # Trigger onSelectionChange handler if present
            if dropdown.eventHandlers.hasKey("onSelectionChange"):
              let handler = dropdown.eventHandlers["onSelectionChange"]
              handler(dropdown.dropdownOptions[dropdown.dropdownSelectedIndex])
          handled = true

        elif IsKeyPressed(KEY_TAB):
          # Close dropdown on TAB (don't select)
          dropdown.dropdownIsOpen = false
          dropdown.dropdownHoveredIndex = -1
          backend.state.focusedDropdown = nil
          handled = true

      # If no dropdown-specific key was pressed, don't handle further
      return

  # Handle input field keyboard input
  if backend.state.focusedInput == nil:
    backend.state.backspaceHoldTimer = 0.0  # Reset timer when no focus
    return

  # Get current value
  var currentValue = backend.state.inputValues.getOrDefault(backend.state.focusedInput, "")
  var textChanged = false

  # Handle character input
  while true:
    let char = GetCharPressed()
    if char == 0:
      break
    # Add printable characters
    if char >= 32 and char < 127:
      currentValue.add(char.chr)
      textChanged = true
      backend.state.backspaceHoldTimer = 0.0  # Reset backspace timer when typing

  # Handle backspace with repeat logic
  let backspacePressed = IsKeyDown(KEY_BACKSPACE)
  if backspacePressed and currentValue.len > 0:
    if IsKeyPressed(KEY_BACKSPACE):
      # First press - delete one character immediately
      currentValue.setLen(currentValue.len - 1)
      textChanged = true
      backend.state.backspaceHoldTimer = 0.0  # Start the hold timer
    else:
      # Key is being held down
      backend.state.backspaceHoldTimer += 1.0 / 60.0  # Increment by frame time (~16ms at 60fps)

      # Check if we should delete more characters
      if backend.state.backspaceHoldTimer >= backend.state.backspaceRepeatDelay:
        # Calculate how many characters to delete based on hold time
        let holdBeyondDelay = backend.state.backspaceHoldTimer - backend.state.backspaceRepeatDelay
        let charsToDelete = min(int(holdBeyondDelay / backend.state.backspaceRepeatRate), currentValue.len)

        if charsToDelete > 0:
          currentValue.setLen(currentValue.len - charsToDelete)
          textChanged = true
          # Adjust timer to maintain repeat rate
          backend.state.backspaceHoldTimer = backend.state.backspaceRepeatDelay +
                                      (charsToDelete.float * backend.state.backspaceRepeatRate)
  else:
    # Backspace not pressed - reset timer
    backend.state.backspaceHoldTimer = 0.0

  # Handle other special keys
  if IsKeyPressed(KEY_ENTER):
    # Trigger onSubmit handler if present
    if backend.state.focusedInput.eventHandlers.hasKey("onSubmit"):
      backend.state.focusedInput.eventHandlers["onSubmit"]()

  # Update stored value if changed
  if textChanged:
    backend.state.inputValues[backend.state.focusedInput] = currentValue

  # Trigger onChange handler if present
  if textChanged and backend.state.focusedInput.eventHandlers.hasKey("onChange"):
    let handler = backend.state.focusedInput.eventHandlers["onChange"]
    handler(currentValue)

  # Trigger onValueChange handler for two-way binding
  if textChanged and backend.state.focusedInput.eventHandlers.hasKey("onValueChange"):
    let handler = backend.state.focusedInput.eventHandlers["onValueChange"]
    handler(currentValue)

# ============================================================================
# Main Loop
# ============================================================================

proc run*(backend: var RaylibBackend, root: Element) =
  ## Run the application with the given root element

  backend.rootElement = root
  backend.running = true

  # Initialize Raylib window
  InitWindow(backend.windowWidth.cint, backend.windowHeight.cint, backend.windowTitle.cstring)
  SetTargetFPS(60)

  # Load font based on fontFamily property or default font
  var fontPath = ""
  var fontName = resolvePreferredFont(root)

  if fontName.len > 0:
    fontPath = findFontByName(fontName)

  # If no specific font found, try default font configuration
  if fontPath.len == 0:
    let fontInfo = getDefaultFontInfo()
    if fontInfo.path.len > 0:
      fontPath = fontInfo.path
      fontName = fontInfo.name
      echo "Using default font: " & fontName & " at " & fontPath

  # Load the font
  if fontPath.len > 0:
    echo "Loading font: " & fontPath
    if fileExists(fontPath):
      echo "Font file exists on disk"
      backend.font = LoadFont(fontPath.cstring)
      if backend.font.baseSize > 0:  # Check if font loaded successfully
        echo "Successfully loaded font: " & fontName & " (baseSize: " & $backend.font.baseSize & ")"
      else:
        echo "Warning: Failed to load font (baseSize=0), using Raylib default font"
        backend.font = GetFontDefault()
    else:
      echo "Error: Font file does not exist: " & fontPath
      backend.font = GetFontDefault()
  else:
    echo "No font found, using Raylib default font"
    backend.font = GetFontDefault()

  setLayoutFont(backend.font)

  # Calculate initial layout
  calculateLayout(root, 0, 0, backend.windowWidth.float, backend.windowHeight.float)

  # Main game loop
  while not WindowShouldClose():
    # Update cursor blink timer
    backend.state.cursorBlink += 1.0 / 60.0  # Assuming 60 FPS
    if backend.state.cursorBlink >= 1.0:
      backend.state.cursorBlink = 0.0

    # Update mouse cursor based on hover state
    if checkHoverCursor(root):
      SetMouseCursor(MOUSE_CURSOR_POINTING_HAND)
    else:
      SetMouseCursor(MOUSE_CURSOR_DEFAULT)

    # Handle mouse input
    backend.handleInput(root)

    # Handle keyboard input
    backend.handleKeyboardInput(root)

    # Only recalculate layout when there are dirty elements (intelligent updates)
    if hasDirtyElements():
      calculateLayout(root, 0, 0, backend.windowWidth.float, backend.windowHeight.float)

    # Render
    BeginDrawing()

    # Find the actual Body element (child of root) and check for background color
    var actualBody: Element = nil
    for child in root.children:
      if child.kind == ekBody:
        actualBody = child
        break

    # Check for both "background" and "backgroundColor" properties
    var bodyBg: Option[Value] = none(Value)
    if actualBody != nil:
      bodyBg = actualBody.getProp("backgroundColor")
      if bodyBg.isNone:
        bodyBg = actualBody.getProp("background")

    if bodyBg.isSome:
      let bgColor = bodyBg.get.getColor()
      ClearBackground(bgColor.toRaylibColor())
    else:
      ClearBackground(backend.backgroundColor.toRaylibColor())

    # Render all elements with normal z-index sorting
    backend.renderElement(root)

    # Render dropdown menus on top of everything else
    backend.renderDropdownMenus(root)

    EndDrawing()

  # Cleanup
  UnloadFont(backend.font)
  layoutFontInitialized = false
  CloseWindow()
  backend.running = false
