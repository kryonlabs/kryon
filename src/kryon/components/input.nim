## Input Component
##
## Extracts input field properties for rendering.
## Manages focus, cursor, and scrolling state.

import ../core
import ../state/backendState
import ../state/inputManager
import ../rendering/renderingContext
import options

type
  InputData* = object
    ## Extracted input rendering data
    x*, y*, width*, height*: float
    value*: string
    displayText*: string
    backgroundColor*: Color
    textColor*: Color
    displayColor*: Color
    fontSize*: int
    borderWidth*: float
    borderColor*: Color
    isFocused*: bool
    showCursor*: bool
    cursorX*, cursorY*: float
    scrollOffset*: float
    padding*: float
    visibleWidth*: float

proc extractInputData*(elem: Element, state: var BackendState,
                       inheritedColor: Option[Color] = none(Color)): InputData =
  ## Extract input properties from element and update state
  ##
  ## Properties supported:
  ##   - value: Initial/bound value (default: "")
  ##   - placeholder: Placeholder text (default: "")
  ##   - fontSize: Text size (default: 20)
  ##   - backgroundColor: Background color (default: #FFFFFF)
  ##   - color: Text color (default: #000000)
  ##   - borderWidth: Border thickness (default: 1.0, 2.0 when focused)
  ##   - borderColor: Border color (default: #CCCCCC, #4A90E2 when focused)

  # Get current value (prioritize state over property for user input)
  let reactiveValue = elem.getProp("value").get(val("")).getString()
  result.value = getInputValue(state, elem, reactiveValue)

  # Sync reactive value changes when not focused
  let focused = isFocused(state, elem)
  if not focused and reactiveValue != result.value:
    setInputValue(state, elem, reactiveValue)

  # Extract properties
  let placeholder = elem.getProp("placeholder").get(val("")).getString()
  result.fontSize = getFontSize(elem, 20)
  result.backgroundColor = getBackgroundColor(elem, "#FFFFFF")
  result.textColor = elem.getProp("color").get(val("#000000")).getColor()

  # Get border properties (different defaults when focused)
  let borderWidth = getBorderWidth(elem, 1.0)
  let defaultBorderColor = if focused: "#4A90E2" else: "#CCCCCC"
  result.borderColor = elem.getProp("borderColor").get(val(defaultBorderColor)).getColor()
  result.borderWidth = if focused and borderWidth == 1.0: 2.0 else: borderWidth

  # Determine what text to display
  result.displayText = if result.value.len > 0: result.value else: placeholder
  result.displayColor = if result.value.len > 0: result.textColor else: parseColor("#999999")

  # Set up clipping for text scrolling
  result.padding = 8.0
  result.visibleWidth = elem.width - (result.padding * 2.0)

  # Cursor data
  result.isFocused = focused
  result.showCursor = shouldShowCursor(state, elem)
  result.scrollOffset = getInputScrollOffset(state, elem)
  result.cursorY = elem.y + 4.0

  # Position
  result.x = elem.x
  result.y = elem.y
  result.width = elem.width
  result.height = elem.height
