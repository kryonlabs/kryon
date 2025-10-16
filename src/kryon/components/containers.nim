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
    sortedChildren*: seq[Element]

  RowData* = object
    ## Extracted row rendering data
    sortedChildren*: seq[Element]

proc extractColumnData*(elem: Element, inheritedColor: Option[Color] = none(Color)): ColumnData =
  ## Extract column properties from element
  ##
  ## Layout is handled by the layout engine (mainAxisAlignment, crossAxisAlignment, gap).
  ## This just provides sorted children for rendering.

  # Column doesn't render itself, just provides children (sorted by z-index)
  result.sortedChildren = sortChildrenByZIndex(elem.children)

proc extractRowData*(elem: Element, inheritedColor: Option[Color] = none(Color)): RowData =
  ## Extract row properties from element
  ##
  ## Layout is handled by the layout engine (mainAxisAlignment, crossAxisAlignment, gap).
  ## This just provides sorted children for rendering.

  # Row doesn't render itself, just provides children (sorted by z-index)
  result.sortedChildren = sortChildrenByZIndex(elem.children)
