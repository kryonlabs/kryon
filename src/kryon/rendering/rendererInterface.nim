## Renderer Interface
##
## This module defines the interface that rendering backends must implement.
## Uses duck typing - any type implementing these procedures can be used as a renderer.
##
## Backends must implement the following drawing primitives:
##
## Drawing Operations:
##   - drawRectangle(x, y, width, height, color)
##   - drawRectangleBorder(x, y, width, height, borderWidth, color)
##   - drawText(text, x, y, fontSize, color)
##   - drawLine(x1, y1, x2, y2, thickness, color)
##   - drawImage(imagePath, x, y, width, height)
##
## Clipping (for scroll areas, dropdowns):
##   - beginClipping(x, y, width, height)
##   - endClipping()
##
## Text Measurement:
##   - measureText(text, fontSize) -> (width, height)
##
## The component system will use these primitives to render UI elements
## in a backend-agnostic way.

## Renderer Trait Documentation
##
## A type implementing the Renderer interface should provide:
##
## Drawing Primitives:
##   proc drawRectangle*(r: var RendererType, x, y, width, height: float, color: Color)
##   proc drawRectangleBorder*(r: var RendererType, x, y, width, height, borderWidth: float, color: Color)
##   proc drawText*(r: var RendererType, text: string, x, y: float, fontSize: int, color: Color)
##   proc drawLine*(r: var RendererType, x1, y1, x2, y2, thickness: float, color: Color)
##   proc drawImage*(r: var RendererType, imagePath: string, x, y, width, height: float)
##
## Clipping:
##   proc beginClipping*(r: var RendererType, x, y, width, height: float)
##   proc endClipping*(r: var RendererType)
##
## Text Measurement:
##   proc measureText*(r: RendererType, text: string, fontSize: int): tuple[width: float, height: float]

# Helper type for geometric calculations
type
  Rect* = object
    x*, y*: float
    width*, height*: float

proc rect*(x, y, width, height: float): Rect =
  ## Create a rectangle
  Rect(x: x, y: y, width: width, height: height)

proc contains*(r: Rect, px, py: float): bool =
  ## Check if a point is inside a rectangle
  px >= r.x and px <= r.x + r.width and
  py >= r.y and py <= r.y + r.height

proc center*(r: Rect): tuple[x: float, y: float] =
  ## Get the center point of a rectangle
  (x: r.x + r.width / 2.0, y: r.y + r.height / 2.0)
