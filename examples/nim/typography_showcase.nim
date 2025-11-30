## Typography Showcase - Comprehensive Test of Extended Typography Features

import kryon_dsl

let app = kryonApp:
  Header:
    windowWidth = 900
    windowHeight = 700
    windowTitle = "Typography Showcase"

  Body:
    backgroundColor = "#f8fafc"

    Column:
      width = 100.pct
      height = 100.pct
      padding = 30
      gap = 20

      # Title
      Text:
        text = "Extended Typography Features"
        fontSize = 32
        fontWeight = 700
        color = "#1e293b"
        textAlign = "center"

      # Font Weights Section
      Container:
        width = 100.pct
        backgroundColor = "white"
        borderRadius = 12
        padding = 20
        gap = 12

        Column:
          gap = 10

          Text:
            text = "Font Weights"
            fontSize = 20
            fontWeight = 600
            color = "#334155"

          Text:
            text = "Weight 300 - Light"
            fontSize = 16
            fontWeight = 300
            color = "#475569"

          Text:
            text = "Weight 400 - Normal"
            fontSize = 16
            fontWeight = 400
            color = "#475569"

          Text:
            text = "Weight 600 - Semibold"
            fontSize = 16
            fontWeight = 600
            color = "#475569"

          Text:
            text = "Weight 700 - Bold"
            fontSize = 16
            fontWeight = 700
            color = "#475569"

          Text:
            text = "Weight 900 - Black"
            fontSize = 16
            fontWeight = 900
            color = "#475569"

      # Spacing Section
      Container:
        width = 100.pct
        backgroundColor = "white"
        borderRadius = 12
        padding = 20
        gap = 12

        Column:
          gap = 10

          Text:
            text = "Letter & Word Spacing"
            fontSize = 20
            fontWeight = 600
            color = "#334155"

          Text:
            text = "Normal spacing - no adjustments"
            fontSize = 14
            color = "#475569"

          Text:
            text = "W I D E   L E T T E R S"
            fontSize = 14
            letterSpacing = 4.0
            fontWeight = 600
            color = "#6366f1"

          Text:
            text = "Tight letters"
            fontSize = 14
            letterSpacing = -1.0
            color = "#475569"

          Text:
            text = "Extra   word   spacing"
            fontSize = 14
            wordSpacing = 10.0
            color = "#8b5cf6"

      # Text Alignment Section
      Container:
        width = 100.pct
        backgroundColor = "white"
        borderRadius = 12
        padding = 20
        gap = 12

        Column:
          gap = 10

          Text:
            text = "Text Alignment"
            fontSize = 20
            fontWeight = 600
            color = "#334155"

          Text:
            text = "Left aligned text (default)"
            fontSize = 14
            textAlign = "left"
            color = "#475569"

          Text:
            text = "Center aligned text"
            fontSize = 14
            textAlign = "center"
            color = "#475569"

          Text:
            text = "Right aligned text"
            fontSize = 14
            textAlign = "right"
            color = "#475569"

      # Text Decoration Section
      Container:
        width = 100.pct
        backgroundColor = "white"
        borderRadius = 12
        padding = 20
        gap = 12

        Column:
          gap = 10

          Text:
            text = "Text Decorations"
            fontSize = 20
            fontWeight = 600
            color = "#334155"

          Text:
            text = "Underlined text"
            fontSize = 14
            textDecoration = "underline"
            color = "#475569"

          Text:
            text = "Line-through text"
            fontSize = 14
            textDecoration = "line-through"
            color = "#dc2626"

          Text:
            text = "Overline text"
            fontSize = 14
            textDecoration = "overline"
            color = "#475569"

          Text:
            text = "Multiple decorations"
            fontSize = 14
            textDecoration = ["underline", "overline"]
            fontWeight = 600
            color = "#059669"

      # Line Height Section
      Container:
        width = 100.pct
        backgroundColor = "white"
        borderRadius = 12
        padding = 20
        gap = 12

        Column:
          gap = 10

          Text:
            text = "Line Height"
            fontSize = 20
            fontWeight = 600
            color = "#334155"

          Text:
            text = "Tight line height (1.0x)"
            fontSize = 14
            lineHeight = 1.0
            color = "#475569"

          Text:
            text = "Normal line height (1.5x)"
            fontSize = 14
            lineHeight = 1.5
            color = "#475569"

          Text:
            text = "Loose line height (2.0x)"
            fontSize = 14
            lineHeight = 2.0
            color = "#475569"

      # Combined Features
      Container:
        width = 100.pct
        backgroundColor = "#6366f1"
        borderRadius = 12
        padding = 25

        Center:
          Text:
            text = "ALL FEATURES COMBINED"
            fontSize = 24
            fontWeight = 900
            letterSpacing = 3.0
            textDecoration = "underline"
            color = "white"
            textAlign = "center"
