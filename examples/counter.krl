; Counter component in KRL
; Demonstrates state management and event handlers

(function "sh" increment ()
  "count = count + 1")

(function "sh" decrement ()
  "count = count - 1")

(function "sh" reset ()
  "count = 0")

(App
  (windowWidth 400)
  (windowHeight 300)
  (windowTitle "Counter Demo - KRL")

  (Center
    (Column
      (gap 20)
      (alignItems "center")

      (Text
        (text "Counter Demo")
        (fontSize 24)
        (fontWeight "bold"))

      (Text
        (text "$count")
        (fontSize 48)
        (color "#007AFF"))

      (Row
        (gap 10)

        (Button
          (text "-")
          (width 60)
          (height 40)
          (onClick "decrement"))

        (Button
          (text "Reset")
          (width 80)
          (height 40)
          (onClick "reset"))

        (Button
          (text "+")
          (width 60)
          (height 40)
          (onClick "increment"))))))
