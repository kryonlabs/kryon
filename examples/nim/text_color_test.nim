## Text Color Test - Verifying color aliases work correctly

import kryon_dsl


let app = kryonApp:
  Header:
    windowWidth = 800
    windowHeight = 600
    windowTitle = "Text Color Test"

  Body:
    backgroundColor = "#191919FF"

    Column:
      gap = 20
      padding = 20

      # Test Button with pink text
      Button:
        width = 200
        height = 60
        text = "Pink Button"
        color = "pink"
        backgroundColor = "#404080FF"

      # Test Button with orange text
      Button:
        width = 200
        height = 60
        text = "Orange Button"
        color = "orange"
        backgroundColor = "#404080FF"

      # Test Button with purple text
      Button:
        width = 200
        height = 60
        text = "Purple Button"
        color = "purple"
        backgroundColor = "#404080FF"

      # Test Button with red text
      Button:
        width = 200
        height = 60
        text = "Red Button"
        color = "red"
        backgroundColor = "#404080FF"

      # Test Button with lightblue text
      Button:
        width = 200
        height = 60
        text = "Light Blue Button"
        color = "lightblue"
        backgroundColor = "#404080FF"
