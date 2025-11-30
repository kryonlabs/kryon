## Shadows & Filters Demo - Box Shadows and CSS Filters

import kryon_dsl

let app = kryonApp:
  Header:
    windowWidth = 1000
    windowHeight = 800
    windowTitle = "Shadows & Filters Demo"

  Body:
    backgroundColor = "#f0f4f8"

    Column:
      width = 100.pct
      height = 100.pct
      padding = 25
      gap = 20

      # Title
      Text:
        text = "Box Shadows & CSS Filters"
        fontSize = 28
        fontWeight = 700
        color = "#1a202c"
        textAlign = "center"

      Row:
        width = 100.pct
        gap = 20

        # Box Shadows Column
        Column:
          width = 48.pct
          gap = 15

          Text:
            text = "Box Shadows"
            fontSize = 20
            fontWeight = 600
            color = "#2d3748"

          # Drop shadow
          Container:
            width = 100.pct
            height = 100
            backgroundColor = "white"
            borderRadius = 8
            boxShadow = (offsetY: 4.0, blur: 12.0, color: "#00000020")

            Center:
              Text:
                text = "Drop Shadow"
                fontSize = 16
                fontWeight = 600
                color = "#4a5568"

          # Elevated shadow
          Container:
            width = 100.pct
            height = 100
            backgroundColor = "white"
            borderRadius = 8
            boxShadow = (offsetY: 8.0, blur: 24.0, color: "#00000030")

            Center:
              Text:
                text = "Elevated Shadow"
                fontSize = 16
                fontWeight = 600
                color = "#4a5568"

          # Inset shadow
          Container:
            width = 100.pct
            height = 100
            backgroundColor = "#e2e8f0"
            borderRadius = 8
            boxShadow = (offsetY: 2.0, blur: 8.0, color: "#00000030", inset: true)

            Center:
              Text:
                text = "Inset Shadow"
                fontSize = 16
                fontWeight = 600
                color = "#4a5568"

          # Colored shadow
          Container:
            width = 100.pct
            height = 100
            backgroundColor = "white"
            borderRadius = 8
            boxShadow = (offsetY: 6.0, blur: 20.0, color: "#6366f180")

            Center:
              Text:
                text = "Colored Shadow"
                fontSize = 16
                fontWeight = 600
                color = "#6366f1"

        # CSS Filters Column
        Column:
          width = 48.pct
          gap = 15

          Text:
            text = "CSS Filters"
            fontSize = 20
            fontWeight = 600
            color = "#2d3748"

          # Blur
          Container:
            width = 100.pct
            height = 80
            backgroundColor = "#3b82f6"
            borderRadius = 8
            blur = 2.0

            Center:
              Text:
                text = "Blur (2px)"
                fontSize = 14
                fontWeight = 600
                color = "white"

          # Brightness
          Container:
            width = 100.pct
            height = 80
            backgroundColor = "#10b981"
            borderRadius = 8
            brightness = 1.3

            Center:
              Text:
                text = "Brightness (1.3x)"
                fontSize = 14
                fontWeight = 600
                color = "white"

          # Contrast
          Container:
            width = 100.pct
            height = 80
            backgroundColor = "#f59e0b"
            borderRadius = 8
            contrast = 1.5

            Center:
              Text:
                text = "Contrast (1.5x)"
                fontSize = 14
                fontWeight = 600
                color = "white"

          # Grayscale
          Container:
            width = 100.pct
            height = 80
            backgroundColor = "#ec4899"
            borderRadius = 8
            grayscale = 0.8

            Center:
              Text:
                text = "Grayscale (80%)"
                fontSize = 14
                fontWeight = 600
                color = "white"

      Row:
        width = 100.pct
        gap = 15

        # Hue Rotate
        Container:
          width = 19.pct
          height = 80
          backgroundColor = "#6366f1"
          borderRadius = 8
          hueRotate = 90.0

          Center:
            Text:
              text = "Hue +90°"
              fontSize = 12
              fontWeight = 600
              color = "white"

        # Invert
        Container:
          width = 19.pct
          height = 80
          backgroundColor = "#3b82f6"
          borderRadius = 8
          invert = 1.0

          Center:
            Text:
              text = "Inverted"
              fontSize = 12
              fontWeight = 600
              color = "white"

        # Filter Opacity
        Container:
          width = 19.pct
          height = 80
          backgroundColor = "#10b981"
          borderRadius = 8
          filterOpacity = 0.5

          Center:
            Text:
              text = "Opacity 50%"
              fontSize = 12
              fontWeight = 600
              color = "white"

        # Saturate
        Container:
          width = 19.pct
          height = 80
          backgroundColor = "#f59e0b"
          borderRadius = 8
          saturate = 2.0

          Center:
            Text:
              text = "Saturate 2x"
              fontSize = 12
              fontWeight = 600
              color = "white"

        # Sepia
        Container:
          width = 19.pct
          height = 80
          backgroundColor = "#ef4444"
          borderRadius = 8
          sepia = 0.8

          Center:
            Text:
              text = "Sepia 80%"
              fontSize = 12
              fontWeight = 600
              color = "white"

      # Combined effects
      Container:
        width = 100.pct
        height = 120
        backgroundColor = "#8b5cf6"
        borderRadius = 12
        boxShadow = (offsetY: 10.0, blur: 30.0, color: "#00000040")
        brightness = 1.1
        saturate = 1.3

        Center:
          Text:
            text = "COMBINED: Shadow + Brightness + Saturate"
            fontSize = 18
            fontWeight = 900
            color = "white"
            letterSpacing = 2.0
