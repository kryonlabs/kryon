; Test component definition

(component Counter ((initialValue 0) (step 1))
  ; State variable
  (var count initialValue)

  ; Functions
  (function "rc" increment () "count = count + step;")
  (function "rc" decrement () "count = count - step;")

  ; UI tree
  (Column
    (Text (text $count))
    (Row
      (Button (text "-") (onClick decrement))
      (Button (text "+") (onClick increment)))))

; Use the component
(Counter (initialValue 10))
