0BSDimport kryon_dsl

let app = kryonApp:
  Header:
    windowWidth = 400
    windowHeight = 400
    windowTitle = "Spacer Test"

  Body:
    backgroundColor = "#f0f0f0"
    padding = 20

    Column:
      gap = 5

      Text:
        text = "Item 1"
        fontSize = 20

      Spacer()              # Default spacing - super simple!

      Text:
        text = "Item 2"
        fontSize = 20

      Gap(height = 30)      # Custom spacing using Gap alias

      Text:
        text = "Item 3"
        fontSize = 20

      Spacer(height = 1, backgroundColor = "#cccccc")  # Divider line

      Text:
        text = "Item 4"
        fontSize = 20
