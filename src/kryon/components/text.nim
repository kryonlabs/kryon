## Text Component
##
## Extracts text properties for rendering.
## Supports color inheritance from parent elements.

import ../core
import ../rendering/renderingContext
import options

type
  TextAlignment* = enum
    ## Text alignment options
    taLeft = "left"
    taCenter = "center"
    taRight = "right"

  TextData* = object
    ## Extracted text rendering data
    text*: string
    x*, y*: float
    fontSize*: int
    color*: Color
    alignment*: TextAlignment

proc extractTextData*(elem: Element, inheritedColor: Option[Color] = none(Color)): TextData =
  ## Extract text properties from element
  ##
  ## Properties supported:
  ##   - text: Text content (default: "")
  ##   - color: Text color (inherits from parent if not set)
  ##   - fontSize: Text size (default: 20)
  ##   - textAlignment: Text alignment (left, center, right; default: left)

  result.text = elem.getProp("text").get(val("")).getString()
  result.color = getTextColor(elem, inheritedColor, "#FFFFFF")
  result.fontSize = getFontSize(elem, 20)
  result.x = elem.x
  result.y = elem.y

  # Extract text alignment
  let alignmentProp = elem.getProp("textAlignment").get(val("left")).getString()
  result.alignment = case alignmentProp:
    of "center": taCenter
    of "right": taRight
    else: taLeft
