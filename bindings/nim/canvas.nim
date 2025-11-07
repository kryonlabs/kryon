## Kryon Canvas - Drawing Context Implementation
##
## Provides HTML5 Canvas-like drawing API that maps to C core commands

import core_kryon, runtime

# ============================================================================
# Canvas Drawing Context
# ============================================================================

type
  Canvas* = ref object
    cmdBuf*: KryonCmdBuf
    width*: int
    height*: int
    fillColor*: uint32
    strokeColor*: uint32
    lineWidth*: int

# ============================================================================
# Canvas Creation
# ============================================================================

proc newCanvas*(width, height: int): Canvas =
  ## Create new drawing canvas
  result = Canvas()
  result.width = width
  result.height = height
  result.cmdBuf = createKryonCmdBuf()
  result.fillColor = rgb(0, 0, 0)
  result.strokeColor = rgb(0, 0, 0)
  result.lineWidth = 1

# ============================================================================
# Drawing State Management
# ============================================================================

proc setFillColor*(canvas: Canvas, r, g, b, a: int) =
  ## Set fill color
  canvas.fillColor = rgba(r, g, b, a)

proc setFillColor*(canvas: Canvas, color: uint32) =
  ## Set fill color
  canvas.fillColor = color

proc setStrokeColor*(canvas: Canvas, r, g, b, a: int) =
  ## Set stroke color
  canvas.strokeColor = rgba(r, g, b, a)

proc setStrokeColor*(canvas: Canvas, color: uint32) =
  ## Set stroke color
  canvas.strokeColor = color

proc setLineWidth*(canvas: Canvas, width: int) =
  ## Set line width
  canvas.lineWidth = width

# ============================================================================
# Basic Drawing Operations
# ============================================================================

proc fillRect*(canvas: Canvas, x, y, width, height: int) =
  ## Draw filled rectangle
  discard kryon_draw_rect(
    canvas.cmdBuf,
    int16(x), int16(y),
    uint16(width), uint16(height),
    canvas.fillColor
  )

proc strokeRect*(canvas: Canvas, x, y, width, height: int) =
  ## Draw rectangle outline
  # Draw 4 lines to create rectangle outline
  discard kryon_draw_line(canvas.cmdBuf, int16(x), int16(y), int16(x + width), int16(y), canvas.strokeColor)
  discard kryon_draw_line(canvas.cmdBuf, int16(x + width), int16(y), int16(x + width), int16(y + height), canvas.strokeColor)
  discard kryon_draw_line(canvas.cmdBuf, int16(x + width), int16(y + height), int16(x), int16(y + height), canvas.strokeColor)
  discard kryon_draw_line(canvas.cmdBuf, int16(x), int16(y + height), int16(x), int16(y), canvas.strokeColor)

proc clearRect*(canvas: Canvas, x, y, width, height: int) =
  ## Clear rectangle area
  # This would need to be implemented as transparent fill or background color
  discard kryon_draw_rect(
    canvas.cmdBuf,
    int16(x), int16(y),
    uint16(width), uint16(height),
    rgba(0, 0, 0, 0)  # Transparent
  )

proc drawLine*(canvas: Canvas, x1, y1, x2, y2: int) =
  ## Draw line
  discard kryon_draw_line(
    canvas.cmdBuf,
    int16(x1), int16(y1),
    int16(x2), int16(y2),
    canvas.strokeColor
  )

# ============================================================================
# Text Drawing
# ============================================================================

proc fillText*(canvas: Canvas, text: string, x, y: int) =
  ## Draw filled text
  discard kryon_draw_text(
    canvas.cmdBuf,
    cstring(text),
    int16(x), int16(y),
    uint16(0),  # Default font
    canvas.fillColor
  )

proc strokeText*(canvas: Canvas, text: string, x, y: int) =
  ## Draw text outline
  discard kryon_draw_text(
    canvas.cmdBuf,
    cstring(text),
    int16(x), int16(y),
    uint16(0),  # Default font
    canvas.strokeColor
  )

# ============================================================================
# Path Operations (Simplified)
# ============================================================================

proc beginPath*(canvas: Canvas) =
  ## Begin new path
  # This would start a new path context
  # Simplified implementation - paths not fully supported in C core yet
  discard

proc closePath*(canvas: Canvas) =
  ## Close current path
  # This would close the path
  discard

proc moveTo*(canvas: Canvas, x, y: int) =
  ## Move to position
  # This would move the pen without drawing
  discard

proc lineTo*(canvas: Canvas, x, y: int) =
  ## Draw line to position
  # This would add a line segment to the path
  discard

proc arc*(canvas: Canvas, cx, cy, radius: int, startAngle, endAngle: float, anticlockwise: bool = false) =
  ## Draw arc
  # Convert to degrees for C core
  let startDeg = int16((startAngle * 180.0 / PI) * 100)  # Fixed-point
  let endDeg = int16((endAngle * 180.0 / PI) * 100)

  discard kryon_draw_arc(
    canvas.cmdBuf,
    int16(cx), int16(cy),
    uint16(radius),
    startDeg, endDeg,
    canvas.fillColor
  )

proc fill*(canvas: Canvas) =
  ## Fill current path
  # This would fill the current path
  discard

proc stroke*(canvas: Canvas) =
  ## Stroke current path
  # This would stroke the current path
  discard

# ============================================================================
# Canvas Utilities
# ============================================================================

proc clear*(canvas: Canvas) =
  ## Clear entire canvas
  kryon_cmd_buf_clear(canvas.cmdBuf)

proc getWidth*(canvas: Canvas): int =
  ## Get canvas width
  canvas.width

proc getHeight*(canvas: Canvas): int =
  ## Get canvas height
  canvas.height

proc getCommandBuffer*(canvas: Canvas): KryonCmdBuf =
  ## Get underlying command buffer
  canvas.cmdBuf

# ============================================================================
# HTML5 Canvas Compatibility Functions
# ============================================================================

proc createLinearGradient*(canvas: Canvas, x0, y0, x1, y1: int): CanvasGradient =
  ## Create linear gradient (placeholder)
  result = CanvasGradient()

proc createRadialGradient*(canvas: Canvas, x0, y0, r0, x1, y1, r1: int): CanvasGradient =
  ## Create radial gradient (placeholder)
  result = CanvasGradient()

proc createPattern*(canvas: Canvas, image: CanvasImage, repetition: string): CanvasPattern =
  ## Create pattern (placeholder)
  result = CanvasPattern()

# ============================================================================
# Image Support (Simplified)
# ============================================================================

proc drawImage*(canvas: Canvas, image: CanvasImage, dx, dy: int) =
  ## Draw image at position
  # This would use kryon_draw_texture
  discard

proc getImageData*(canvas: Canvas, sx, sy, sw, sh: int): ImageData =
  ## Get image data from canvas (placeholder)
  result = ImageData()

proc putImageData*(canvas: Canvas, imageData: ImageData, dx, dy: int) =
  ## Put image data to canvas (placeholder)
  discard

# ============================================================================
# Type Definitions for Placeholder Features
# ============================================================================

type
  CanvasGradient* = ref object
  CanvasPattern* = ref object
  CanvasImage* = ref object
  ImageData* = ref object

# ============================================================================
# Mathematical Constants
# ============================================================================

const
  PI* = 3.141592653589793

# ============================================================================
# Canvas Animation Support
# =============================================================================

proc requestAnimationFrame*(callback: proc(timestamp: float)) =
  ## Request animation frame callback
  # This would integrate with the main application loop
  discard