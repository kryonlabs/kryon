## Window Test Demo - Test Window Properties
##
## Tests windowWidth, windowHeight, and windowTitle functionality

import kryon_dsl

# Define the UI with explicit window properties
let app = kryonApp:
  Header:
    windowWidth = 1024
    windowHeight = 768
    windowTitle = "Window Test - 1024x768"

  Body:
    backgroundColor = "#2C3E50FF"  # Dark blue background

    Center:
      Container:
        width = 800
        height = 600
        backgroundColor = "#34495EFF"  # Lighter blue container

        Text:
          text = "Window Properties Test"
          fontSize = 32
          color = "#FFFFFFFF"
          y = 50

        Text:
          text = "Window should be 1024x768"
          fontSize = 24
          color = "#ECF0F1FF"
          y = 120

        Text:
          text = "Title should be 'Window Test - 1024x768'"
          fontSize = 20
          color = "#BDC3C7FF"
          y = 180

        Button:
          width = 200
          height = 60
          text = "Test Button"
          backgroundColor = "#3498DBFF"
          fontSize = 18
          y = 300
          onClick = proc() =
            echo "✓ Window properties test successful!"