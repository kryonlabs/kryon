## Hello World Example

import kryon_dsl

# Define the UI
let app = kryonApp:
  Header:
    width = 800
    height = 600
    title = "Hello World Example"

  Container:
    posX = 200
    posY = 100
    width = 200
    height = 100
    backgroundColor = "#191970FF"
    borderColor = "#0099FFFF"
    borderWidth = 2
    contentAlignment = "center"

    Text:
      text = "Hello World"
      color = "yellow"
