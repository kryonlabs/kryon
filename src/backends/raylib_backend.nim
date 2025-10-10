## Raylib Backend for Kryon
##
## This backend renders Kryon UI elements using Raylib for native desktop applications.

import ../kryon/core
import options, tables
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
    focusedInput*: Element  # Currently focused input element
    cursorBlink*: float     # Timer for cursor blinking
    inputValues*: Table[Element, string]  # Store current text for each input
    checkboxStates*: Table[Element, bool]  # Store checked state for each checkbox
    inputScrollOffsets*: Table[Element, float]  # Store scroll offset for each input
    backspaceHoldTimer*: float  # Timer for backspace repeat
    backspaceRepeatDelay*: float  # Initial delay before repeat starts
    backspaceRepeatRate*: float   # Rate of repeat deletion

proc newRaylibBackend*(width, height: int, title: string): RaylibBackend =
  ## Create a new Raylib backend
  result = RaylibBackend(
    windowWidth: width,
    windowHeight: height,
    windowTitle: title,
    backgroundColor: rgba(30, 30, 30, 255),
    running: false,
    inputValues: initTable[Element, string](),
    checkboxStates: initTable[Element, bool](),
    inputScrollOffsets: initTable[Element, float](),
    backspaceHoldTimer: 0.0,
    backspaceRepeatDelay: 0.5,  # 500ms initial delay
    backspaceRepeatRate: 0.05    # 50ms repeat rate
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
    inputValues: initTable[Element, string](),
    checkboxStates: initTable[Element, bool](),
    inputScrollOffsets: initTable[Element, float](),
    backspaceHoldTimer: 0.0,
    backspaceRepeatDelay: 0.5,  # 500ms initial delay
    backspaceRepeatRate: 0.05    # 50ms repeat rate
  )

# ============================================================================
# Color Conversion
# ============================================================================

proc toRaylibColor*(c: Color): RColor {.inline.} =
  ## Convert Kryon Color to Raylib RColor
  rcolor(c.r, c.g, c.b, c.a)

# ============================================================================
# Layout Engine
# ============================================================================

proc calculateLayout*(elem: Element, x, y, parentWidth, parentHeight: float) =
  ## Recursively calculate layout for all elements

  # Check for absolute positioning (posX, posY)
  let posXOpt = elem.getProp("posX")
  let posYOpt = elem.getProp("posY")

  # Get element dimensions
  let widthOpt = elem.getProp("width")
  let heightOpt = elem.getProp("height")

  # Set position (use posX/posY if provided, otherwise use x/y)
  elem.x = if posXOpt.isSome: posXOpt.get.getFloat() else: x
  elem.y = if posYOpt.isSome: posYOpt.get.getFloat() else: y

  # Special handling for Text elements - measure actual text size
  if elem.kind == ekText:
    let text = elem.getProp("text").get(val("")).getString()
    let fontSize = elem.getProp("fontSize").get(val(20)).getInt()

    # Measure text dimensions
    let textWidth = MeasureText(text.cstring, fontSize.cint).float
    let textHeight = fontSize.float

    elem.width = if widthOpt.isSome: widthOpt.get.getFloat() else: textWidth
    elem.height = if heightOpt.isSome: heightOpt.get.getFloat() else: textHeight
  else:
    # Regular element dimension handling
    if widthOpt.isSome:
      elem.width = widthOpt.get.getFloat()
    else:
      elem.width = parentWidth

    if heightOpt.isSome:
      elem.height = heightOpt.get.getFloat()
    else:
      elem.height = parentHeight

  # Layout children based on container type
  # Children should be positioned relative to THIS element's calculated position
  case elem.kind:
  of ekHeader:
    # Header is metadata only - skip layout
    discard

  of ekBody:
    # Body is a wrapper - layout children in same space
    for child in elem.children:
      calculateLayout(child, elem.x, elem.y, elem.width, elem.height)

  of ekColumn:
    var currentY = elem.y
    let gap = elem.getProp("gap").get(val(0)).getFloat()

    for child in elem.children:
      calculateLayout(child, elem.x, currentY, elem.width, elem.height)
      currentY += child.height + gap

  of ekRow:
    var currentX = elem.x
    let gap = elem.getProp("gap").get(val(0)).getFloat()

    for child in elem.children:
      calculateLayout(child, currentX, elem.y, elem.width, elem.height)
      currentX += child.width + gap

  of ekCenter:
    # Center multiple children as a group - needs multi-pass layout
    if elem.children.len > 0:
      # First pass: Calculate all children sizes at (0, 0) to determine dimensions
      var totalHeight = 0.0
      var maxWidth = 0.0
      var childSizes: seq[tuple[w, h: float]] = @[]

      for child in elem.children:
        calculateLayout(child, 0, 0, elem.width, elem.height)
        childSizes.add((w: child.width, h: child.height))
        totalHeight += child.height
        maxWidth = max(maxWidth, child.width)

      # Calculate starting Y position to center the group vertically
      let startY = elem.y + (elem.height - totalHeight) / 2.0

      # Second pass: Position children centered horizontally and stacked vertically
      var currentY = startY
      let gap = elem.getProp("gap").get(val(10)).getFloat()  # Default gap of 10

      for i, child in elem.children:
        let centerX = elem.x + (elem.width - childSizes[i].w) / 2.0

        # Apply gap between elements (but not after the last one)
        if i > 0:
          currentY += gap

        calculateLayout(child, centerX, currentY, childSizes[i].w, childSizes[i].h)
        currentY += childSizes[i].h

  of ekContainer:
    # Check for contentAlignment property
    let alignment = elem.getProp("contentAlignment")
    if alignment.isSome and alignment.get.getString() == "center":
      # Center children - needs two-pass layout
      for child in elem.children:
        # First pass: Calculate child layout at (0, 0) to determine its size
        calculateLayout(child, 0, 0, elem.width, elem.height)

        # Second pass: Now we know child.width and child.height, so center it
        let centerX = elem.x + (elem.width - child.width) / 2.0
        let centerY = elem.y + (elem.height - child.height) / 2.0

        # Recalculate at centered position
        calculateLayout(child, centerX, centerY, child.width, child.height)
    else:
      # Default: layout children in same space as parent (inside the container)
      for child in elem.children:
        calculateLayout(child, elem.x, elem.y, elem.width, elem.height)

  else:
    # Default: just layout children in same space as parent
    for child in elem.children:
      calculateLayout(child, elem.x, elem.y, elem.width, elem.height)

# ============================================================================
# Rendering with Real Raylib
# ============================================================================

proc renderElement*(backend: var RaylibBackend, elem: Element) =
  ## Render an element using Raylib

  case elem.kind:
  of ekHeader:
    # Header is metadata only - don't render it
    discard

  of ekBody:
    # Body is a wrapper - render its children
    for child in elem.children:
      backend.renderElement(child)

  of ekContainer:
    let rect = rrect(elem.x, elem.y, elem.width, elem.height)

    # Draw background
    let bgColor = elem.getProp("backgroundColor")
    if bgColor.isSome:
      DrawRectangleRec(rect, bgColor.get.getColor().toRaylibColor())

    # Draw border
    let borderColor = elem.getProp("borderColor")
    if borderColor.isSome:
      let borderWidth = elem.getProp("borderWidth").get(val(1)).getFloat()
      DrawRectangleLinesEx(rect, borderWidth, borderColor.get.getColor().toRaylibColor())

  of ekText:
    let text = elem.getProp("text").get(val("")).getString()
    let color = elem.getProp("color").get(val("#FFFFFF")).getColor()
    let fontSize = elem.getProp("fontSize").get(val(20)).getInt()

    DrawText(text.cstring, elem.x.cint, elem.y.cint, fontSize.cint, color.toRaylibColor())

  of ekButton:
    let text = elem.getProp("text").get(val("Button")).getString()
    let bgColor = elem.getProp("backgroundColor").get(val("#4A90E2"))
    let textColor = elem.getProp("color").get(val("#FFFFFF")).getColor()
    let fontSize = elem.getProp("fontSize").get(val(20)).getInt()

    # Draw button background
    let rect = rrect(elem.x, elem.y, elem.width, elem.height)
    DrawRectangleRec(rect, bgColor.getColor().toRaylibColor())

    # Draw button border
    DrawRectangleLinesEx(rect, 2.0, DARKGRAY)

    # Center text in button
    let textWidth = MeasureText(text.cstring, fontSize.cint)
    let textX = elem.x + (elem.width - textWidth.float) / 2.0
    let textY = elem.y + (elem.height - fontSize.float) / 2.0

    DrawText(text.cstring, textX.cint, textY.cint, fontSize.cint, textColor.toRaylibColor())

  of ekInput:
    # Get current value from backend state, or use initial value from property
    let value = if backend.inputValues.hasKey(elem):
      backend.inputValues[elem]
    else:
      elem.getProp("value").get(val("")).getString()

    let placeholder = elem.getProp("placeholder").get(val("")).getString()
    let fontSize = elem.getProp("fontSize").get(val(20)).getInt()
    let bgColor = elem.getProp("backgroundColor").get(val("#FFFFFF"))
    let textColor = elem.getProp("color").get(val("#000000")).getColor()
    let borderColor = if backend.focusedInput == elem:
      parseColor("#4A90E2")  # Blue when focused
    else:
      parseColor("#CCCCCC")  # Gray when not focused

    # Draw input background
    let rect = rrect(elem.x, elem.y, elem.width, elem.height)
    DrawRectangleRec(rect, bgColor.getColor().toRaylibColor())

    # Draw border (thicker if focused)
    let borderWidth = if backend.focusedInput == elem: 2.0 else: 1.0
    DrawRectangleLinesEx(rect, borderWidth, borderColor.toRaylibColor())

    # Text rendering with scrolling and clipping
    let padding = 8.0
    let displayText = if value.len > 0: value else: placeholder
    let displayColor = if value.len > 0: textColor else: parseColor("#999999")

    # Calculate text dimensions (textWidth not needed for scrolling logic)
    # let textWidth = if displayText.len > 0:
    #   MeasureText(displayText.cstring, fontSize.cint).float
    # else:
    #   0.0

    # Calculate cursor position in text (for focused input)
    let cursorTextPos = if value.len > 0 and backend.focusedInput == elem:
      # Measure text up to cursor position (cursor is at end for now)
      MeasureText(value.cstring, fontSize.cint).float
    else:
      0.0

    # Get or initialize scroll offset for this input
    var scrollOffset = backend.inputScrollOffsets.getOrDefault(elem, 0.0)

    # Calculate visible text area
    let visibleWidth = elem.width - (padding * 2.0)

    # Adjust scroll offset to keep cursor visible
    if backend.focusedInput == elem:
      # Calculate total text width to ensure we don't scroll past the end
      let totalTextWidth = if value.len > 0:
        MeasureText(value.cstring, fontSize.cint).float
      else:
        0.0

      # If cursor is approaching right edge, scroll to keep it visible
      if cursorTextPos - scrollOffset > visibleWidth - 20.0:
        scrollOffset = cursorTextPos - visibleWidth + 20.0

      # If cursor is approaching left edge, scroll left
      if cursorTextPos < scrollOffset + 20.0:
        scrollOffset = max(0.0, cursorTextPos - 20.0)

      # Don't scroll past the beginning or allow scrolling beyond text end
      scrollOffset = max(0.0, scrollOffset)

      # If text is shorter than visible area, reset scroll to 0
      if totalTextWidth <= visibleWidth:
        scrollOffset = 0.0
      else:
        # Don't allow scrolling past the end of the text
        let maxScrollOffset = max(0.0, totalTextWidth - visibleWidth)
        scrollOffset = min(scrollOffset, maxScrollOffset)

      # Store updated scroll offset
      backend.inputScrollOffsets[elem] = scrollOffset

    # Set up clipping to prevent text overflow
    let clipRect = rrect(elem.x + padding, elem.y + 2.0, elem.width - (padding * 2.0), elem.height - 4.0)
    BeginScissorMode(clipRect.x.cint, clipRect.y.cint, clipRect.width.cint, clipRect.height.cint)

    # Draw text with scroll offset
    if displayText.len > 0:
      let textX = elem.x + padding - scrollOffset
      let textY = elem.y + (elem.height - fontSize.float) / 2.0
      DrawText(displayText.cstring, textX.cint, textY.cint, fontSize.cint, displayColor.toRaylibColor())

    # End clipping
    EndScissorMode()

    # Draw cursor if focused and visible
    if backend.focusedInput == elem and backend.cursorBlink < 0.5:
      let cursorX = elem.x + padding + cursorTextPos - scrollOffset
      let cursorY = elem.y + 4.0

      # Only draw cursor if it's within the visible area
      if cursorX >= elem.x + padding and cursorX <= elem.x + elem.width - padding:
        DrawRectangle(cursorX.cint, cursorY.cint, 1, fontSize.cint - 2, textColor.toRaylibColor())

  of ekColumn, ekRow, ekCenter:
    # Layout containers don't render themselves, just their children
    discard

  else:
    # Unsupported element - skip
    discard

  # Render children
  for child in elem.children:
    backend.renderElement(child)

# ============================================================================
# Input Handling
# ============================================================================

proc checkHoverCursor*(elem: Element): bool =
  ## Check if mouse is hovering over any interactive element that needs pointer cursor
  ## Returns true if pointer cursor should be shown

  case elem.kind:
  of ekButton:
    let mousePos = GetMousePosition()
    let rect = rrect(elem.x, elem.y, elem.width, elem.height)
    if CheckCollisionPointRec(mousePos, rect):
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

  of ekBody:
    # Body is a wrapper - handle children's input
    for child in elem.children:
      backend.handleInput(child)

  of ekButton:
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
        backend.focusedInput = elem
        backend.cursorBlink = 0.0  # Reset cursor blink

        # Initialize input value if not already present
        if not backend.inputValues.hasKey(elem):
          let initialValue = elem.getProp("value").get(val("")).getString()
          backend.inputValues[elem] = initialValue
      else:
        # Clicked outside - unfocus if this was focused
        if backend.focusedInput == elem:
          backend.focusedInput = nil

  of ekColumn, ekRow, ekCenter, ekContainer:
    # Layout containers - only handle children's input, don't process children twice
    for child in elem.children:
      backend.handleInput(child)

  else:
    # Other element types - just check children
    for child in elem.children:
      backend.handleInput(child)

proc handleKeyboardInput*(backend: var RaylibBackend) =
  ## Handle keyboard input for focused input element
  if backend.focusedInput == nil:
    backend.backspaceHoldTimer = 0.0  # Reset timer when no focus
    return

  # Get current value
  var currentValue = backend.inputValues.getOrDefault(backend.focusedInput, "")
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
      backend.backspaceHoldTimer = 0.0  # Reset backspace timer when typing

  # Handle backspace with repeat logic
  let backspacePressed = IsKeyDown(KEY_BACKSPACE)
  if backspacePressed and currentValue.len > 0:
    if IsKeyPressed(KEY_BACKSPACE):
      # First press - delete one character immediately
      currentValue.setLen(currentValue.len - 1)
      textChanged = true
      backend.backspaceHoldTimer = 0.0  # Start the hold timer
    else:
      # Key is being held down
      backend.backspaceHoldTimer += 1.0 / 60.0  # Increment by frame time (~16ms at 60fps)

      # Check if we should delete more characters
      if backend.backspaceHoldTimer >= backend.backspaceRepeatDelay:
        # Calculate how many characters to delete based on hold time
        let holdBeyondDelay = backend.backspaceHoldTimer - backend.backspaceRepeatDelay
        let charsToDelete = min(int(holdBeyondDelay / backend.backspaceRepeatRate), currentValue.len)

        if charsToDelete > 0:
          currentValue.setLen(currentValue.len - charsToDelete)
          textChanged = true
          # Adjust timer to maintain repeat rate
          backend.backspaceHoldTimer = backend.backspaceRepeatDelay +
                                      (charsToDelete.float * backend.backspaceRepeatRate)
  else:
    # Backspace not pressed - reset timer
    backend.backspaceHoldTimer = 0.0

  # Handle other special keys
  if IsKeyPressed(KEY_ENTER):
    # Trigger onSubmit handler if present
    if backend.focusedInput.eventHandlers.hasKey("onSubmit"):
      backend.focusedInput.eventHandlers["onSubmit"]()

  # Update stored value if changed
  if textChanged:
    backend.inputValues[backend.focusedInput] = currentValue

  # Trigger onChange handler if present
  if textChanged and backend.focusedInput.eventHandlers.hasKey("onChange"):
    backend.focusedInput.eventHandlers["onChange"]()

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

  # Calculate initial layout
  calculateLayout(root, 0, 0, backend.windowWidth.float, backend.windowHeight.float)

  # Main game loop
  while not WindowShouldClose():
    # Update cursor blink timer
    backend.cursorBlink += 1.0 / 60.0  # Assuming 60 FPS
    if backend.cursorBlink >= 1.0:
      backend.cursorBlink = 0.0

    # Update mouse cursor based on hover state
    if checkHoverCursor(root):
      SetMouseCursor(MOUSE_CURSOR_POINTING_HAND)
    else:
      SetMouseCursor(MOUSE_CURSOR_DEFAULT)

    # Handle mouse input
    backend.handleInput(root)

    # Handle keyboard input
    backend.handleKeyboardInput()

    # Render
    BeginDrawing()
    ClearBackground(backend.backgroundColor.toRaylibColor())

    backend.renderElement(root)

    EndDrawing()

  # Cleanup
  CloseWindow()
  backend.running = false
