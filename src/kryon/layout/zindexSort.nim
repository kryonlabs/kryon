## Z-Index Sorting
##
## This module provides utilities for sorting elements by z-index for correct layered rendering.
## Elements with lower z-index are rendered first (behind), elements with higher z-index are rendered last (on top).

import ../core
import algorithm
import options

proc sortChildrenByZIndex*(children: seq[Element]): seq[Element] =
  ## Sort children by z-index for correct layered rendering
  ## Elements with lower z-index are rendered first (behind)
  ## Elements with higher z-index are rendered last (on top)
  ##
  ## Used during rendering to determine draw order
  result = children
  result.sort(proc(a, b: Element): int =
    let aZIndex = a.getProp("zIndex").get(val(0)).getInt()
    let bZIndex = b.getProp("zIndex").get(val(0)).getInt()
    result = cmp(aZIndex, bZIndex)
  )

proc sortChildrenByZIndexReverse*(children: seq[Element]): seq[Element] =
  ## Sort children by z-index in reverse order for input handling
  ## Elements with higher z-index should receive input first (on top)
  ##
  ## Used during input handling to process top-most elements first
  result = children
  result.sort(proc(a, b: Element): int =
    let aZIndex = a.getProp("zIndex").get(val(0)).getInt()
    let bZIndex = b.getProp("zIndex").get(val(0)).getInt()
    result = cmp(bZIndex, aZIndex)  # Reverse order
  )
