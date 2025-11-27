## Theme Demo
##
## Demonstrates the theme switcher with 6 beautiful preset themes
## All colors use @variables - they update instantly when theme changes!

import kryon_dsl
import components/themes

# Initialize with default theme (Midnight)
initTheme()

let app = kryonApp:
  Header:
    windowWidth = 900
    windowHeight = 700
    windowTitle = "Kryon Theme Demo"

  Body:
    backgroundColor = @background

    Column:
      padding = 30
      gap = 24

      # Header row with title and theme switcher
      Row:
        mainAxisAlignment = "space-between"
        crossAxisAlignment = "center"

        Text:
          text = "Theme Demo"
          fontSize = 32
          color = @text

        # Theme switcher dropdown
        Dropdown:
          width = 200
          height = 40
          placeholder = "Select Theme..."
          options = themeNames()
          selectedIndex = currentThemeIndex
          backgroundColor = "#3d3d3d"
          borderColor = "#555555"
          textColor = "#ffffff"
          onChange = proc(index: int) =
            setThemeByIndex(index)

      # Subtitle
      Text:
        text = "Select a theme from the dropdown to see instant color updates!"
        fontSize = 16
        color = @textMuted

      # Main content card
      Container:
        backgroundColor = @card
        padding = 24
        borderColor = @border
        borderWidth = 1
        borderRadius = 12

        Column:
          gap = 16

          Text:
            text = "Welcome to Kryon"
            fontSize = 24
            color = @text

          Text:
            text = "This card uses theme colors - background, text, and border all update instantly when you switch themes. No rebuild needed!"
            fontSize = 14
            color = @textMuted

          # Button row
          Row:
            gap = 12
            marginTop = 8

            Button:
              title = "Primary"
              backgroundColor = @primary
              textColor = @textOnPrimary
              paddingX = 20
              paddingY = 10
              borderRadius = 6

            Button:
              title = "Secondary"
              backgroundColor = @secondary
              textColor = @text
              paddingX = 20
              paddingY = 10
              borderRadius = 6

            Button:
              title = "Accent"
              backgroundColor = @accent
              textColor = @textOnPrimary
              paddingX = 20
              paddingY = 10
              borderRadius = 6

      # Semantic colors section
      Text:
        text = "Semantic Colors"
        fontSize = 20
        color = @text

      Row:
        gap = 16

        # Success
        Container:
          backgroundColor = @success
          padding = 16
          borderRadius = 8
          width = 120

          Column:
            crossAxisAlignment = "center"
            gap = 4

            Text:
              text = "Success"
              fontSize = 14
              color = @textOnPrimary

        # Warning
        Container:
          backgroundColor = @warning
          padding = 16
          borderRadius = 8
          width = 120

          Column:
            crossAxisAlignment = "center"
            gap = 4

            Text:
              text = "Warning"
              fontSize = 14
              color = @textOnPrimary

        # Error
        Container:
          backgroundColor = @error
          padding = 16
          borderRadius = 8
          width = 120

          Column:
            crossAxisAlignment = "center"
            gap = 4

            Text:
              text = "Error"
              fontSize = 14
              color = @textOnPrimary

      # Surface colors section
      Text:
        text = "Surface Colors"
        fontSize = 20
        color = @text

      Row:
        gap = 16

        Container:
          backgroundColor = @surface
          padding = 16
          borderRadius = 8
          borderColor = @border
          borderWidth = 1

          Text:
            text = "Surface"
            fontSize = 14
            color = @text

        Container:
          backgroundColor = @surfaceHover
          padding = 16
          borderRadius = 8
          borderColor = @border
          borderWidth = 1

          Text:
            text = "Surface Hover"
            fontSize = 14
            color = @text

        Container:
          backgroundColor = @card
          padding = 16
          borderRadius = 8
          borderColor = @border
          borderWidth = 1

          Text:
            text = "Card"
            fontSize = 14
            color = @text

      # Color palette display
      Text:
        text = "Color Palette"
        fontSize = 20
        color = @text

      Row:
        gap = 8
        flexWrap = "wrap"

        # Primary colors
        Container:
          backgroundColor = @primary
          width = 50
          height = 50
          borderRadius = 8

        Container:
          backgroundColor = @primaryHover
          width = 50
          height = 50
          borderRadius = 8

        Container:
          backgroundColor = @secondary
          width = 50
          height = 50
          borderRadius = 8

        Container:
          backgroundColor = @accent
          width = 50
          height = 50
          borderRadius = 8

        # Spacer
        Container:
          width = 20
          height = 50

        # Background colors
        Container:
          backgroundColor = @background
          width = 50
          height = 50
          borderRadius = 8
          borderColor = @border
          borderWidth = 1

        Container:
          backgroundColor = @surface
          width = 50
          height = 50
          borderRadius = 8
          borderColor = @border
          borderWidth = 1

        Container:
          backgroundColor = @card
          width = 50
          height = 50
          borderRadius = 8
          borderColor = @border
          borderWidth = 1

        # Spacer
        Container:
          width = 20
          height = 50

        # Semantic colors
        Container:
          backgroundColor = @success
          width = 50
          height = 50
          borderRadius = 8

        Container:
          backgroundColor = @warning
          width = 50
          height = 50
          borderRadius = 8

        Container:
          backgroundColor = @error
          width = 50
          height = 50
          borderRadius = 8
