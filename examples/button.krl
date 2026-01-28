; Button demo in KRL (KRyon Lisp)
; Demonstrates styling and event handlers

(style "base"
  (backgroundColor "#F5F5F5"))

(function "rc" handleButtonClick ()
  "
    print(\"🎯 Button clicked! Hello from the scripting runtime!\")
")

(App
  (windowWidth 600)
  (windowHeight 400)
  (windowTitle "Interactive Button Demo")
  (windowResizable true)
  (keepAspectRatio false)
  (class "base")

  (Center
    (Button
      (width 150)
      (height 50)
      (text "Click Me!")
      (textAlign "center")
      (backgroundColor "#404080FF")
      (borderColor "#0099FFFF")
      (onClick "handleButtonClick"))))
