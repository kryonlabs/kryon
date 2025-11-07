
import kryon_dsl

var showMessage = true

proc toggleMessage() =
  showMessage = not showMessage
  echo "Toggled showMessage to: " & $showMessage

let app = kryonApp:
  Header:
    title = "Runtime If/Else Test"
    description = "Testing reactive conditional rendering with var and if"

  Body:
    background = "#f0f0f0"

    Column:
      padding = 20
      gap = 15

      Text:
        text = "Runtime if Test with var"
        fontSize = 24
        color = "#999"

      Button:
        text = "Toggle Message"
        width = 200
        height = 40
        background = "#2196F3"
        color = "white"
        onClick = toggleMessage

      if showMessage:
        Container:
          background = "blue"
          padding = 15

          Text:
            text = "Message is VISIBLE"
            color = "yellow"
            fontSize = 16




