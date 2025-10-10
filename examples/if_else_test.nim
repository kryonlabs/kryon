
import ../src/kryon
import ../src/backends/raylib_backend

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
        color = "#333"

      Button:
        text = "Toggle Message"
        width = 200
        height = 40
        background = "#2196F3"
        color = "white"
        onClick = toggleMessage

      if showMessage:
        Container:
          background = "#4CAF50"
          padding = 15

          Text:
            text = "Message is VISIBLE"
            color = "white"
            fontSize = 16



# Debug: Print the app structure
echo "=== App Structure ==="
app.printTree()

# Run the application
when isMainModule:
  var backend = newRaylibBackendFromApp(app)
  backend.run(app)
