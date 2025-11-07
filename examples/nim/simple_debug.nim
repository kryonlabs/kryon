## Simple Debug - Test Center component directly

import kryon_dsl

let app = kryonApp:
  Header:
    windowWidth = 400
    windowHeight = 300
    windowTitle = "Simple Debug"

  Body:
    backgroundColor = "#FF0000FF"  # Red background

    # Test Center directly with minimal nesting
    Center:
      Container:
        width = 200
        height = 100
        backgroundColor = "#00FF00FF"  # Green container

        Button:
          width = 80
          height = 30
          text = "Test"
          backgroundColor = "#0000FFFF"  # Blue button