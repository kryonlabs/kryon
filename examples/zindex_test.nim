import ../src/kryon
import ../src/backends/raylib_backend

let app = kryonApp:
  Header:
    windowWidth = 800
    windowHeight = 600
    windowTitle = "Z-Index Overlap Test"
    backgroundColor = "#f0f0f0"

  Body:
    # Instructions text (highest z-index)
    Text:
      text = "Z-Index Test: Blue should be on top, Green in middle, Red at bottom"
      fontSize = 16
      color = "#333"
      posX = 50
      posY = 50
      zIndex = 3000

    # Bottom layer - Red container (should be behind others)
    Container:
      width = 300
      height = 200
      backgroundColor = "#ff4444"
      posX = 100
      posY = 150
      zIndex = 1

      Text:
        text = "RED Layer (z-index: 1)"
        fontSize = 18
        color = "#ffffff"
        posX = 110
        posY = 160

      Text:
        text = "Should be BEHIND others"
        fontSize = 14
        color = "#ffffff"
        posX = 110
        posY = 190

    # Middle layer - Green container (should be above red, below blue)
    Container:
      width = 280
      height = 180
      backgroundColor = "#44aa44"
      posX = 180
      posY = 200
      zIndex = 500

      Text:
        text = "GREEN Layer (z-index: 500)"
        fontSize = 18
        color = "#ffffff"
        posX = 190
        posY = 210

      Text:
        text = "Should be MIDDLE layer"
        fontSize = 14
        color = "#ffffff"
        posX = 190
        posY = 240

    # Top layer - Blue container (should be above all others)
    Container:
      width = 260
      height = 160
      backgroundColor = "#4444ff"
      posX = 260
      posY = 250
      zIndex = 1000

      Text:
        text = "BLUE Layer (z-index: 1000)"
        fontSize = 18
        color = "#ffffff"
        posX = 270
        posY = 260

      Text:
        text = "Should be ON TOP"
        fontSize = 14
        color = "#ffffff"
        posX = 270
        posY = 290

# Run the application
when isMainModule:
  var backend = newRaylibBackendFromApp(app)
  backend.run(app)
