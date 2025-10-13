## Simple Navigation Demo
##
## Demonstrates the state-driven enum routing system in Kryon.
## This example shows how to create a multi-screen application
## using the existing reactive system for navigation.

import ../src/kryon
import ../src/kryon/navigation

# ============================================================================
# Route Definition
# ============================================================================

# Define the screens for this app
type AppRoute = enum
  HomeScreen
  AboutScreen
  SettingsScreen

# ============================================================================
# Global State
# ============================================================================

# Define the global state for the current route
var currentRoute = HomeScreen

# ============================================================================
# Screen Components
# ============================================================================

proc HomeScreen(): Element =
  ## Home screen component with navigation buttons
  result = Column:
    mainAxisAlignment = "center"
    gap = 30
    padding = 40

    Text:
      text = "Welcome to Kryon Navigation!"
      fontSize = 32
      color = "#2C3E50"
      textAlign = "center"

    Text:
      text = "This demo shows simple enum-based routing"
      fontSize = 18
      color = "#7F8C8D"
      textAlign = "center"

    Row:
      mainAxisAlignment = "center"
      gap = 20

      Button:
        text = "About"
        width = 120
        height = 50
        backgroundColor = "#3498DB"
        fontSize = 16
        onClick = proc() = navigateTo(currentRoute, AboutScreen)

      Button:
        text = "Settings"
        width = 120
        height = 50
        backgroundColor = "#2ECC71"
        fontSize = 16
        onClick = proc() = navigateTo(currentRoute, SettingsScreen)

    Text:
      text = "Current route: " & $currentRoute
      fontSize = 14
      color = "#95A5A6"
      textAlign = "center"

proc AboutScreen(): Element =
  ## About screen component with app information
  result = Column:
    mainAxisAlignment = "center"
    gap = 25
    padding = 40

    Text:
      text = "About This Demo"
      fontSize = 28
      color = "#2C3E50"
      textAlign = "center"

    Container:
      width = 500
      height = 200
      backgroundColor = "#ECF0F1"
      contentAlignment = "center"

      Column:
        gap = 15
        padding = 20

        Text:
          text = "Kryon Navigation System"
          fontSize = 20
          color = "#34495E"
          textAlign = "center"

        Text:
          text = "A simple enum-based routing approach"
          fontSize = 16
          color = "#7F8C8D"
          textAlign = "center"

        Text:
          text = "• Type-safe with enums"
          fontSize = 14
          color = "#95A5A6"

        Text:
          text = "• Reactive navigation"
          fontSize = 14
          color = "#95A5A6"

        Text:
          text = "• Clean separation of screens"
          fontSize = 14
          color = "#95A5A6"

    Button:
      text = "← Back to Home"
      width = 150
      height = 45
      backgroundColor = "#95A5A6"
      fontSize = 16
      onClick = proc() = navigateTo(currentRoute, HomeScreen)

proc SettingsScreen(): Element =
  ## Settings screen component with demo options
  result = Column:
    mainAxisAlignment = "center"
    gap = 20
    padding = 40

    Text:
      text = "Settings"
      fontSize = 28
      color = "#2C3E50"
      textAlign = "center"

    Container:
      width = 400
      height = 250
      backgroundColor = "#ECF0F1"
      contentAlignment = "center"

      Column:
        gap = 15
        padding = 20

        Row:
          gap = 15
          mainAxisAlignment = "spaceBetween"
          width = 350

          Text:
            text = "Enable Notifications"
            fontSize = 16
            color = "#34495E"

          Checkbox:
            checked = true

        Row:
          gap = 15
          mainAxisAlignment = "spaceBetween"
          width = 350

          Text:
            text = "Dark Mode"
            fontSize = 16
            color = "#34495E"

          Checkbox:
            checked = false

        Row:
          gap = 15
          mainAxisAlignment = "spaceBetween"
          width = 350

          Text:
            text = "Auto-save"
            fontSize = 16
            color = "#34495E"

          Checkbox:
            checked = true

        Input:
          placeholder = "Enter your name"
          width = 350
          height = 40

    Row:
      mainAxisAlignment = "center"
      gap = 15

      Button:
        text = "Save Settings"
        width = 120
        height = 45
        backgroundColor = "#27AE60"
        fontSize = 16
        onClick = proc() = echo "Settings saved!"

      Button:
        text = "Back to Home"
        width = 120
        height = 45
        backgroundColor = "#E74C3C"
        fontSize = 16
        onClick = proc() = navigateTo(currentRoute, HomeScreen)

# ============================================================================
# Main Application
# ============================================================================

let app = kryonApp:
  Header:
    windowWidth = 800
    windowHeight = 600
    windowTitle = "Kryon Navigation Demo"

  Body:
    backgroundColor = "#FFFFFF"

    # Use a case statement to render the correct screen
    # This is reactive! When `currentRoute` changes, this block re-evaluates.
    case currentRoute:
    of HomeScreen:
      HomeScreen()
    of AboutScreen:
      AboutScreen()
    of SettingsScreen:
      SettingsScreen()