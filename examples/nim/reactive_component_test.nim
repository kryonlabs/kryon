## Test for kryonComponent automatic reactive detection
##
## This example tests the new kryonComponent pragma that automatically:
## 1. Detects variables used in onClick handlers
## 2. Detects variables used in text expressions
## 3. Transforms them to namedReactiveVar
## 4. Serializes to .kir v2.1 with reactive manifest

import kryon_dsl

# Counter component using kryonComponent pragma
proc Counter*(init: int = 0): KryonComponent {.kryonComponent.} =
  var value = init
  result = Row:
    padding = 10
    gap = 10
    Button:
      text = "-"
      onClick = proc() = value -= 1
    Text:
      content = $value
    Button:
      text = "+"
      onClick = proc() = value += 1

# Main application
discard kryonApp:
  Header:
    windowWidth = 400
    windowHeight = 300
    windowTitle = "Reactive Component Test"

  Body:
    backgroundColor = "#1a1a2eFF"

    Center:
      Column:
        gap = 20

        Text:
          content = "kryonComponent Test"
          color = "white"

        Counter(init = 5)
        Counter(init = 10)
