## Container Component
##
## Extracts container properties for rendering.
## Container elements have backgrounds/borders and manage child rendering.

import ../core
import ../rendering/renderingContext
import ../layout/zindexSort
import options

type
  ContainerData* = object
    ## Extracted container rendering data
    x*, y*, width*, height*: float
    hasBackground*: bool
    backgroundColor*: Color
    hasBorder*: bool
    borderColor*: Color
    borderWidth*: float
    sortedChildren*: seq[Element]

  BodyData* = object
    ## Extracted body rendering data
    colorToInherit*: Option[Color]
    sortedChildren*: seq[Element]

  CenterData* = object
    ## Extracted center rendering data
    sortedChildren*: seq[Element]

proc extractContainerData*(elem: Element, inheritedColor: Option[Color] = none(Color)): ContainerData =
  ## Extract container properties from element
  ##
  ## Properties supported:
  ##   - backgroundColor/background: Background color
  ##   - borderColor: Border color
  ##   - borderWidth: Border thickness (default: 1.0)

  result.x = elem.x
  result.y = elem.y
  result.width = elem.width
  result.height = elem.height

  # Check for background
  let bgColor = elem.getProp("backgroundColor")
  if bgColor.isNone:
    let bg = elem.getProp("background")
    if bg.isSome:
      result.hasBackground = true
      result.backgroundColor = bg.get.getColor()
  elif bgColor.isSome:
    result.hasBackground = true
    result.backgroundColor = bgColor.get.getColor()

  # Check for border
  let borderColor = elem.getProp("borderColor")
  if borderColor.isSome:
    result.hasBorder = true
    result.borderColor = borderColor.get.getColor()
    result.borderWidth = getBorderWidth(elem, 1.0)

  # Sort children by z-index
  result.sortedChildren = sortChildrenByZIndex(elem.children)

proc extractBodyData*(elem: Element, inheritedColor: Option[Color] = none(Color)): BodyData =
  ## Extract body properties from element
  ##
  ## Body is a wrapper that can specify color inheritance for text elements.

  # Check for color inheritance
  let bodyColor = elem.getProp("color")
  result.colorToInherit = if bodyColor.isSome:
    some(bodyColor.get.getColor())
  else:
    inheritedColor

  # Sort children by z-index
  result.sortedChildren = sortChildrenByZIndex(elem.children)

proc extractCenterData*(elem: Element, inheritedColor: Option[Color] = none(Color)): CenterData =
  ## Extract center properties from element
  ##
  ## Center is just like Container but with centered layout (handled by layout engine).

  # Sort children by z-index
  result.sortedChildren = sortChildrenByZIndex(elem.children)
