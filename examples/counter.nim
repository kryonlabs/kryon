## Counter Example - Kryon Nim
##
## Shows state management with increment/decrement buttons

import ../src/kryon
import ../src/backends/raylib_backend

# State - just a regular Nim variable
var count = 0

# Event handlers
proc increment() =
  count = count + 1
  echo "Count: ", count

proc decrement() =
  count = count - 1
  echo "Count: ", count

# Define the UI
let app = kryonApp:
  Container:
    width: 600
    height: 400
    backgroundColor: "#191919FF"

    Center:
      Column:
        gap: 16

        Text:
          text: "Counter Example"
          color: "#FFD700"

        Text:
          text: "Count: " & $count
          color: "#FFFFFF"

        Row:
          gap: 8

          Button:
            width: 60
            height: 40
            text: "-"
            backgroundColor: "#E74C3C"
            onClick: decrement

          Button:
            width: 60
            height: 40
            text: "+"
            backgroundColor: "#2ECC71"
            onClick: increment

# Run the application
when isMainModule:
  var backend = newRaylibBackend(600, 400, "Counter - Kryon")
  backend.run(app)
