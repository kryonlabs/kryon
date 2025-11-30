## Typography Test - Font Weights

import kryon_dsl

let app = kryonApp:
  Header:
    windowWidth = 600
    windowHeight = 500
    windowTitle = "Typography Test"

  Body:
    backgroundColor = "#f5f5f5"

    Column:
      width = 100.pct
      padding = 30
      gap = 15

      Text:
        text = "Font Weight 300 (Light)"
        fontSize = 20
        fontWeight = 300
        color = "#374151"

      Text:
        text = "Font Weight 400 (Normal)"
        fontSize = 20
        fontWeight = 400
        color = "#374151"

      Text:
        text = "Font Weight 600 (Semibold)"
        fontSize = 20
        fontWeight = 600
        color = "#374151"

      Text:
        text = "Font Weight 700 (Bold)"
        fontSize = 20
        fontWeight = 700
        color = "#374151"

      Text:
        text = "Font Weight 800 (Extrabold)"
        fontSize = 20
        fontWeight = 800
        color = "#374151"

      Container:
        width = 100.pct
        height = 100
        backgroundColor = "#6366f1"
        borderRadius = 8

        Center:
          Text:
            text = "Centered Heavy Text"
            fontSize = 24
            fontWeight = 900
            color = "white"
