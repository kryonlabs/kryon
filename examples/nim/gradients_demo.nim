## Gradients Demo - Linear, Radial, and Conic Gradients

import kryon_dsl

let app = kryonApp:
  Header:
    windowWidth = 1200
    windowHeight = 800
    windowTitle = "Gradients Demo"

  Body:
    backgroundColor = "#f5f5f5"

    Column:
      width = 100.pct
      height = 100.pct
      padding = 20
      gap = 20

      # Title
      Text:
        text = "CSS Gradients"
        fontSize = 32
        fontWeight = 700
        color = "#1a1a1a"
        textAlign = "center"

      # Linear Gradients
      Container:
        width = 100.pct
        backgroundColor = "white"
        padding = 15
        borderRadius = 8
        boxShadow = (offsetY: 2.0, blur: 8.0, color: "#00000010")

        Column:
          gap = 12

          Text:
            text = "Linear Gradients"
            fontSize = 20
            fontWeight = 600
            color = "#333"

          Row:
            width = 100.pct
            gap = 12

            # Horizontal gradient (0 degrees)
            Container:
              width = 32.pct
              height = 120
              borderRadius = 8
              background = ("linear", 0, [(0.0, "#ff6b6b"), (1.0, "#4ecdc4")])
              Center:
                Text:
                  text = "0° (Horizontal)"
                  color = "white"
                  fontWeight = 600
                  fontSize = 14

            # Diagonal gradient (45 degrees)
            Container:
              width = 32.pct
              height = 120
              borderRadius = 8
              background = ("linear", 45, [(0.0, "#a8e6cf"), (1.0, "#3d5a80")])
              Center:
                Text:
                  text = "45° (Diagonal)"
                  color = "white"
                  fontWeight = 600
                  fontSize = 14

            # Vertical gradient (90 degrees)
            Container:
              width = 32.pct
              height = 120
              borderRadius = 8
              background = ("linear", 90, [(0.0, "#ffd93d"), (1.0, "#ff6b6b")])
              Center:
                Text:
                  text = "90° (Vertical)"
                  color = "white"
                  fontWeight = 600
                  fontSize = 14

      # Multi-stop Linear Gradients
      Container:
        width = 100.pct
        backgroundColor = "white"
        padding = 15
        borderRadius = 8
        boxShadow = (offsetY: 2.0, blur: 8.0, color: "#00000010")

        Column:
          gap = 12

          Text:
            text = "Multi-Stop Linear Gradients"
            fontSize = 20
            fontWeight = 600
            color = "#333"

          Row:
            width = 100.pct
            gap = 12

            # Rainbow gradient
            Container:
              width = 48.pct
              height = 120
              borderRadius = 8
              background = ("linear", 90, [
                (0.0, "#ff0000"),
                (0.2, "#ffff00"),
                (0.4, "#00ff00"),
                (0.6, "#00ffff"),
                (0.8, "#0000ff"),
                (1.0, "#ff00ff")
              ])
              Center:
                Text:
                  text = "Rainbow"
                  color = "white"
                  fontWeight = 700
                  fontSize = 16

            # Sunset gradient
            Container:
              width = 48.pct
              height = 120
              borderRadius = 8
              background = ("linear", 180, [
                (0.0, "#ff9a56"),
                (0.3, "#ff6b6b"),
                (0.6, "#ee5a6f"),
                (1.0, "#1a1a2e")
              ])
              Center:
                Text:
                  text = "Sunset"
                  color = "white"
                  fontWeight = 700
                  fontSize = 16

      # Radial Gradients
      Container:
        width = 100.pct
        backgroundColor = "white"
        padding = 15
        borderRadius = 8
        boxShadow = (offsetY: 2.0, blur: 8.0, color: "#00000010")

        Column:
          gap = 12

          Text:
            text = "Radial Gradients"
            fontSize = 20
            fontWeight = 600
            color = "#333"

          Row:
            width = 100.pct
            gap = 12

            # Centered radial
            Container:
              width = 32.pct
              height = 120
              borderRadius = 8
              background = ("radial", 0.5, [(0.0, "#ffeaa7"), (1.0, "#fd79a8")])
              Center:
                Text:
                  text = "Centered"
                  color = "#333"
                  fontWeight = 600
                  fontSize = 14

            # Top-left radial
            Container:
              width = 32.pct
              height = 120
              borderRadius = 8
              background = ("radial", (0.0, 0.0), [(0.0, "#74b9ff"), (1.0, "#0984e3")])
              Center:
                Text:
                  text = "Top-Left Origin"
                  color = "white"
                  fontWeight = 600
                  fontSize = 14

            # Bottom-right radial
            Container:
              width = 32.pct
              height = 120
              borderRadius = 8
              background = ("radial", (1.0, 1.0), [(0.0, "#a29bfe"), (1.0, "#6c5ce7")])
              Center:
                Text:
                  text = "Bottom-Right"
                  color = "white"
                  fontWeight = 600
                  fontSize = 14

      # Conic Gradients
      Container:
        width = 100.pct
        backgroundColor = "white"
        padding = 15
        borderRadius = 8
        boxShadow = (offsetY: 2.0, blur: 8.0, color: "#00000010")

        Column:
          gap = 12

          Text:
            text = "Conic Gradients (Color Wheels)"
            fontSize = 20
            fontWeight = 600
            color = "#333"

          Row:
            width = 100.pct
            gap = 12

            # Full color wheel
            Container:
              width = 200
              height = 200
              borderRadius = 100
              background = ("conic", 0.5, [
                (0.0, "#ff0000"),
                (0.17, "#ffff00"),
                (0.33, "#00ff00"),
                (0.5, "#00ffff"),
                (0.67, "#0000ff"),
                (0.83, "#ff00ff"),
                (1.0, "#ff0000")
              ])

            # Pastel color wheel
            Container:
              width = 200
              height = 200
              borderRadius = 100
              background = ("conic", 0.5, [
                (0.0, "#ffd1dc"),
                (0.25, "#ffffb5"),
                (0.5, "#c1ffc1"),
                (0.75, "#c1e1ff"),
                (1.0, "#ffd1dc")
              ])

            # Two-tone spinner
            Container:
              width = 200
              height = 200
              borderRadius = 100
              background = ("conic", 0.5, [
                (0.0, "#667eea"),
                (0.5, "#764ba2"),
                (1.0, "#667eea")
              ])

      # Practical Examples
      Container:
        width = 100.pct
        backgroundColor = "white"
        padding = 15
        borderRadius = 8
        boxShadow = (offsetY: 2.0, blur: 8.0, color: "#00000010")

        Column:
          gap = 12

          Text:
            text = "Practical Examples"
            fontSize = 20
            fontWeight = 600
            color = "#333"

          Row:
            width = 100.pct
            gap = 12

            # Glossy button
            Container:
              width = 30.pct
              height = 60
              borderRadius = 30
              background = ("linear", 180, [(0.0, "#4facfe"), (1.0, "#00f2fe")])
              boxShadow = (offsetY: 4.0, blur: 12.0, color: "#00000020")
              Center:
                Text:
                  text = "Glossy Button"
                  color = "white"
                  fontWeight = 700

            # Card header
            Container:
              width = 30.pct
              height = 60
              borderRadius = 8
              background = ("linear", 135, [(0.0, "#667eea"), (1.0, "#764ba2")])
              boxShadow = (offsetY: 4.0, blur: 12.0, color: "#00000020")
              Center:
                Text:
                  text = "Card Header"
                  color = "white"
                  fontWeight = 700

            # Status badge
            Container:
              width = 30.pct
              height = 60
              borderRadius = 8
              background = ("linear", 45, [(0.0, "#11998e"), (1.0, "#38ef7d")])
              boxShadow = (offsetY: 4.0, blur: 12.0, color: "#00000020")
              Center:
                Text:
                  text = "Active Status"
                  color = "white"
                  fontWeight = 700
