## Kryon Canvas - Love2D Style API
##
## Provides Love2D-inspired immediate mode drawing functions
## that map to the enhanced C canvas implementation

import core_kryon, core_kryon_canvas, runtime

export core_kryon

# ============================================================================
# Types and Enums (matching C definitions)
# ============================================================================

type
  DrawMode* = enum
    Fill = 0
    Line = 1

  LineStyle* = enum
    Solid = 0
    Rough = 1
    Smooth = 2

  LineJoin* = enum
    Miter = 0
    Bevel = 1
    Round = 2

  BlendMode* = enum
    Alpha = 0        # Standard alpha blending
    Additive = 1     # Additive blending
    Multiply = 2     # Multiplicative blending
    Subtract = 3     # Subtractive blending
    Screen = 4       # Screen blending
    Replace = 5      # Replace (no blending)

  Point* = tuple[x: float, y: float]

# ============================================================================
# Constants
# ============================================================================

const
  PI* = 3.141592653589793

  # Predefined colors (matching C definitions)
  COLOR_RED*     = 0xFF0000FF'u32
  COLOR_GREEN*   = 0x00FF00FF'u32
  COLOR_BLUE*    = 0x0000FFFF'u32
  COLOR_WHITE*   = 0xFFFFFFFF'u32
  COLOR_BLACK*   = 0x000000FF'u32
  COLOR_YELLOW*  = 0xFFFF00FF'u32
  COLOR_CYAN*    = 0x00FFFFFF'u32
  COLOR_MAGENTA* = 0xFF00FFFF'u32
  COLOR_GRAY*    = 0x808080FF'u32
  COLOR_ORANGE*  = 0xFFA500FF'u32
  COLOR_PURPLE*  = 0x800080FF'u32

# ============================================================================
# Canvas Lifecycle and Global State Management
# ============================================================================

proc initCanvas*(width, height: int) =
  ## Initialize the global canvas system
  kryonCanvasInit(uint16(width), uint16(height))

proc shutdownCanvas*() =
  ## Shutdown the global canvas system
  kryonCanvasShutdown()

proc resizeCanvas*(width, height: int) =
  ## Resize the canvas
  kryonCanvasResize(uint16(width), uint16(height))

proc clearCanvas*() =
  ## Clear the canvas with background color
  kryonCanvasClear()

proc clearCanvas*(color: uint32) =
  ## Clear the canvas with specific color
  kryonCanvasClearColor(color)

proc clearCanvas*(r, g, b, a: int) =
  ## Clear the canvas with specific RGBA color
  kryonCanvasClearColor(rgb(r, g, b, a))

# ============================================================================
# Drawing State Management (Love2D Style)
# ============================================================================

proc setColor*(color: uint32) =
  ## Set current drawing color
  kryonCanvasSetColor(color)

proc setColor*(r, g, b: int) =
  ## Set current drawing color (RGB)
  kryonCanvasSetColorRgb(uint8(r), uint8(g), uint8(b))

proc setColor*(r, g, b, a: int) =
  ## Set current drawing color (RGBA)
  kryonCanvasSetColorRgba(uint8(r), uint8(g), uint8(b), uint8(a))

proc getColor*(): uint32 =
  ## Get current drawing color
  kryonCanvasGetColor()

proc setBackgroundColor*(color: uint32) =
  ## Set background color
  kryonCanvasSetBackgroundColor(color)

proc setBackgroundColor*(r, g, b, a: int) =
  ## Set background color (RGBA)
  kryonCanvasSetBackgroundColorRgba(uint8(r), uint8(g), uint8(b), uint8(a))

proc setLineWidth*(width: float) =
  ## Set line width in pixels
  kryonCanvasSetLineWidth(floatToFp(width))

proc getLineWidth*(): float =
  ## Get current line width
  fpToFloat(kryonCanvasGetLineWidth())

proc setLineStyle*(style: LineStyle) =
  ## Set line style
  kryonCanvasSetLineStyle(cast[ kryon_line_style_t ](ord(style)))

proc getLineStyle*(): LineStyle =
  ## Get current line style
  LineStyle(ord(kryonCanvasGetLineStyle()))

proc setLineJoin*(join: LineJoin) =
  ## Set line join style
  kryonCanvasSetLineJoin(cast[ kryon_line_join_t ](ord(join)))

proc getLineJoin*(): LineJoin =
  ## Get current line join style
  LineJoin(ord(kryonCanvasGetLineJoin()))

proc setFont*(fontId: int) =
  ## Set current font by ID
  kryonCanvasSetFont(uint16(fontId))

proc getFont*(): int =
  ## Get current font ID
  int(kryonCanvasGetFont())

proc setBlendMode*(mode: BlendMode) =
  ## Set blend mode
  kryonCanvasSetBlendMode(cast[ kryon_blend_mode_t ](ord(mode)))

proc getBlendMode*(): BlendMode =
  ## Get current blend mode
  BlendMode(ord(kryonCanvasGetBlendMode()))

# ============================================================================
# Transform System (Love2D Style)
# ============================================================================

proc origin*() =
  ## Reset transform to identity
  kryonCanvasOrigin()

proc translate*(x, y: float) =
  ## Translate the coordinate system
  kryonCanvasTranslate(floatToFp(x), floatToFp(y))

proc rotate*(angle: float) =
  ## Rotate the coordinate system (angle in degrees)
  kryonCanvasRotate(floatToFp(angle))

proc rotateRad*(angle: float) =
  ## Rotate the coordinate system (angle in radians)
  kryonCanvasRotate(floatToFp(angle * 180.0 / PI))

proc scale*(sx, sy: float) =
  ## Scale the coordinate system
  kryonCanvasScale(floatToFp(sx), floatToFp(sy))

proc shear*(kx, ky: float) =
  ## Shear the coordinate system
  kryonCanvasShear(floatToFp(kx), floatToFp(ky))

proc push*() =
  ## Push current transform state onto the stack
  kryonCanvasPush()

proc pop*() =
  ## Pop transform state from the stack
  kryonCanvasPop()

# ============================================================================
# Basic Drawing Primitives (Love2D Style)
# ============================================================================

proc rectangle*(mode: DrawMode, x, y, width, height: float) =
  ## Draw a rectangle (filled or outline)
  kryonCanvasRectangle(
    cast[ kryon_draw_mode_t ](ord(mode)),
    floatToFp(x), floatToFp(y),
    floatToFp(width), floatToFp(height)
  )

proc rectangle*(mode: DrawMode, x, y, width, height: int) =
  ## Draw a rectangle (filled or outline) - integer version
  rectangle(mode, float(x), float(y), float(width), float(height))

proc circle*(mode: DrawMode, x, y, radius: float) =
  ## Draw a circle (filled or outline)
  kryonCanvasCircle(
    cast[ kryon_draw_mode_t ](ord(mode)),
    floatToFp(x), floatToFp(y),
    floatToFp(radius)
  )

proc circle*(mode: DrawMode, x, y, radius: int) =
  ## Draw a circle (filled or outline) - integer version
  circle(mode, float(x), float(y), float(radius))

proc ellipse*(mode: DrawMode, x, y, rx, ry: float) =
  ## Draw an ellipse (filled or outline)
  kryonCanvasEllipse(
    cast[ kryon_draw_mode_t ](ord(mode)),
    floatToFp(x), floatToFp(y),
    floatToFp(rx), floatToFp(ry)
  )

proc ellipse*(mode: DrawMode, x, y, rx, ry: int) =
  ## Draw an ellipse (filled or outline) - integer version
  ellipse(mode, float(x), float(y), float(rx), float(ry))

proc polygon*(mode: DrawMode, vertices: openArray[Point]) =
  ## Draw a polygon (filled or outline)
  if vertices.len == 0: return

  # Convert vertices to fixed-point array
  var fpVertices = newSeq[kryon_fp_t](vertices.len * 2)
  for i, vertex in vertices:
    fpVertices[i * 2] = floatToFp(vertex.x)
    fpVertices[i * 2 + 1] = floatToFp(vertex.y)

  kryonCanvasPolygon(
    cast[ kryon_draw_mode_t ](ord(mode)),
    fpVertices[0].addr, uint16(vertices.len)
  )

proc line*(x1, y1, x2, y2: float) =
  ## Draw a line between two points
  kryonCanvasLine(floatToFp(x1), floatToFp(y1), floatToFp(x2), floatToFp(y2))

proc line*(x1, y1, x2, y2: int) =
  ## Draw a line between two points - integer version
  line(float(x1), float(y1), float(x2), float(y2))

proc line*(points: openArray[Point]) =
  ## Draw connected lines through multiple points
  if points.len < 2: return

  # Convert vertices to fixed-point array
  var fpVertices = newSeq[kryon_fp_t](points.len * 2)
  for i, point in points:
    fpVertices[i * 2] = floatToFp(point.x)
    fpVertices[i * 2 + 1] = floatToFp(point.y)

  kryonCanvasLinePoints(fpVertices[0].addr, uint16(points.len))

proc point*(x, y: float) =
  ## Draw a single point
  kryonCanvasPoint(floatToFp(x), floatToFp(y))

proc point*(x, y: int) =
  ## Draw a single point - integer version
  point(float(x), float(y))

proc points*(points: openArray[Point]) =
  ## Draw multiple points
  if points.len == 0: return

  # Convert vertices to fixed-point array
  var fpVertices = newSeq[kryon_fp_t](points.len * 2)
  for i, point in points:
    fpVertices[i * 2] = floatToFp(point.x)
    fpVertices[i * 2 + 1] = floatToFp(point.y)

  kryonCanvasPoints(fpVertices[0].addr, uint16(points.len))

proc arc*(mode: DrawMode, cx, cy, radius: float, startAngle, endAngle: float) =
  ## Draw an arc (angle in radians)
  kryonCanvasArc(
    cast[ kryon_draw_mode_t ](ord(mode)),
    floatToFp(cx), floatToFp(cy),
    floatToFp(radius),
    floatToFp(startAngle), floatToFp(endAngle)
  )

proc arc*(mode: DrawMode, cx, cy, radius: int, startAngle, endAngle: float) =
  ## Draw an arc (angle in radians) - integer position/radius version
  arc(mode, float(cx), float(cy), float(radius), startAngle, endAngle)

# ============================================================================
# Text Rendering (Love2D Style)
# ============================================================================

proc print*(text: string, x, y: float) =
  ## Print text at the specified position
  kryonCanvasPrint(text.cstring, floatToFp(x), floatToFp(y))

proc print*(text: string, x, y: int) =
  ## Print text at the specified position - integer version
  print(text, float(x), float(y))

proc printf*(text: string, x, y: float, wrapLimit: float, align: int = 0) =
  ## Print formatted text with word wrapping and alignment
  kryonCanvasPrintf(text.cstring, floatToFp(x), floatToFp(y), floatToFp(wrapLimit), int32(align))

proc printf*(text: string, x, y: int, wrapLimit: int, align: int = 0) =
  ## Print formatted text with word wrapping and alignment - integer version
  printf(text, float(x), float(y), float(wrapLimit), align)

proc getTextWidth*(text: string): float =
  ## Get the width of text in pixels
  fpToFloat(kryonCanvasGetTextWidth(text.cstring))

proc getTextHeight*(): float =
  ## Get the height of current font in pixels
  fpToFloat(kryonCanvasGetTextHeight())

proc getFontSize*(): float =
  ## Get the current font size
  fpToFloat(kryonCanvasGetFontSize())

# ============================================================================
# Convenience Functions (Love2D style)
# ============================================================================

proc background*(color: uint32) =
  ## Set background color and clear canvas
  setBackgroundColor(color)
  clearCanvas()

proc background*(r, g, b, a: int) =
  ## Set background color and clear canvas (RGBA)
  setBackgroundColor(r, g, b, a)
  clearCanvas()

proc background*(r, g, b: int) =
  ## Set background color and clear canvas (RGB)
  setBackgroundColor(r, g, b, 255)
  clearCanvas()

proc fill*(color: uint32) =
  ## Set fill color
  setColor(color)

proc fill*(r, g, b, a: int) =
  ## Set fill color (RGBA)
  setColor(r, g, b, a)

proc fill*(r, g, b: int) =
  ## Set fill color (RGB)
  setColor(r, g, b, 255)

proc stroke*(color: uint32) =
  ## Set stroke color
  setColor(color)

proc stroke*(r, g, b, a: int) =
  ## Set stroke color (RGBA)
  setColor(r, g, b, a)

proc stroke*(r, g, b: int) =
  ## Set stroke color (RGB)
  setColor(r, g, b, 255)

proc strokeWeight*(weight: float) =
  ## Set stroke weight (line width)
  setLineWidth(weight)

proc noFill*() =
  ## Disable fill (draw outlines only)
  setColor(0) # Transparent color

proc noStroke*() =
  ## Disable stroke (no outlines)
  setLineWidth(0)

# ============================================================================
# Shape Drawing Convenience Functions
# ============================================================================

proc rect*(x, y, width, height: float) =
  ## Draw a filled rectangle
  rectangle(DrawMode.Fill, x, y, width, height)

proc rect*(x, y, width, height: int) =
  ## Draw a filled rectangle - integer version
  rectangle(DrawMode.Fill, x, y, width, height)

proc rectMode*() =
  ## Rectangle drawing mode (placeholder for compatibility)
  discard

proc ellipseMode*() =
  ## Ellipse drawing mode (placeholder for compatibility)
  discard

# ============================================================================
# Color Utilities
# ============================================================================

proc color*(r, g, b, a: int): uint32 =
  ## Create color from RGBA values
  rgb(r, g, b, a)

proc color*(r, g, b: int): uint32 =
  ## Create color from RGB values
  rgb(r, g, b, 255)

proc color*(gray: int): uint32 =
  ## Create grayscale color
  rgb(gray, gray, gray, 255)

proc color*(gray, alpha: int): uint32 =
  ## Create grayscale color with alpha
  rgb(gray, gray, gray, alpha)

proc red*(color: uint32): int =
  ## Get red component from color
  int(colorRed(color))

proc green*(color: uint32): int =
  ## Get green component from color
  int(colorGreen(color))

proc blue*(color: uint32): int =
  ## Get blue component from color
  int(colorBlue(color))

proc alpha*(color: uint32): int =
  ## Get alpha component from color
  int(colorAlpha(color))

# ============================================================================
# Math Utilities
# ============================================================================

proc radians*(degrees: float): float =
  ## Convert degrees to radians
  degrees * PI / 180.0

proc degrees*(radians: float): float =
  ## Convert radians to degrees
  radians * 180.0 / PI

proc random*(low, high: float): float =
  ## Generate random float between low and high
  # Simple implementation - in real use would use proper random generator
  low + (high - low) * 0.5

proc random*(max: float): float =
  ## Generate random float between 0 and max
  random(0.0, max)

proc map*(value, inMin, inMax, outMin, outMax: float): float =
  ## Map value from one range to another
  (value - inMin) / (inMax - inMin) * (outMax - outMin) + outMin

# ============================================================================
# Advanced Drawing Functions
# ============================================================================

proc drawHeart*(x, y, size: float) =
  ## Draw a heart shape at the specified position
  push()
  translate(x, y)
  scale(size / 50.0, size / 50.0)

  # Heart shape using bezier curves approximation
  setColor(255, 0, 0, 255)

  # Draw using circles and transforms for simple heart
  push()
  translate(-20, -15)
  circle(Fill, 0, 0, 15)
  translate(40, 0)
  circle(Fill, 0, 0, 15)
  pop()

  # Bottom triangle
  polygon(Fill, [
    (0.0, 10.0),    # Bottom point
    (-25.0, -5.0),  # Left top
    (25.0, -5.0)    # Right top
  ])

  pop()

proc drawStar*(x, y, size: float, points: int = 5) =
  ## Draw a star shape
  if points < 3: return

  push()
  translate(x, y)
  scale(size / 50.0, size / 50.0)

  var vertices = newSeq[Point](points * 2)

  for i in 0..<points:
    let angle1 = float(i * 360 / points)
    let angle2 = float((i * 2 + 1) * 180 / points)

    # Outer point
    vertices[i * 2] = (
      cos(radians(angle1)) * 25.0,
      sin(radians(angle1)) * 25.0
    )

    # Inner point
    vertices[i * 2 + 1] = (
      cos(radians(angle2)) * 10.0,
      sin(radians(angle2)) * 10.0
    )

  polygon(Fill, vertices)
  pop()

# ============================================================================
# Template for Common Patterns
# ============================================================================

template withTransform*(body: untyped) =
  ## Execute drawing code with pushed transform stack
  push()
  body
  pop()

template withColor*(color: uint32, body: untyped) =
  ## Execute drawing code with a specific color, then restore original
  let oldColor = getColor()
  setColor(color)
  body
  setColor(oldColor)

template withStyle*(strokeColor: uint32 = 0, fillColor: uint32 = 0,
                   lineWidth: float = 1.0, body: untyped) =
  ## Execute drawing code with specific style, then restore
  let oldColor = getColor()
  let oldLineWidth = getLineWidth()

  if strokeColor != 0:
    setColor(strokeColor)
  if fillColor != 0:
    setColor(fillColor)
  if lineWidth > 0:
    setLineWidth(lineWidth)

  body
  setColor(oldColor)
  setLineWidth(oldLineWidth)