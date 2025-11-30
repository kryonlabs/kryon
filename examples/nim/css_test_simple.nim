## Simple CSS Test - Typography & Shadows

import kryon_dsl

let app = kryonApp:
  Header:
    windowWidth = 800
    windowHeight = 600
    windowTitle = "CSS Test"

  Body:
    backgroundColor = "#f5f5f5"

    Column:
      width = 100.pct
      height = 100.pct
      padding = 20
      gap = 20

      # Title
      Text:
        text = "CSS Features Test"
        fontSize = 28
        fontWeight = 700
        color = "#1f2937"

      # Font weights test
      Container:
        width = 100.pct
        backgroundColor = "white"
        borderRadius = 8
        padding = 20
        boxShadow = (offsetY: 2.0, blur: 8.0, color: "#00000020")

        Column:
          gap = 12

          Text:
            text = "Font Weight 300 (Light)"
            fontSize = 18
            fontWeight = 300
            color = "#374151"

          Text:
            text = "Font Weight 600 (Semibold)"
            fontSize = 18
            fontWeight = 600
            color = "#374151"

          Text:
            text = "Wide Letter Spacing"
            fontSize = 16
            letterSpacing = 4.0
            color = "#6366f1"
            fontWeight = 600

          Text:
            text = "Underlined Text"
            fontSize = 16
            textDecoration = "underline"
            color = "#374151"

          Text:
            text = "Text with Shadow"
            fontSize = 18
            fontWeight = 700
            color = "#8b5cf6"
            textShadow = (2.0, 2.0, 4.0, "#00000040")

      # Color boxes
      Row:
        width = 100.pct
        gap = 15

        Container:
          width = 25.pct
          height = 80
          backgroundColor = "#ec4899"
          borderRadius = 8

          Center:
            Text:
              text = "Pink"
              fontSize = 14
              color = "white"
              fontWeight = 600

        Container:
          width = 25.pct
          height = 80
          backgroundColor = "#10b981"
          borderRadius = 8

          Center:
            Text:
              text = "Green"
              fontSize = 14
              color = "white"
              fontWeight = 600

        Container:
          width = 25.pct
          height = 80
          backgroundColor = "#3b82f6"
          borderRadius = 8
          boxShadow = (offsetY: 4.0, blur: 12.0, color: "#00000030")

          Center:
            Text:
              text = "Blue Shadow"
              fontSize = 14
              color = "white"
              fontWeight = 600
