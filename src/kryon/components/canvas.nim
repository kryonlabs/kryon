## Canvas Component
##
## Extracts canvas properties for rendering.
## Canvas elements provide a drawing surface for custom graphics.

import std/math
import ../core
import ../rendering/renderingContext
import ../pipeline/renderCommands
import options

# Forward declaration for raylib backend
type RaylibBackend {.importc.} = ref object

# Forward declarations for path management functions
proc beginPath*(ctx: DrawingContext)
proc moveTo*(ctx: DrawingContext, x, y: float)
proc lineTo*(ctx: DrawingContext, x, y: float)
proc arc*(ctx: DrawingContext, x, y, radius, startAngle, endAngle: float)
proc closePath*(ctx: DrawingContext)
proc fill*(ctx: DrawingContext)
proc stroke*(ctx: DrawingContext)

type
  CanvasData* = object
    ## Extracted canvas rendering data
    x*, y*, width*, height*: float
    hasBackground*: bool
    backgroundColor*: core.Color
    drawProc*: proc(ctx: DrawingContext, width, height: float) {.closure.}

proc extractCanvasData*(elem: Element, inheritedColor: Option[core.Color] = none(core.Color)): CanvasData =
  ## Extract canvas properties from element
  ##
  ## Properties supported:
  ##   - backgroundColor/background: Canvas background color (optional)
  ##   - onDraw: Drawing callback procedure

  result.x = elem.x
  result.y = elem.y
  result.width = elem.width
  result.height = elem.height

  # Check for background color
  let bgColor = elem.getProp("backgroundColor")
  if bgColor.isNone:
    let bg = elem.getProp("background")
    if bg.isSome:
      result.hasBackground = true
      result.backgroundColor = bg.get.getColor()
  elif bgColor.isSome:
    result.hasBackground = true
    result.backgroundColor = bgColor.get.getColor()

  # Extract the drawing procedure from the element
  if elem.canvasDrawProc != nil:
    result.drawProc = cast[proc(ctx: DrawingContext, width, height: float) {.closure.}](elem.canvasDrawProc)

template addRenderCommand*(ctx: DrawingContext, cmd: untyped) =
  ## Helper template to add a render command to the drawing context
  if ctx.renderCommands != nil:
    var commands = cast[ptr seq[renderCommands.RenderCommand]](ctx.renderCommands)
    commands[].add(cmd)

proc newDrawingContext*(): DrawingContext =
  ## Create a new drawing context with default values
  result = DrawingContext()
  # Set default values
  result.fillStyle = rgba(0, 0, 0, 255)  # Black
  result.strokeStyle = rgba(0, 0, 0, 255)  # Black
  result.lineWidth = 1.0
  result.font = "Arial 12"
  result.textAlign = TextAlign.Start
  result.textBaseline = TextBaseline.Alphabetic
  result.globalAlpha = 1.0
  result.globalCompositeOperation = GlobalCompositeOperation.SourceOver
  result.lineCap = LineCap.Butt
  result.lineJoin = LineJoin.Miter
  result.stateStack = @[]
  result.currentPath = @[]
  result.pathInProgress = false
  result.backendData = nil

# ============================================================================
# Canvas Drawing Operations
# ============================================================================

proc clear*(ctx: DrawingContext, color: core.Color) =
  ## Clears the entire canvas to a specific color
  # Note: For now, we'll use a simpler approach since clearBackground affects the whole screen
  discard

proc fillRect*(ctx: DrawingContext, x, y, w, h: float) =
  ## Draws a filled rectangle with the current fillStyle
  # Create a simple path for rectangle and add to render commands
  ctx.beginPath()
  ctx.moveTo(x, y)
  ctx.lineTo(x + w, y)
  ctx.lineTo(x + w, y + h)
  ctx.lineTo(x, y + h)
  ctx.closePath()
  ctx.fill()

proc strokeRect*(ctx: DrawingContext, x, y, w, h: float) =
  ## Draws a rectangle outline with the current strokeStyle and lineWidth
  # Create a simple path for rectangle outline and add to render commands
  ctx.beginPath()
  ctx.moveTo(x, y)
  ctx.lineTo(x + w, y)
  ctx.lineTo(x + w, y + h)
  ctx.lineTo(x, y + h)
  ctx.closePath()
  ctx.stroke()

proc beginPath*(ctx: DrawingContext) =
  ## Starts a new path, discarding any previous one
  ctx.currentPath.setLen(0)
  ctx.pathInProgress = true

proc moveTo*(ctx: DrawingContext, x, y: float) =
  ## Moves the "pen" to a new location without drawing
  ctx.currentPath.add(PathCommand(kind: PathCommandKind.MoveTo, moveToX: x, moveToY: y))

proc lineTo*(ctx: DrawingContext, x, y: float) =
  ## Adds a straight line to the current point
  ctx.currentPath.add(PathCommand(kind: PathCommandKind.LineTo, lineToX: x, lineToY: y))

proc arc*(ctx: DrawingContext, x, y, radius, startAngle, endAngle: float) =
  ## Adds a circular arc to the current point
  ctx.currentPath.add(PathCommand(kind: PathCommandKind.Arc, arcX: x, arcY: y, arcRadius: radius, arcStartAngle: startAngle, arcEndAngle: endAngle))

proc bezierCurveTo*(ctx: DrawingContext, cp1x, cp1y, cp2x, cp2y, x, y: float) =
  ## Adds a cubic Bezier curve to the current point
  ctx.currentPath.add(PathCommand(kind: PathCommandKind.BezierCurveTo, cp1x: cp1x, cp1y: cp1y, cp2x: cp2x, cp2y: cp2y, bezierX: x, bezierY: y))

proc closePath*(ctx: DrawingContext) =
  ## Adds a line from the current point back to the start
  ctx.currentPath.add(PathCommand(kind: PathCommandKind.ClosePath))

proc fill*(ctx: DrawingContext) =
  ## Fills the current path with the fillStyle
  if ctx.currentPath.len == 0:
    return

  # Emit a drawPath render command for the current path
  let pathCopy = ctx.currentPath  # Copy the current path
  let cmd = drawPath(pathCopy, ctx.fillStyle, ctx.strokeStyle, ctx.lineWidth, shouldFill = true, shouldStroke = false)
  addRenderCommand(ctx, cmd)

  # Clear the current path after emitting the command
  ctx.currentPath.setLen(0)
  ctx.pathInProgress = false

proc stroke*(ctx: DrawingContext) =
  ## Draws the outline of the current path with the strokeStyle
  if ctx.currentPath.len == 0:
    return

  # Emit a drawPath render command for the current path
  let pathCopy = ctx.currentPath  # Copy the current path
  let cmd = drawPath(pathCopy, ctx.fillStyle, ctx.strokeStyle, ctx.lineWidth, shouldFill = false, shouldStroke = true)
  addRenderCommand(ctx, cmd)

  # Clear the current path after emitting the command
  ctx.currentPath.setLen(0)
  ctx.pathInProgress = false

proc save*(ctx: DrawingContext) =
  ## Pushes the current drawing state onto the stack
  let currentState = DrawingState(
    fillStyle: ctx.fillStyle,
    strokeStyle: ctx.strokeStyle,
    lineWidth: ctx.lineWidth,
    font: ctx.font,
    textAlign: ctx.textAlign,
    textBaseline: ctx.textBaseline,
    globalAlpha: ctx.globalAlpha,
    globalCompositeOperation: ctx.globalCompositeOperation,
    lineCap: ctx.lineCap,
    lineJoin: ctx.lineJoin
  )
  ctx.stateStack.add(currentState)

proc restore*(ctx: DrawingContext) =
  ## Pops the state from the stack, restoring the previous state
  if ctx.stateStack.len > 0:
    let state = ctx.stateStack.pop()
    ctx.fillStyle = state.fillStyle
    ctx.strokeStyle = state.strokeStyle
    ctx.lineWidth = state.lineWidth
    ctx.font = state.font
    ctx.textAlign = state.textAlign
    ctx.textBaseline = state.textBaseline
    ctx.globalAlpha = state.globalAlpha
    ctx.globalCompositeOperation = state.globalCompositeOperation
    ctx.lineCap = state.lineCap
    ctx.lineJoin = state.lineJoin

proc fillText*(ctx: DrawingContext, text: string, x, y: float) =
  ## Draws filled text at the specified position
  ## This will be implemented by the backend

proc strokeText*(ctx: DrawingContext, text: string, x, y: float) =
  ## Draws outlined text at the specified position
  ## This will be implemented by the backend

proc translate*(ctx: DrawingContext, x, y: float) =
  ## Moves the origin (0,0) of the canvas
  ## This will be implemented by the backend

proc rotate*(ctx: DrawingContext, angleRadians: float) =
  ## Rotates the canvas around the current origin
  ## This will be implemented by the backend

proc scale*(ctx: DrawingContext, x, y: float) =
  ## Scales the canvas non-uniformly
  ## This will be implemented by the backend

# ============================================================================
# High-level Drawing Functions (similar to Raylib)
# ============================================================================

proc drawRectangle*(ctx: DrawingContext, x, y, width, height: float) =
  ## Draw a filled rectangle
  ctx.fillRect(x, y, width, height)

proc drawRectangleLines*(ctx: DrawingContext, x, y, width, height: float) =
  ## Draw rectangle outline
  ctx.strokeRect(x, y, width, height)

proc drawCircle*(ctx: DrawingContext, centerX, centerY, radius: float) =
  ## Draw a filled circle
  ctx.beginPath()
  ctx.arc(centerX, centerY, radius, 0, 2 * PI)
  ctx.fill()

proc drawCircleLines*(ctx: DrawingContext, centerX, centerY, radius: float) =
  ## Draw circle outline
  ctx.beginPath()
  ctx.arc(centerX, centerY, radius, 0, 2 * PI)
  ctx.stroke()

proc drawTriangle*(ctx: DrawingContext, x1, y1, x2, y2, x3, y3: float) =
  ## Draw a filled triangle
  ctx.beginPath()
  ctx.moveTo(x1, y1)
  ctx.lineTo(x2, y2)
  ctx.lineTo(x3, y3)
  ctx.closePath()
  ctx.fill()

proc drawTriangleLines*(ctx: DrawingContext, x1, y1, x2, y2, x3, y3: float) =
  ## Draw triangle outline
  ctx.beginPath()
  ctx.moveTo(x1, y1)
  ctx.lineTo(x2, y2)
  ctx.lineTo(x3, y3)
  ctx.closePath()
  ctx.stroke()

proc drawLine*(ctx: DrawingContext, startX, startY, endX, endY: float) =
  ## Draw a line
  ctx.beginPath()
  ctx.moveTo(startX, startY)
  ctx.lineTo(endX, endY)
  ctx.stroke()

proc drawPoly*(ctx: DrawingContext, center: tuple[x, y: float], sides: int, radius: float) =
  ## Draw a filled polygon
  ctx.beginPath()
  for i in 0..<sides:
    let angle = (2 * PI * i.float) / sides.float - PI / 2.0
    let x = center.x + cos(angle) * radius
    let y = center.y + sin(angle) * radius
    if i == 0:
      ctx.moveTo(x, y)
    else:
      ctx.lineTo(x, y)
  ctx.closePath()
  ctx.fill()

proc drawPolyLines*(ctx: DrawingContext, center: tuple[x, y: float], sides: int, radius: float) =
  ## Draw polygon outline
  ctx.beginPath()
  for i in 0..<sides:
    let angle = (2 * PI * i.float) / sides.float - PI / 2.0
    let x = center.x + cos(angle) * radius
    let y = center.y + sin(angle) * radius
    if i == 0:
      ctx.moveTo(x, y)
    else:
      ctx.lineTo(x, y)
  ctx.closePath()
  ctx.stroke()

proc drawStar*(ctx: DrawingContext, centerX, centerY: float, outerRadius, innerRadius: float, points: int) =
  ## Draw a filled star
  ctx.beginPath()
  for i in 0..<points * 2:
    let angle = (PI * i.float) / points.float - PI / 2.0
    let radius = if i mod 2 == 0: outerRadius else: innerRadius
    let x = centerX + cos(angle) * radius
    let y = centerY + sin(angle) * radius
    if i == 0:
      ctx.moveTo(x, y)
    else:
      ctx.lineTo(x, y)
  ctx.closePath()
  ctx.fill()

proc drawStarLines*(ctx: DrawingContext, centerX, centerY: float, outerRadius, innerRadius: float, points: int) =
  ## Draw star outline
  ctx.beginPath()
  for i in 0..<points * 2:
    let angle = (PI * i.float) / points.float - PI / 2.0
    let radius = if i mod 2 == 0: outerRadius else: innerRadius
    let x = centerX + cos(angle) * radius
    let y = centerY + sin(angle) * radius
    if i == 0:
      ctx.moveTo(x, y)
    else:
      ctx.lineTo(x, y)
  ctx.closePath()
  ctx.stroke()

proc drawEllipse*(ctx: DrawingContext, centerX, centerY: float, radiusX, radiusY: float) =
  ## Draw a filled ellipse
  ctx.beginPath()
  for i in 0..360:
    let angle = (PI * 2 * i.float) / 360.0
    let x = centerX + cos(angle) * radiusX
    let y = centerY + sin(angle) * radiusY
    if i == 0:
      ctx.moveTo(x, y)
    else:
      ctx.lineTo(x, y)
  ctx.closePath()
  ctx.fill()

proc drawEllipseLines*(ctx: DrawingContext, centerX, centerY: float, radiusX, radiusY: float) =
  ## Draw ellipse outline
  ctx.beginPath()
  for i in 0..360:
    let angle = (PI * 2 * i.float) / 360.0
    let x = centerX + cos(angle) * radiusX
    let y = centerY + sin(angle) * radiusY
    if i == 0:
      ctx.moveTo(x, y)
    else:
      ctx.lineTo(x, y)
  ctx.closePath()
  ctx.stroke()

proc drawRing*(ctx: DrawingContext, centerX, centerY: float, innerRadius, outerRadius: float) =
  ## Draw a filled ring (circle with hole)
  ctx.beginPath()
  for i in 0..360:
    let angle = (PI * 2 * i.float) / 360.0
    let x = centerX + cos(angle) * outerRadius
    let y = centerY + sin(angle) * outerRadius
    if i == 0:
      ctx.moveTo(x, y)
    else:
      ctx.lineTo(x, y)
  ctx.closePath()
  ctx.fill()

  # Cut out the inner circle
  ctx.beginPath()
  for i in 0..360:
    let angle = (PI * 2 * i.float) / 360.0
    let x = centerX + cos(angle) * innerRadius
    let y = centerY + sin(angle) * innerRadius
    if i == 0:
      ctx.moveTo(x, y)
    else:
      ctx.lineTo(x, y)
  ctx.closePath()
  ctx.fill()

proc drawSineWave*(ctx: DrawingContext, startX, startY, endX, amplitude: float, frequency: float = 0.05) =
  ## Draw a sine wave
  ctx.beginPath()
  for x in 0..(endX - startX).int:
    let y = startY + sin(x.float * frequency) * amplitude
    if x == 0:
      ctx.moveTo(startX + x.float, y)
    else:
      ctx.lineTo(startX + x.float, y)
  ctx.stroke()