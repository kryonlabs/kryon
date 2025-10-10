## Interactive Button Demo
##
## Matches the original button.kry example
## Shows button with click handler

import ../src/kryon
import ../src/backends/raylib_backend

proc handleButtonClick() =
  echo "🎯 Button clicked! Hello from Kryon-Nim!"

# Define the UI (matches original button.kry)
let app = kryonApp:
  Container:
    windowWidth = 600
    windowHeight = 400
    windowTitle = "Interactive Button Demo"
    backgroundColor = "#191919FF"

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
