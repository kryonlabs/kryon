## Debug Center - Show layout boundaries

import kryon_dsl

let app = kryonApp:
  Header:
    windowWidth = 600
    windowHeight = 400
    windowTitle = "Debug Center"

  Body:
    backgroundColor = "#FF0000FF"  # Red background (full window)

    Container:
      width = 600   # Fill window
      height = 400  # Fill window
      backgroundColor = "#880000FF"  # Dark red (should fill window)

      Center:
        Container:
          width = 400
          height = 200
          backgroundColor = "#0088FFFF"  # Blue (centered container)

          Center:
            Button:
              width = 150
              height = 50
              text = "Centered Button"
              backgroundColor = "#00FF00FF"  # Green (should be centered in blue)