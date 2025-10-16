## Checkbox Component
##
## Extracts checkbox properties for rendering.
## Manages checkbox checked state.

import ../core
import ../state/backendState
import ../state/checkboxManager
import ../rendering/renderingContext
import options
import math

type
  CheckmarkSegment* = object
    x*, y*: float
    thickness*: float

  CheckboxData* = object
    ## Extracted checkbox rendering data
    x*, y*, width*, height*: float
    checkboxX*, checkboxY*, checkboxSize*: float
    backgroundColor*: Color
    borderColor*: Color
    borderWidth*: float
    checked*: bool
    checkColor*: Color
    checkThickness*: float
    checkmarkSegments1*: seq[CheckmarkSegment]
    checkmarkSegments2*: seq[CheckmarkSegment]
    hasLabel*: bool
    label*: string
    labelX*, labelY*: float
    labelColor*: Color
    fontSize*: int

proc extractCheckboxData*(elem: Element, state: var BackendState,
                          inheritedColor: Option[Color] = none(Color)): CheckboxData =
  ## Extract checkbox properties from element and update state
  ##
  ## Properties supported:
  ##   - label: Text label next to checkbox (default: "")
  ##   - checked: Initial checked state (default: false)
  ##   - fontSize: Text size for label (default: 16)
  ##   - backgroundColor: Checkbox background (default: #FFFFFF)
  ##   - color: Text color for label (default: #000000)
  ##   - borderWidth: Border thickness (default: 1.0)
  ##   - borderColor: Border color (default: #CCCCCC)
  ##   - checkColor: Color of checkmark (default: #4A90E2)
  ##   - labelColor: Label text color (overrides color)

  # Get checkbox state
  let initialChecked = elem.getProp("checked").get(val(false)).getBool()
  initCheckbox(state, elem, initialChecked)
  result.checked = isChecked(state, elem, initialChecked)

  # Extract properties
  result.label = elem.getProp("label").get(val("")).getString()
  result.fontSize = getFontSize(elem, 16)
  result.backgroundColor = getBackgroundColor(elem, "#FFFFFF")
  let textColor = elem.getProp("color").get(val("#000000")).getColor()
  result.borderWidth = getBorderWidth(elem, 1.0)
  result.borderColor = getBorderColor(elem, "#CCCCCC")
  result.checkColor = elem.getProp("checkColor").get(val("#4A90E2")).getColor()
  result.labelColor = elem.getProp("labelColor").get(val(textColor)).getColor()

  # Calculate checkbox square size and position
  result.checkboxSize = min(elem.height, result.fontSize.float + 8.0)
  result.checkboxX = elem.x
  result.checkboxY = elem.y + (elem.height - result.checkboxSize) / 2.0

  # Position
  result.x = elem.x
  result.y = elem.y
  result.width = elem.width
  result.height = elem.height

  # Prepare checkmark segments if checked
  if result.checked:
    let padding = result.checkboxSize * 0.2
    result.checkThickness = max(1.0, result.checkboxSize / 8.0)

    # Checkmark coordinates (✓ shape)
    let startX = result.checkboxX + padding
    let startY = result.checkboxY + result.checkboxSize / 2.0
    let middleX = result.checkboxX + result.checkboxSize / 2.5
    let middleY = result.checkboxY + result.checkboxSize - padding
    let endX = result.checkboxX + result.checkboxSize - padding
    let endY = result.checkboxY + padding

    # First part of checkmark (bottom-left to middle)
    let line1Length = sqrt(pow(middleX - startX, 2) + pow(middleY - startY, 2))
    let segments1 = int(line1Length / 2.0)
    result.checkmarkSegments1 = @[]
    for i in 0..<segments1:
      let t = i.float / segments1.float
      let x = startX + t * (middleX - startX) - result.checkThickness / 2
      let y = startY + t * (middleY - startY) - result.checkThickness / 2
      result.checkmarkSegments1.add(CheckmarkSegment(x: x, y: y, thickness: result.checkThickness))

    # Second part of checkmark (middle to top-right)
    let line2Length = sqrt(pow(endX - middleX, 2) + pow(endY - middleY, 2))
    let segments2 = int(line2Length / 2.0)
    result.checkmarkSegments2 = @[]
    for i in 0..<segments2:
      let t = i.float / segments2.float
      let x = middleX + t * (endX - middleX) - result.checkThickness / 2
      let y = middleY + t * (endY - middleY) - result.checkThickness / 2
      result.checkmarkSegments2.add(CheckmarkSegment(x: x, y: y, thickness: result.checkThickness))

  # Label position
  result.hasLabel = result.label.len > 0
  if result.hasLabel:
    result.labelX = result.checkboxX + result.checkboxSize + 10.0  # 10px spacing
    result.labelY = elem.y + (elem.height - result.fontSize.float) / 2.0
