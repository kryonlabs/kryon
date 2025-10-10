## Body Layout Test - Verify relative positioning works
##
## This test verifies that Body stacks children vertically by default

import ../src/kryon
import ../src/backends/raylib_backend

# Define the UI - direct children of Body should stack vertically
let app = kryonApp:
  Header:
    width = 800
    height = 600
    title = "Body Layout Test"

  Text:
    text = "First Text Element"
    color = "yellow"

  Text:
    text = "Second Text Element"
    color = "cyan"

  Container:
    width = 200
    height = 100
    backgroundColor = "#191970FF"
    borderColor = "#0099FFFF"
    borderWidth = 2

    Text:
      text = "Container Text"
      color = "white"

  Text:
    text = "Third Text Element"
    color = "lime"

  Button:
    text = "Click Me"
    backgroundColor = "#4A90E2"
    onClick = proc() = echo "Button clicked!"

# Run the application
when isMainModule:
  var backend = newRaylibBackendFromApp(app)
  backend.run(app)
