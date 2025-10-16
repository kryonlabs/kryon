## Dropdown Component
##
## Extracts dropdown properties for rendering.
## Manages dropdown open/closed state and selected option.

import ../core
import ../state/backendState
import ../state/dropdownManager
import ../rendering/renderingContext
import options
import math

type
  DropdownOption* = object
    text*: string
    x*, y*, width*, height*: float
    isHovered*: bool
    isSelected*: bool
    textX*, textY*: float
    maxTextWidth*: float

  DropdownButtonData* = object
    ## Extracted dropdown button rendering data
    x*, y*, width*, height*: float
    backgroundColor*: Color
    borderColor*: Color
    borderWidth*: float
    fontSize*: int
    textColor*: Color
    isOpen*: bool
    arrowChar*: string
    arrowX*, arrowY*: float
    displayText*: string
    displayColor*: Color
    textX*, textY*: float
    maxTextWidth*: float
    textPadding*: float

  DropdownMenuData* = object
    ## Extracted dropdown menu rendering data
    isOpen*: bool
    x*, y*, width*, height*: float
    backgroundColor*: Color
    borderColor*: Color
    borderWidth*: float
    fontSize*: int
    textColor*: Color
    itemHeight*: float
    textPadding*: float
    options*: seq[DropdownOption]
    hoveredHighlightColor*: Color
    selectedHighlightColor*: Color

proc extractDropdownButtonData*(elem: Element, state: var BackendState,
                                 inheritedColor: Option[Color] = none(Color)): DropdownButtonData =
  ## Extract dropdown button properties from element
  ##
  ## Properties supported:
  ##   - placeholder: Text when no selection (default: "Select...")
  ##   - fontSize: Text size (default: 16)
  ##   - backgroundColor: Background color (default: #FFFFFF)
  ##   - color: Text color (default: #000000)
  ##   - borderWidth: Border thickness (default: 1.0)
  ##   - borderColor: Border color (default: #CCCCCC)

  # Extract properties
  result.isOpen = isDropdownOpen(elem)
  result.fontSize = getFontSize(elem, 16)
  result.backgroundColor = getBackgroundColor(elem, "#FFFFFF")
  result.textColor = elem.getProp("color").get(val("#000000")).getColor()
  result.borderWidth = getBorderWidth(elem, 1.0)
  result.borderColor = getBorderColor(elem, "#CCCCCC")
  let placeholder = elem.getProp("placeholder").get(val("Select...")).getString()

  # Position
  result.x = elem.x
  result.y = elem.y
  result.width = elem.width
  result.height = elem.height

  # Arrow indicator
  result.arrowChar = if result.isOpen: "^" else: "v"
  result.arrowX = elem.x + elem.width - 15.0
  result.arrowY = elem.y + (elem.height - result.fontSize.float) / 2.0

  # Display text
  let selectedText = getSelectedOption(elem)
  result.displayText = if selectedText.len > 0: selectedText else: placeholder
  result.displayColor = if selectedText.len > 0: result.textColor else: parseColor("#999999")

  # Text positioning
  result.textPadding = 10.0
  result.textX = elem.x + result.textPadding
  result.textY = elem.y + (elem.height - result.fontSize.float) / 2.0
  result.maxTextWidth = elem.width - (result.textPadding * 2.0) - 20.0  # Leave space for arrow

proc extractDropdownMenuData*(elem: Element, state: var BackendState,
                               inheritedColor: Option[Color] = none(Color)): DropdownMenuData =
  ## Extract dropdown menu properties from element
  ##
  ## Returns data needed to render the dropdown menu (when open)

  # Check if should render
  result.isOpen = isDropdownOpen(elem)
  if not result.isOpen or elem.dropdownOptions.len == 0:
    return

  # Extract properties
  let options = elem.dropdownOptions
  let selectedIndex = getSelectedIndex(elem)
  let hoveredIndex = getDropdownHoverIndex(elem)
  result.fontSize = getFontSize(elem, 16)
  result.backgroundColor = getBackgroundColor(elem, "#FFFFFF")
  result.textColor = elem.getProp("color").get(val("#000000")).getColor()
  result.borderWidth = getBorderWidth(elem, 1.0)
  result.borderColor = getBorderColor(elem, "#CCCCCC")

  # Calculate dropdown menu dimensions
  result.itemHeight = result.fontSize.float + 10.0
  result.textPadding = 10.0
  result.width = elem.width
  result.height = min(options.len.float * result.itemHeight, 200.0)  # Max 200px
  result.x = elem.x
  result.y = elem.y + elem.height  # Below main button

  # Highlight colors
  result.hoveredHighlightColor = parseColor("#E3F2FD")  # Light blue
  result.selectedHighlightColor = parseColor("#BBDEFB")  # Medium blue

  # Prepare option rendering data
  let maxVisibleItems = int(result.height / result.itemHeight)
  let startIndex = 0  # Simple implementation - no scrolling
  let endIndex = min(options.len, startIndex + maxVisibleItems)

  result.options = @[]
  for i in startIndex..<endIndex:
    var opt: DropdownOption
    opt.text = options[i]
    opt.y = result.y + (i - startIndex).float * result.itemHeight
    opt.x = result.x
    opt.width = result.width
    opt.height = result.itemHeight
    opt.isHovered = (i == hoveredIndex)
    opt.isSelected = (i == selectedIndex)
    opt.textX = result.x + result.textPadding
    opt.textY = opt.y + (result.itemHeight - result.fontSize.float) / 2.0
    opt.maxTextWidth = result.width - (result.textPadding * 2.0)
    result.options.add(opt)
