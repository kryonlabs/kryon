## Text Input Demo
##
## Simple demo with a single text input field
## Matches the design from the original specification

import ../src/kryon
import ../src/backends/raylib_backend

let app = kryonApp:
  Header:
    windowWidth = 400
    windowHeight = 200
    windowTitle = "Text Input Demo"

  Body:
    backgroundColor = "#f8f9fa"

    Center:
      Text:
        text = "Enter your name:"
        fontSize = 16
        color = "#6b7280"

      Input:
        width = 250
        height = 40
        placeholder = "Type here..."
        fontSize = 14
        backgroundColor = "#ffffff"

# Run the application
var backend = newRaylibBackendFromApp(app)
backend.run(app)