## Counters Demo
##
## Demonstrates reusable Counter components with independent state
## Each counter maintains its own value and can be incremented/decremented independently

import kryon_dsl

# Define a reusable Counter component with automatic reactive detection
# The {.kryonComponent.} pragma automatically:
# 1. Detects variables used in onClick handlers
# 2. Detects variables used in text expressions
# 3. Transforms them to namedReactiveVar for serialization
proc Counter*(initialValue: int = 0): Element {.kryonComponent.} =
  # Each counter instance has its own state variable
  var value = initialValue

  # Return the counter UI
  result = Row:
    alignItems = "center"
    gap = 32

    Button:
      text = "-"
      width = 60
      height = 50
      backgroundColor = "#E74C3C"
      fontSize = 24
      onClick = proc() = value -= 1

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
      onClick = proc() = value += 1

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
      crossAxisAlignment = "center"
      gap = 20

      Counter(5)

      Counter()
