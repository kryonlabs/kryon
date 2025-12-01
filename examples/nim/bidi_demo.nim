## Simple Bidirectional Text Demo
## Shows RTL text support for Hebrew and Arabic

import kryon_dsl

discard kryonApp:
  Header:
    windowWidth = 500
    windowHeight = 400
    windowTitle = "BiDi Demo"

  Body:
    backgroundColor = "#1a1a1a"
    padding = 30
    layoutDirection = 1  # Column
    gap = 20

    # Title
    Text:
      content = "BiDi Text Support"
      fontSize = 24
      color = "#ffffff"

    # English (LTR)
    Container:
      width = 440
      height = auto
      backgroundColor = "#2d2d2d"
      borderRadius = 8
      padding = 15
      layoutDirection = 1
      gap = 5

      Text:
        content = "English (LTR):"
        fontSize = 14
        color = "#4CAF50"

      Text:
        content = "Hello World"
        fontSize = 18
        color = "#e0e0e0"

    # Hebrew (RTL)
    Container:
      width = 440
      height = auto
      backgroundColor = "#2d2d2d"
      borderRadius = 8
      padding = 15
      layoutDirection = 1
      gap = 5

      Text:
        content = "Hebrew (RTL):"
        fontSize = 14
        color = "#2196F3"

      Text:
        content = "שלום עולם"
        fontSize = 18
        color = "#e0e0e0"

    # Arabic (RTL)
    Container:
      width = 440
      height = auto
      backgroundColor = "#2d2d2d"
      borderRadius = 8
      padding = 15
      layoutDirection = 1
      gap = 5

      Text:
        content = "Arabic (RTL):"
        fontSize = 14
        color = "#FF9800"

      Text:
        content = "مرحبا بالعالم"
        fontSize = 18
        color = "#e0e0e0"
