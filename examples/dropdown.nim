import ../src/kryon
import ../src/backends/raylib_backend

let app = kryonApp:
  Header:
    windowWidth = 400
    windowHeight = 300
    windowTitle = "Dropdown Demo"
    backgroundColor = "#f8f9fa"

  Body:
    Column:
      mainAxisAlignment = "center"
      crossAxisAlignment = "center"
      gap = 20
      padding = 40

      Text:
        text = "Select your favorite color:"
        fontSize = 16
        color = "#6b7280"

      Dropdown:
        width = 200
        height = 40
        placeholder = "Choose a color..."
        fontSize = 14
        backgroundColor = "#ffffff"
        borderColor = "#d1d5db"
        borderWidth = 1
        color = "#2d3748"
        options = @["Red", "Green", "Blue", "Yellow", "Purple", "Orange"]
        selectedIndex = 0

      Text:
        text = "Select your country:"
        fontSize = 16
        color = "#6b7280"

      Dropdown:
        width = 200
        height = 40
        placeholder = "Choose a country..."
        fontSize = 14
        backgroundColor = "#ffffff"
        borderColor = "#d1d5db"
        borderWidth = 1
        color = "#2d3748"
        options = @["United States", "Canada", "United Kingdom", "Australia", "Germany", "France"]
        selectedIndex = 0

# Run the application
when isMainModule:
  var backend = newRaylibBackendFromApp(app)
  backend.run(app)