## Interactive Button Demo - Pure Kryon DSL

import kryon_dsl


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
        color = "pink"
        backgroundColor = "#404080FF"
        onClick = handleButtonClick

