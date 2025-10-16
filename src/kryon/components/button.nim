## Button Component
##
## Extracts button properties for rendering.

import ../core
import ../rendering/renderingContext
import options

type
  ButtonData* = object
    ## Extracted button rendering data
    text*: string
    x*, y*, width*, height*: float
    backgroundColor*: Color
    textColor*: Color
    fontSize*: int
    borderWidth*: float
    borderColor*: Color

proc extractButtonData*(elem: Element, inheritedColor: Option[Color] = none(Color)): ButtonData =
  ## Extract button properties from element
  ##
  ## Properties supported:
  ##   - text: Button label (default: "Button")
  ##   - backgroundColor: Background color (default: #4A90E2)
  ##   - color: Text color (default: #FFFFFF)
  ##   - fontSize: Text size (default: 20)
  ##   - borderWidth: Border thickness (default: 2.0)
  ##   - borderColor: Border color (default: #D1D5DB)

  result.text = elem.getProp("text").get(val("Button")).getString()
  result.backgroundColor = elem.getProp("backgroundColor").get(val("#4A90E2")).getColor()
  result.textColor = elem.getProp("color").get(val("#FFFFFF")).getColor()
  result.fontSize = getFontSize(elem, 20)
  result.borderWidth = getBorderWidth(elem, 2.0)
  result.borderColor = getBorderColor(elem, "#D1D5DB")
  result.x = elem.x
  result.y = elem.y
  result.width = elem.width
  result.height = elem.height
