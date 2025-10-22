## Raylib Backend for Kryon
##
## This backend renders Kryon UI elements using Raylib for native desktop applications.

import ../../kryon/core
import ../../kryon/fonts
import ../../kryon/layout/layoutEngine
import ../../kryon/layout/zindexSort
import ../../kryon/state/backendState
import ../../kryon/rendering/renderingContext
import ../../kryon/rendering/sharedRenderer
import ../../kryon/interactions/dragDrop
import ../../kryon/interactions/tabReordering
import ../../kryon/interactions/interactionState
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
    state: newBackendState()
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
    state: newBackendState()
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
    DrawRectangle(x1.cint, y1.cint, (x2 - x1).abs.cint + 1, (y2 - y1).abs.cint + 1, color.toRaylibColor())
  else:
    # For thicker lines, draw a rectangle rotated
    # Simple implementation: just draw a thick horizontal or vertical line
    if abs(x2 - x1) > abs(y2 - y1):
      # More horizontal
      DrawRectangle(min(x1, x2).cint, (y1 - thickness / 2).cint, abs(x2 - x1).cint, thickness.cint, color.toRaylibColor())
    else:
      # More vertical
      DrawRectangle((x1 - thickness / 2).cint, min(y1, y2).cint, thickness.cint, abs(y2 - y1).cint, color.toRaylibColor())

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

  if layoutFontInitialized:
    let size = MeasureTextEx(layoutFont, text.cstring, fontSize.cfloat, 0.0)
    return (width: size.x, height: size.y)
  elif m.backend != nil:
    let size = MeasureTextEx(m.backend.font, text.cstring, fontSize.cfloat, 0.0)
    return (width: size.x, height: size.y)
  else:
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
    if elem.condition != nil:
      let conditionResult = elem.condition()
      let activeBranch = if conditionResult: elem.trueBranch else: elem.falseBranch
      if activeBranch != nil:
        if checkHoverCursor(activeBranch):
          return true

  of ekButton:
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

    if CheckCollisionPointRec(mousePos, mainRect):
      return true

    if elem.dropdownIsOpen and elem.dropdownOptions.len > 0:
      let fontSize = elem.getProp("fontSize").get(val(16)).getInt()
      let itemHeight = fontSize.float + 10.0
      let dropdownHeight = min(elem.dropdownOptions.len.float * itemHeight, 200.0)
      let dropdownRect = rrect(elem.x, elem.y + elem.height, elem.width, dropdownHeight)
      if CheckCollisionPointRec(mousePos, dropdownRect):
        return true

  of ekCheckbox:
    let mousePos = GetMousePosition()
    let fontSize = elem.getProp("fontSize").get(val(16)).getInt()
    let checkboxSize = min(elem.height, fontSize.float + 8.0)
    let checkboxRect = rrect(elem.x, elem.y + (elem.height - checkboxSize) / 2.0, checkboxSize, checkboxSize)

    let label = elem.getProp("label").get(val("")).getString()
    var hoverArea = checkboxRect

    if label.len > 0:
      hoverArea.x = elem.x
      hoverArea.y = elem.y
      hoverArea.width = elem.width
      hoverArea.height = elem.height

    if CheckCollisionPointRec(mousePos, hoverArea):
      return true

  else:
    discard

  for child in elem.children:
    if checkHoverCursor(child):
      return true

  return false


proc handleInput*(backend: var RaylibBackend, elem: Element) =
  ## Handle mouse input for interactive elements

  let mousePos = GetMousePosition()
  let mouseX = mousePos.x.float
  let mouseY = mousePos.y.float

  if elem == backend.rootElement:
    if globalInteractionState.shouldClearDragOverride:
      resetTabContentOverride()

    if IsMouseButtonPressed(MOUSE_BUTTON_LEFT):
      discard handleDragStart(backend.rootElement, mouseX, mouseY)

    if globalInteractionState.draggedElement != nil and IsMouseButtonDown(MOUSE_BUTTON_LEFT):
      handleDragMove(backend.rootElement, mouseX, mouseY)
      if globalInteractionState.dragHasMoved and not globalInteractionState.isLiveReordering:
        var container = globalInteractionState.draggedElement.parent
        while container != nil and not hasDropTargetBehavior(container):
          container = container.parent
        if container != nil:
          discard initTabReorder(container, globalInteractionState.draggedElement)
      if globalInteractionState.isLiveReordering and
         globalInteractionState.potentialDropTarget != nil and
         globalInteractionState.potentialDropTarget.kind == ekTabBar:
        updateTabInsertIndex(globalInteractionState.potentialDropTarget, mouseX)
        updateLiveTabOrder()

    if globalInteractionState.draggedElement != nil and IsMouseButtonReleased(MOUSE_BUTTON_LEFT):
      let draggedElem = globalInteractionState.draggedElement
      let savedInsertIndex = globalInteractionState.dragInsertIndex
      let dropTargetBeforeDrop = globalInteractionState.potentialDropTarget
      let dropSuccessful = handleDragEnd()
      var afterInteraction = getInteractionState()
      if afterInteraction.isLiveReordering:
        afterInteraction.dragInsertIndex = savedInsertIndex
        var shouldCommit = dropSuccessful
        if not shouldCommit and dropTargetBeforeDrop != nil:
          shouldCommit = dropTargetBeforeDrop == afterInteraction.reorderableContainer
        finalizeTabReorder(draggedElem, shouldCommit)

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

    let interaction = getInteractionState()
    if isPressed and not interaction.isLiveReordering:
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
    renderElement(backend, backend.state, root)

    # Render dropdown menus on top of everything else
    renderDropdownMenus(backend, backend.state, root)

    # Render drag-and-drop visual effects on top
    renderDragAndDropEffects(backend, root)

    EndDrawing()

  # Cleanup
  UnloadFont(backend.font)
  layoutFontInitialized = false
  CloseWindow()
  backend.running = false
