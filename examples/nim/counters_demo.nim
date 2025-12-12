## Generated from counters_demo_static.kir
## Kryon Code Generator - v3.0 Component-Based Output

import kryon_dsl

# =============================================================================
# Component Definitions
# =============================================================================

proc Counter*(initialValue: int = 0): Element {.kryonComponent.} =
  var value = initialValue

  result = Row:
      alignItems = "center"
      gap = 32
      Button:
        fontSize = 24
        text = "-"
        height = 50
        backgroundColor = "#E74C3C"
        width = 60
        onClick = proc() = value -= 1
      Text:
        fontSize = 32
        color = "#FFFFFF"
        text = $value
      Button:
        fontSize = 24
        text = "+"
        height = 50
        backgroundColor = "#2ECC71"
        width = 60
        onClick = proc() = value += 1

# =============================================================================
# Application
# =============================================================================

let app = kryonApp:
  Header:
    windowWidth = 800
    windowHeight = 600
    windowTitle = "Counters Demo"

  Body:
    backgroundColor = "#1E1E1E"

    Column:
      justifyContent = "center"
      alignItems = "center"
      height = 100.percent
      gap = 20
      width = 100.percent

      Text:
        fontSize = 24
        color = "#FFFFFF"
        text = "Multiple Independent Counters"

      Counter(initialValue = 5)

      Counter()

      Counter(initialValue = 10)
