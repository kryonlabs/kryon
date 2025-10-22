## Shared Renderer Pipeline
##
## This module provides backend-agnostic rendering routines that consume
## component extraction data and interact with the shared interaction state.
## Any backend that implements the renderer interface primitives can reuse
## these procedures to render the element tree, dropdown overlays, and
## drag-and-drop visuals.

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
import renderingContext
import ../state/backendState
import ../state/inputManager
import ../interactions/interactionState
import options
import math

# ============================================================================
# Helpers
# ============================================================================

proc measureTextWidth[R](renderer: R, text: string, fontSize: int): float =
  ## Convenience wrapper to get the width of a text string using the renderer.
  if text.len == 0:
    return 0.0
  let (width, _) = renderer.measureText(text, fontSize)
  return width

proc getDragContentOverridePanel(elem: Element): Element =
  ## Return the tab panel that should be rendered while dragging, if any.
  let interaction = getInteractionState()
  if interaction.isLiveReordering and interaction.dragTabContentPanel != nil and elem.kind == ekTabContent:
    return interaction.dragTabContentPanel
  return nil

# ============================================================================
# Tree Rendering
# ============================================================================

proc renderElement*[R](renderer: var R, state: var BackendState,
                       elem: Element, inheritedColor: Option[Color] = none(Color)) =
  ## Render a single element (and its children) using the provided renderer.
  ## The renderer type only needs to provide the drawing primitives declared
  ## in `renderer_interface.nim`.

  let interaction = getInteractionState()

  case elem.kind:
  of ekHeader, ekResources, ekFont:
    # Non-visual metadata elements.
    discard

  of ekConditional:
    if elem.condition != nil:
      let conditionResult = elem.condition()
      let activeBranch = if conditionResult: elem.trueBranch else: elem.falseBranch
      if activeBranch != nil:
        renderElement(renderer, state, activeBranch, inheritedColor)

  of ekForLoop:
    for child in elem.children:
      renderElement(renderer, state, child, inheritedColor)

  of ekBody:
    let data = extractBodyData(elem, inheritedColor)
    for child in data.sortedChildren:
      renderElement(renderer, state, child, data.colorToInherit)

  of ekContainer:
    let data = extractContainerData(elem, inheritedColor)

    if data.hasBackground:
      renderer.drawRectangle(data.x, data.y, data.width, data.height, data.backgroundColor)
    if data.hasBorder:
      renderer.drawRectangleBorder(data.x, data.y, data.width, data.height, data.borderWidth, data.borderColor)
    for child in data.sortedChildren:
      renderElement(renderer, state, child, inheritedColor)

  of ekText:
    let data = extractTextData(elem, inheritedColor)
    renderer.drawText(data.text, data.x, data.y, data.fontSize, data.color)

  of ekButton:
    let data = extractButtonData(elem, inheritedColor)

    let bgColor = if data.disabled: getDisabledColor(data.backgroundColor) else: data.backgroundColor
    let textColor = if data.disabled: getDisabledColor(data.textColor) else: data.textColor
    let borderColor = if data.disabled: getDisabledColor(data.borderColor) else: data.borderColor

    renderer.drawRectangle(data.x, data.y, data.width, data.height, bgColor)
    renderer.drawRectangleBorder(data.x, data.y, data.width, data.height, data.borderWidth, borderColor)
    let textMeasurements = renderer.measureText(data.text, data.fontSize)
    let textX = data.x + (data.width - textMeasurements.width) / 2.0
    let textY = data.y + (data.height - textMeasurements.height) / 2.0
    renderer.drawText(data.text, textX, textY, data.fontSize, textColor)

  of ekInput:
    let data = extractInputData(elem, state, inheritedColor)

    renderer.drawRectangle(data.x, data.y, data.width, data.height, data.backgroundColor)
    renderer.drawRectangleBorder(data.x, data.y, data.width, data.height, data.borderWidth, data.borderColor)

    let cursorTextPos = if data.value.len > 0 and data.isFocused:
      measureTextWidth(renderer, data.value, data.fontSize)
    else:
      0.0

    var scrollOffset = data.scrollOffset

    if data.isFocused:
      let totalTextWidth = if data.value.len > 0:
        measureTextWidth(renderer, data.value, data.fontSize)
      else:
        0.0

      # Keep cursor visible within the clipping region.
      if cursorTextPos - scrollOffset > data.visibleWidth - 20.0:
        scrollOffset = cursorTextPos - data.visibleWidth + 20.0
      if cursorTextPos < scrollOffset + 20.0:
        scrollOffset = max(0.0, cursorTextPos - 20.0)
      scrollOffset = max(0.0, scrollOffset)

      if totalTextWidth <= data.visibleWidth:
        scrollOffset = 0.0
      else:
        let maxScrollOffset = max(0.0, totalTextWidth - data.visibleWidth)
        scrollOffset = min(scrollOffset, maxScrollOffset)

      state.inputScrollOffsets[elem] = scrollOffset

    renderer.beginClipping(data.x + data.padding, data.y + 2.0, data.visibleWidth, data.height - 4.0)

    if data.displayText.len > 0:
      let textX = data.x + data.padding - scrollOffset
      let textY = data.y + (data.height - data.fontSize.float) / 2.0
      renderer.drawText(data.displayText, textX, textY, data.fontSize, data.displayColor)

    renderer.endClipping()

    if data.showCursor:
      let cursorX = data.x + data.padding + cursorTextPos - scrollOffset
      let cursorY = data.cursorY
      if cursorX >= data.x + data.padding and cursorX <= data.x + data.width - data.padding:
        renderer.drawRectangle(cursorX, cursorY, 1.0, data.fontSize.float - 2.0, data.textColor)

  of ekDropdown:
    let data = extractDropdownButtonData(elem, state, inheritedColor)

    renderer.drawRectangle(data.x, data.y, data.width, data.height, data.backgroundColor)
    renderer.drawRectangleBorder(data.x, data.y, data.width, data.height, data.borderWidth, data.borderColor)
    renderer.drawText(data.arrowChar, data.arrowX, data.arrowY, data.fontSize, data.textColor)

    renderer.beginClipping(data.textX, data.textY, data.maxTextWidth, data.fontSize.float)
    renderer.drawText(data.displayText, data.textX, data.textY, data.fontSize, data.displayColor)
    renderer.endClipping()

  of ekCheckbox:
    let data = extractCheckboxData(elem, state, inheritedColor)

    renderer.drawRectangle(data.checkboxX, data.checkboxY, data.checkboxSize, data.checkboxSize, data.backgroundColor)

    if data.borderWidth > 0:
      renderer.drawRectangleBorder(data.checkboxX, data.checkboxY, data.checkboxSize, data.checkboxSize, data.borderWidth, data.borderColor)

    if data.checked:
      for segment in data.checkmarkSegments1:
        renderer.drawRectangle(segment.x, segment.y, segment.thickness, segment.thickness, data.checkColor)
      for segment in data.checkmarkSegments2:
        renderer.drawRectangle(segment.x, segment.y, segment.thickness, segment.thickness, data.checkColor)

    if data.hasLabel:
      renderer.drawText(data.label, data.labelX, data.labelY, data.fontSize, data.labelColor)

  of ekColumn:
    let data = extractColumnData(elem, inheritedColor)
    for child in data.sortedChildren:
      renderElement(renderer, state, child, inheritedColor)

  of ekRow:
    let data = extractRowData(elem, inheritedColor)
    for child in data.sortedChildren:
      renderElement(renderer, state, child, inheritedColor)

  of ekCenter:
    let data = extractCenterData(elem, inheritedColor)
    for child in data.sortedChildren:
      renderElement(renderer, state, child, inheritedColor)

  of ekTabGroup:
    let data = extractTabGroupData(elem, inheritedColor)
    if data.hasBackground:
      renderer.drawRectangle(data.x, data.y, data.width, data.height, data.backgroundColor)
    for child in data.sortedChildren:
      if child.kind != ekConditional and child.kind != ekForLoop:
        renderElement(renderer, state, child, inheritedColor)

  of ekTabBar:
    let data = extractTabBarData(elem, inheritedColor)
    if data.hasBackground:
      renderer.drawRectangle(data.x, data.y, data.width, data.height, data.backgroundColor)
    if data.hasBorder:
      renderer.drawRectangle(data.x, data.borderY, data.width, data.borderWidth, data.borderColor)
    for child in data.sortedChildren:
      if child != interaction.draggedElement:
        renderElement(renderer, state, child, inheritedColor)

  of ekTab:
    let data = extractTabData(elem, inheritedColor)

    renderer.drawRectangle(data.x, data.y, data.width, data.height, data.backgroundColor)
    renderer.drawRectangleBorder(data.x, data.y, data.width, data.height, data.borderWidth, data.borderColor)

    if elem != interaction.draggedElement:
      renderer.drawRectangle(data.x, data.bottomBorderY, data.width, data.bottomBorderWidth, data.bottomBorderColor)

    if data.label.len > 0:
      let textMeasurements = renderer.measureText(data.label, data.fontSize)
      let textX = data.x + (data.width - textMeasurements.width) / 2.0
      let textY = data.y + (data.height - textMeasurements.height) / 2.0
      renderer.drawText(data.label, textX, textY, data.fontSize, data.labelColor)

  of ekTabContent:
    let data = extractTabContentData(elem, inheritedColor)

    if data.hasBackground:
      renderer.drawRectangle(data.x, data.y, data.width, data.height, data.backgroundColor)

    let dragOverridePanel = getDragContentOverridePanel(elem)
    if dragOverridePanel != nil:
      renderElement(renderer, state, dragOverridePanel, inheritedColor)
    elif data.hasActivePanel:
      renderElement(renderer, state, data.activePanel, inheritedColor)

  of ekTabPanel:
    let data = extractTabPanelData(elem, inheritedColor)

    if data.hasBackground:
      renderer.drawRectangle(data.x, data.y, data.width, data.height, data.backgroundColor)

    for child in data.sortedChildren:
      renderElement(renderer, state, child, inheritedColor)

  else:
    discard

  if elem.kind notin {ekColumn, ekRow, ekCenter, ekBody, ekTabGroup, ekTabBar, ekTabContent, ekTabPanel}:
    let sortedChildren = sortChildrenByZIndex(elem.children)
    for child in sortedChildren:
      renderElement(renderer, state, child, inheritedColor)

# ============================================================================
# Dropdown Overlay Rendering
# ============================================================================

proc renderDropdownMenus*[R](renderer: var R, state: var BackendState, elem: Element) =
  ## Render all open dropdown menus above the main content.
  case elem.kind:
  of ekDropdown:
    let data = extractDropdownMenuData(elem, state, none(Color))
    if not data.isOpen:
      return

    renderer.drawRectangle(data.x, data.y, data.width, data.height, data.backgroundColor)
    renderer.drawRectangleBorder(data.x, data.y, data.width, data.height, data.borderWidth, data.borderColor)

    for opt in data.options:
      if opt.isHovered:
        renderer.drawRectangle(opt.x, opt.y, opt.width, opt.height, data.hoveredHighlightColor)
      if opt.isSelected:
        renderer.drawRectangle(opt.x, opt.y, opt.width, opt.height, data.selectedHighlightColor)
      renderer.beginClipping(opt.textX, opt.textY, opt.maxTextWidth, data.fontSize.float)
      renderer.drawText(opt.text, opt.textX, opt.textY, data.fontSize, data.textColor)
      renderer.endClipping()

  of ekConditional:
    if elem.condition != nil:
      let conditionResult = elem.condition()
      let activeBranch = if conditionResult: elem.trueBranch else: elem.falseBranch
      if activeBranch != nil:
        renderDropdownMenus(renderer, state, activeBranch)

  else:
    for child in elem.children:
      renderDropdownMenus(renderer, state, child)

# ============================================================================
# Drag-and-Drop Visuals
# ============================================================================

proc renderDropTargetHighlights[R](renderer: var R, elem: Element) =
  ## Draw visual highlights for hovered drop targets (except TabBar during reorder).
  if elem.withBehaviors.len > 0 and elem.dropTargetState.isHovered and elem.kind != ekTabBar:
    let highlightColor = rgba(82, 196, 26, 60)
    renderer.drawRectangle(elem.x, elem.y, elem.width, elem.height, highlightColor)
    let borderColor = rgba(82, 196, 26, 200)
    renderer.drawRectangleBorder(elem.x, elem.y, elem.width, elem.height, 3.0, borderColor)

  for child in elem.children:
    renderDropTargetHighlights(renderer, child)

proc renderDraggedElement[R](renderer: var R, elem: Element, offsetX, offsetY: float) =
  ## Render a dragged element at the specified offset with transparency.
  if elem.kind == ekContainer:
    let bgColor = elem.getProp("backgroundColor").get(val("#2a2a2a")).getColor()
    let transparentBg = rgba(bgColor.r, bgColor.g, bgColor.b, 180)
    renderer.drawRectangle(elem.x + offsetX, elem.y + offsetY, elem.width, elem.height, transparentBg)

    let borderWidth = elem.getProp("borderWidth").get(val(2)).getFloat()
    if borderWidth > 0:
      let borderColor = elem.getProp("borderColor").get(val("#4a4a4a")).getColor()
      let transparentBorder = rgba(borderColor.r, borderColor.g, borderColor.b, 180)
      renderer.drawRectangleBorder(elem.x + offsetX, elem.y + offsetY, elem.width, elem.height, borderWidth, transparentBorder)

  elif elem.kind == ekTab:
    let bgColor = elem.getProp("backgroundColor").get(val("#3d3d3d")).getColor()
    let transparentBg = rgba(bgColor.r, bgColor.g, bgColor.b, 180)
    renderer.drawRectangle(elem.x + offsetX, elem.y + offsetY, elem.width, elem.height, transparentBg)

    let borderWidth = 1.0
    let borderColor = rgba(74, 74, 74, 180)
    renderer.drawRectangleBorder(elem.x + offsetX, elem.y + offsetY, elem.width, elem.height, borderWidth, borderColor)

    let title = elem.getProp("title").get(val("Tab")).getString()
    let textColor = elem.getProp("textColor").get(val("#cccccc")).getColor()
    let transparentText = rgba(textColor.r, textColor.g, textColor.b, 180)
    let fontSize = 16
    let textMeasurements = renderer.measureText(title, fontSize)
    let textX = elem.x + offsetX + (elem.width - textMeasurements.width) / 2.0
    let textY = elem.y + offsetY + (elem.height - textMeasurements.height) / 2.0
    renderer.drawText(title, textX, textY, fontSize, transparentText)

  for child in elem.children:
    renderDraggedElement(renderer, child, offsetX, offsetY)

  if elem.kind == ekText:
    let textColor = elem.getProp("color").get(val("#ffffff")).getColor()
    let transparentText = rgba(textColor.r, textColor.g, textColor.b, 180)
    let fontSize = elem.getProp("fontSize").get(val(16)).getInt()
    let text = elem.getProp("text").get(val("")).getString()
    renderer.drawText(text, elem.x + offsetX, elem.y + offsetY, fontSize, transparentText)

proc renderDragAndDropEffects*[R](renderer: var R, root: Element) =
  ## Render highlights and the ghost element for drag-and-drop interactions.
  renderDropTargetHighlights(renderer, root)

  let interaction = getInteractionState()

  if interaction.draggedElement != nil:
    let draggedElem = interaction.draggedElement
    let offsetX = draggedElem.dragState.currentOffsetX
    let offsetY = draggedElem.dragState.currentOffsetY
    let baseOffsetX = draggedElem.dragState.elementStartX - draggedElem.x
    let baseOffsetY = draggedElem.dragState.elementStartY - draggedElem.y
    let totalOffsetX = offsetX + baseOffsetX
    let totalOffsetY = offsetY + baseOffsetY

    renderDraggedElement(renderer, draggedElem, totalOffsetX, totalOffsetY)

    if draggedElem.kind == ekTab:
      let indicatorColor = rgba(74, 144, 226, 255)
      let indicatorWidth = 3.0

      let dragX = draggedElem.x + totalOffsetX
      renderer.drawRectangle(dragX - indicatorWidth / 2.0,
                             draggedElem.y + totalOffsetY + draggedElem.height - 3.0,
                             draggedElem.width + indicatorWidth,
                             indicatorWidth,
                             indicatorColor)
