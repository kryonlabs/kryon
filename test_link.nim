## Simple Link Test
##
## Test the Link widget implementation

import ../src/kryon

let app* = kryonApp:
  Header:
    windowWidth = 800
    windowHeight = 600
    windowTitle = "Link Test"

  Body:
    backgroundColor = "#FFFFFF"

    Column:
      gap = 20
      padding = 40

      Text:
        text = "Link Widget Test"
        fontSize = 24
        color = "#333333"

      Link:
        text = "Test Link"
        to = "test.nim"

      Text:
        text = "Click the link above to test navigation"
        fontSize = 16
        color = "#666666"