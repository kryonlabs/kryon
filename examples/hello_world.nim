## Hello World Example

import ../src/kryon
import ../src/backends/raylib_backend

# Define the UI
let app = kryonApp:
  Container:
    posX = 200
    posY = 100
    width = 200
    height = 100
    backgroundColor = "#191970FF"

    Text:
      text = "Hello World"
      color = "yellow"

# Run the application
when isMainModule:
  var backend = newRaylibBackend(600, 400, "Hello World Example")
  backend.run(app)
