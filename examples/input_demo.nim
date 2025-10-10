## Input Demo
##
## Demonstrates text input functionality with keyboard handling
## Note: Reactive state (auto-updating UI) will be added in Phase 4

import ../src/kryon
import ../src/backends/raylib_backend

proc handleSubmit() =
  echo "Submit button clicked!"
  echo "(Note: Input value extraction will be added with reactive state in Phase 4)"

let app = kryonApp:
  Container:
    width = 600
    height = 400
    backgroundColor = "#2C3E50"

    Center:
      Column:
        gap = 16

        Text:
          text = "Input Element Demo"
          color = "#ECF0F1"
          fontSize = 28

        Text:
          text = "Click the input below and start typing"
          color = "#BDC3C7"
          fontSize = 16

        Input:
          width = 500
          height = 45
          placeholder = "Type something here..."
          value = ""
          fontSize = 20

        Text:
          text = "Try clicking to focus, typing, backspace, etc."
          color = "#BDC3C7"
          fontSize = 14

        Input:
          width = 500
          height = 45
          placeholder = "Another input field"
          value = "Pre-filled text"
          fontSize = 20

        Button:
          width = 500
          height = 50
          text = "Submit"
          backgroundColor = "#27AE60"
          fontSize = 20
          onClick = handleSubmit

# Run the application
var backend = newRaylibBackend(600, 400, "Input Demo - Kryon")
backend.run(app)
