## Test without Link widget

import ../src/kryon

let app* = kryonApp:
  Header:
    windowWidth = 800
    windowHeight = 600
    windowTitle = "Test Without Link"

  Body:
    backgroundColor = "#FFFFFF"

    Column:
      gap = 20
      padding = 40

      Text:
        text = "Test Without Link"
        fontSize = 24
        color = "#333333"

      Button:
        text = "Test Button"

      Text:
        text = "This should work"
        fontSize = 16
        color = "#666666"