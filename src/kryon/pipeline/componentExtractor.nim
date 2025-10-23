## Component Extractor
##
## This module extracts rendering data from elements and converts them
## into backend-agnostic RenderCommands. This is the bridge between the
## high-level component tree and low-level drawing instructions.
##
## Key principle: NO backend-specific code here. Everything produces
## RenderCommands that can be executed by any renderer.

import ../core
import ../components/button
import ../components/text
import ../components/input
import ../components/checkbox
import ../components/dropdown
import ../components/container
import ../components/containers
import ../components/tabs
import ../layout/zindexSort
import ../state/backendState
import ../interactions/interactionState
import renderCommands
import options

# ============================================================================
# Text Measurement Interface
# ============================================================================

type
  TextMeasurer* = concept m
    ## Duck-typed interface for text measurement
    ## Backends must provide this for layout calculations
    m.measureText(string, int) is tuple[width: float, height: float]

# ============================================================================
# Component Extraction to RenderCommands
# ============================================================================

proc extractButton*(elem: Element, inheritedColor: Option[Color] = none(Color)): RenderCommandList =
  ## Extract button rendering commands
  let data = extractButtonData(elem, inheritedColor)

  result = @[]

  # Background
  result.add drawRectangle(data.x, data.y, data.width, data.height, data.backgroundColor)

  # Border
  if data.borderWidth > 0:
    result.add drawBorder(data.x, data.y, data.width, data.height, data.borderWidth, data.borderColor)

  # Image (if present)
  if data.hasImage:
    let imgX = data.x + 10
    let imgY = data.y + 10
    let imgWidth = data.width - 20
    let imgHeight = data.height - 20
    result.add drawImage(data.imagePath, imgX, imgY, imgWidth, imgHeight)

  # Text (if present)
  if data.text.len > 0:
    # Note: Text positioning will need text measurements
    # For now, we'll add the command and let the renderer handle positioning
    # In a future iteration, we should pre-calculate positions
    result.add drawText(data.text, data.x, data.y, data.fontSize, data.textColor)

proc extractText*(elem: Element, measurer: TextMeasurer, inheritedColor: Option[Color] = none(Color)): RenderCommandList =
  ## Extract text rendering commands
  let data = extractTextData(elem, inheritedColor)
  result = @[]

  if data.text.len > 0:
    # Measure text for alignment
    let textMeasurements = measurer.measureText(data.text, data.fontSize)

    let alignedX = case data.alignment:
      of taCenter:
        if elem.parent != nil:
          data.x + (elem.parent.width - textMeasurements.width) / 2.0
        else:
          data.x
      of taRight:
        if elem.parent != nil:
          data.x + elem.parent.width - textMeasurements.width
        else:
          data.x
      else:
        data.x

    result.add drawText(data.text, alignedX, data.y, data.fontSize, data.color)

proc extractContainer*(elem: Element, inheritedColor: Option[Color] = none(Color)): RenderCommandList =
  ## Extract container rendering commands
  let data = extractContainerData(elem, inheritedColor)
  result = @[]

  # Background
  if data.hasBackground:
    result.add drawRectangle(data.x, data.y, data.width, data.height, data.backgroundColor)

  # Border
  if data.hasBorder:
    result.add drawBorder(data.x, data.y, data.width, data.height, data.borderWidth, data.borderColor)

proc extractCheckbox*(elem: Element, state: var BackendState, inheritedColor: Option[Color] = none(Color)): RenderCommandList =
  ## Extract checkbox rendering commands
  let data = extractCheckboxData(elem, state, inheritedColor)
  result = @[]

  # Background
  result.add drawRectangle(data.checkboxX, data.checkboxY, data.checkboxSize, data.checkboxSize, data.backgroundColor)

  # Border
  result.add drawBorder(data.checkboxX, data.checkboxY, data.checkboxSize, data.checkboxSize, data.borderWidth, data.borderColor)

  # Checkmark if checked
  if data.checked:
    # Draw checkmark segments (from component data)
    for segment in data.checkmarkSegments1:
      result.add drawLine(segment.x, segment.y, segment.x, segment.y, segment.thickness, data.checkColor)

    for segment in data.checkmarkSegments2:
      result.add drawLine(segment.x, segment.y, segment.x, segment.y, segment.thickness, data.checkColor)

  # Label text if present
  if data.hasLabel:
    result.add drawText(data.label, data.labelX, data.labelY, data.fontSize, data.labelColor)

proc extractInput*(elem: Element, measurer: TextMeasurer, state: var BackendState, inheritedColor: Option[Color] = none(Color)): RenderCommandList =
  ## Extract input field rendering commands
  let data = extractInputData(elem, state, inheritedColor)
  result = @[]

  # Background
  result.add drawRectangle(data.x, data.y, data.width, data.height, data.backgroundColor)

  # Border
  result.add drawBorder(data.x, data.y, data.width, data.height, data.borderWidth, data.borderColor)

  # Text (clipped)
  if data.displayText.len > 0:
    result.add beginClipping(data.x + data.padding, data.y, data.visibleWidth, data.height)
    result.add drawText(data.displayText, data.x + data.padding - data.scrollOffset,
                       data.y + 4.0, data.fontSize, data.displayColor)
    result.add endClipping()

  # Cursor if focused
  if data.isFocused and data.showCursor:
    let textWidth = if data.value.len > 0:
      measurer.measureText(data.value, data.fontSize).width
    else:
      0.0
    let cursorX = data.x + data.padding + textWidth - data.scrollOffset
    result.add drawLine(cursorX, data.cursorY, cursorX, data.cursorY + data.fontSize.float,
                       2.0, data.textColor)

proc extractDropdown*(elem: Element, measurer: TextMeasurer, state: var BackendState, inheritedColor: Option[Color] = none(Color)): RenderCommandList =
  ## Extract dropdown rendering commands
  let buttonData = extractDropdownButtonData(elem, state, inheritedColor)
  result = @[]

  # Main button background
  result.add drawRectangle(buttonData.x, buttonData.y, buttonData.width, buttonData.height, buttonData.backgroundColor)

  # Border
  result.add drawBorder(buttonData.x, buttonData.y, buttonData.width, buttonData.height,
                       buttonData.borderWidth, buttonData.borderColor)

  # Display text
  if buttonData.displayText.len > 0:
    result.add drawText(buttonData.displayText, buttonData.textX, buttonData.textY,
                       buttonData.fontSize, buttonData.displayColor)

  # Arrow indicator
  result.add drawText(buttonData.arrowChar, buttonData.arrowX, buttonData.arrowY,
                     buttonData.fontSize, buttonData.textColor)

  # Dropdown menu if open
  if buttonData.isOpen:
    let menuData = extractDropdownMenuData(elem, state, inheritedColor)

    # Menu background
    result.add drawRectangle(menuData.x, menuData.y, menuData.width, menuData.height, menuData.backgroundColor)

    # Menu border
    result.add drawBorder(menuData.x, menuData.y, menuData.width, menuData.height,
                         menuData.borderWidth, menuData.borderColor)

    # Render options
    for option in menuData.options:
      # Highlight if hovered or selected
      if option.isHovered:
        result.add drawRectangle(option.x, option.y, option.width, option.height, menuData.hoveredHighlightColor)
      elif option.isSelected:
        result.add drawRectangle(option.x, option.y, option.width, option.height, menuData.selectedHighlightColor)

      # Option text
      result.add drawText(option.text, option.textX, option.textY, menuData.fontSize, menuData.textColor)

proc extractTabBar*(elem: Element, inheritedColor: Option[Color] = none(Color)): RenderCommandList =
  ## Extract tab bar rendering commands
  let data = extractTabBarData(elem, inheritedColor)
  result = @[]

  if data.hasBackground:
    result.add drawRectangle(data.x, data.y, data.width, data.height, data.backgroundColor)

  if data.hasBorder:
    result.add drawRectangle(data.x, data.borderY, data.width, data.borderWidth, data.borderColor)

proc extractTab*(elem: Element, measurer: TextMeasurer, inheritedColor: Option[Color] = none(Color)): RenderCommandList =
  ## Extract tab rendering commands
  let data = extractTabData(elem, inheritedColor)
  result = @[]

  # Background
  result.add drawRectangle(data.x, data.y, data.width, data.height, data.backgroundColor)

  # Border
  if data.borderWidth > 0:
    result.add drawBorder(data.x, data.y, data.width, data.height, data.borderWidth, data.borderColor)

  # Bottom border (active indicator)
  if data.bottomBorderWidth > 0:
    result.add drawRectangle(data.x, data.bottomBorderY, data.width, data.bottomBorderWidth, data.bottomBorderColor)

  # Label
  if data.label.len > 0:
    let textMeasurements = measurer.measureText(data.label, data.fontSize)
    let textX = data.x + (data.width - textMeasurements.width) / 2.0
    let textY = data.y + (data.height - textMeasurements.height) / 2.0
    result.add drawText(data.label, textX, textY, data.fontSize, data.labelColor)

# ============================================================================
# Main Extraction Function
# ============================================================================

proc extractElement*(elem: Element, measurer: TextMeasurer, state: var BackendState, inheritedColor: Option[Color] = none(Color)): RenderCommandList =
  ## Extract render commands for an element and its children
  ## This is the main entry point for component extraction

  result = @[]

  case elem.kind:
  of ekHeader, ekResources, ekFont:
    # Non-visual metadata elements
    discard

  of ekConditional:
    # Handle conditional rendering
    if elem.condition != nil:
      let conditionResult = elem.condition()
      let activeBranch = if conditionResult: elem.trueBranch else: elem.falseBranch
      if activeBranch != nil:
        result.add extractElement(activeBranch, measurer, state, inheritedColor)

  of ekForLoop:
    # ForLoop children are already resolved by preprocessor
    for child in elem.children:
      result.add extractElement(child, measurer, state, inheritedColor)

  of ekBody:
    let data = extractBodyData(elem, inheritedColor)
    for child in data.sortedChildren:
      result.add extractElement(child, measurer, state, data.colorToInherit)

  of ekContainer:
    result.add extractContainer(elem, inheritedColor)
    let data = extractContainerData(elem, inheritedColor)
    for child in data.sortedChildren:
      result.add extractElement(child, measurer, state, inheritedColor)

  of ekText, ekH1, ekH2, ekH3:
    result.add extractText(elem, measurer, inheritedColor)

  of ekButton:
    result.add extractButton(elem, inheritedColor)

  of ekCheckbox:
    result.add extractCheckbox(elem, state, inheritedColor)

  of ekInput:
    result.add extractInput(elem, measurer, state, inheritedColor)

  of ekDropdown:
    result.add extractDropdown(elem, measurer, state, inheritedColor)

  of ekTabGroup:
    let data = extractTabGroupData(elem, inheritedColor)
    if data.hasBackground:
      result.add drawRectangle(data.x, data.y, data.width, data.height, data.backgroundColor)
    for child in data.sortedChildren:
      result.add extractElement(child, measurer, state, inheritedColor)

  of ekTabBar:
    result.add extractTabBar(elem, inheritedColor)
    let data = extractTabBarData(elem, inheritedColor)
    for child in data.sortedChildren:
      result.add extractElement(child, measurer, state, inheritedColor)

  of ekTab:
    result.add extractTab(elem, measurer, inheritedColor)

  of ekTabContent:
    let data = extractTabContentData(elem, inheritedColor)
    if data.hasBackground:
      result.add drawRectangle(data.x, data.y, data.width, data.height, data.backgroundColor)
    if data.hasActivePanel:
      result.add extractElement(data.activePanel, measurer, state, inheritedColor)

  of ekTabPanel:
    let data = extractTabPanelData(elem, inheritedColor)
    if data.hasBackground:
      result.add drawRectangle(data.x, data.y, data.width, data.height, data.backgroundColor)
    for child in data.sortedChildren:
      result.add extractElement(child, measurer, state, inheritedColor)

  of ekColumn:
    let data = extractColumnData(elem, inheritedColor)
    for child in data.sortedChildren:
      result.add extractElement(child, measurer, state, inheritedColor)

  of ekRow:
    let data = extractRowData(elem, inheritedColor)
    for child in data.sortedChildren:
      result.add extractElement(child, measurer, state, inheritedColor)

  of ekCenter:
    let data = extractCenterData(elem, inheritedColor)
    for child in data.sortedChildren:
      result.add extractElement(child, measurer, state, inheritedColor)

  else:
    # For other elements, just recurse to children
    let sortedChildren = sortChildrenByZIndex(elem.children)
    for child in sortedChildren:
      result.add extractElement(child, measurer, state, inheritedColor)

# ============================================================================
# Drag-and-Drop Visual Effects
# ============================================================================

proc extractDropTargetHighlights(elem: Element): RenderCommandList =
  ## Extract drop target highlight rendering commands
  result = @[]

  # Draw highlight if element is a hovered drop target (except TabBar during reorder)
  if elem.withBehaviors.len > 0 and elem.dropTargetState.isHovered and elem.kind != ekTabBar:
    let highlightColor = rgba(82, 196, 26, 60)
    result.add drawRectangle(elem.x, elem.y, elem.width, elem.height, highlightColor)

    let borderColor = rgba(82, 196, 26, 200)
    result.add drawBorder(elem.x, elem.y, elem.width, elem.height, 3.0, borderColor)

  # Recurse to children
  for child in elem.children:
    result.add extractDropTargetHighlights(child)

proc extractDraggedElement(elem: Element, measurer: TextMeasurer, offsetX, offsetY: float): RenderCommandList =
  ## Extract dragged element rendering commands with transparency
  result = @[]

  if elem.kind == ekContainer:
    let bgColor = elem.getProp("backgroundColor").get(val("#2a2a2a")).getColor()
    let transparentBg = rgba(bgColor.r, bgColor.g, bgColor.b, 180)
    result.add drawRectangle(elem.x + offsetX, elem.y + offsetY, elem.width, elem.height, transparentBg)

    let borderWidth = elem.getProp("borderWidth").get(val(2)).getFloat()
    if borderWidth > 0:
      let borderColor = elem.getProp("borderColor").get(val("#4a4a4a")).getColor()
      let transparentBorder = rgba(borderColor.r, borderColor.g, borderColor.b, 180)
      result.add drawBorder(elem.x + offsetX, elem.y + offsetY, elem.width, elem.height, borderWidth, transparentBorder)

  elif elem.kind == ekTab:
    let bgColor = elem.getProp("backgroundColor").get(val("#3d3d3d")).getColor()
    let transparentBg = rgba(bgColor.r, bgColor.g, bgColor.b, 180)
    result.add drawRectangle(elem.x + offsetX, elem.y + offsetY, elem.width, elem.height, transparentBg)

    let borderWidth = 1.0
    let borderColor = rgba(74, 74, 74, 180)
    result.add drawBorder(elem.x + offsetX, elem.y + offsetY, elem.width, elem.height, borderWidth, borderColor)

    let title = elem.getProp("title").get(val("Tab")).getString()
    let textColor = elem.getProp("textColor").get(val("#cccccc")).getColor()
    let transparentText = rgba(textColor.r, textColor.g, textColor.b, 180)
    let fontSize = 16
    let textMeasurements = measurer.measureText(title, fontSize)
    let textX = elem.x + offsetX + (elem.width - textMeasurements.width) / 2.0
    let textY = elem.y + offsetY + (elem.height - textMeasurements.height) / 2.0
    result.add drawText(title, textX, textY, fontSize, transparentText)

proc extractDragAndDropEffects*(root: Element, measurer: TextMeasurer): RenderCommandList =
  ## Extract all drag-and-drop visual effects as render commands
  result = @[]

  # Extract drop target highlights
  result.add extractDropTargetHighlights(root)

  # Extract dragged element ghost if dragging
  let interaction = getInteractionState()
  if interaction.draggedElement != nil:
    let draggedElem = interaction.draggedElement
    let offsetX = draggedElem.dragState.currentOffsetX
    let offsetY = draggedElem.dragState.currentOffsetY
    let baseOffsetX = draggedElem.dragState.elementStartX - draggedElem.x
    let baseOffsetY = draggedElem.dragState.elementStartY - draggedElem.y
    let totalOffsetX = offsetX + baseOffsetX
    let totalOffsetY = offsetY + baseOffsetY

    # Render dragged element ghost
    result.add extractDraggedElement(draggedElem, measurer, totalOffsetX, totalOffsetY)

    # Add drop indicator for tab reordering
    if draggedElem.kind == ekTab:
      let indicatorColor = rgba(74, 144, 226, 255)
      let indicatorWidth = 3.0
      let dragX = draggedElem.x + totalOffsetX
      result.add drawRectangle(
        dragX - indicatorWidth / 2.0,
        draggedElem.y + totalOffsetY + draggedElem.height - 3.0,
        draggedElem.width + indicatorWidth,
        indicatorWidth,
        indicatorColor
      )
