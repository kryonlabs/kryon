## Raylib Backend for Kryon
##
## This backend renders Kryon UI elements using Raylib (via naylib) for native desktop applications.

import raylib_config  # Must be imported BEFORE raylib to enable image format support
import raylib
import ../../kryon/core
import ../../kryon/fonts
import ../../kryon/components/canvas
import ../../kryon/layout/layoutEngine
import ../../kryon/layout/zindexSort
import ../../kryon/state/backendState
import ../../kryon/rendering/renderingContext
import ../../kryon/interactions/dragDrop
import ../../kryon/interactions/tabReordering
import ../../kryon/interactions/interactionState
import ../../kryon/pipeline/processor
import ../../kryon/pipeline/renderCommands
import options, tables, math, os, times, sets

# ============================================================================
# Backend Type
# ============================================================================

type
  RaylibBackend* = object
    windowWidth*: int
    windowHeight*: int
    windowTitle*: string
    backgroundColor*: core.Color
    running*: bool
    rootElement*: Element
    font*: raylib.Font           # Raylib font for rendering
    state*: BackendState  # Shared state for all interactive elements
    textureCache*: Table[string, raylib.Texture2D]  # Cache loaded textures

proc newRaylibBackend*(width, height: int, title: string): RaylibBackend =
  ## Create a new Raylib backend
  result = RaylibBackend(
    windowWidth: width,
    windowHeight: height,
    windowTitle: title,
    backgroundColor: rgba(30, 30, 30, 255),
    running: false,
    state: newBackendState(),
    textureCache: initTable[string, raylib.Texture2D]()
  )

proc newRaylibBackendFromApp*(app: Element): RaylibBackend =
  ## Create backend from app element (extracts config from Header and Body)
  var width = 800
  var height = 600
  var title = "Kryon App"
  var bgColor: Option[Value] = none(Value)

  # Look for Header and Body children in app
  for child in app.children:
    if child.kind == ekHeader:
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
      bgColor = child.getProp("backgroundColor")

  result = RaylibBackend(
    windowWidth: width,
    windowHeight: height,
    windowTitle: title,
    backgroundColor: if bgColor.isSome: bgColor.get.getColor() else: rgba(30, 30, 30, 255),
    running: false,
    state: newBackendState(),
    textureCache: initTable[string, raylib.Texture2D]()
  )

# ============================================================================
# Font Management for Layout
# ============================================================================

var
  layoutFont: ptr raylib.Font
  layoutFontInitialized = false

proc setLayoutFont*(font: var raylib.Font) =
  ## Cache the active font for layout measurements
  layoutFont = addr font
  layoutFontInitialized = true

proc measureLayoutTextWidth(text: string, fontSize: int): float =
  ## Measure text width for layout calculations using the cached font
  if text.len == 0:
    return 0.0
  if layoutFontInitialized:
    let size = measureText(layoutFont[], text, fontSize.float32, 0.0)
    return size.x
  else:
    return measureText(text, fontSize.int32).float

proc measureTextWidth*(backend: RaylibBackend, text: string, fontSize: int): float =
  ## Measure text width using the backend's active font
  if text.len == 0:
    return 0.0
  let size = measureText(backend.font, text, fontSize.float32, 0.0)
  return size.x

# ============================================================================
# TextMeasurer Interface Implementation
# ============================================================================

proc measureText*(backend: RaylibBackend, text: string, fontSize: int): tuple[width: float, height: float] =
  ## Implement TextMeasurer interface for layout engine
  if text.len == 0:
    return (width: 0.0, height: fontSize.float)
  let size = measureText(backend.font, text, fontSize.float32, 0.0)
  return (width: size.x, height: size.y)

# ============================================================================
# Color Conversion
# ============================================================================

proc toRaylibColor*(c: core.Color): raylib.Color {.inline.} =
  ## Convert Kryon Color to Raylib Color
  ## Both have same structure (r,g,b,a: uint8), so we can cast
  raylib.Color(r: c.r, g: c.g, b: c.b, a: c.a)

proc toKryonColor*(c: raylib.Color): core.Color {.inline.} =
  ## Convert Raylib Color to Kryon Color
  core.Color(r: c.r, g: c.g, b: c.b, a: c.a)

# ============================================================================
# Helper constructors
# ============================================================================

proc vec2*(x, y: float): raylib.Vector2 {.inline.} =
  raylib.Vector2(x: x.float32, y: y.float32)

proc rect*(x, y, width, height: float): raylib.Rectangle {.inline.} =
  raylib.Rectangle(x: x.float32, y: y.float32, width: width.float32, height: height.float32)

# ============================================================================
# Renderer Interface Implementation
# ============================================================================

proc drawRectangle*(backend: var RaylibBackend, x, y, width, height: float, color: core.Color) =
  ## Draw a filled rectangle
  drawRectangle(rect(x, y, width, height), color.toRaylibColor())

proc drawRectangleBorder*(backend: var RaylibBackend, x, y, width, height, borderWidth: float, color: core.Color) =
  ## Draw a rectangle border
  drawRectangleLines(rect(x, y, width, height), borderWidth.float32, color.toRaylibColor())

proc drawText*(backend: var RaylibBackend, text: string, x, y: float, fontSize: int, color: core.Color) =
  ## Draw text using the backend's font
  drawText(backend.font, text, vec2(x, y), fontSize.float32, 0.0, color.toRaylibColor())

proc drawLine*(backend: var RaylibBackend, x1, y1, x2, y2, thickness: float, color: core.Color) =
  ## Draw a line
  if thickness <= 1.0:
    raylib.drawLine(x1.int32, y1.int32, x2.int32, y2.int32, color.toRaylibColor())
  else:
    # For thicker lines, draw a rectangle
    if abs(x2 - x1) > abs(y2 - y1):
      # More horizontal
      drawRectangle(backend, min(x1, x2), y1 - thickness / 2, abs(x2 - x1), thickness, color)
    else:
      # More vertical
      drawRectangle(backend, x1 - thickness / 2, min(y1, y2), thickness, abs(y2 - y1), color)

proc drawImage*(backend: var RaylibBackend, imagePath: string, x, y, width, height: float) =
  ## Draw an image at specified position with given dimensions
  if not fileExists(imagePath):
    # File doesn't exist, draw placeholder
    drawRectangle(backend, x, y, width, height, rgba(204, 204, 204, 255))
    drawText(backend, "X", x + width/2 - 8, y + height/2 - 8, 16, rgba(255, 0, 0, 255))
    return

  # Check if texture is already cached
  if not backend.textureCache.hasKey(imagePath):
    try:
      let texture = loadTexture(imagePath)
      backend.textureCache[imagePath] = texture
      echo "Loaded and cached texture: ", imagePath
    except:
      # Failed to load image, draw placeholder
      echo "Failed to load image: ", imagePath, " - ", getCurrentExceptionMsg()
      drawRectangle(backend, x, y, width, height, rgba(204, 204, 204, 255))
      drawText(backend, "X", x + width/2 - 8, y + height/2 - 8, 16, rgba(255, 0, 0, 255))
      return

  # Draw the cached texture
  # Access width/height directly to avoid copying the Texture
  let texWidth = backend.textureCache[imagePath].width.float
  let texHeight = backend.textureCache[imagePath].height.float
  let sourceRect = rect(0, 0, texWidth, texHeight)
  let destRect = rect(x, y, width, height)
  drawTexture(backend.textureCache[imagePath], sourceRect, destRect, vec2(0, 0), 0, raylib.White)

proc beginClipping*(backend: var RaylibBackend, x, y, width, height: float) =
  ## Begin clipping region (scissor mode)
  beginScissorMode(x.int32, y.int32, width.int32, height.int32)

proc endClipping*(backend: var RaylibBackend) =
  ## End clipping region
  endScissorMode()

# ============================================================================
# Canvas Drawing Functions
# ============================================================================

# Forward declarations
proc executeCommand*(backend: var RaylibBackend, cmd: renderCommands.RenderCommand)
proc executeCanvasCommand*(backend: var RaylibBackend, cmd: renderCommands.RenderCommand, canvasX, canvasY: float)

proc drawCanvas*(backend: var RaylibBackend, x, y, width, height: float,
                 drawProc: proc(ctx: DrawingContext, width, height: float) {.closure.},
                 backgroundColor: Option[core.Color]) =
  ## Draw a canvas with custom rendering using proper render command pipeline
  # Clear background if specified
  if backgroundColor.isSome:
    let bgColor = backgroundColor.get()
    raylib.drawRectangle(x.int32, y.int32, width.int32, height.int32,
                         raylib.Color(r: bgColor.r, g: bgColor.g, b: bgColor.b, a: bgColor.a))

  # Create a temporary render command list for canvas operations
  var canvasCommands: seq[renderCommands.RenderCommand] = @[]

  # Create drawing context and connect it to the render command list
  let ctx = newDrawingContext()
  ctx.renderCommands = addr canvasCommands

  # Store canvas position in backendData for coordinate translation (if needed)
  ctx.backendData = cast[pointer](alloc(sizeof(tuple[x: float, y: float])))
  cast[ptr tuple[x: float, y: float]](ctx.backendData)[] = (x: x, y: y)

  # Set default values
  ctx.fillStyle = rgba(0.uint8, 0.uint8, 0.uint8, 255.uint8)
  ctx.strokeStyle = rgba(0.uint8, 0.uint8, 0.uint8, 255.uint8)
  ctx.lineWidth = 1.0

  # Enable scissor mode to clip drawing to canvas area
  raylib.beginScissorMode(x.int32, y.int32, width.int32, height.int32)

  # Execute the user's drawing procedure - this will add render commands to canvasCommands
  if drawProc != nil:
    drawProc(ctx, width, height)

  # Execute all canvas render commands with coordinate translation
  for cmd in canvasCommands:
    backend.executeCanvasCommand(cmd, x, y)

  # End clipping region
  raylib.endScissorMode()

  # Clean up
  dealloc(ctx.backendData)

proc drawPath*(backend: var RaylibBackend, commands: seq[PathCommand],
               fillStyle: core.Color, strokeStyle: core.Color, lineWidth: float,
               shouldFill: bool, shouldStroke: bool) =
  ## Draw a path using raylib
  if commands.len == 0:
    return

  # Collect all points from the path
  var points: seq[raylib.Vector2] = @[]
  var currentX, currentY: float = 0.0
  var hasArc = false
  var isClosed = false

  for cmd in commands:
    case cmd.kind
    of PathCommandKind.MoveTo:
      currentX = cmd.moveToX
      currentY = cmd.moveToY
      points.add(raylib.Vector2(x: currentX.float32, y: currentY.float32))
    of PathCommandKind.LineTo:
      currentX = cmd.lineToX
      currentY = cmd.lineToY
      points.add(raylib.Vector2(x: currentX.float32, y: currentY.float32))
    of PathCommandKind.Arc:
      # Handle arcs - draw arc sectors with proper start/end angles
      hasArc = true
      # Convert radians to degrees (Raylib uses degrees, Canvas API uses radians)
      let startAngleDeg = (cmd.arcStartAngle * 180.0 / PI).float32
      let endAngleDeg = (cmd.arcEndAngle * 180.0 / PI).float32
      let center = raylib.Vector2(x: cmd.arcX.float32, y: cmd.arcY.float32)
      let segments = 36.int32  # Good quality for smooth arcs

      if shouldFill:
        raylib.drawCircleSector(
          center, cmd.arcRadius.float32,
          startAngleDeg, endAngleDeg, segments,
          raylib.Color(r: fillStyle.r, g: fillStyle.g, b: fillStyle.b, a: fillStyle.a)
        )
      if shouldStroke:
        raylib.drawCircleSectorLines(
          center, cmd.arcRadius.float32,
          startAngleDeg, endAngleDeg, segments,
          raylib.Color(r: strokeStyle.r, g: strokeStyle.g, b: strokeStyle.b, a: strokeStyle.a)
        )
    of PathCommandKind.BezierCurveTo:
      # Simple approximation: just add the end point
      # A full implementation would sample points along the curve
      currentX = cmd.bezierX
      currentY = cmd.bezierY
      points.add(raylib.Vector2(x: currentX.float32, y: currentY.float32))
    of PathCommandKind.ClosePath:
      # Mark path as closed
      isClosed = true

  # Draw the collected points as a polygon
  if points.len >= 3 and not hasArc:
    if shouldFill:
      # Check if this is a simple rectangle (4 points forming axis-aligned rect)
      if points.len == 4:
        # Use Raylib's optimized rectangle drawing
        let minX = min(min(points[0].x, points[1].x), min(points[2].x, points[3].x))
        let minY = min(min(points[0].y, points[1].y), min(points[2].y, points[3].y))
        let maxX = max(max(points[0].x, points[1].x), max(points[2].x, points[3].x))
        let maxY = max(max(points[0].y, points[1].y), max(points[2].y, points[3].y))
        raylib.drawRectangle(
          minX.int32, minY.int32, (maxX - minX).int32, (maxY - minY).int32,
          raylib.Color(r: fillStyle.r, g: fillStyle.g, b: fillStyle.b, a: fillStyle.a)
        )
      else:
        # Draw filled polygon using triangle fan for other shapes
        raylib.drawTriangleFan(
          points,
          raylib.Color(r: fillStyle.r, g: fillStyle.g, b: fillStyle.b, a: fillStyle.a)
        )

    if shouldStroke:
      # Draw outline by connecting consecutive points
      # Note: lineWidth is currently ignored due to raylib binding limitations
      let lineCount = if isClosed: points.len else: points.len - 1
      for i in 0..<lineCount:
        let nextI = if isClosed: (i + 1) mod points.len else: i + 1
        raylib.drawLine(
          points[i].x.int32, points[i].y.int32,
          points[nextI].x.int32, points[nextI].y.int32,
          raylib.Color(r: strokeStyle.r, g: strokeStyle.g, b: strokeStyle.b, a: strokeStyle.a)
        )
  elif points.len == 2:
    # Just a line
    if shouldStroke:
      raylib.drawLine(
        points[0].x.int32, points[0].y.int32,
        points[1].x.int32, points[1].y.int32,
        raylib.Color(r: strokeStyle.r, g: strokeStyle.g, b: strokeStyle.b, a: strokeStyle.a)
      )

proc clearCanvas*(backend: var RaylibBackend, x, y, width, height: float, color: core.Color) =
  ## Clear a canvas area with the specified color
  raylib.drawRectangle(
    x.int32, y.int32, width.int32, height.int32,
    raylib.Color(r: color.r, g: color.g, b: color.b, a: color.a)
  )

# ============================================================================
# Pipeline Command Executor
# ============================================================================

proc executeCommand*(backend: var RaylibBackend, cmd: renderCommands.RenderCommand) =
  ## Execute a single RenderCommand
  ## This is the ONLY backend-specific rendering code needed!
  case cmd.kind:
  of rcDrawRectangle:
    # Draw filled rectangle
    backend.drawRectangle(cmd.rectX, cmd.rectY, cmd.rectWidth, cmd.rectHeight, cmd.rectColor)

  of rcDrawBorder:
    # Draw rectangle outline
    backend.drawRectangleBorder(cmd.borderX, cmd.borderY, cmd.borderWidth, cmd.borderHeight,
                                cmd.borderThickness, cmd.borderColor)

  of rcDrawText:
    # Draw text
    backend.drawText(cmd.textContent, cmd.textX, cmd.textY, cmd.textSize, cmd.textColor)

  of rcDrawImage:
    # Draw image
    backend.drawImage(cmd.imagePath, cmd.imageX, cmd.imageY, cmd.imageWidth, cmd.imageHeight)

  of rcDrawLine:
    # Draw line
    backend.drawLine(cmd.lineX1, cmd.lineY1, cmd.lineX2, cmd.lineY2,
                     cmd.lineThickness, cmd.lineColor)

  of rcBeginClip:
    # Start clipping region
    backend.beginClipping(cmd.clipX, cmd.clipY, cmd.clipWidth, cmd.clipHeight)

  of rcEndClip:
    # End clipping region
    backend.endClipping()

  of rcDrawCanvas:
    # Draw canvas with custom rendering
    backend.drawCanvas(cmd.canvasX, cmd.canvasY, cmd.canvasWidth, cmd.canvasHeight,
                       cmd.canvasDrawProc, cmd.canvasBackgroundColor)

  of rcDrawPath:
    # Draw path
    backend.drawPath(cmd.pathCommands, cmd.pathFillStyle, cmd.pathStrokeStyle,
                     cmd.pathLineWidth, cmd.pathShouldFill, cmd.pathShouldStroke)

  of rcClearCanvas:
    # Clear canvas area
    backend.clearCanvas(cmd.clearX, cmd.clearY, cmd.clearWidth, cmd.clearHeight, cmd.clearColor)

  of rcSaveCanvasState:
    # Save canvas state (will be handled in context)
    discard

  of rcRestoreCanvasState:
    # Restore canvas state (will be handled in context)
    discard

proc executeCanvasCommand*(backend: var RaylibBackend, cmd: renderCommands.RenderCommand, canvasX, canvasY: float) =
  ## Execute a single RenderCommand with canvas coordinate translation
  ## All coordinates are translated by (canvasX, canvasY) to map canvas-local coords to screen coords
  case cmd.kind:
  of rcDrawRectangle:
    # Draw filled rectangle with translation
    backend.drawRectangle(cmd.rectX + canvasX, cmd.rectY + canvasY, cmd.rectWidth, cmd.rectHeight, cmd.rectColor)
  of rcDrawBorder:
    # Draw rectangle outline with translation
    backend.drawRectangleBorder(cmd.borderX + canvasX, cmd.borderY + canvasY, cmd.borderWidth, cmd.borderHeight,
                                cmd.borderThickness, cmd.borderColor)
  of rcDrawText:
    # Draw text with translation
    backend.drawText(cmd.textContent, cmd.textX + canvasX, cmd.textY + canvasY, cmd.textSize, cmd.textColor)
  of rcDrawImage:
    # Draw image with translation
    backend.drawImage(cmd.imagePath, cmd.imageX + canvasX, cmd.imageY + canvasY, cmd.imageWidth, cmd.imageHeight)
  of rcDrawLine:
    # Draw line with translation
    backend.drawLine(cmd.lineX1 + canvasX, cmd.lineY1 + canvasY, cmd.lineX2 + canvasX, cmd.lineY2 + canvasY,
                     cmd.lineThickness, cmd.lineColor)
  of rcBeginClip:
    # Start clipping region with translation
    backend.beginClipping(cmd.clipX + canvasX, cmd.clipY + canvasY, cmd.clipWidth, cmd.clipHeight)
  of rcEndClip:
    # End clipping region
    backend.endClipping()
  of rcDrawPath:
    # Draw path with translation - translate all path commands
    var translatedCommands: seq[PathCommand] = @[]
    for pathCmd in cmd.pathCommands:
      case pathCmd.kind:
      of MoveTo:
        translatedCommands.add(PathCommand(kind: MoveTo, moveToX: pathCmd.moveToX + canvasX, moveToY: pathCmd.moveToY + canvasY))
      of LineTo:
        translatedCommands.add(PathCommand(kind: LineTo, lineToX: pathCmd.lineToX + canvasX, lineToY: pathCmd.lineToY + canvasY))
      of Arc:
        # Arc center needs translation
        translatedCommands.add(PathCommand(kind: Arc, arcX: pathCmd.arcX + canvasX, arcY: pathCmd.arcY + canvasY,
                                           arcRadius: pathCmd.arcRadius, arcStartAngle: pathCmd.arcStartAngle, arcEndAngle: pathCmd.arcEndAngle))
      of BezierCurveTo:
        # All bezier control points need translation
        translatedCommands.add(PathCommand(kind: BezierCurveTo,
                                           cp1x: pathCmd.cp1x + canvasX, cp1y: pathCmd.cp1y + canvasY,
                                           cp2x: pathCmd.cp2x + canvasX, cp2y: pathCmd.cp2y + canvasY,
                                           bezierX: pathCmd.bezierX + canvasX, bezierY: pathCmd.bezierY + canvasY))
      of ClosePath:
        translatedCommands.add(PathCommand(kind: ClosePath))
    backend.drawPath(translatedCommands, cmd.pathFillStyle, cmd.pathStrokeStyle,
                     cmd.pathLineWidth, cmd.pathShouldFill, cmd.pathShouldStroke)
  of rcDrawCanvas:
    # Canvas within canvas - translate the nested canvas position
    backend.drawCanvas(cmd.canvasX + canvasX, cmd.canvasY + canvasY, cmd.canvasWidth, cmd.canvasHeight,
                       cmd.canvasDrawProc, cmd.canvasBackgroundColor)
  of rcClearCanvas:
    # Clear canvas area with translation
    backend.clearCanvas(cmd.clearX + canvasX, cmd.clearY + canvasY, cmd.clearWidth, cmd.clearHeight, cmd.clearColor)
  of rcSaveCanvasState, rcRestoreCanvasState:
    # Canvas state operations (no coordinates to translate)
    discard

# ============================================================================
# Layout Engine Wrapper
# ============================================================================

type LayoutTextMeasurer = object
  backend: ptr RaylibBackend

proc measureText(m: LayoutTextMeasurer, text: string, fontSize: int): tuple[width: float, height: float] =
  ## Measure text for layout using cached font or backend font
  if text.len == 0:
    return (width: 0.0, height: fontSize.float)

  if layoutFontInitialized:
    let size = measureText(layoutFont[], text, fontSize.float32, 0.0)
    return (width: size.x, height: size.y)
  elif m.backend != nil:
    let size = measureText(m.backend.font, text, fontSize.float32, 0.0)
    return (width: size.x, height: size.y)
  else:
    return (width: measureText(text, fontSize.int32).float, height: fontSize.float)

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
  var measurer = LayoutTextMeasurer(backend: nil)
  layoutEngine.calculateLayout(measurer, elem, x, y, parentWidth, parentHeight)

# ============================================================================
# Interaction Helpers
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
    let rectArea = rect(elem.x, elem.y, elem.width, elem.height)
    if checkCollisionPointRec(mousePos, rectArea):
      return true

  of ekTab:
    let rectArea = rect(elem.x, elem.y, elem.width, elem.height)
    if checkCollisionPointRec(mousePos, rectArea):
      return true

  of ekDropdown:
    let mainRect = rect(elem.x, elem.y, elem.width, elem.height)
    if checkCollisionPointRec(mousePos, mainRect):
      return true

    if elem.dropdownIsOpen and elem.dropdownOptions.len > 0:
      let fontSize = elem.getProp("fontSize").get(val(16)).getInt()
      let itemHeight = fontSize.float + 10.0
      let dropdownHeight = min(elem.dropdownOptions.len.float * itemHeight, 200.0)
      let dropdownRect = rect(elem.x, elem.y + elem.height, elem.width, dropdownHeight)
      if checkCollisionPointRec(mousePos, dropdownRect):
        return true

  of ekCheckbox:
    let fontSize = elem.getProp("fontSize").get(val(16)).getInt()
    let checkboxSize = min(elem.height, fontSize.float + 8.0)
    let checkboxRect = rect(elem.x, elem.y + (elem.height - checkboxSize) / 2.0, checkboxSize, checkboxSize)

    let label = elem.getProp("label").get(val("")).getString()
    var hoverArea = checkboxRect

    if label.len > 0:
      hoverArea = rect(elem.x, elem.y, elem.width, elem.height)

    if checkCollisionPointRec(mousePos, hoverArea):
      return true

  else:
    discard

  for child in elem.children:
    if checkHoverCursor(child):
      return true

  return false

# ============================================================================
# Input Handling
# ============================================================================

proc handleInput*(backend: var RaylibBackend, elem: Element) =
  ## Handle mouse input for interactive elements
  let mousePos = getMousePosition()
  let mouseX = mousePos.x.float
  let mouseY = mousePos.y.float

  if elem == backend.rootElement:
    if globalInteractionState.shouldClearDragOverride:
      resetTabContentOverride()

    if isMouseButtonPressed(MouseButton.Left):
      discard handleDragStart(backend.rootElement, mouseX, mouseY)

    if globalInteractionState.draggedElement != nil and isMouseButtonDown(MouseButton.Left):
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

    if globalInteractionState.draggedElement != nil and isMouseButtonReleased(MouseButton.Left):
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

  # Element-specific input handling
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
    # Button clicks are now handled by the new pipeline system to avoid double-clicks
    discard

  of ekInput:
    if isMouseButtonPressed(MouseButton.Left):
      let rectArea = rect(elem.x, elem.y, elem.width, elem.height)
      if checkCollisionPointRec(mousePos, rectArea):
        backend.state.focusedInput = elem
        backend.state.cursorBlink = 0.0
        if not backend.state.inputValues.hasKey(elem):
          let initialValue = elem.getProp("value").get(val("")).getString()
          backend.state.inputValues[elem] = initialValue
      else:
        if backend.state.focusedInput == elem:
          backend.state.focusedInput = nil

  of ekCheckbox:
    # Checkbox clicks are now handled by the new pipeline system to avoid double-clicks
    discard

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
    let rectArea = rect(elem.x, elem.y, elem.width, elem.height)
    let isPressed = isMouseButtonPressed(MouseButton.Left)

    let interaction = getInteractionState()
    if isPressed and not interaction.isLiveReordering:
      if checkCollisionPointRec(mousePos, rectArea):
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
            echo "TAB CHANGED: from index ", oldSelectedIndex, " to index ", tabGroup.tabSelectedIndex

            if tabGroup.eventHandlers.hasKey("onSelectedIndexChanged"):
              echo "TRIGGERING: onSelectedIndexChanged event for TabGroup"
              tabGroup.eventHandlers["onSelectedIndexChanged"]()

            invalidateRelatedValues("tabSelectedIndex")

            markDirty(tabGroup)
            for child in tabGroup.children:
              markDirty(child)
              if child.kind == ekTabContent:
                echo "Marking TabContent children as dirty for tab change"
                for panel in child.children:
                  markDirty(panel)
                  markAllDescendantsDirty(panel)

        if elem.eventHandlers.hasKey("onClick"):
          elem.eventHandlers["onClick"]()

  of ekDropdown:
    # Dropdown clicks are now handled by the new pipeline system to avoid double-clicks
    # Keep hover state for visual feedback
    if elem.dropdownIsOpen and elem.dropdownOptions.len > 0:
      let fontSize = elem.getProp("fontSize").get(val(16)).getInt()
      let itemHeight = fontSize.float + 10.0
      let dropdownHeight = min(elem.dropdownOptions.len.float * itemHeight, 200.0)
      let dropdownRect = rect(elem.x, elem.y + elem.height, elem.width, dropdownHeight)

      if checkCollisionPointRec(mousePos, dropdownRect):
        let relativeY = mousePos.y - dropdownRect.y
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

proc handleKeyboardInput*(backend: var RaylibBackend, root: Element) =
  ## Handle keyboard input for focused elements

  # Handle checkbox keyboard input
  let mousePos = getMousePosition()
  proc findCheckboxUnderMouse(elem: Element): Element =
    if elem.kind == ekCheckbox:
      let fontSize = elem.getProp("fontSize").get(val(16)).getInt()
      let checkboxSize = min(elem.height, fontSize.float + 8.0)
      let checkboxRect = rect(elem.x, elem.y + (elem.height - checkboxSize) / 2.0, checkboxSize, checkboxSize)

      let label = elem.getProp("label").get(val("")).getString()
      var clickArea = checkboxRect

      if label.len > 0:
        clickArea = rect(elem.x, elem.y, elem.width, elem.height)

      if checkCollisionPointRec(mousePos, clickArea):
        return elem

    for child in elem.children:
      let found = findCheckboxUnderMouse(child)
      if found != nil:
        return found

    return nil

  let hoveredCheckbox = findCheckboxUnderMouse(root)
  if hoveredCheckbox != nil:
    if isKeyPressed(KeyboardKey.Enter):
      var currentState = backend.state.checkboxStates.getOrDefault(hoveredCheckbox, false)
      currentState = not currentState
      backend.state.checkboxStates[hoveredCheckbox] = currentState

      if hoveredCheckbox.eventHandlers.hasKey("onClick"):
        hoveredCheckbox.eventHandlers["onClick"]()

      if hoveredCheckbox.eventHandlers.hasKey("onChange"):
        let handler = hoveredCheckbox.eventHandlers["onChange"]
        handler($currentState)

  # Handle dropdown keyboard input
  if backend.state.focusedDropdown != nil:
    let dropdown = backend.state.focusedDropdown

    if isKeyPressed(KeyboardKey.Escape):
      dropdown.dropdownIsOpen = false
      dropdown.dropdownHoveredIndex = -1
      backend.state.focusedDropdown = nil

    elif dropdown.dropdownIsOpen:
      if dropdown.dropdownOptions.len > 0:
        if isKeyPressed(KeyboardKey.Up):
          dropdown.dropdownHoveredIndex = if dropdown.dropdownHoveredIndex <= 0:
            dropdown.dropdownOptions.len - 1
          else:
            dropdown.dropdownHoveredIndex - 1

        elif isKeyPressed(KeyboardKey.Down):
          dropdown.dropdownHoveredIndex = if dropdown.dropdownHoveredIndex >= dropdown.dropdownOptions.len - 1:
            0
          else:
            dropdown.dropdownHoveredIndex + 1

        elif isKeyPressed(KeyboardKey.Enter):
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

        elif isKeyPressed(KeyboardKey.Tab):
          dropdown.dropdownIsOpen = false
          dropdown.dropdownHoveredIndex = -1
          backend.state.focusedDropdown = nil

      return

  # Handle input field keyboard input
  if backend.state.focusedInput == nil:
    backend.state.backspaceHoldTimer = 0.0
    return

  var currentValue = backend.state.inputValues.getOrDefault(backend.state.focusedInput, "")
  var textChanged = false

  # Handle character input
  while true:
    let char = getCharPressed()
    if char == 0:
      break
    if char >= 32 and char < 127:
      currentValue.add(char.chr)
      textChanged = true
      backend.state.backspaceHoldTimer = 0.0

  # Handle backspace
  let backspacePressed = isKeyDown(KeyboardKey.Backspace)
  if backspacePressed and currentValue.len > 0:
    if isKeyPressed(KeyboardKey.Backspace):
      currentValue.setLen(currentValue.len - 1)
      textChanged = true
      backend.state.backspaceHoldTimer = 0.0
    else:
      backend.state.backspaceHoldTimer += 1.0 / 60.0

      if backend.state.backspaceHoldTimer >= backend.state.backspaceRepeatDelay:
        let holdBeyondDelay = backend.state.backspaceHoldTimer - backend.state.backspaceRepeatDelay
        let charsToDelete = min(int(holdBeyondDelay / backend.state.backspaceRepeatRate), currentValue.len)

        if charsToDelete > 0:
          currentValue.setLen(currentValue.len - charsToDelete)
          textChanged = true
          backend.state.backspaceHoldTimer = backend.state.backspaceRepeatDelay +
                                      (charsToDelete.float * backend.state.backspaceRepeatRate)
  else:
    backend.state.backspaceHoldTimer = 0.0

  # Handle enter key
  if isKeyPressed(KeyboardKey.Enter):
    if backend.state.focusedInput.eventHandlers.hasKey("onSubmit"):
      backend.state.focusedInput.eventHandlers["onSubmit"]()

  # Update stored value if changed
  if textChanged:
    backend.state.inputValues[backend.state.focusedInput] = currentValue

  # Trigger event handlers
  if textChanged and backend.state.focusedInput.eventHandlers.hasKey("onChange"):
    let handler = backend.state.focusedInput.eventHandlers["onChange"]
    handler(currentValue)

  if textChanged and backend.state.focusedInput.eventHandlers.hasKey("onTextChange"):
    let handler = backend.state.focusedInput.eventHandlers["onTextChange"]
    handler(currentValue)

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
  initWindow(backend.windowWidth.int32, backend.windowHeight.int32, backend.windowTitle)
  setTargetFPS(60)

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
      echo "Using default font: " & fontName & " at " & fontPath

  if fontPath.len > 0:
    echo "Loading font: " & fontPath
    if fileExists(fontPath):
      echo "Font file exists on disk"
      backend.font = loadFont(fontPath)
      if backend.font.baseSize > 0:
        echo "Successfully loaded font: " & fontName & " (baseSize: " & $backend.font.baseSize & ")"
      else:
        echo "Warning: Failed to load font (baseSize=0), using Raylib default font"
        backend.font = getFontDefault()
    else:
      echo "Error: Font file does not exist: " & fontPath
      backend.font = getFontDefault()
  else:
    echo "No font found, using Raylib default font"
    backend.font = getFontDefault()

  setLayoutFont(backend.font)

  # Calculate initial layout
  calculateLayout(root, 0, 0, backend.windowWidth.float, backend.windowHeight.float)

  # Frame timing for deltaTime calculations
  var lastTime = raylib.getTime()
  var currentTime: float
  var deltaTime: float

  # Main game loop
  while not windowShouldClose():
    # Calculate real deltaTime
    currentTime = raylib.getTime()
    deltaTime = currentTime - lastTime
    lastTime = currentTime

    # Update cursor blink timer with real deltaTime
    backend.state.cursorBlink += deltaTime
    if backend.state.cursorBlink >= 1.0:
      backend.state.cursorBlink = 0.0

    # Update mouse cursor based on hover state
    if checkHoverCursor(root):
      setMouseCursor(MouseCursor.PointingHand)
    else:
      setMouseCursor(MouseCursor.Default)

    # Handle input
    backend.handleInput(root)
    backend.handleKeyboardInput(root)

    # Recalculate layout when there are dirty elements
    if hasDirtyElements():
      calculateLayout(root, 0, 0, backend.windowWidth.float, backend.windowHeight.float)

    # Render
    beginDrawing()

    # Find the actual Body element and check for background color
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

    if bodyBg.isSome:
      let bgColor = bodyBg.get.getColor()
      clearBackground(bgColor.toRaylibColor())
    else:
      clearBackground(backend.backgroundColor.toRaylibColor())

    # Render using pipeline
    let mousePos = getMousePosition()
    let config = newPipelineConfig(
      windowWidth = backend.windowWidth,
      windowHeight = backend.windowHeight,
      mouseX = mousePos.x,
      mouseY = mousePos.y,
      mousePressed = isMouseButtonPressed(MouseButton.Left),
      mouseReleased = isMouseButtonReleased(MouseButton.Left),
      deltaTime = deltaTime
    )

    # Process frame through pipeline - this does ALL the work!
    let commands = processFrame(root, backend, backend.state, config)

    # Execute commands (includes drag-and-drop effects)
    for cmd in commands:
      backend.executeCommand(cmd)

    endDrawing()

  # Cleanup
  layoutFontInitialized = false
  closeWindow()
  backend.running = false
