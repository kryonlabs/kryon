## Custom Component Demo
##
## Simple function-based custom component example

import ../src/kryon
import ../src/backends/raylib_backend

# Simple custom component function
proc Counter*(initial: int): Element =
  var count = initial

  Container:
    width = 200
    height = 120
    backgroundColor = "#404040"

    Column:
      gap = 10

      Text:
        text = "Count: " & $count
        fontSize = 16
        color = "white"

      Button:
        width = 100
        height = 40
        text = "Click!"
        fontSize = 14
        backgroundColor = "#4A90E2"
        onClick = proc() =
          count += 1
          echo "Count is now: ", count

# Use the custom component
let app = kryonApp:
  Header:
    windowWidth = 800
    windowHeight = 600
    windowTitle = "Custom Component Demo"

  Body:
    Text:
      text = "hello"
    Counter(0)

# Run
when isMainModule:
  var backend = newRaylibBackendFromApp(app)
  backend.run(app)