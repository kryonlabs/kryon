## Debug Clicks - Test if buttons register clicks

import ../src/kryon
import ../src/backends/raylib_backend

# Simple test with ONE button
var clickCount = 0

proc handleClick() =
  clickCount += 1
  echo "BUTTON CLICKED! Count: ", clickCount

let app = kryonApp:
  Header:
    windowWidth = 800
    windowHeight = 600
    windowTitle = "Debug Clicks"

  Body:
    backgroundColor = "#1E1E1E"

    Button:
      text = "Click Me"
      width = 200
      height = 80
      backgroundColor = "#2ECC71"
      fontSize = 24
      onClick = handleClick

when isMainModule:
  echo "=== DEBUG CLICKS TEST ==="
  echo "Click the button - you should see 'BUTTON CLICKED!' messages"
  echo ""

  var backend = newRaylibBackendFromApp(app)
  backend.run(app)
