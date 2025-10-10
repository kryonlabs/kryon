## Const Expression Test
##
## Demonstrates const variables and expression evaluation in properties
## Tests arithmetic operations (+, -, *, /) and conditional expressions

import ../src/kryon
import ../src/backends/raylib_backend

# Define const values
const containerWidth = 800
const baseHeight = 60
const margin = 10
const isEnabled = false

# Define the UI
let app = kryonApp:
  Header:
    windowWidth = containerWidth
    windowHeight = 400
    windowTitle = "Expression Evaluation Test"

  Body:
    backgroundColor = "#f0f0f0"
    color = "black"
    Column:
      gap = 20
      padding = margin

      Text:
        text = "Testing Arithmetic Expressions"
        fontSize = 18
        fontWeight = "bold"

      Container:
        width = containerWidth - 40
        height = baseHeight + 20
        backgroundColor = "#e0e0e0"
        padding = margin

        Text:
          text = "Width calculated as: containerWidth - 40"
          fontSize = 14

      Container:
        width = containerWidth / 2
        height = baseHeight * 2
        backgroundColor = "#d0d0d0"
        padding = margin + 5

        Text:
          text = "Width: containerWidth / 2, Height = baseHeight * 2"
          fontSize = 14

      Container:
        width = 300
        height = baseHeight
        backgroundColor = (if isEnabled: "#90EE90" else: "#FFB6C1")
        padding = margin

        Text:
          text = (if isEnabled: "Enabled (green)" else: "Disabled (pink)")
          fontSize = 14

# Run the application
when isMainModule:
  echo "=== CONST EXPRESSION TEST ==="
  echo "Tests const variables and arithmetic expressions"
  echo "- containerWidth = ", containerWidth
  echo "- baseHeight = ", baseHeight
  echo "- margin = ", margin
  echo "- isEnabled = ", isEnabled
  echo ""

  var backend = newRaylibBackendFromApp(app)
  backend.run(app)
