## Skia Backend for Kryon
##
## This backend renders Kryon UI elements using Skia for high-quality 2D graphics.
## Uses SDL2 for windowing and event handling, with Skia for rendering.

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
import skia_ffi
import sdl2_ffi  # For windowing only

# ============================================================================
# Backend Type
# ============================================================================

type
  SkiaBackend* = object
    window*: ptr SDL_Window
    surface*: SkSurface
    canvas*: SkCanvas
    paint*: SkPaint
    font*: SkFont
    typeface*: SkTypeface
    windowWidth*: int
    windowHeight*: int
    windowTitle*: string
    backgroundColor*: Color
    running*: bool
    rootElement*: Element
    state*: BackendState
    textInputEnabled*: bool
    keyStates*: array[512, bool]

proc newSkiaBackend*(width, height: int, title: string): SkiaBackend =
  ## Create a new Skia backend
  result = SkiaBackend(
    window: nil,
    surface: nil,
    canvas: nil,
    paint: nil,
    font: nil,
    typeface: nil,
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
    ),
    textInputEnabled: false
  )

proc newSkiaBackendFromApp*(app: Element): SkiaBackend =
  ## Create backend from app element (extracts config from Header and Body)
  var width = 800
  var height = 600
  var title = "Kryon App"
  var bgColor: Option[Value] = none(Value)

  # Look for Header and Body children in app
  for child in app.children:
    if child.kind == ekHeader:
      # Extract window config from Header (support both full names and aliases)
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

  result = SkiaBackend(
    window: nil,
    surface: nil,
    canvas: nil,
    paint: nil,
    font: nil,
    typeface: nil,
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
    ),
    textInputEnabled: false
  )

# ============================================================================
# Text Measurement for Layout
# ============================================================================

var
  layoutFont: SkFont
  layoutPaint: SkPaint
  layoutFontInitialized = false

proc setLayoutFont*(font: SkFont, paint: SkPaint) =
  ## Cache the active font for layout measurements
  layoutFont = font
  layoutPaint = paint
  layoutFontInitialized = true

proc measureLayoutTextWidth(text: string, fontSize: int): float =
  ## Measure text width for layout calculations using the cached font
  if text.len == 0:
    return 0.0
  if layoutFontInitialized:
    var bounds: SkRectC
    let width = sk_font_measure_text(layoutFont, text.cstring, text.len.csize_t, kUTF8_Encoding, addr bounds, layoutPaint)
    return width
  else:
    # Fallback approximation
    return text.len.float * fontSize.float * 0.6

proc measureTextWidth*(backend: SkiaBackend, text: string, fontSize: int): float =
  ## Measure text width using the backend's active font
  if text.len == 0:
    return 0.0
  var bounds: SkRectC
  let width = sk_font_measure_text(backend.font, text.cstring, text.len.csize_t, kUTF8_Encoding, addr bounds, backend.paint)
  return width

# ============================================================================
# TextMeasurer Interface Implementation
# ============================================================================

proc measureText*(backend: SkiaBackend, text: string, fontSize: int): tuple[width: float, height: float] =
  ## Implement TextMeasurer interface for layout engine
  if text.len == 0:
    return (width: 0.0, height: fontSize.float)
  var bounds: SkRectC
  let width = sk_font_measure_text(backend.font, text.cstring, text.len.csize_t, kUTF8_Encoding, addr bounds, backend.paint)
  return (width: width, height: fontSize.float)

# ============================================================================
# Color Conversion
# ============================================================================

proc toSkiaColor*(c: Color): SkColor {.inline.} =
  ## Convert Kryon Color to Skia SkColor
  SkColorSetARGB(c.a, c.r, c.g, c.b)

# ============================================================================
# Renderer Interface Implementation
# ============================================================================

proc drawRectangle*(backend: var SkiaBackend, x, y, width, height: float, color: Color) =
  ## Draw a filled rectangle
  sk_paint_set_style(backend.paint, kFill_Style)
  sk_paint_set_color(backend.paint, color.toSkiaColor())
  var rect = SkRectMakeXYWH(x.cfloat, y.cfloat, width.cfloat, height.cfloat)
  sk_canvas_draw_rect(backend.canvas, addr rect, backend.paint)

proc drawRectangleBorder*(backend: var SkiaBackend, x, y, width, height, borderWidth: float, color: Color) =
  ## Draw a rectangle border
  sk_paint_set_style(backend.paint, kStroke_Style)
  sk_paint_set_stroke_width(backend.paint, borderWidth.cfloat)
  sk_paint_set_color(backend.paint, color.toSkiaColor())
  var rect = SkRectMakeXYWH(x.cfloat, y.cfloat, width.cfloat, height.cfloat)
  sk_canvas_draw_rect(backend.canvas, addr rect, backend.paint)

proc drawText*(backend: var SkiaBackend, text: string, x, y: float, fontSize: int, color: Color) =
  ## Draw text using the backend's font
  sk_paint_set_style(backend.paint, kFill_Style)
  sk_paint_set_color(backend.paint, color.toSkiaColor())
  sk_canvas_draw_text(backend.canvas, text.cstring, text.len.csize_t, x.cfloat, y.cfloat + fontSize.cfloat, backend.font, backend.paint)

proc drawLine*(backend: var SkiaBackend, x1, y1, x2, y2, thickness: float, color: Color) =
  ## Draw a line
  sk_paint_set_style(backend.paint, kStroke_Style)
  sk_paint_set_stroke_width(backend.paint, thickness.cfloat)
  sk_paint_set_color(backend.paint, color.toSkiaColor())
  sk_canvas_draw_line(backend.canvas, x1.cfloat, y1.cfloat, x2.cfloat, y2.cfloat, backend.paint)

proc beginClipping*(backend: var SkiaBackend, x, y, width, height: float) =
  ## Begin clipping region (scissor mode)
  discard sk_canvas_save(backend.canvas)
  var rect = SkRectMakeXYWH(x.cfloat, y.cfloat, width.cfloat, height.cfloat)
  sk_canvas_clip_rect(backend.canvas, addr rect, true)

proc endClipping*(backend: var SkiaBackend) =
  ## End clipping region
  sk_canvas_restore(backend.canvas)

# ============================================================================
# Layout Engine Wrapper
# ============================================================================

type LayoutTextMeasurer = object
  backend: ptr SkiaBackend

proc measureText(m: LayoutTextMeasurer, text: string, fontSize: int): tuple[width: float, height: float] =
  ## Measure text for layout using cached font or backend font
  if text.len == 0:
    return (width: 0.0, height: fontSize.float)

  # Use cached layout font if available, otherwise use backend font
  if layoutFontInitialized:
    var bounds: SkRectC
    let width = sk_font_measure_text(layoutFont, text.cstring, text.len.csize_t, kUTF8_Encoding, addr bounds, layoutPaint)
    return (width: width, height: fontSize.float)
  elif m.backend != nil:
    var bounds: SkRectC
    let width = sk_font_measure_text(m.backend.font, text.cstring, text.len.csize_t, kUTF8_Encoding, addr bounds, m.backend.paint)
    return (width: width, height: fontSize.float)
  else:
    # Fallback to basic measurement
    return (width: text.len.float * fontSize.float * 0.6, height: fontSize.float)

proc measureTextWidth(m: LayoutTextMeasurer, text: string, fontSize: int): float =
  ## Measure text width for layout - optimization when height not needed
  let (w, _) = measureText(m, text, fontSize)
  return w

proc calculateLayout*(backend: var SkiaBackend, elem: Element, x, y, parentWidth, parentHeight: float) =
  ## Wrapper that calls the shared layout engine
  var measurer = LayoutTextMeasurer(backend: addr backend)
  layoutEngine.calculateLayout(measurer, elem, x, y, parentWidth, parentHeight)

proc calculateLayout*(elem: Element, x, y, parentWidth, parentHeight: float) =
  ## Convenience wrapper for layout calculation without backend reference
  var measurer = LayoutTextMeasurer(backend: nil)
  layoutEngine.calculateLayout(measurer, elem, x, y, parentWidth, parentHeight)

# ============================================================================
# Rendering with Skia
# ============================================================================

proc renderElement*(backend: var SkiaBackend, elem: Element, inheritedColor: Option[Color] = none(Color)) =
  ## Render an element using Skia
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

    # Calculate cursor position and scroll offset
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
        backend.drawRectangle(cursorX, cursorY, 1.0, data.fontSize.float - 2.0, data.textColor)

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
        backend.drawRectangle(segment.x, segment.y, segment.thickness, segment.thickness, data.checkColor)
      for segment in data.checkmarkSegments2:
        backend.drawRectangle(segment.x, segment.y, segment.thickness, segment.thickness, data.checkColor)

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

proc renderDropdownMenus*(backend: var SkiaBackend, elem: Element) =
  ## Render all open dropdown menus on top of everything else

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
# Input Handling (same as SDL2 backend)
# ============================================================================

proc closeOtherDropdowns*(backend: var SkiaBackend, keepOpen: Element) =
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

proc getMousePosition*(): tuple[x, y: float] =
  ## Get current mouse position
  var mouseX, mouseY: cint
  let _ = SDL_GetMouseState(addr mouseX, addr mouseY)
  (x: mouseX.float, y: mouseY.float)

proc checkHoverCursor*(elem: Element): bool =
  ## Check if mouse is hovering over any interactive element

  let mousePos = getMousePosition()

  case elem.kind:
  of ekConditional:
    if elem.condition != nil:
      let conditionResult = elem.condition()
      let activeBranch = if conditionResult: elem.trueBranch else: elem.falseBranch
      if activeBranch != nil:
        if checkHoverCursor(activeBranch):
          return true

  of ekButton:
    if isDisabled(elem):
      return false
    if mousePos.x >= elem.x and mousePos.x < elem.x + elem.width and
       mousePos.y >= elem.y and mousePos.y < elem.y + elem.height:
      return true

  of ekTab:
    if mousePos.x >= elem.x and mousePos.x < elem.x + elem.width and
       mousePos.y >= elem.y and mousePos.y < elem.y + elem.height:
      return true

  of ekDropdown:
    if mousePos.x >= elem.x and mousePos.x < elem.x + elem.width and
       mousePos.y >= elem.y and mousePos.y < elem.y + elem.height:
      return true

    if elem.dropdownIsOpen and elem.dropdownOptions.len > 0:
      let fontSize = elem.getProp("fontSize").get(val(16)).getInt()
      let itemHeight = fontSize.float + 10.0
      let dropdownHeight = min(elem.dropdownOptions.len.float * itemHeight, 200.0)
      if mousePos.x >= elem.x and mousePos.x < elem.x + elem.width and
         mousePos.y >= elem.y + elem.height and mousePos.y < elem.y + elem.height + dropdownHeight:
        return true

  of ekCheckbox:
    let fontSize = elem.getProp("fontSize").get(val(16)).getInt()
    let checkboxSize = min(elem.height, fontSize.float + 8.0)
    let label = elem.getProp("label").get(val("")).getString()

    var hoverWidth = checkboxSize
    if label.len > 0:
      hoverWidth = elem.width

    if mousePos.x >= elem.x and mousePos.x < elem.x + hoverWidth and
       mousePos.y >= elem.y and mousePos.y < elem.y + elem.height:
      return true

  else:
    discard

  # Check children recursively
  for child in elem.children:
    if checkHoverCursor(child):
      return true

  return false

proc handleInput*(backend: var SkiaBackend, elem: Element) =
  ## Handle mouse input for interactive elements

  case elem.kind:
  of ekHeader:
    discard

  of ekConditional:
    if elem.condition != nil:
      let conditionResult = elem.condition()
      let activeBranch = if conditionResult: elem.trueBranch else: elem.falseBranch
      if activeBranch != nil:
        backend.handleInput(activeBranch)

  of ekBody:
    let sortedChildren = sortChildrenByZIndexReverse(elem.children)
    for child in sortedChildren:
      backend.handleInput(child)

  of ekButton:
    if not isDisabled(elem):
      let mousePos = getMousePosition()
      if mousePos.x >= elem.x and mousePos.x < elem.x + elem.width and
         mousePos.y >= elem.y and mousePos.y < elem.y + elem.height:
        if elem.eventHandlers.hasKey("onClick"):
          elem.eventHandlers["onClick"]()

  of ekInput:
    let mousePos = getMousePosition()
    if mousePos.x >= elem.x and mousePos.x < elem.x + elem.width and
       mousePos.y >= elem.y and mousePos.y < elem.y + elem.height:
      backend.state.focusedInput = elem
      backend.state.cursorBlink = 0.0
      backend.textInputEnabled = true

      if not backend.state.inputValues.hasKey(elem):
        let initialValue = elem.getProp("value").get(val("")).getString()
        backend.state.inputValues[elem] = initialValue
    else:
      if backend.state.focusedInput == elem:
        backend.state.focusedInput = nil
        backend.textInputEnabled = false

  of ekCheckbox:
    let mousePos = getMousePosition()
    let fontSize = elem.getProp("fontSize").get(val(16)).getInt()
    let checkboxSize = min(elem.height, fontSize.float + 8.0)
    let label = elem.getProp("label").get(val("")).getString()

    var clickWidth = checkboxSize
    if label.len > 0:
      clickWidth = elem.width

    if mousePos.x >= elem.x and mousePos.x < elem.x + clickWidth and
       mousePos.y >= elem.y and mousePos.y < elem.y + elem.height:
      var currentState = backend.state.checkboxStates.getOrDefault(elem, false)
      currentState = not currentState
      backend.state.checkboxStates[elem] = currentState

      if elem.eventHandlers.hasKey("onClick"):
        elem.eventHandlers["onClick"]()

      if elem.eventHandlers.hasKey("onChange"):
        let handler = elem.eventHandlers["onChange"]
        handler($currentState)

  of ekTabGroup, ekTabBar, ekTabContent:
    let sortedChildren = sortChildrenByZIndexReverse(elem.children)
    for child in sortedChildren:
      if child.kind == ekConditional:
        backend.handleInput(child)
      elif child.kind == ekForLoop:
        for grandchild in child.children:
          backend.handleInput(grandchild)
      else:
        backend.handleInput(child)

  of ekTab:
    let mousePos = getMousePosition()
    if mousePos.x >= elem.x and mousePos.x < elem.x + elem.width and
       mousePos.y >= elem.y and mousePos.y < elem.y + elem.height:
      var parent = elem.parent
      var tabGroup: Element = nil

      while parent != nil:
        if parent.kind == ekTabGroup:
          tabGroup = parent
          break
        parent = parent.parent

      if tabGroup != nil:
        let oldSelectedIndex = tabGroup.tabSelectedIndex
        tabGroup.tabSelectedIndex = elem.tabIndex

        if oldSelectedIndex != tabGroup.tabSelectedIndex:
          if tabGroup.eventHandlers.hasKey("onSelectedIndexChanged"):
            tabGroup.eventHandlers["onSelectedIndexChanged"]()

          invalidateRelatedValues("tabSelectedIndex")
          markDirty(tabGroup)
          for child in tabGroup.children:
            markDirty(child)
            if child.kind == ekTabContent:
              for panel in child.children:
                markDirty(panel)
                markAllDescendantsDirty(panel)

      if elem.eventHandlers.hasKey("onClick"):
        elem.eventHandlers["onClick"]()

  of ekDropdown:
    let mousePos = getMousePosition()
    if mousePos.x >= elem.x and mousePos.x < elem.x + elem.width and
       mousePos.y >= elem.y and mousePos.y < elem.y + elem.height:
      if elem.dropdownIsOpen:
        elem.dropdownIsOpen = false
        elem.dropdownHoveredIndex = -1
        backend.state.focusedDropdown = nil
      else:
        elem.dropdownIsOpen = true
        elem.dropdownHoveredIndex = elem.dropdownSelectedIndex
        backend.state.focusedDropdown = elem
        backend.closeOtherDropdowns(elem)

    elif elem.dropdownIsOpen and elem.dropdownOptions.len > 0:
      let fontSize = elem.getProp("fontSize").get(val(16)).getInt()
      let itemHeight = fontSize.float + 10.0
      let dropdownHeight = min(elem.dropdownOptions.len.float * itemHeight, 200.0)

      if mousePos.x >= elem.x and mousePos.x < elem.x + elem.width and
         mousePos.y >= elem.y + elem.height and mousePos.y < elem.y + elem.height + dropdownHeight:
        let relativeY = mousePos.y - (elem.y + elem.height)
        let clickedIndex = int(relativeY / itemHeight)

        if clickedIndex >= 0 and clickedIndex < elem.dropdownOptions.len:
          elem.dropdownSelectedIndex = clickedIndex
          elem.dropdownIsOpen = false
          elem.dropdownHoveredIndex = -1
          backend.state.focusedDropdown = nil

          if elem.eventHandlers.hasKey("onChange"):
            let handler = elem.eventHandlers["onChange"]
            handler(elem.dropdownOptions[clickedIndex])

          if elem.eventHandlers.hasKey("onSelectionChange"):
            let handler = elem.eventHandlers["onSelectionChange"]
            handler(elem.dropdownOptions[clickedIndex])
      else:
        elem.dropdownIsOpen = false
        elem.dropdownHoveredIndex = -1
        backend.state.focusedDropdown = nil
    else:
      if backend.state.focusedDropdown == elem:
        backend.state.focusedDropdown = nil

    # Handle hover state
    if elem.dropdownIsOpen and elem.dropdownOptions.len > 0:
      let fontSize = elem.getProp("fontSize").get(val(16)).getInt()
      let itemHeight = fontSize.float + 10.0
      let dropdownHeight = min(elem.dropdownOptions.len.float * itemHeight, 200.0)

      if mousePos.x >= elem.x and mousePos.x < elem.x + elem.width and
         mousePos.y >= elem.y + elem.height and mousePos.y < elem.y + elem.height + dropdownHeight:
        let relativeY = mousePos.y - (elem.y + elem.height)
        let hoveredIndex = int(relativeY / itemHeight)

        if hoveredIndex >= 0 and hoveredIndex < elem.dropdownOptions.len:
          elem.dropdownHoveredIndex = hoveredIndex
        else:
          elem.dropdownHoveredIndex = -1
      else:
        elem.dropdownHoveredIndex = -1

  of ekColumn, ekRow, ekCenter, ekContainer:
    let sortedChildren = sortChildrenByZIndexReverse(elem.children)
    for child in sortedChildren:
      backend.handleInput(child)

  else:
    let sortedChildren = sortChildrenByZIndexReverse(elem.children)
    for child in sortedChildren:
      backend.handleInput(child)

proc handleKeyboardInput*(backend: var SkiaBackend, root: Element) =
  ## Handle keyboard input for focused input and dropdown elements

  # Handle dropdown keyboard input
  if backend.state.focusedDropdown != nil:
    let dropdown = backend.state.focusedDropdown

    if backend.keyStates[KEY_ESCAPE.int]:
      dropdown.dropdownIsOpen = false
      dropdown.dropdownHoveredIndex = -1
      backend.state.focusedDropdown = nil

    elif dropdown.dropdownIsOpen and dropdown.dropdownOptions.len > 0:
      if backend.keyStates[KEY_UP.int]:
        dropdown.dropdownHoveredIndex = if dropdown.dropdownHoveredIndex <= 0:
          dropdown.dropdownOptions.len - 1
        else:
          dropdown.dropdownHoveredIndex - 1

      elif backend.keyStates[KEY_DOWN.int]:
        dropdown.dropdownHoveredIndex = if dropdown.dropdownHoveredIndex >= dropdown.dropdownOptions.len - 1:
          0
        else:
          dropdown.dropdownHoveredIndex + 1

      elif backend.keyStates[KEY_ENTER.int]:
        if dropdown.dropdownHoveredIndex >= 0 and dropdown.dropdownHoveredIndex < dropdown.dropdownOptions.len:
          dropdown.dropdownSelectedIndex = dropdown.dropdownHoveredIndex
          dropdown.dropdownIsOpen = false
          dropdown.dropdownHoveredIndex = -1
          backend.state.focusedDropdown = nil

          if dropdown.eventHandlers.hasKey("onChange"):
            let handler = dropdown.eventHandlers["onChange"]
            handler(dropdown.dropdownOptions[dropdown.dropdownSelectedIndex])

          if dropdown.eventHandlers.hasKey("onSelectionChange"):
            let handler = dropdown.eventHandlers["onSelectionChange"]
            handler(dropdown.dropdownOptions[dropdown.dropdownSelectedIndex])

      elif backend.keyStates[KEY_TAB.int]:
        dropdown.dropdownIsOpen = false
        dropdown.dropdownHoveredIndex = -1
        backend.state.focusedDropdown = nil

  # Handle input field keyboard input
  if backend.state.focusedInput == nil:
    backend.state.backspaceHoldTimer = 0.0
    return

  var currentValue = backend.state.inputValues.getOrDefault(backend.state.focusedInput, "")
  var textChanged = false

  # Handle backspace
  if backend.keyStates[KEY_BACKSPACE.int] and currentValue.len > 0:
    currentValue.setLen(currentValue.len - 1)
    textChanged = true
    backend.state.backspaceHoldTimer = 0.0
  else:
    backend.state.backspaceHoldTimer = 0.0

  # Handle Enter
  if backend.keyStates[KEY_ENTER.int]:
    if backend.state.focusedInput.eventHandlers.hasKey("onSubmit"):
      backend.state.focusedInput.eventHandlers["onSubmit"]()

  # Update stored value if changed
  if textChanged:
    backend.state.inputValues[backend.state.focusedInput] = currentValue

    if backend.state.focusedInput.eventHandlers.hasKey("onTextChange"):
      let handler = backend.state.focusedInput.eventHandlers["onTextChange"]
      handler(currentValue)

    if backend.state.focusedInput.eventHandlers.hasKey("onChange"):
      let handler = backend.state.focusedInput.eventHandlers["onChange"]
      handler(currentValue)

    if backend.state.focusedInput.eventHandlers.hasKey("onValueChange"):
      let handler = backend.state.focusedInput.eventHandlers["onValueChange"]
      handler(currentValue)

# ============================================================================
# Main Loop
# ============================================================================

proc run*(backend: var SkiaBackend, root: Element) =
  ## Run the application with the given root element

  backend.rootElement = root
  backend.running = true

  # Initialize SDL2 for windowing only
  if SDL_Init(SDL_INIT_VIDEO) != 0:
    echo "Failed to initialize SDL2"
    return

  # Create window
  backend.window = SDL_CreateWindow(
    backend.windowTitle.cstring,
    SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
    backend.windowWidth.cint, backend.windowHeight.cint,
    SDL_WINDOW_SHOWN
  )

  if backend.window == nil:
    echo "Failed to create SDL2 window"
    SDL_Quit()
    return

  # Create Skia raster surface
  backend.surface = sk_surface_new_raster(
    backend.windowWidth.cint,
    backend.windowHeight.cint,
    kRGBA_8888_ColorType,
    kPremul_AlphaType,
    nil
  )

  if backend.surface == nil:
    echo "Failed to create Skia surface"
    SDL_DestroyWindow(backend.window)
    SDL_Quit()
    return

  # Get canvas from surface
  backend.canvas = sk_surface_get_canvas(backend.surface)

  # Create paint and font
  backend.paint = sk_paint_new()
  sk_paint_set_antialias(backend.paint, true)

  # Load font
  var fontPath = ""
  var fontName = resolvePreferredFont(root)

  if fontName.len > 0:
    fontPath = findFontByName(fontName)

  if fontPath.len == 0:
    let fontInfo = getDefaultFontInfo()
    if fontInfo.path.len > 0:
      fontPath = fontInfo.path
      fontName = fontInfo.name

  if fontPath.len > 0 and fileExists(fontPath):
    echo "Loading font: " & fontPath
    backend.typeface = sk_typeface_create_from_file(fontPath.cstring, 0)
    if backend.typeface != nil:
      backend.font = sk_font_new_with_values(backend.typeface, 16.0, 1.0, 0.0)
      echo "Successfully loaded font: " & fontName
    else:
      echo "Failed to load font, using default"
      backend.typeface = sk_typeface_create_default()
      backend.font = sk_font_new_with_values(backend.typeface, 16.0, 1.0, 0.0)
  else:
    echo "Using default font"
    backend.typeface = sk_typeface_create_default()
    backend.font = sk_font_new_with_values(backend.typeface, 16.0, 1.0, 0.0)

  setLayoutFont(backend.font, backend.paint)

  # Calculate initial layout
  calculateLayout(root, 0, 0, backend.windowWidth.float, backend.windowHeight.float)

  # Create cursor
  var defaultCursor = SDL_CreateSystemCursor(MOUSE_CURSOR_DEFAULT)
  var handCursor = SDL_CreateSystemCursor(MOUSE_CURSOR_POINTING_HAND)
  var currentCursor = defaultCursor

  # Main loop
  var running = true
  var event: SDL_Event

  while running:
    # Handle events
    while SDL_PollEvent(addr event) != 0:
      let eventType = getEventType(addr event)

      case eventType:
      of SDL_EVENT_QUIT:
        running = false

      of SDL_EVENT_KEYDOWN:
        let keyEvent = getKeyEvent(addr event)
        if keyEvent.keysym.scancode < 512:
          backend.keyStates[keyEvent.keysym.scancode] = true

      of SDL_EVENT_KEYUP:
        let keyEvent = getKeyEvent(addr event)
        if keyEvent.keysym.scancode < 512:
          backend.keyStates[keyEvent.keysym.scancode] = false

      of SDL_EVENT_MOUSEBUTTONDOWN:
        let buttonEvent = getMouseButtonEvent(addr event)
        if buttonEvent.button == SDL_BUTTON_LEFT:
          backend.handleInput(root)

      of SDL_EVENT_MOUSEMOTION:
        if checkHoverCursor(root):
          if currentCursor != handCursor:
            SDL_SetCursor(handCursor)
            currentCursor = handCursor
        else:
          if currentCursor != defaultCursor:
            SDL_SetCursor(defaultCursor)
            currentCursor = defaultCursor

      of SDL_EVENT_TEXTINPUT:
        if backend.textInputEnabled and backend.state.focusedInput != nil:
          let textEvent = getTextInputEvent(addr event)
          let text = $textEvent.text
          var currentValue = backend.state.inputValues.getOrDefault(backend.state.focusedInput, "")
          currentValue.add(text)
          backend.state.inputValues[backend.state.focusedInput] = currentValue

          if backend.state.focusedInput.eventHandlers.hasKey("onTextChange"):
            let handler = backend.state.focusedInput.eventHandlers["onTextChange"]
            handler(currentValue)

          if backend.state.focusedInput.eventHandlers.hasKey("onChange"):
            let handler = backend.state.focusedInput.eventHandlers["onChange"]
            handler(currentValue)

          if backend.state.focusedInput.eventHandlers.hasKey("onValueChange"):
            let handler = backend.state.focusedInput.eventHandlers["onValueChange"]
            handler(currentValue)

      else:
        discard

    # Update cursor blink timer
    backend.state.cursorBlink += 1.0 / 60.0
    if backend.state.cursorBlink >= 1.0:
      backend.state.cursorBlink = 0.0

    # Handle keyboard input
    backend.handleKeyboardInput(root)

    # Recalculate layout if needed
    if hasDirtyElements():
      calculateLayout(root, 0, 0, backend.windowWidth.float, backend.windowHeight.float)

    # Clear canvas
    var actualBody: Element = nil
    for child in root.children:
      if child.kind == ekBody:
        actualBody = child
        break

    var bodyBg: Option[Value] = none(Value)
    if actualBody != nil:
      bodyBg = actualBody.getProp("backgroundColor")
      if bodyBg.isNone:
        bodyBg = actualBody.getProp("background")

    let clearColor = if bodyBg.isSome: bodyBg.get.getColor() else: backend.backgroundColor
    sk_canvas_clear(backend.canvas, clearColor.toSkiaColor())

    # Render all elements
    backend.renderElement(root)

    # Render dropdown menus on top
    backend.renderDropdownMenus(root)

    # Flush canvas
    sk_canvas_flush(backend.canvas)

    # TODO: Present to SDL2 window (requires pixel buffer copy or GPU integration)
    # For now, this is a placeholder - full integration would require:
    # 1. Getting pixel data from Skia surface
    # 2. Creating SDL texture from pixel data
    # 3. Rendering to SDL window
    # OR using Skia's GPU backend with SDL2's OpenGL context

  # Cleanup
  if backend.font != nil:
    sk_font_delete(backend.font)
  if backend.typeface != nil:
    sk_typeface_unref(backend.typeface)
  if backend.paint != nil:
    sk_paint_delete(backend.paint)
  if backend.surface != nil:
    sk_surface_unref(backend.surface)
  if defaultCursor != nil:
    SDL_FreeCursor(defaultCursor)
  if handCursor != nil:
    SDL_FreeCursor(handCursor)
  if backend.window != nil:
    SDL_DestroyWindow(backend.window)
  SDL_Quit()
  backend.running = false
