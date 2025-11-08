## Static For Loop Test
##
## Tests the new staticFor macro implementation

import kryon_dsl

# Define test data
const testItems = ["Red", "Green", "Blue"]

# Define the UI using staticFor
let app = kryonApp:
  Header:
    windowWidth = 800
    windowHeight = 600
    windowTitle = "Static For Loop Test"

  Body:
    backgroundColor = "#f8fafc"

    Row:
      mainAxisAlignment = "spaceEvenly"
      alignItems = "center"
      gap = 10
      padding = 20

      # Use staticFor to generate containers for each color
      staticFor(color in testItems):
        Container:
          width = 100
          height = 100
          backgroundColor = "#" & color.toLowerAscii()
          padding = 10

          Text:
            text = color
            color = "#ffffff"
            # Center text in container