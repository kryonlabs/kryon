## Layout Container Components
##
## Extracts flex layout container properties (Column, Row).
## These are layout-only elements that manage their children in specific arrangements.
## The actual layout is handled by the layout engine.

import ../core
import ../layout/zindexSort
import options

type
  ColumnData* = object
    ## Extracted column rendering data
    x*, y*, width*, height*: float
    hasBackground*: bool
    backgroundColor*: Color
    sortedChildren*: seq[Element]

  RowData* = object
    ## Extracted row rendering data
    x*, y*, width*, height*: float
    hasBackground*: bool
    backgroundColor*: Color
    sortedChildren*: seq[Element]

  SpacerData* = object
    ## Extracted spacer rendering data
    width*: float
    height*: float

proc extractColumnData*(elem: Element, inheritedColor: Option[Color] = none(Color)): ColumnData =
  ## Extract column properties from element
  ##
  ## Layout is handled by the layout engine (mainAxisAlignment, crossAxisAlignment, gap).
  ## This provides children (sorted by z-index) and background rendering.

  result.x = elem.x
  result.y = elem.y
  result.width = elem.width
  result.height = elem.height

  # Check for background (same logic as Container)
  let bgColor = elem.getProp("backgroundColor")
  if bgColor.isNone:
    let bg = elem.getProp("background")
    if bg.isSome:
      result.hasBackground = true
      result.backgroundColor = bg.get.getColor()
  elif bgColor.isSome:
    result.hasBackground = true
    result.backgroundColor = bgColor.get.getColor()

  # Sort children by z-index
  result.sortedChildren = sortChildrenByZIndex(elem.children)

proc extractSpacerData*(elem: Element): SpacerData =
  ## Extract spacer properties from element
  ##
  ## Properties supported:
  ##   - width: Horizontal space (default: 0.0)
  ##   - height: Vertical space (default: 16.0)

  result.width = elem.getProp("width").get(val(0.0)).getFloat()
  result.height = elem.getProp("height").get(val(16.0)).getFloat()

proc extractRowData*(elem: Element, inheritedColor: Option[Color] = none(Color)): RowData =
  ## Extract row properties from element
  ##
  ## Layout is handled by the layout engine (mainAxisAlignment, crossAxisAlignment, gap).
  ## This provides children (sorted by z-index) and background rendering.

  result.x = elem.x
  result.y = elem.y
  result.width = elem.width
  result.height = elem.height

  # Check for background (same logic as Container)
  let bgColor = elem.getProp("backgroundColor")
  if bgColor.isNone:
    let bg = elem.getProp("background")
    if bg.isSome:
      result.hasBackground = true
      result.backgroundColor = bg.get.getColor()
  elif bgColor.isSome:
    result.hasBackground = true
    result.backgroundColor = bgColor.get.getColor()

  # Sort children by z-index
  result.sortedChildren = sortChildrenByZIndex(elem.children)
