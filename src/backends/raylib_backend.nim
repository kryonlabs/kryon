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

proc newRaylibBackend*(width, height: int, title: string): RaylibBackend =
  ## Create a new Raylib backend
  result = RaylibBackend(
    windowWidth: width,
    windowHeight: height,
    windowTitle: title,
    backgroundColor: rgba(30, 30, 30, 255),
    running: false,
    inputValues: initTable[Element, string](),
    checkboxStates: initTable[Element, bool]()
  )

proc newRaylibBackendFromApp*(app: Element): RaylibBackend =
  ## Create backend from app element (extracts windowWidth, windowHeight, windowTitle, etc.)
  let width = app.getProp("windowWidth").get(val(800)).getInt()
  let height = app.getProp("windowHeight").get(val(600)).getInt()
  let title = app.getProp("windowTitle").get(val("Kryon App")).getString()
  let bgColor = app.getProp("backgroundColor")

  result = RaylibBackend(
    windowWidth: width,
    windowHeight: height,
    windowTitle: title,
    backgroundColor: if bgColor.isSome: bgColor.get.getColor() else: rgba(30, 30, 30, 255),
    running: false,
    inputValues: initTable[Element, string](),
    checkboxStates: initTable[Element, bool]()
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
    # Default: just layout children in same space as parent (inside the container)
    for child in elem.children:
      calculateLayout(child, elem.x, elem.y, elem.width, elem.height)

# ============================================================================
# Rendering with Real Raylib
# ============================================================================

proc renderElement*(backend: var RaylibBackend, elem: Element) =
  ## Render an element using Raylib

  case elem.kind:
  of ekContainer:
    let bgColor = elem.getProp("backgroundColor")
    if bgColor.isSome:
      let rect = rrect(elem.x, elem.y, elem.width, elem.height)
      DrawRectangleRec(rect, bgColor.get.getColor().toRaylibColor())

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

    # Draw text or placeholder
    let displayText = if value.len > 0: value else: placeholder
    let displayColor = if value.len > 0: textColor else: parseColor("#999999")

    if displayText.len > 0:
      let padding = 8.0
      DrawText(displayText.cstring, (elem.x + padding).cint, (elem.y + padding).cint,
               fontSize.cint, displayColor.toRaylibColor())

    # Draw cursor if focused
    if backend.focusedInput == elem and backend.cursorBlink < 0.5:
      let padding = 8.0
      let textWidth = if value.len > 0:
        MeasureText(value.cstring, fontSize.cint).float
      else:
        0.0
      let cursorX = elem.x + padding + textWidth
      let cursorY = elem.y + padding
      DrawRectangle(cursorX.cint, cursorY.cint, 2, fontSize.cint, textColor.toRaylibColor())

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

proc handleInput*(backend: var RaylibBackend, elem: Element) =
  ## Handle mouse input for interactive elements

  case elem.kind:
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

  else:
    discard

  # Check children for input
  for child in elem.children:
    backend.handleInput(child)

proc handleKeyboardInput*(backend: var RaylibBackend) =
  ## Handle keyboard input for focused input element
  if backend.focusedInput == nil:
    return

  # Get current value
  var currentValue = backend.inputValues.getOrDefault(backend.focusedInput, "")

  # Handle character input
  while true:
    let char = GetCharPressed()
    if char == 0:
      break
    # Add printable characters
    if char >= 32 and char < 127:
      currentValue.add(char.chr)

  # Handle special keys
  if IsKeyPressed(KEY_BACKSPACE) and currentValue.len > 0:
    currentValue.setLen(currentValue.len - 1)

  if IsKeyPressed(KEY_ENTER):
    # Trigger onSubmit handler if present
    if backend.focusedInput.eventHandlers.hasKey("onSubmit"):
      backend.focusedInput.eventHandlers["onSubmit"]()

  # Update stored value
  backend.inputValues[backend.focusedInput] = currentValue

  # Trigger onChange handler if present
  if backend.focusedInput.eventHandlers.hasKey("onChange"):
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
