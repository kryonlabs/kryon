## Container Queries Demo - Responsive Components with Container Queries

import kryon_dsl

let app = kryonApp:
  Header:
    windowWidth = 1200
    windowHeight = 800
    windowTitle = "Container Queries Demo"

  Body:
    backgroundColor = "#f0f0f0"
    padding = 20

    Column:
      width = 100.pct
      height = 100.pct
      gap = 20

      # Title
      Text:
        text = "Container Queries"
        fontSize = 32
        fontWeight = 700
        color = "#1a1a1a"
        textAlign = "center"

      # Description
      Text:
        text = "Resize the window to see components adapt to their container size, not the viewport"
        fontSize = 16
        color = "#666"
        textAlign = "center"

      # Main demo area
      Row:
        width = 100.pct
        flex = 1
        gap = 20

        # Sidebar (container query context)
        Container:
          containerType = "size"
          containerName = "sidebar"
          width = 300
          backgroundColor = "#ffffff"
          borderRadius = 8
          padding = 15
          boxShadow = (offsetY: 2.0, blur: 8.0, color: "#00000010")

          Column:
            width = 100.pct
            gap = 12

            Text:
              text = "Sidebar"
              fontSize = 20
              fontWeight = 600
              color = "#333"

            # Responsive card (responds to sidebar width)
            Container:
              width = 100.pct
              backgroundColor = "#f9f9f9"
              padding = 12
              borderRadius = 6

              # Default: compact layout
              Column:
                width = 100.pct
                gap = 8

                Text:
                  text = "Card"
                  fontSize = 14
                  fontWeight = 600

                Text:
                  text = "Compact view"
                  fontSize = 12
                  color = "#666"

                # Breakpoint: expand when sidebar >= 400px
                breakpoint = (minWidth: 400, height: 150)

        # Main content area (another container query context)
        Container:
          containerType = "size"
          containerName = "main"
          flex = 1
          backgroundColor = "#ffffff"
          borderRadius = 8
          padding = 20
          boxShadow = (offsetY: 2.0, blur: 8.0, color: "#00000010")

          Column:
            width = 100.pct
            gap = 16

            Text:
              text = "Main Content"
              fontSize = 20
              fontWeight = 600
              color = "#333"

            # Grid that responds to container width
            Row:
              width = 100.pct
              gap = 12
              flexWrap = true

              # Card 1
              Container:
                width = 100.pct
                backgroundColor = "#6366f1"
                padding = 16
                borderRadius = 8
                height = 100

                Center:
                  Text:
                    text = "Card 1"
                    color = "white"
                    fontWeight = 600

                # Breakpoint: 2-column when main >= 600px
                breakpoint = (minWidth: 600, width: 48.pct)

                # Breakpoint: 3-column when main >= 900px
                breakpoint = (minWidth: 900, width: 32.pct)

              # Card 2
              Container:
                width = 100.pct
                backgroundColor = "#8b5cf6"
                padding = 16
                borderRadius = 8
                height = 100

                Center:
                  Text:
                    text = "Card 2"
                    color = "white"
                    fontWeight = 600

                breakpoint = (minWidth: 600, width: 48.pct)
                breakpoint = (minWidth: 900, width: 32.pct)

              # Card 3
              Container:
                width = 100.pct
                backgroundColor = "#ec4899"
                padding = 16
                borderRadius = 8
                height = 100

                Center:
                  Text:
                    text = "Card 3"
                    color = "white"
                    fontWeight = 600

                breakpoint = (minWidth: 600, width: 48.pct)
                breakpoint = (minWidth: 900, width: 32.pct)

              # Card 4 - hides at small sizes
              Container:
                width = 100.pct
                backgroundColor = "#f59e0b"
                padding = 16
                borderRadius = 8
                height = 100

                Center:
                  Text:
                    text = "Card 4"
                    color = "white"
                    fontWeight = 600

                # Hidden by default (small containers)
                breakpoint = (maxWidth: 600, visible: false)

                # Show and size when main >= 600px
                breakpoint = (minWidth: 600, width: 48.pct, visible: true)
                breakpoint = (minWidth: 900, width: 32.pct)

      # Info panel showing how container queries work
      Container:
        width = 100.pct
        backgroundColor = "#e0e7ff"
        padding = 16
        borderRadius = 8

        Column:
          width = 100.pct
          gap = 8

          Text:
            text = "How It Works"
            fontSize = 18
            fontWeight = 600
            color = "#3730a3"

          Text:
            text = "• Sidebar has containerType=\"size\" making it a query container"
            fontSize = 14
            color = "#4338ca"

          Text:
            text = "• Cards inside have breakpoints that respond to their parent container width"
            fontSize = 14
            color = "#4338ca"

          Text:
            text = "• Cards change layout at minWidth: 600px and 900px of their container"
            fontSize = 14
            color = "#4338ca"

          Text:
            text = "• Card 4 uses visibility breakpoints to hide/show based on container size"
            fontSize = 14
            color = "#4338ca"
