## Raylib Backend for Kryon
##
## This backend renders Kryon UI elements using Raylib for native desktop applications.

import ../../kryon/core
import ../../kryon/fonts
import ../../kryon/layout/layoutEngine
import ../../kryon/layout/zindexSort
import ../../kryon/state/backendState
import ../../kryon/components/button
import ../../kryon/components/text
import ../../kryon/components/input
import ../../kryon/components/checkbox
import ../../kryon/components/dropdown
import ../../kryon/components/container
import ../../kryon/components/containers
import ../../kryon/components/tabs
import ../../kryon/rendering/renderingContext
import options, tables, math, os, times, sets
import ../windowing/raylib/raylib_ffi

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

proc getDragContentOverridePanel*(backend: RaylibBackend, elem: Element): Element =
  ## Check if this TabContent should use drag content override
  ## Returns the override panel if in drag mode, otherwise nil
  if backend.state.isLiveReordering and backend.state.dragTabContentPanel != nil and elem.kind == ekTabContent:
    # During drag, always show the dragged tab's original content
    return backend.state.dragTabContentPanel
  return nil

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
    # Skip the dragged element - it will be rendered at mouse position in drag effects
    for child in data.sortedChildren:
      if child != backend.state.draggedElement:
        backend.renderElement(child, inheritedColor)

  of ekTab:
    # Extract tab data and render it
    let data = extractTabData(elem, inheritedColor)

    # Draw tab background
    backend.drawRectangle(data.x, data.y, data.width, data.height, data.backgroundColor)

    # Draw tab border
    backend.drawRectangleBorder(data.x, data.y, data.width, data.height, data.borderWidth, data.borderColor)

    # Draw bottom border (active or inactive) as a horizontal line
    # Skip if this is the dragged element to prevent double blue lines during drag
    if elem != backend.state.draggedElement:
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

    # Check for drag content override first
    let dragOverridePanel = backend.getDragContentOverridePanel(elem)
    if dragOverridePanel != nil:
      # During drag, always render the dragged tab's original content
      echo "[DRAG CONTENT OVERRIDE] Rendering dragged tab's original content"
      backend.renderElement(dragOverridePanel, inheritedColor)
    elif data.hasActivePanel:
      # Normal mode: render the active TabPanel
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
# Drag-and-Drop Helpers
# ============================================================================

proc hasDraggableBehavior*(elem: Element): bool =
  ## Check if element has a Draggable behavior attached
  if elem.withBehaviors.len == 0:
    return false
  for behavior in elem.withBehaviors:
    if behavior.kind == ekDraggable:
      return true
  return false

proc hasDropTargetBehavior*(elem: Element): bool =
  ## Check if element has a DropTarget behavior attached
  if elem.withBehaviors.len == 0:
    return false
  for behavior in elem.withBehaviors:
    if behavior.kind == ekDropTarget:
      return true
  return false

proc getDraggableBehavior*(elem: Element): Element =
  ## Get the Draggable behavior element from an element
  if elem.withBehaviors.len > 0:
    for behavior in elem.withBehaviors:
      if behavior.kind == ekDraggable:
        return behavior
  return nil

proc getDropTargetBehavior*(elem: Element): Element =
  ## Get the DropTarget behavior element from an element
  if elem.withBehaviors.len > 0:
    for behavior in elem.withBehaviors:
      if behavior.kind == ekDropTarget:
        return behavior
  return nil

proc isMouseOverElement*(elem: Element, mousePos: RVector2): bool =
  ## Check if mouse position is over the element's bounds
  let rect = rrect(elem.x, elem.y, elem.width, elem.height)
  return CheckCollisionPointRec(mousePos, rect)

proc findDraggableElementAtPoint*(elem: Element, mousePos: RVector2): Element =
  ## Recursively find a draggable element at the given mouse position
  ## Returns the topmost draggable element at that position

  # Check children first (reverse z-order, highest first)
  let sortedChildren = sortChildrenByZIndexReverse(elem.children)
  for child in sortedChildren:
    let found = findDraggableElementAtPoint(child, mousePos)
    if found != nil:
      return found

  # Then check this element
  if hasDraggableBehavior(elem) and isMouseOverElement(elem, mousePos):
    return elem

  return nil

proc findDropTargetAtPoint*(elem: Element, mousePos: RVector2, itemType: string): Element =
  ## Recursively find a drop target element at the given mouse position
  ## that accepts the given itemType
  ## Returns the topmost matching drop target

  # Check children first (reverse z-order, highest first)
  let sortedChildren = sortChildrenByZIndexReverse(elem.children)
  for child in sortedChildren:
    let found = findDropTargetAtPoint(child, mousePos, itemType)
    if found != nil:
      return found

  # Then check this element
  if hasDropTargetBehavior(elem) and isMouseOverElement(elem, mousePos):
    # Check if itemType matches
    let dropBehavior = getDropTargetBehavior(elem)
    let targetItemType = dropBehavior.getProp("itemType").get(val("")).getString()
    if targetItemType.len == 0 or targetItemType == itemType:
      return elem

  return nil

proc renderDropTargetHighlights*(backend: var RaylibBackend, elem: Element) =
  ## Recursively render drop target highlights for a single element and its children
  ## Skip TabBar elements to avoid distracting green border during tab reordering
  if hasDropTargetBehavior(elem) and elem.dropTargetState.isHovered and elem.kind != ekTabBar:
    # Draw a semi-transparent green overlay to indicate valid drop target
    let highlightColor = rgba(82, 196, 26, 60)  # Green with transparency
    backend.drawRectangle(elem.x, elem.y, elem.width, elem.height, highlightColor)

    # Draw a bright border to make it more visible
    let borderColor = rgba(82, 196, 26, 200)
    backend.drawRectangleBorder(elem.x, elem.y, elem.width, elem.height, 3.0, borderColor)

  # Check children recursively
  for child in elem.children:
    renderDropTargetHighlights(backend, child)

proc renderDraggedElement*(backend: var RaylibBackend, elem: Element, offsetX, offsetY: float) =
  ## Render a dragged element at the specified offset with transparency
  # For containers, render background and border with transparency
  if elem.kind == ekContainer:
    let bgColor = elem.getProp("backgroundColor").get(val("#2a2a2a")).getColor()
    let transparentBg = rgba(bgColor.r, bgColor.g, bgColor.b, 180)
    backend.drawRectangle(elem.x + offsetX, elem.y + offsetY, elem.width, elem.height, transparentBg)

    let borderWidth = elem.getProp("borderWidth").get(val(2)).getFloat()
    if borderWidth > 0:
      let borderColor = elem.getProp("borderColor").get(val("#4a4a4a")).getColor()
      let transparentBorder = rgba(borderColor.r, borderColor.g, borderColor.b, 180)
      backend.drawRectangleBorder(elem.x + offsetX, elem.y + offsetY, elem.width, elem.height, borderWidth, transparentBorder)

  # For tabs, render with transparency
  elif elem.kind == ekTab:
    # Extract tab styling
    let bgColor = elem.getProp("backgroundColor").get(val("#3d3d3d")).getColor()
    let transparentBg = rgba(bgColor.r, bgColor.g, bgColor.b, 180)
    backend.drawRectangle(elem.x + offsetX, elem.y + offsetY, elem.width, elem.height, transparentBg)

    # Draw tab border with transparency
    let borderWidth = 1.0
    let borderColor = rgba(74, 74, 74, 180)  # Semi-transparent border
    backend.drawRectangleBorder(elem.x + offsetX, elem.y + offsetY, elem.width, elem.height, borderWidth, borderColor)

    # Draw tab text (title)
    let title = elem.getProp("title").get(val("Tab")).getString()
    let textColor = elem.getProp("textColor").get(val("#cccccc")).getColor()
    let transparentText = rgba(textColor.r, textColor.g, textColor.b, 180)
    let fontSize = 16
    let textMeasurements = backend.measureText(title, fontSize)
    let textX = elem.x + offsetX + (elem.width - textMeasurements.width) / 2.0
    let textY = elem.y + offsetY + (elem.height - textMeasurements.height) / 2.0
    backend.drawText(title, textX, textY, fontSize, transparentText)

  # Render children recursively
  for child in elem.children:
    renderDraggedElement(backend, child, offsetX, offsetY)

  # Render text elements with transparency
  if elem.kind == ekText:
    let textColor = elem.getProp("color").get(val("#ffffff")).getColor()
    let transparentText = rgba(textColor.r, textColor.g, textColor.b, 180)
    let fontSize = elem.getProp("fontSize").get(val(16)).getInt()
    let text = elem.getProp("text").get(val("")).getString()
    backend.drawText(text, elem.x + offsetX, elem.y + offsetY, fontSize, transparentText)

proc renderDragAndDropEffects*(backend: var RaylibBackend, elem: Element) =
  ## Render drag-and-drop visual effects (highlights and dragged element)
  ## This is called AFTER all normal rendering to ensure effects appear on top

  # Render drop target highlights first
  renderDropTargetHighlights(backend, elem)

  # Render the dragged element last (on top of everything)
  if backend.state.draggedElement != nil:
    let draggedElem = backend.state.draggedElement
    let mousePos = GetMousePosition()

    # Calculate dragged element position (follow mouse with offset)
    var dragX = mousePos.x - backend.state.dragOffsetX
    var dragY = mousePos.y - backend.state.dragOffsetY

    # Apply axis locking to rendered position
    let dragBehavior = getDraggableBehavior(draggedElem)
    if dragBehavior != nil:
      let lockAxis = dragBehavior.getProp("lockAxis").get(val("none")).getString()
      if lockAxis == "x":
        # Lock to horizontal movement - keep original Y position
        dragY = draggedElem.y
      elif lockAxis == "y":
        # Lock to vertical movement - keep original X position
        dragX = draggedElem.x

    # Don't draw ghost - other tabs will slide into position creating a gap
    # Calculate offset from original position to mouse position
    let offsetX = dragX - draggedElem.x
    let offsetY = dragY - draggedElem.y

    # Render the dragged element with offset
    renderDraggedElement(backend, draggedElem, offsetX, offsetY)

    # Draw insert indicator under the dragged tab during drag (for tab reordering)
    if backend.state.draggedElement != nil and backend.state.draggedElement.kind == ekTab:
      let draggedTab = backend.state.draggedElement
      # Position the blue underline under the dragged tab as it moves
      let indicatorColor = rgba(74, 144, 226, 255)  # Bright blue
      let indicatorWidth = 3.0

      # Calculate current dragged tab position (follows mouse movement)
      var dragX = GetMousePosition().x - backend.state.dragOffsetX
      var dragY = draggedTab.y

      # Apply axis locking if specified
      let dragBehavior = getDraggableBehavior(draggedTab)
      if dragBehavior != nil:
        let lockAxis = dragBehavior.getProp("lockAxis").get(val("none")).getString()
        if lockAxis == "x":
          dragY = draggedTab.y  # Keep original Y position
        elif lockAxis == "y":
          dragX = draggedTab.x  # Keep original X position

      # Draw a horizontal line under the dragged tab
      backend.drawRectangle(dragX - indicatorWidth / 2.0, draggedTab.y + draggedTab.height - 3.0, draggedTab.width + indicatorWidth, indicatorWidth, indicatorColor)

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

  # ============================================================================
  # Drag-and-Drop Input Handling (global, before element-specific handling)
  # ============================================================================

  let mousePos = GetMousePosition()

  # CRITICAL: Clear drag content override on next frame if flag is set
  # This ensures proper synchronization before clearing the override
  if backend.state.shouldClearDragOverride:
    backend.state.dragTabContentPanel = nil
    backend.state.dragOriginalTabIndex = -1
    backend.state.dragOriginalContentMapping.clear()
    backend.state.shouldClearDragOverride = false
    echo "[DRAG OVERRIDE CLEARED] Cleared drag content override on next frame"

  # Handle drag start (mouse down on draggable element)
  if IsMouseButtonPressed(MOUSE_BUTTON_LEFT) and backend.state.draggedElement == nil:
    let draggableElem = findDraggableElementAtPoint(elem, mousePos)
    if draggableElem != nil:
      # Start dragging
      backend.state.draggedElement = draggableElem
      backend.state.dragSource = draggableElem
      backend.state.dragOffsetX = mousePos.x - draggableElem.x
      backend.state.dragOffsetY = mousePos.y - draggableElem.y
      backend.state.dragStartTime = epochTime()

      # Store drag data from the Draggable behavior
      let dragBehavior = getDraggableBehavior(draggableElem)
      if dragBehavior != nil:
        let dragDataValue = dragBehavior.getProp("data").get(val(""))
        draggableElem.dragState.dragData = dragDataValue
        draggableElem.dragState.isDragging = true
        draggableElem.dragState.dragStartX = mousePos.x
        draggableElem.dragState.dragStartY = mousePos.y

      # Check if parent is a reorderable container (has DropTarget behavior)
      if draggableElem.parent != nil and hasDropTargetBehavior(draggableElem.parent):
        let container = draggableElem.parent
        # Enable live reordering mode
        backend.state.isLiveReordering = true
        backend.state.reorderableContainer = container
        backend.state.originalChildOrder = @[]

        # Copy children order (save for potential restore if needed)
        for child in container.children:
          backend.state.originalChildOrder.add(child)

        # Find dragged child index
        backend.state.draggedChildIndex = -1
        for i, child in backend.state.originalChildOrder:
          if child == draggableElem:
            backend.state.draggedChildIndex = i
            break

        echo "[LIVE REORDER] Enabled for container kind: ", container.kind, ", dragged child index: ", backend.state.draggedChildIndex

      # CRITICAL FIX: Immediately select the dragged tab to ensure its content is shown
      if draggableElem.kind == ekTab:
        var parent = draggableElem.parent
        var tabGroup: Element = nil

        # Find the TabGroup (not just TabBar)
        while parent != nil:
          if parent.kind == ekTabGroup:
            tabGroup = parent
            break
          parent = parent.parent

        if tabGroup != nil:
          # CRITICAL: Store the original tab index and content for drag content locking
          backend.state.dragOriginalTabIndex = draggableElem.tabIndex

          # CRITICAL: Capture original content mapping from ALL tabs to their panels
          backend.state.dragOriginalContentMapping.clear()

          # Find the TabBar to get all tabs and their original indices
          var tabBar: Element = nil
          for child in tabGroup.children:
            if child.kind == ekTabBar:
              tabBar = child
              break

          # Find the TabContent to get all panels
          var tabContent: Element = nil
          for child in tabGroup.children:
            if child.kind == ekTabContent:
              tabContent = child
              break

          if tabBar != nil and tabContent != nil:
            # Create mapping from original tab indices to their corresponding panels
            for tab in tabBar.children:
              if tab.kind == ekTab:
                let originalTabIndex = tab.tabIndex
                # Find the panel that corresponds to this tab's original content
                for panel in tabContent.children:
                  if panel.kind == ekTabPanel and panel.tabIndex == originalTabIndex:
                    backend.state.dragOriginalContentMapping[originalTabIndex] = panel
                    echo "[CONTENT MAPPING] Mapped tab ", originalTabIndex, " to panel"
                    break

            # Store the dragged tab's specific panel for quick access
            if backend.state.dragOriginalContentMapping.hasKey(backend.state.dragOriginalTabIndex):
              backend.state.dragTabContentPanel = backend.state.dragOriginalContentMapping[backend.state.dragOriginalTabIndex]
              echo "[DRAG CONTENT LOCK] Stored original content for tab ", backend.state.dragOriginalTabIndex

          # Find the current visual position of the dragged tab
          var draggedVisualIndex = -1
          for i, child in tabGroup.children:
            if child.kind == ekTabBar:
              for j, tab in child.children:
                if tab == draggableElem:
                  draggedVisualIndex = j
                  break
              break

          # Select the dragged tab immediately
          if draggedVisualIndex >= 0:
            let oldSelectedIndex = tabGroup.tabSelectedIndex
            tabGroup.tabSelectedIndex = draggedVisualIndex

            if oldSelectedIndex != draggedVisualIndex:
              echo "[TAB DRAG SELECT] Immediately selected dragged tab at index ", draggedVisualIndex, " (original index: ", backend.state.dragOriginalTabIndex, ")"

              # Trigger the onSelectedIndexChanged event
              if tabGroup.eventHandlers.hasKey("onSelectedIndexChanged"):
                tabGroup.eventHandlers["onSelectedIndexChanged"]()

              # Invalidate related reactive values
              invalidateRelatedValues("tabSelectedIndex")

              # Mark TabGroup and children as dirty for immediate visual updates
              markDirty(tabGroup)
              for child in tabGroup.children:
                markDirty(child)
                if child.kind == ekTabContent:
                  for panel in child.children:
                    markDirty(panel)
                    markAllDescendantsDirty(panel)

      echo "[DRAG START] Started dragging element at (", draggableElem.x, ", ", draggableElem.y, ")"

  # Handle drag movement
  if backend.state.draggedElement != nil and IsMouseButtonDown(MOUSE_BUTTON_LEFT):
    let draggedElem = backend.state.draggedElement

    # Update drag state (apply axis locking if specified)
    let dragBehavior = getDraggableBehavior(draggedElem)
    var currentOffsetX = mousePos.x - draggedElem.dragState.dragStartX
    var currentOffsetY = mousePos.y - draggedElem.dragState.dragStartY

    # Apply axis locking
    if dragBehavior != nil:
      let lockAxis = dragBehavior.getProp("lockAxis").get(val("none")).getString()
      if lockAxis == "x":
        # Lock to horizontal movement only
        currentOffsetY = 0.0
      elif lockAxis == "y":
        # Lock to vertical movement only
        currentOffsetX = 0.0

    draggedElem.dragState.currentOffsetX = currentOffsetX
    draggedElem.dragState.currentOffsetY = currentOffsetY

    # Find potential drop target under mouse
    if dragBehavior != nil:
      let itemType = dragBehavior.getProp("itemType").get(val("")).getString()
      let dropTarget = findDropTargetAtPoint(backend.rootElement, mousePos, itemType)

      # Update hover states
      if backend.state.potentialDropTarget != dropTarget:
        # Clear old target hover state
        if backend.state.potentialDropTarget != nil:
          backend.state.potentialDropTarget.dropTargetState.isHovered = false

        # Set new target hover state
        backend.state.potentialDropTarget = dropTarget
        if dropTarget != nil:
          dropTarget.dropTargetState.isHovered = true
          dropTarget.dropTargetState.canAcceptDrop = true
          echo "[HOVER] Hovering over drop target"

      # Calculate insert position for tab reordering
      if dropTarget != nil and dropTarget.kind == ekTabBar:
        # Calculate which tab position the mouse is over using actual tab positions
        let relativeX = mousePos.x - dropTarget.x

        # Calculate insert index based on actual tab positions and widths
        var insertIdx = 0
        var currentX = 0.0

        # Find which tab position the mouse is closest to
        for child in dropTarget.children:
          if child.kind == ekTab:
            let tabWidth = child.width
            let tabCenter = currentX + tabWidth / 2.0

            if relativeX >= tabCenter:
              insertIdx += 1
            else:
              break
            currentX += tabWidth

        # Clamp to valid range [0, tabCount]
        var tabCount = 0
        for child in dropTarget.children:
          if child.kind == ekTab:
            tabCount += 1

        insertIdx = max(0, min(tabCount, insertIdx))
        backend.state.dragInsertIndex = insertIdx
        echo "[INSERT CALC] Mouse at X=", relativeX, ", calculated insert index: ", insertIdx, " (tabCount: ", tabCount, ")"
      else:
        backend.state.dragInsertIndex = -1

      # Update container children directly for immediate visual reordering
      if backend.state.isLiveReordering and backend.state.dragInsertIndex >= 0:
        let fromIdx = backend.state.draggedChildIndex
        var toIdx = backend.state.dragInsertIndex

        # Only update if indices are different
        if fromIdx >= 0 and fromIdx != toIdx:
          let container = backend.state.reorderableContainer

          # Build new reordered visual children
          var reorderedVisualChildren: seq[Element] = @[]
          var insertedDragged = false

          for i in 0..<backend.state.originalChildOrder.len:
            # Skip the dragged element at its original position
            if i == fromIdx:
              continue

            # Insert dragged element at the target position
            if reorderedVisualChildren.len == toIdx and not insertedDragged:
              reorderedVisualChildren.add(backend.state.originalChildOrder[fromIdx])
              insertedDragged = true

            reorderedVisualChildren.add(backend.state.originalChildOrder[i])

          # If we haven't inserted yet (inserting at end), add it now
          if not insertedDragged:
            reorderedVisualChildren.add(backend.state.originalChildOrder[fromIdx])

          # Update container.children directly with reordered visual elements
          # (behaviors like DropTarget are in withBehaviors, not children, so this is safe)
          container.children = reorderedVisualChildren

          # CRITICAL FIX: During drag, ensure dragged tab's content follows the dragged tab
          # This ensures tab content always matches the dragged tab during movement
          var tabGroup = container.parent
          while tabGroup != nil and tabGroup.kind != ekTabGroup:
            tabGroup = tabGroup.parent

          if tabGroup != nil:
            # CRITICAL: During drag, the content should follow the DRAGGED tab, not the selected index
            # Set selected index to match the dragged tab's original content
            if backend.state.draggedElement != nil:
              let draggedTabIndex = backend.state.draggedElement.tabIndex
              tabGroup.tabSelectedIndex = draggedTabIndex
              echo "[DRAG CONTENT SYNC] Set selected index to dragged tab's content: ", draggedTabIndex

            for child in tabGroup.children:
              if child.kind == ekTabContent:
                # Reorder tab panels to match the new tab order during drag
                var reorderedPanels: seq[Element] = @[]
                for i in 0..<reorderedVisualChildren.len:
                  let tab = reorderedVisualChildren[i]
                  # Find corresponding panel by original tabIndex
                  for panel in child.children:
                    if panel.kind == ekTabPanel and panel.tabIndex == tab.tabIndex:
                      reorderedPanels.add(panel)
                      break

                # Update TabContent children to match tab order
                child.children = reorderedPanels

                # CRITICAL: Update panel tabIndex values to match new visual order
                # This ensures content visibility follows the dragged tab during movement
                for i, panel in reorderedPanels:
                  panel.tabIndex = i

                # CRITICAL: During drag, ensure the dragged tab's original content is visible
                # by setting the selected index to match the dragged tab's content
                if backend.state.draggedElement != nil:
                  let draggedOriginalIndex = backend.state.draggedElement.tabIndex

                  # Find which panel now contains the dragged tab's original content
                  for i, panel in reorderedPanels:
                    if panel.tabIndex == draggedOriginalIndex:
                      tabGroup.tabSelectedIndex = i
                      echo "[DRAG CONTENT SYNC] Set selected index to panel with dragged content: ", i, " (original: ", draggedOriginalIndex, ")"
                      break

                echo "[LIVE REORDER SYNC] Reordered ", reorderedPanels.len, " tab panels to follow dragged tab"
                break

          # Mark container as dirty to trigger layout recalculation
          markDirty(container)
          echo "[LIVE REORDER] Updated container.children directly, dragged from ", fromIdx, " to ", toIdx

  # Handle drag end (mouse release)
  if backend.state.draggedElement != nil and IsMouseButtonReleased(MOUSE_BUTTON_LEFT):
    let draggedElem = backend.state.draggedElement
    let dropTarget = backend.state.potentialDropTarget

    # Check if we're over a valid drop target
    if dropTarget != nil:
      let dropBehavior = getDropTargetBehavior(dropTarget)
      if dropBehavior != nil and dropBehavior.eventHandlers.hasKey("onDrop"):
        # Trigger the onDrop event with the drag data
        let dragData = draggedElem.dragState.dragData
        var dataStr = if dragData.kind == vkString:
          dragData.getString()
        elif dragData.kind == vkInt:
          $dragData.getInt()
        elif dragData.kind == vkFloat:
          $dragData.getFloat()
        else:
          ""

        # If we have a calculated insert index, append it to the data
        # Format: "originalData|insertIndex"
        if backend.state.dragInsertIndex >= 0:
          dataStr = dataStr & "|" & $backend.state.dragInsertIndex
          echo "[DROP] Drop successful! Data: ", dataStr, " (insert index: ", backend.state.dragInsertIndex, ")"
        else:
          echo "[DROP] Drop successful! Data: ", dataStr

        let handler = dropBehavior.eventHandlers["onDrop"]
        handler(dataStr)

    # Clean up live reordering state (children already reordered during drag)
    if backend.state.isLiveReordering:
      echo "[LIVE REORDER] Drag ended, children already in correct order"

      # Handle proper tab selection after reordering
      if draggedElem.kind == ekTab:
        var parent = draggedElem.parent
        var tabGroup: Element = nil

        # Find the TabGroup (not just TabBar)
        while parent != nil:
          if parent.kind == ekTabGroup:
            tabGroup = parent
            break
          parent = parent.parent

        if tabGroup != nil:
          # Find the dragged tab's new position in the visual order
          var newTabIndex = -1
          for i, child in tabGroup.children:
            if child.kind == ekTabBar:
              for j, tab in child.children:
                if tab == draggedElem:
                  newTabIndex = j
                  break
              break

          # CRITICAL FIX: After drag, ensure the DRAGGED tab remains selected with correct content
          if newTabIndex >= 0:
            let oldSelectedIndex = tabGroup.tabSelectedIndex

            # CRITICAL: Always select the dragged tab after reordering to ensure consistency
            tabGroup.tabSelectedIndex = newTabIndex

            # CRITICAL FIX: Update tabIndex values on all tabs and tab panels to match new visual order
            # This ensures tab content visibility works correctly after reordering
            for i, child in tabGroup.children:
              if child.kind == ekTabBar:
                # Update tabIndex values on all tabs to match their new visual positions
                for j, tab in child.children:
                  tab.tabIndex = j
                  echo "[TAB INDEX FIX] Updated tab[", j, "] tabIndex to ", j
                break

            # CRITICAL: Fix post-drag content synchronization using preserved mapping
            for child in tabGroup.children:
              if child.kind == ekTabContent:
                # Find the TabBar to get the new tab order
                var tabBar: Element = nil
                for sibling in tabGroup.children:
                  if sibling.kind == ekTabBar:
                    tabBar = sibling
                    break

                if tabBar != nil:
                  # CRITICAL FIX: Build the correct content mapping based on final tab positions
                  # Use the dragged element's final position to ensure correct content mapping
                  var reorderedPanels: seq[Element] = @[]

                  # CRITICAL FIX: Build the correct content mapping by tracking tab movements
                  # We need to determine where each original tab ended up after the drag
                  var originalIndexToNewPosition: Table[int, int]

                  # For each position in the final tab order, find which original tab is there
                  for newPosition, tab in tabBar.children:
                    if tab.kind == ekTab:
                      # Find which original index this tab corresponds to by checking the original order
                      var originalIndex = -1
                      for i, originalTab in backend.state.originalChildOrder:
                        if originalTab == tab:
                          originalIndex = i
                          break

                      if originalIndex >= 0:
                        originalIndexToNewPosition[originalIndex] = newPosition
                        echo "[POST-DRAG MAPPING] Original tab ", originalIndex, " is now at position ", newPosition

                  # Create the final panel order based on where each original tab's content should now appear
                  reorderedPanels = newSeq[Element](tabBar.children.len)

                  # Place each original panel in its new position based on where its tab moved
                  for originalIndex, newPosition in originalIndexToNewPosition:
                    if backend.state.dragOriginalContentMapping.hasKey(originalIndex):
                      let originalPanel = backend.state.dragOriginalContentMapping[originalIndex]
                      if newPosition >= 0 and newPosition < reorderedPanels.len:
                        reorderedPanels[newPosition] = originalPanel
                        echo "[POST-DRAG SYNC] Original panel ", originalIndex, " moved to position ", newPosition

                  # Fill any remaining gaps (shouldn't happen if mapping is complete)
                  for j in 0..<reorderedPanels.len:
                    if reorderedPanels[j] == nil:
                      # Find any panel not yet placed
                      for originalIdx, panel in backend.state.dragOriginalContentMapping:
                        var alreadyPlaced = false
                        for placedPanel in reorderedPanels:
                          if placedPanel == panel:
                            alreadyPlaced = true
                            break
                        if not alreadyPlaced:
                          reorderedPanels[j] = panel
                          echo "[POST-DRAG SYNC] Filled gap at position ", j, " with unmapped panel ", originalIdx
                          break

                  # Update TabContent children to match the exact new tab order
                  child.children = reorderedPanels

                  # CRITICAL: Update panel tabIndex values to match new visual order
                  # This ensures the tab content system works correctly after drag
                  for j, panel in reorderedPanels:
                    if panel != nil:
                      panel.tabIndex = j
                      echo "[POST-DRAG SYNC] Updated panel[", j, "] tabIndex to ", j

                  # CRITICAL: Ensure the dragged tab remains selected with correct content
                  # Find which position the dragged tab ended up in
                  var draggedFinalPosition = -1
                  for j, tab in tabBar.children:
                    if tab.kind == ekTab and tab == draggedElem:
                      draggedFinalPosition = j
                      break

                  if draggedFinalPosition >= 0:
                    tabGroup.tabSelectedIndex = draggedFinalPosition
                    echo "[POST-DRAG SYNC] Selected dragged tab at final position ", draggedFinalPosition

                break

            if oldSelectedIndex != newTabIndex:
              echo "[TAB REORDER SELECT] Updated selected index to dragged tab at position ", newTabIndex, " after reordering"

              # Trigger the onSelectedIndexChanged event
              if tabGroup.eventHandlers.hasKey("onSelectedIndexChanged"):
                tabGroup.eventHandlers["onSelectedIndexChanged"]()

              # Invalidate related reactive values
              invalidateRelatedValues("tabSelectedIndex")

              # Mark TabGroup and children as dirty for complete visual update
              markDirty(tabGroup)
              for child in tabGroup.children:
                markDirty(child)
                if child.kind == ekTabContent:
                  for panel in child.children:
                    markDirty(panel)
                    markAllDescendantsDirty(panel)

      # Reset live reordering state
      backend.state.isLiveReordering = false
      backend.state.reorderableContainer = nil
      backend.state.originalChildOrder = @[]
      backend.state.liveChildOrder = @[]
      backend.state.draggedChildIndex = -1

      # CRITICAL: DO NOT clear drag content override immediately
      # The tab content system needs time to synchronize with the new order
      # We'll clear it on the next frame after content has been properly synchronized
      backend.state.shouldClearDragOverride = true
      echo "[DRAG CLEANUP] Preserving drag content override for one more frame to ensure proper sync"

    # Clean up drag state
    draggedElem.dragState.isDragging = false
    draggedElem.dragState.currentOffsetX = 0.0
    draggedElem.dragState.currentOffsetY = 0.0

    if dropTarget != nil:
      dropTarget.dropTargetState.isHovered = false
      dropTarget.dropTargetState.canAcceptDrop = false

    backend.state.draggedElement = nil
    backend.state.dragSource = nil
    backend.state.potentialDropTarget = nil
    backend.state.dragOffsetX = 0.0
    backend.state.dragOffsetY = 0.0
    backend.state.dragInsertIndex = -1

    echo "[DRAG END] Drag ended"

  # ============================================================================
  # Element-Specific Input Handling
  # ============================================================================

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

    if isPressed and not backend.state.isLiveReordering:
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

  # Trigger onTextChange handler for real-time text updates
  if textChanged and backend.state.focusedInput.eventHandlers.hasKey("onTextChange"):
    let handler = backend.state.focusedInput.eventHandlers["onTextChange"]
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

    # Render drag-and-drop visual effects on top
    backend.renderDragAndDropEffects(root)

    EndDrawing()

  # Cleanup
  UnloadFont(backend.font)
  layoutFontInitialized = false
  CloseWindow()
  backend.running = false
