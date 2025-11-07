## Simple Styled App - Pure Kryon DSL
##
## Demonstrates basic styling with clean declarative UI syntax
## NO direct SDL3 code - pure Kryon DSL implementation

import kryon_dsl

# Test application with inline styles using pure Kryon DSL
let app = kryonApp:
  Header:
    windowWidth = 300
    windowHeight = 200
    windowTitle = "Styled App"

  Body:
    Container:
      width = 300
      height = 200
      backgroundColor = "violet"

      Text:
        content = "Hello Styled World!"
        color = "blue"

      Button:
        text = "Click Me"
        backgroundColor = "purple"
        color = "red"