## Simple Input Test
import ../src/kryon
import ../src/backends/raylib_backend

let app = kryonApp:
  Container:
    width = 600
    height = 400
    backgroundColor = "#2C3E50"

    Input:
      width = 400
      height = 60
      placeholder = "Test placeholder"
      value = ""
      fontSize = 24

# Run the application
var backend = newRaylibBackend(600, 400, "Simple Input Test")
backend.run(app)
