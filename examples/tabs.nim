## Tabs Example
##
## Demonstrates TabGroup, TabBar, Tab, TabContent, and TabPanel components
## Shows how to create a tabbed interface with different content panels

import ../src/kryon
import ../src/backends/raylib_backend

# Application state
var selectedTab = 0

# Define the UI
let app = kryonApp:
  Header:
    width = 800
    height = 600
    title = "Tabs Example"

  Body:
    background = "#2c2c2c"

    Column:
      gap = 20
      padding = 20

      Text:
        text = "Tabs Demo"
        fontSize = 32
        color = "#ffffff"

      Text:
        text = "Click on the tabs below to switch between different panels"
        fontSize = 16
        color = "#aaaaaa"

      Button:
        text = "TEST BUTTON - Click me!"
        onClick = proc() = echo "🔥🔥🔥 TEST BUTTON CLICKED! Clicks DO work in this app! 🔥🔥🔥"

      TabGroup:
        selectedIndex = selectedTab
        width = 760
        height = 400

        TabBar:
          backgroundColor = "#1a1a1a"

          Tab:
            title = "Home"
            backgroundColor = "#3d3d3d"
            activeBackgroundColor = "#4a90e2"
            textColor = "#cccccc"
            activeTextColor = "#ffffff"

          Tab:
            title = "Profile"
            backgroundColor = "#3d3d3d"
            activeBackgroundColor = "#4a90e2"
            textColor = "#cccccc"
            activeTextColor = "#ffffff"

          Tab:
            title = "Settings"
            backgroundColor = "#3d3d3d"
            activeBackgroundColor = "#4a90e2"
            textColor = "#cccccc"
            activeTextColor = "#ffffff"

          Tab:
            title = "About"
            backgroundColor = "#3d3d3d"
            activeBackgroundColor = "#4a90e2"
            textColor = "#cccccc"
            activeTextColor = "#ffffff"

        TabContent:
          backgroundColor = "#1e1e1e"

          # Home Panel
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
                text = "This is the first tab panel. You can put any content here."
                fontSize = 16
                color = "#cccccc"

              Text:
                text = "Try clicking on other tabs to see different content."
                fontSize = 14
                color = "#888888"

          # Profile Panel
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

              Text:
                text = "Role: Developer"
                fontSize = 16
                color = "#cccccc"

          # Settings Panel
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

          # About Panel
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
                fontSize = 18
                color = "#cccccc"

              Text:
                text = "Version: 0.2.0"
                fontSize = 16
                color = "#888888"

              Text:
                text = "Built with Nim and Raylib"
                fontSize = 14
                color = "#888888"

# Run the application
when isMainModule:
  var backend = newRaylibBackendFromApp(app)
  backend.run(app)
