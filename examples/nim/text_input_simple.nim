## Simple Text Input Example

import kryon_dsl

var textValue = ""

# Define the UI
let app = kryonApp:
  Header:
    width = 500
    height = 300
    title = "Simple Text Input"

  Body:
    backgroundColor = "#f5f5f5"

    Column:
      gap = 15
      backgroundColor = "white"
      padding = 20

      Text:
        text = "Enter some text:"
        color = "#333"
        fontSize = 16

      Input:
        placeholder = "Type here..."
        value = textValue
        width = 300
        height = 40
        backgroundColor = "white"
        borderColor = "#bdc3c7"
        borderWidth = 1

      Text:
        text = "You typed: " & textValue
        color = "#7f8c8d"
        fontSize = 14
