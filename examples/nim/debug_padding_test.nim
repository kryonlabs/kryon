## Debug test for padding issue - shows exact bounds of each component

import kryon_dsl

let app = kryonApp:
  Header:
    windowWidth = 800
    windowHeight = 400
    windowTitle = "Debug Padding Test"

  Body:
    backgroundColor = "#0a0e27"
    padding = 40

    # Info box replica from animations_demo
    Container:
      width = 100.pct
      maxWidth = 600
      backgroundColor = "#1e1b4b"
      padding = 20
      borderRadius = 12

      Text:
        text = "Available Preset Animations"
        fontSize = 16
        fontWeight = 600
        color = "#e0e7ff"

      Text:
        text = "pulse(duration, iterations) - Scale in/out effect"
        fontSize = 13
        color = "#c7d2fe"

      Text:
        text = "fadeInOut(duration, iterations) - Opacity 0 to 1 to 0"
        fontSize = 13
        color = "#c7d2fe"

      Text:
        text = "slideInLeft(duration) - Slide in from left edge"
        fontSize = 13
        color = "#c7d2fe"

      Text:
        text = "Use -1 for infinite loop, 1 for once, N for N times"
        fontSize = 12
        color = "#a5b4fc"
