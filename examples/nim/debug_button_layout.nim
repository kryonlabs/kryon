## Debug Button Layout - Visualize container boundaries

import kryon_dsl

proc handleButtonClick() =
  echo "🎯 Button clicked!"

let app = kryonApp:
  Header:
    windowWidth = 600
    windowHeight = 400
    windowTitle = "Debug Button Layout"

  Body:
    backgroundColor = "#FF0000FF"  # Red background (full window)

    Center:
      Container:
        width = 600  # Should fill window
        height = 400 # Should fill window
        backgroundColor = "#00FF00FF"  # Green (should cover red if Center works)
        justifyContent = "center"
        alignItems = "center"

        Button:
          width = 150
          height = 50
          text = "Centered Button"
          backgroundColor = "#0000FFFF"  # Blue button