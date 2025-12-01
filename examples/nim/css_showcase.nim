0BSD## CSS Features Showcase - Demonstrates new CSS capabilities
## Typography, Shadows, Grid, and Responsive Design

import kryon_dsl
import strformat

# State for pagination
var currentPage = 1
const totalPages = 3

proc nextPage() =
  if currentPage < totalPages:
    currentPage += 1
    echo "Page ", currentPage

proc prevPage() =
  if currentPage > 1:
    currentPage -= 1
    echo "Page ", currentPage

# Define the UI
let app = kryonApp:
  Header:
    windowWidth = 1200
    windowHeight = 800
    windowTitle = "Kryon CSS Showcase"

  Body:
    backgroundColor = "#f5f5f5"

    # Main container
    Column:
      width = 100.pct
      height = 100.pct
      padding = 20
      gap = 20

      # Header
      Container:
        width = 100.pct
        height = 80
        backgroundColor = "#6366f1"
        borderRadius = 12
        padding = 20

        Row:
          width = 100.pct
          alignItems = "center"
          justifyContent = "space-between"

          Text:
            text = "🎨 Kryon CSS Features Showcase"
            fontSize = 28
            fontWeight = 700
            color = "white"
            letterSpacing = -0.5

          Text:
            text = fmt"Page {currentPage}/{totalPages}"
            fontSize = 16
            color = "white"

      # Page content area
      Container:
        width = 100.pct

        # PAGE 1: Typography & Text Effects
        if currentPage == 1:
          Column:
            width = 100.pct
            gap = 20

            Text:
              text = "Extended Typography Features"
              fontSize = 24
              fontWeight = 600
              color = "#1f2937"

            # Font weights
            Container:
              width = 100.pct
              backgroundColor = "white"
              borderRadius = 8
              padding = 20
              boxShadow = (offsetY: 2.0, blur: 8.0, color: "#00000010")

              Column:
                gap = 12

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
                  text = "Font Weight 800 (Extrabold)"
                  fontSize = 20
                  fontWeight = 800
                  color = "#374151"

            # Letter spacing & text decoration
            Container:
              width = 100.pct
              backgroundColor = "white"
              borderRadius = 8
              padding = 20
              boxShadow = (offsetY: 2.0, blur: 8.0, color: "#00000010")

              Column:
                gap = 12

                Text:
                  text = "T I G H T   L E T T E R   S P A C I N G"
                  fontSize = 16
                  letterSpacing = 8.0
                  color = "#6366f1"
                  fontWeight = 600

                Text:
                  text = "Underlined Text"
                  fontSize = 18
                  textDecoration = "underline"
                  color = "#374151"

                Text:
                  text = "Line Through Text"
                  fontSize = 18
                  textDecoration = "line-through"
                  color = "#9ca3af"

                Text:
                  text = "Text with Shadow Effect"
                  fontSize = 20
                  fontWeight = 700
                  color = "#8b5cf6"
                  textShadow = (2.0, 2.0, 4.0, "#00000040")

        # PAGE 2: Box Shadows & Colors
        if currentPage == 2:
          Column:
            width = 100.pct
            gap = 20

            Text:
              text = "Box Shadows & Visual Effects"
              fontSize = 24
              fontWeight = 600
              color = "#1f2937"

            # Shadow examples
            Row:
              width = 100.pct
              gap = 20

              Container:
                width = 30.pct
                height = 120
                backgroundColor = "white"
                borderRadius = 8
                boxShadow = (offsetY: 4.0, blur: 12.0, color: "#00000020")

                Center:
                  Text:
                    text = "Drop Shadow"
                    fontSize = 16
                    fontWeight = 600
                    color = "#374151"

              Container:
                width = 30.pct
                height = 120
                backgroundColor = "#f3f4f6"
                borderRadius = 8
                boxShadow = (blur: 15.0, spread: -3.0, color: "#00000030", inset: true)  # inset

                Center:
                  Text:
                    text = "Inset Shadow"
                    fontSize = 16
                    fontWeight = 600
                    color = "#374151"

              Container:
                width = 30.pct
                height = 120
                backgroundColor = "white"
                borderRadius = 8
                boxShadow = (offsetY: 10.0, blur: 25.0, spread: 5.0, color: "#6366f140")

                Center:
                  Text:
                    text = "Colored Shadow"
                    fontSize = 16
                    fontWeight = 600
                    color = "#6366f1"

            # Color variations
            Row:
              width = 100.pct
              gap = 15

              Container:
                width = 23.pct
                height = 100
                backgroundColor = "#ec4899"
                borderRadius = 8

                Center:
                  Text:
                    text = "Pink"
                    fontSize = 16
                    color = "white"
                    fontWeight = 600

              Container:
                width = 23.pct
                height = 100
                backgroundColor = "#10b981"
                borderRadius = 8

                Center:
                  Text:
                    text = "Green"
                    fontSize = 16
                    color = "white"
                    fontWeight = 600

              Container:
                width = 23.pct
                height = 100
                backgroundColor = "#f59e0b"
                borderRadius = 8

                Center:
                  Text:
                    text = "Orange"
                    fontSize = 16
                    color = "white"
                    fontWeight = 600

              Container:
                width = 23.pct
                height = 100
                backgroundColor = "#3b82f6"
                borderRadius = 8

                Center:
                  Text:
                    text = "Blue"
                    fontSize = 16
                    color = "white"
                    fontWeight = 600

        # PAGE 3: Layout & Structure
        if currentPage == 3:
          Column:
            width = 100.pct
            gap = 20

            Text:
              text = "Layout Examples"
              fontSize = 24
              fontWeight = 600
              color = "#1f2937"

            # Card layout example
            Row:
              width = 100.pct
              gap = 15

              Container:
                width = 48.pct
                backgroundColor = "white"
                borderRadius = 8
                padding = 20
                boxShadow = (offsetY: 2.0, blur: 8.0, color: "#00000010")

                Column:
                  gap = 10

                  Text:
                    text = "Card Title"
                    fontSize = 20
                    fontWeight = 700
                    color = "#1f2937"

                  Text:
                    text = "This is a card component with shadow and padding."
                    fontSize = 14
                    color = "#6b7280"

              Container:
                width = 48.pct
                backgroundColor = "white"
                borderRadius = 8
                padding = 20
                boxShadow = (offsetY: 2.0, blur: 8.0, color: "#00000010")

                Column:
                  gap = 10

                  Text:
                    text = "Another Card"
                    fontSize = 20
                    fontWeight = 700
                    color = "#1f2937"

                  Text:
                    text = "Cards can contain any content and layout."
                    fontSize = 14
                    color = "#6b7280"

            # Nested containers
            Container:
              width = 100.pct
              backgroundColor = "#f3f4f6"
              borderRadius = 8
              padding = 15

              Row:
                width = 100.pct
                gap = 15

                Container:
                  width = 25.pct
                  height = 80
                  backgroundColor = "#6366f1"
                  borderRadius = 6

                  Center:
                    Text:
                      text = "1"
                      fontSize = 24
                      fontWeight = 700
                      color = "white"

                Container:
                  width = 25.pct
                  height = 80
                  backgroundColor = "#8b5cf6"
                  borderRadius = 6

                  Center:
                    Text:
                      text = "2"
                      fontSize = 24
                      fontWeight = 700
                      color = "white"

                Container:
                  width = 25.pct
                  height = 80
                  backgroundColor = "#a855f7"
                  borderRadius = 6

                  Center:
                    Text:
                      text = "3"
                      fontSize = 24
                      fontWeight = 700
                      color = "white"

                Container:
                  width = 25.pct
                  height = 80
                  backgroundColor = "#c084fc"
                  borderRadius = 6

                  Center:
                    Text:
                      text = "4"
                      fontSize = 24
                      fontWeight = 700
                      color = "white"

      # Navigation buttons (fixed at bottom)
      Row:
        width = 100.pct
        justifyContent = "center"
        gap = 15
        marginTop = 20

        Button:
          width = 120
          height = 45
          text = "← Previous"
          backgroundColor = if currentPage > 1: "#6366f1" else: "#d1d5db"
          color = "white"
          fontWeight = 600
          borderRadius = 8
          onClick = prevPage
          disabled = currentPage == 1

        Button:
          width = 120
          height = 45
          text = "Next →"
          backgroundColor = if currentPage < totalPages: "#6366f1" else: "#d1d5db"
          color = "white"
          fontWeight = 600
          borderRadius = 8
          onClick = nextPage
          disabled = currentPage == totalPages
