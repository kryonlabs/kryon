## Reorderable Tabs Example
##
## Demonstrates automatic tab reordering with drag-and-drop
## The framework handles all reordering logic automatically!

import ../src/kryon

# Define the app using pure Kryon DSL
let app = kryonApp:
  Header:
    width = 800
    height = 600
    title = "Reorderable Tabs - Kryon DSL"

  Body:
    background = "#2c2c2c"

    Column:
      gap = 20
      padding = 20

      Text:
        text = "Reorderable Tabs Demo"
        fontSize = 32
        color = "#ffffff"

      Text:
        text = "Drag tabs horizontally to reorder them! The framework handles everything automatically."
        fontSize = 16
        color = "#aaaaaa"

      TabGroup:
        selectedIndex = 0
        width = 760
        height = 400

        TabBar:
          backgroundColor = "#1a1a1a"
          reorderable = true  # This one property enables drag-to-reorder!

          # Tab 1: Home
          Tab:
            title = "Home"
            backgroundColor = "#3d3d3d"
            activeBackgroundColor = "#4a90e2"
            textColor = "#cccccc"
            activeTextColor = "#ffffff"

          # Tab 2: Profile
          Tab:
            title = "Profile"
            backgroundColor = "#3d3d3d"
            activeBackgroundColor = "#4a90e2"
            textColor = "#cccccc"
            activeTextColor = "#ffffff"

          # Tab 3: Settings
          Tab:
            title = "Settings"
            backgroundColor = "#3d3d3d"
            activeBackgroundColor = "#4a90e2"
            textColor = "#cccccc"
            activeTextColor = "#ffffff"

          # Tab 4: About
          Tab:
            title = "About"
            backgroundColor = "#3d3d3d"
            activeBackgroundColor = "#4a90e2"
            textColor = "#cccccc"
            activeTextColor = "#ffffff"

        TabContent:
          backgroundColor = "#1e1e1e"

          # Panel 1: Home
          TabPanel:
            backgroundColor = "#1e1e1e"
            padding = 30

            Column:
              gap = 15

              Text:
                text = "Welcome to the Home Panel!"
                fontSize = 24
                color = "#4a90e2"

              Text:
                text = "This is the first tab. Drag tabs to reorder them - watch them smoothly slide into position!"
                fontSize = 16
                color = "#cccccc"

          # Panel 2: Profile
          TabPanel:
            backgroundColor = "#1e1e1e"
            padding = 30

            Column:
              gap = 15

              Text:
                text = "User Profile"
                fontSize = 24
                color = "#52c41a"

              Text:
                text = "Name: John Doe"
                fontSize = 16
                color = "#cccccc"

              Text:
                text = "Email: john.doe@example.com"
                fontSize = 16
                color = "#cccccc"

          # Panel 3: Settings
          TabPanel:
            backgroundColor = "#1e1e1e"
            padding = 30

            Column:
              gap = 15

              Text:
                text = "Application Settings"
                fontSize = 24
                color = "#faad14"

              Text:
                text = "Theme: Dark Mode"
                fontSize = 16
                color = "#cccccc"

              Text:
                text = "Language: English"
                fontSize = 16
                color = "#cccccc"

              Text:
                text = "Notifications: Enabled"
                fontSize = 16
                color = "#cccccc"

          # Panel 4: About
          TabPanel:
            backgroundColor = "#1e1e1e"
            padding = 30

            Column:
              gap = 15

              Text:
                text = "About This App"
                fontSize = 24
                color = "#eb2f96"

              Text:
                text = "Kryon UI Framework"
                fontSize = 16
                color = "#cccccc"

              Text:
                text = "Version: 0.2.0"
                fontSize = 16
                color = "#cccccc"

              Text:
                text = "Built with Nim and Raylib"
                fontSize = 16
                color = "#cccccc"

      # Instructions
      Column:
        gap = 10

        Text:
          text = "How it works:"
          fontSize = 16
          color = "#888888"

        Text:
          text = "• Just add 'reorderable = true' to TabBar"
          fontSize = 14
          color = "#666666"

        Text:
          text = "• Framework automatically makes all Tabs draggable"
          fontSize = 14
          color = "#666666"

        Text:
          text = "• Framework handles live visual reordering"
          fontSize = 14
          color = "#666666"

        Text:
          text = "• NO manual behaviors or handlers needed!"
          fontSize = 14
          color = "#666666"
