## Grid Layout Demo - CSS Grid Layout System

import kryon_dsl

let app = kryonApp:
  Header:
    windowWidth = 1200
    windowHeight = 900
    windowTitle = "Grid Layout Demo"

  Body:
    backgroundColor = "#f5f5f5"

    Column:
      width = 100.pct
      height = 100.pct
      padding = 20
      gap = 20

      # Title
      Text:
        text = "CSS Grid Layout"
        fontSize = 32
        fontWeight = 700
        color = "#1a1a1a"
        textAlign = "center"

      # Basic Grid - 3 columns, auto rows
      Container:
        width = 100.pct
        backgroundColor = "white"
        padding = 15
        borderRadius = 8

        Column:
          gap = 10

          Text:
            text = "Basic Grid (3 columns × auto rows)"
            fontSize = 18
            fontWeight = 600
            color = "#333"

          Grid:
            width = 100.pct
            height = 200
            gridTemplateColumns = "1fr 1fr 1fr"
            gridTemplateRows = "auto auto"
            gap = 10

            Container:
              backgroundColor = "#3b82f6"
              padding = 15
              borderRadius = 4
              Center:
                Text:
                  text = "Item 1"
                  color = "white"
                  fontWeight = 600

            Container:
              backgroundColor = "#10b981"
              padding = 15
              borderRadius = 4
              Center:
                Text:
                  text = "Item 2"
                  color = "white"
                  fontWeight = 600

            Container:
              backgroundColor = "#f59e0b"
              padding = 15
              borderRadius = 4
              Center:
                Text:
                  text = "Item 3"
                  color = "white"
                  fontWeight = 600

            Container:
              backgroundColor = "#ef4444"
              padding = 15
              borderRadius = 4
              Center:
                Text:
                  text = "Item 4"
                  color = "white"
                  fontWeight = 600

            Container:
              backgroundColor = "#8b5cf6"
              padding = 15
              borderRadius = 4
              Center:
                Text:
                  text = "Item 5"
                  color = "white"
                  fontWeight = 600

            Container:
              backgroundColor = "#ec4899"
              padding = 15
              borderRadius = 4
              Center:
                Text:
                  text = "Item 6"
                  color = "white"
                  fontWeight = 600

      # Grid with Explicit Placement
      Container:
        width = 100.pct
        backgroundColor = "white"
        padding = 15
        borderRadius = 8

        Column:
          gap = 10

          Text:
            text = "Grid with Explicit Placement"
            fontSize = 18
            fontWeight = 600
            color = "#333"

          Grid:
            width = 100.pct
            height = 250
            gridTemplateColumns = "1fr 1fr 1fr 1fr"
            gridTemplateRows = "1fr 1fr"
            gap = 10

            # Header spanning all columns
            Container:
              gridRow = "1 / 2"
              gridColumn = "1 / 5"
              backgroundColor = "#1e40af"
              padding = 15
              borderRadius = 4
              Center:
                Text:
                  text = "Header (spans all columns)"
                  color = "white"
                  fontSize = 16
                  fontWeight = 700

            # Sidebar spanning 2 rows
            Container:
              gridRow = "2 / 3"
              gridColumn = "1 / 2"
              backgroundColor = "#059669"
              padding = 15
              borderRadius = 4
              Center:
                Text:
                  text = "Sidebar"
                  color = "white"
                  fontWeight = 600

            # Content area
            Container:
              gridRow = "2 / 3"
              gridColumn = "2 / 4"
              backgroundColor = "#d97706"
              padding = 15
              borderRadius = 4
              Center:
                Text:
                  text = "Content (spans 2 columns)"
                  color = "white"
                  fontWeight = 600

            # Ad space
            Container:
              gridRow = "2 / 3"
              gridColumn = "4 / 5"
              backgroundColor = "#dc2626"
              padding = 15
              borderRadius = 4
              Center:
                Text:
                  text = "Ads"
                  color = "white"
                  fontWeight = 600

      Row:
        width = 100.pct
        gap = 15

        # Fractional Units (fr)
        Container:
          width = 48.pct
          backgroundColor = "white"
          padding = 15
          borderRadius = 8

          Column:
            gap = 10

            Text:
              text = "Fractional Units (1fr 2fr 1fr)"
              fontSize = 16
              fontWeight = 600
              color = "#333"

            Grid:
              width = 100.pct
              height = 120
              gridTemplateColumns = "1fr 2fr 1fr"
              gap = 8

              Container:
                backgroundColor = "#6366f1"
                borderRadius = 4
                Center:
                  Text:
                    text = "1fr"
                    color = "white"
                    fontSize = 14

              Container:
                backgroundColor = "#8b5cf6"
                borderRadius = 4
                Center:
                  Text:
                    text = "2fr (double)"
                    color = "white"
                    fontSize = 14

              Container:
                backgroundColor = "#6366f1"
                borderRadius = 4
                Center:
                  Text:
                    text = "1fr"
                    color = "white"
                    fontSize = 14

        # Mixed Units
        Container:
          width = 48.pct
          backgroundColor = "white"
          padding = 15
          borderRadius = 8

          Column:
            gap = 10

            Text:
              text = "Mixed Units (100px 1fr auto)"
              fontSize = 16
              fontWeight = 600
              color = "#333"

            Grid:
              width = 100.pct
              height = 120
              gridTemplateColumns = "100px 1fr auto"
              gap = 8

              Container:
                backgroundColor = "#14b8a6"
                borderRadius = 4
                Center:
                  Text:
                    text = "100px"
                    color = "white"
                    fontSize = 12

              Container:
                backgroundColor = "#06b6d4"
                borderRadius = 4
                Center:
                  Text:
                    text = "1fr (flexible)"
                    color = "white"
                    fontSize = 12

              Container:
                backgroundColor = "#0ea5e9"
                borderRadius = 4
                padding = 10
                Center:
                  Text:
                    text = "auto"
                    color = "white"
                    fontSize = 12

      # Grid Alignment
      Container:
        width = 100.pct
        backgroundColor = "white"
        padding = 15
        borderRadius = 8

        Column:
          gap = 10

          Text:
            text = "Grid Alignment (justifyItems: center, alignItems: center)"
            fontSize = 18
            fontWeight = 600
            color = "#333"

          Grid:
            width = 100.pct
            height = 200
            gridTemplateColumns = "1fr 1fr 1fr"
            gap = 10
            justifyItems = "center"
            alignItems = "center"

            Container:
              width = 60
              height = 60
              backgroundColor = "#f59e0b"
              borderRadius = 30

            Container:
              width = 80
              height = 80
              backgroundColor = "#ef4444"
              borderRadius = 40

            Container:
              width = 70
              height = 70
              backgroundColor = "#8b5cf6"
              borderRadius = 35

            Container:
              width = 90
              height = 90
              backgroundColor = "#10b981"
              borderRadius = 45

            Container:
              width = 65
              height = 65
              backgroundColor = "#3b82f6"
              borderRadius = 32

            Container:
              width = 75
              height = 75
              backgroundColor = "#ec4899"
              borderRadius = 37
