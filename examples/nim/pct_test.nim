0BSD## Percentage Test with .pct syntax

import kryon_dsl

discard kryonApp:
  Header:
    windowWidth = 800
    windowHeight = 600
    windowTitle = "Percent Test"

  Body:
    backgroundColor = "#1a1a2eFF"
    layoutDirection = 1  # Column
    gap = 20
    padding = 20

    # Header taking 10% height
    Container:
      width = 100.pct
      height = 10.pct
      backgroundColor = "#16213eFF"
      Text:
        content = "Header (10.pct height)"
        color = "#e0e0e0FF"

    # Main area - 80% height
    Container:
      width = 100.pct
      height = 80.pct
      layoutDirection = 0  # Row
      gap = 20

      # Sidebar - 30% width
      Container:
        width = 30.pct
        height = 100.pct
        backgroundColor = "#0f3460FF"
        Text:
          content = "Sidebar (30.pct)"
          color = "#e0e0e0FF"

      # Main - 70% width
      Container:
        width = 70.pct
        height = 100.pct
        backgroundColor = "#1a1a2eFF"
        Text:
          content = "Main (70.pct)"
          color = "#e0e0e0FF"

    # Footer - 10% height
    Container:
      width = 100.pct
      height = 10.pct
      backgroundColor = "#16213eFF"
      Text:
        content = "Footer (10.pct height)"
        color = "#e0e0e0FF"
