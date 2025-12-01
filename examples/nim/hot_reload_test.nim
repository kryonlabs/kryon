## Simplest Hot Reload Test
## Edit this file and save to see changes

import kryon_dsl

proc buttonClicked() =
  echo "BUTTON CLICKED!"

discard kryonApp:
  Header:
    windowWidth = 500
    windowHeight = 300
    windowTitle = "Hot Reload Test"

  Body:
    backgroundColor = "#222222"

    Center:
      Button:
        text = "CLICK ME"
        width = 200
        height = 80
        fontSize = 24
        backgroundColor = "#ff6600"
        color = "#ffffff"
        onClick = buttonClicked
