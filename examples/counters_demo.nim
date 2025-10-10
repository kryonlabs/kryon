## Counters Demo
##
## Demonstrates reusable Counter components with independent state
## Each counter maintains its own value and can be incremented/decremented independently

import ../src/kryon
import ../src/backends/raylib_backend

# Define a reusable Counter component
# Each counter gets its own state (value) captured in a closure
proc Counter*(initialValue: int = 0): Element =
  # Each counter instance has its own state variable
  var value = initialValue

  # Event handlers that modify this counter's state
  proc increment() =
    value += 1
    echo "Counter value: ", value

  proc decrement() =
    value -= 1
    echo "Counter value: ", value

  # Return the counter UI
  result = Row:
    mainAxisAlignment = "center"
    gap = 32

    Button:
      text = "-"
      width = 60
      height = 50
      backgroundColor = "#E74C3C"
      fontSize = 24
      onClick = decrement

    Text:
      text = $value  # Reactive! Updates automatically
      fontSize = 32
      color = "#FFFFFF"

    Button:
      text = "+"
      width = 60
      height = 50
      backgroundColor = "#2ECC71"
      fontSize = 24
      onClick = increment

# --- Now, use your component in the main App ---
let app = kryonApp:
  Header:
    windowWidth = 800
    windowHeight = 600
    windowTitle = "Counters Demo"

  Body:
    backgroundColor = "#1E1E1E"

    Column:
      mainAxisAlignment = "center"
      gap = 20

      Counter(5)

      Counter()

# Run the application
when isMainModule:
  echo "=== COUNTERS DEMO ==="
  echo "Two independent counters with their own state"
  echo "Click +/- buttons to increment/decrement each counter"
  echo ""

  var backend = newRaylibBackendFromApp(app)
  backend.run(app)
