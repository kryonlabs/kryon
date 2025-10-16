## Rendering Context and Utilities
##
## This module provides shared utilities for rendering components,
## including common calculations and helper functions.

import ../core
import rendererInterface
import options

# ============================================================================
# Text Alignment Helpers
# ============================================================================

proc centerTextInRect*(textWidth, textHeight, rectX, rectY, rectWidth, rectHeight: float): tuple[x: float, y: float] =
  ## Calculate position to center text within a rectangle
  ##
  ## Args:
  ##   textWidth, textHeight: Dimensions of the text
  ##   rectX, rectY, rectWidth, rectHeight: Rectangle to center within
  ##
  ## Returns:
  ##   (x, y) position for top-left of text
  let x = rectX + (rectWidth - textWidth) / 2.0
  let y = rectY + (rectHeight - textHeight) / 2.0
  return (x: x, y: y)

proc centerTextHorizontally*(textWidth, rectX, rectWidth: float): float =
  ## Calculate X position to center text horizontally in a rectangle
  return rectX + (rectWidth - textWidth) / 2.0

proc centerTextVertically*(textHeight, rectY, rectHeight: float): float =
  ## Calculate Y position to center text vertically in a rectangle
  return rectY + (rectHeight - textHeight) / 2.0

# ============================================================================
# Property Extraction Helpers
# ============================================================================

proc getBackgroundColor*(elem: Element, default: string = "#FFFFFF"): Color =
  ## Get background color from element, with fallback to default
  let bgColor = elem.getProp("backgroundColor")
  if bgColor.isNone:
    let bg = elem.getProp("background")
    if bg.isSome:
      return bg.get.getColor()

  if bgColor.isSome:
    return bgColor.get.getColor()
  else:
    return parseColor(default)

proc getTextColor*(elem: Element, inheritedColor: Option[Color], default: string = "#000000"): Color =
  ## Get text color from element, considering inheritance and defaults
  let explicitColor = elem.getProp("color")
  if explicitColor.isSome:
    return explicitColor.get.getColor()
  elif inheritedColor.isSome:
    return inheritedColor.get
  else:
    return parseColor(default)

proc getBorderColor*(elem: Element, default: string = "#CCCCCC"): Color =
  ## Get border color from element with default
  elem.getProp("borderColor").get(val(default)).getColor()

proc getBorderWidth*(elem: Element, default: float = 1.0): float =
  ## Get border width from element with default
  elem.getProp("borderWidth").get(val(default)).getFloat()

proc getFontSize*(elem: Element, default: int = 16): int =
  ## Get font size from element with default
  elem.getProp("fontSize").get(val(default)).getInt()

proc getPadding*(elem: Element, default: float = 0.0): float =
  ## Get padding from element with default
  elem.getProp("padding").get(val(default)).getFloat()

proc getGap*(elem: Element, default: float = 0.0): float =
  ## Get gap from element with default
  elem.getProp("gap").get(val(default)).getFloat()

# ============================================================================
# Layout Property Helpers
# ============================================================================

proc getMainAxisAlignment*(elem: Element): Option[string] =
  ## Get main axis alignment property
  let prop = elem.getProp("mainAxisAlignment")
  if prop.isSome:
    return some(prop.get.getString())
  return none(string)

proc getCrossAxisAlignment*(elem: Element): Option[string] =
  ## Get cross axis alignment property
  let prop = elem.getProp("crossAxisAlignment")
  if prop.isSome:
    return some(prop.get.getString())
  return none(string)

proc getContentAlignment*(elem: Element): Option[string] =
  ## Get content alignment property
  let prop = elem.getProp("contentAlignment")
  if prop.isSome:
    return some(prop.get.getString())
  return none(string)

# ============================================================================
# Drawing Helpers
# ============================================================================

proc drawRectWithBorder*[R](renderer: var R, rect: Rect, bgColor, borderColor: Color, borderWidth: float) =
  ## Draw a rectangle with background and border
  ## Generic function that works with any renderer implementing the interface
  renderer.drawRectangle(rect.x, rect.y, rect.width, rect.height, bgColor)
  if borderWidth > 0:
    renderer.drawRectangleBorder(rect.x, rect.y, rect.width, rect.height, borderWidth, borderColor)

proc drawCenteredText*[R](renderer: var R, text: string, rect: Rect, fontSize: int, color: Color) =
  ## Draw text centered in a rectangle
  ## Generic function that works with any renderer implementing the interface
  let (textWidth, textHeight) = renderer.measureText(text, fontSize)
  let (textX, textY) = centerTextInRect(textWidth, textHeight, rect.x, rect.y, rect.width, rect.height)
  renderer.drawText(text, textX, textY, fontSize, color)
