## Hello World Example - Kryon Nim
##
## A simple example showing a container with text

import ../src/kryon
import ../src/backends/raylib_backend

# Define the UI
let app = kryonApp:
  Container:
    width: 400
    height: 300
    backgroundColor: "#191970FF"

    Center:
      Text:
        text: "Hello World from Kryon-Nim!"
        color: "#FFD700"

# Run the application
when isMainModule:
  var backend = newRaylibBackend(400, 300, "Hello World - Kryon")
  backend.run(app)
