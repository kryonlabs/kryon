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

proc newRaylibBackend*(width, height: int, title: string): RaylibBackend =
  ## Create a new Raylib backend
  result = RaylibBackend(
    windowWidth: width,
    windowHeight: height,
    windowTitle: title,
    backgroundColor: rgba(30, 30, 30, 255),
    running: false
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

  # Get element dimensions
  let widthOpt = elem.getProp("width")
  let heightOpt = elem.getProp("height")

  elem.x = x
  elem.y = y

  if widthOpt.isSome:
    elem.width = widthOpt.get.getFloat()
  else:
    elem.width = parentWidth

  if heightOpt.isSome:
    elem.height = heightOpt.get.getFloat()
  else:
    elem.height = parentHeight

  # Layout children based on container type
  case elem.kind:
  of ekColumn:
    var currentY = y
    let gap = elem.getProp("gap").get(val(0)).getFloat()

    for child in elem.children:
      calculateLayout(child, x, currentY, elem.width, elem.height)
      currentY += child.height + gap

  of ekRow:
    var currentX = x
    let gap = elem.getProp("gap").get(val(0)).getFloat()

    for child in elem.children:
      calculateLayout(child, currentX, y, elem.width, elem.height)
      currentX += child.width + gap

  of ekCenter:
    # Center children
    for child in elem.children:
      # First calculate child's own size
      var childWidth, childHeight: float

      let childWidthOpt = child.getProp("width")
      let childHeightOpt = child.getProp("height")

      if childWidthOpt.isSome:
        childWidth = childWidthOpt.get.getFloat()
      else:
        childWidth = elem.width

      if childHeightOpt.isSome:
        childHeight = childHeightOpt.get.getFloat()
      else:
        childHeight = elem.height

      let centerX = x + (elem.width - childWidth) / 2.0
      let centerY = y + (elem.height - childHeight) / 2.0
      calculateLayout(child, centerX, centerY, childWidth, childHeight)

  else:
    # Default: just layout children in same space
    for child in elem.children:
      calculateLayout(child, x, y, elem.width, elem.height)

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

  else:
    discard

  # Check children for input
  for child in elem.children:
    backend.handleInput(child)

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
    # Handle input
    backend.handleInput(root)

    # Render
    BeginDrawing()
    ClearBackground(backend.backgroundColor.toRaylibColor())

    backend.renderElement(root)

    EndDrawing()

  # Cleanup
  CloseWindow()
  backend.running = false
