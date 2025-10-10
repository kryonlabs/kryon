## Test Center Layout
import ../src/kryon
import ../src/backends/raylib_backend

let app = kryonApp:
  Container:
    width = 600
    height = 400
    backgroundColor = "#2C3E50"

    Center:
      Column:
        gap = 16

        Text:
          text = "Centered Column"
          color = "#ECF0F1"
          fontSize = 28

        Input:
          width = 400
          height = 45
          placeholder = "Input in centered column"
          value = ""
          fontSize = 20

        Button:
          width = 400
          height = 50
          text = "Centered Button"
          backgroundColor = "#27AE60"
          fontSize = 20

# Run the application
var backend = newRaylibBackend(600, 400, "Center Layout Test")
backend.run(app)
