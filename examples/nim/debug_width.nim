## Debug Width - Test if Center container gets full width

import kryon_dsl

let app = kryonApp:
  Header:
    windowWidth = 600
    windowHeight = 400
    windowTitle = "Debug Width"

  Body:
    backgroundColor = "#FF0000FF"  # Red background (full window)

    Center:
      # Give Center explicit dimensions to test if width is the issue
      width = 600   # Explicit full width
      height = 400  # Explicit full height
      backgroundColor = "#008800FF"  # Dark green (should cover red if width works)

      Button:
        width = 150
        height = 50
        text = "Should be Centered"
        backgroundColor = "#0000FFFF"  # Blue button