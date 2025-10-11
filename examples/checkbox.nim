## Hello World Example

import ../src/kryon
import ../src/backends/raylib_backend


proc testClick() =
  echo "Checkbox clicked!"

# Define the UI
let app = kryonApp:
  Header:
    width = 800
    height = 600
    title = "Checkbox Example"

  Body:
    contentAlignment = "center"
    backgroundColor = "#f0f0f0"

    Text:
      text = "Simple Checbox Test"
      color = "#333333"

    Checkbox:
      label = "Test checkbox"
      checked = true
      width = 250
      height = 30
      fontSize = 14
      onClick = testClick

    Checkbox:
      label = "Unchecked checkbox"
      checked = false
      width = 250
      height = 30
      fontSize = 14
      onClick = testClick
  
# Run the application
when isMainModule:
  var backend = newRaylibBackendFromApp(app)
  backend.run(app)
