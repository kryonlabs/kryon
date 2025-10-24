## Render Commands
##
## This module defines the intermediate representation between component extraction
## and actual rendering. RenderCommands are backend-agnostic drawing instructions
## that can be executed by any renderer implementing the Renderer interface.
##
## Benefits:
## - Separates "what to draw" from "how to draw it"
## - Enables render command batching and optimization
## - Makes rendering debuggable (can inspect commands)
## - Keeps backends simple (just execute commands)

import ../core
import options

type
  RenderCommandKind* = enum
    ## Types of rendering operations
    rcDrawRectangle
    rcDrawBorder
    rcDrawText
    rcDrawImage
    rcDrawLine
    rcBeginClip
    rcEndClip
    rcDrawCanvas
    rcDrawPath
    rcClearCanvas
    rcSaveCanvasState
    rcRestoreCanvasState

  RenderCommand* = object
    ## A single drawing command to be executed by the renderer
    case kind*: RenderCommandKind
    of rcDrawRectangle:
      rectX*, rectY*, rectWidth*, rectHeight*: float
      rectColor*: Color
    of rcDrawBorder:
      borderX*, borderY*, borderWidth*, borderHeight*: float
      borderThickness*: float
      borderColor*: Color
    of rcDrawText:
      textContent*: string
      textX*, textY*: float
      textSize*: int
      textColor*: Color
    of rcDrawImage:
      imagePath*: string
      imageX*, imageY*, imageWidth*, imageHeight*: float
    of rcDrawLine:
      lineX1*, lineY1*, lineX2*, lineY2*: float
      lineThickness*: float
      lineColor*: Color
    of rcBeginClip:
      clipX*, clipY*, clipWidth*, clipHeight*: float
    of rcEndClip:
      discard
    of rcDrawCanvas:
      canvasX*, canvasY*, canvasWidth*, canvasHeight*: float
      canvasDrawProc*: proc(ctx: DrawingContext, width, height: float) {.closure.}
      canvasBackgroundColor*: Option[Color]
    of rcDrawPath:
      pathCommands*: seq[PathCommand]
      pathFillStyle*: Color
      pathStrokeStyle*: Color
      pathLineWidth*: float
      pathShouldFill*: bool
      pathShouldStroke*: bool
    of rcClearCanvas:
      clearX*, clearY*, clearWidth*, clearHeight*: float
      clearColor*: Color
    of rcSaveCanvasState:
      discard
    of rcRestoreCanvasState:
      discard

  RenderCommandList* = seq[RenderCommand]

# ============================================================================
# Command Constructors
# ============================================================================

proc drawRectangle*(x, y, width, height: float, color: Color): RenderCommand =
  ## Create a rectangle drawing command
  RenderCommand(
    kind: rcDrawRectangle,
    rectX: x, rectY: y, rectWidth: width, rectHeight: height,
    rectColor: color
  )

proc drawBorder*(x, y, width, height, thickness: float, color: Color): RenderCommand =
  ## Create a border drawing command
  RenderCommand(
    kind: rcDrawBorder,
    borderX: x, borderY: y, borderWidth: width, borderHeight: height,
    borderThickness: thickness,
    borderColor: color
  )

proc drawText*(text: string, x, y: float, fontSize: int, color: Color): RenderCommand =
  ## Create a text drawing command
  RenderCommand(
    kind: rcDrawText,
    textContent: text,
    textX: x, textY: y, textSize: fontSize,
    textColor: color
  )

proc drawImage*(path: string, x, y, width, height: float): RenderCommand =
  ## Create an image drawing command
  RenderCommand(
    kind: rcDrawImage,
    imagePath: path,
    imageX: x, imageY: y, imageWidth: width, imageHeight: height
  )

proc drawLine*(x1, y1, x2, y2, thickness: float, color: Color): RenderCommand =
  ## Create a line drawing command
  RenderCommand(
    kind: rcDrawLine,
    lineX1: x1, lineY1: y1, lineX2: x2, lineY2: y2,
    lineThickness: thickness,
    lineColor: color
  )

proc beginClipping*(x, y, width, height: float): RenderCommand =
  ## Create a begin clipping command
  RenderCommand(
    kind: rcBeginClip,
    clipX: x, clipY: y, clipWidth: width, clipHeight: height
  )

proc endClipping*(): RenderCommand =
  ## Create an end clipping command
  RenderCommand(kind: rcEndClip)

proc drawCanvas*(x, y, width, height: float, drawProc: proc(ctx: DrawingContext, width, height: float) {.closure.}, backgroundColor: Option[Color] = none(Color)): RenderCommand =
  ## Create a canvas drawing command
  RenderCommand(
    kind: rcDrawCanvas,
    canvasX: x, canvasY: y, canvasWidth: width, canvasHeight: height,
    canvasDrawProc: drawProc,
    canvasBackgroundColor: backgroundColor
  )

proc drawPath*(commands: seq[PathCommand], fillStyle: Color, strokeStyle: Color, lineWidth: float, shouldFill: bool, shouldStroke: bool): RenderCommand =
  ## Create a path drawing command
  RenderCommand(
    kind: rcDrawPath,
    pathCommands: commands,
    pathFillStyle: fillStyle,
    pathStrokeStyle: strokeStyle,
    pathLineWidth: lineWidth,
    pathShouldFill: shouldFill,
    pathShouldStroke: shouldStroke
  )

proc clearCanvas*(x, y, width, height: float, color: Color): RenderCommand =
  ## Create a clear canvas command
  RenderCommand(
    kind: rcClearCanvas,
    clearX: x, clearY: y, clearWidth: width, clearHeight: height,
    clearColor: color
  )

proc saveCanvasState*(): RenderCommand =
  ## Create a save canvas state command
  RenderCommand(kind: rcSaveCanvasState)

proc restoreCanvasState*(): RenderCommand =
  ## Create a restore canvas state command
  RenderCommand(kind: rcRestoreCanvasState)
