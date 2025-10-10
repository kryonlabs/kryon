## Test Body Relative Layout
##
## This example demonstrates that Body correctly stacks children vertically
## with proper spacing, making all elements visible

import ../src/kryon
import ../src/backends/raylib_backend

# State for interactive demo
var clickCount = 0

proc handleClick() =
  clickCount.inc()
  echo "Button clicked! Count: ", clickCount

# Define UI with various elements stacked vertically in Body
let app = kryonApp:
  Header:
    width = 800
    height = 600
    title = "Body Relative Layout Test"

  # All these elements should stack vertically with 5px gaps (default Body gap)
  Text:
    text = "1. First Text Element (Yellow)"
    color = "yellow"
    fontSize = 24

  Text:
    text = "2. Second Text Element (Cyan)"
    color = "cyan"
    fontSize = 20

  Button:
    text = "3. Click Me! (Button)"
    backgroundColor = "#4A90E2"
    onClick = handleClick

  Container:
    width = 300
    height = 80
    backgroundColor = "#191970FF"
    borderColor = "#0099FFFF"
    borderWidth = 2

    Text:
      posX = 10
      posY = 10
      text = "4. Container with Text Inside"
      color = "white"

  Text:
    text = "5. Third Text Element (Lime)"
    color = "lime"
    fontSize = 18

  Text:
    text = "All elements should be visible and stacked vertically!"
    color = "orange"
    fontSize = 16

# Run the application
when isMainModule:
  echo "Starting Body Relative Layout Test..."
  echo "Expected: All elements stacked vertically with 5px gaps"
  echo "All text and the button should be visible on screen\n"

  var backend = newRaylibBackendFromApp(app)
  backend.run(app)
