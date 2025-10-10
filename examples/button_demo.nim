## Button Demo - Kryon Nim
##
## Shows interactive button with click handler

import ../src/kryon
import ../src/backends/raylib_backend

# Event handler
proc handleButtonClick() =
  echo "Button clicked!"

# Define the UI
let app = kryonApp:
  Container:
    width: 600
    height: 400
    backgroundColor: "#191919FF"

    Center:
      Button:
        width: 150
        height: 50
        text: "Click Me!"
        backgroundColor: "#4A90E2"
        color: "#FFFFFF"
        onClick: handleButtonClick

# Run the application
when isMainModule:
  var backend = newRaylibBackend(600, 400, "Button Demo - Kryon")
  backend.run(app)
