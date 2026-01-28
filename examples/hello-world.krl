; Hello World in KRL (KRyon Lisp)
; This is the simplest possible Kryon application

(App
  (windowWidth 800)
  (windowHeight 600)
  (windowTitle "Hello World - KRL Demo")

  (Center
    (Text
      (text "Hello, World!")
      (fontSize 32)
      (color "#333333"))))
