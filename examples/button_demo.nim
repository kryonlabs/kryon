## Interactive Button Demo
##
## Shows explicit Header/Body syntax
## Demonstrates window configuration in Header

import ../src/kryon
import ../src/backends/raylib_backend

proc handleButtonClick() =
  echo "🎯 Button clicked! Hello from Kryon-Nim!"

# Define the UI with explicit Header and Body
let app = kryonApp:
  Header:
    windowWidth = 600
    windowHeight = 400
    windowTitle = "Interactive Button Demo"

  Body:
    backgroundColor = "#191919FF"  # Window background

    Center:
      Button:
        width = 150
        height = 50
        text = "Click Me!"
        backgroundColor = "#404080FF"
        onClick = handleButtonClick

# Run the application
when isMainModule:
  var backend = newRaylibBackendFromApp(app)
  backend.run(app)
