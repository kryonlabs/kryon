import ../src/kryon

# Define styles outside kryonApp as requested
style containerStyle():
  backgroundColor = "violet"
  padding = 20

style textStyle():
  textColor = "blue"
  fontSize = 16

style buttonStyle():
  backgroundColor = "purple"
  textColor = "red"
  padding = 10

# Test application with styles
let app = kryonApp:
  Container:
    style = containerStyle()
    width = 300
    height = 200

    Text:
      style = textStyle()
      text = "Hello Styled World!"

    Button:
      style = buttonStyle()
      text = "Click Me"
      onClick = echo "Button clicked!"
