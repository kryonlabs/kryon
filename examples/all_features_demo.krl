; KRL All Features Demo
; This file demonstrates all implemented KRL features:
; - Array literals
; - for loops (runtime iteration)
; - if/else conditionals
; - Component definitions

; Define some constants with arrays
(const colors ["#FF5733" "#33FF57" "#3357FF" "#F333FF"])
(const sizes [10 20 30 40 50])

; Define a reusable Counter component
(component Counter ((initialValue 0) (label "Count") (step 1))
  ; Component state
  (var count initialValue)

  ; Component functions
  (function "rc" increment () "count = count + step;")
  (function "rc" decrement () "count = count - step;")
  (function "rc" reset () "count = initialValue;")

  ; Component UI
  (Column
    (Text (text $label))
    (Text (text $count))
    (Row
      (Button (text "-") (onClick decrement))
      (Button (text "Reset") (onClick reset))
      (Button (text "+") (onClick increment)))))

; Main application UI
(Column
  (Text (text "KRL Feature Demo"))

  ; Section 1: Runtime for loop
  (var items ["Apple" "Banana" "Cherry" "Date"])
  (Container
    (Text (text "Fruit List (runtime for):"))
    (Column
      (for (i item) in items
        (Row
          (Text (text $i))
          (Text (text $item))))))

  ; Section 2: Conditionals
  (var score 75)
  (Container
    (Text (text "Grade Calculator (if/else):"))
    (if (>= $score 90)
      (Text (text "Grade: A"))
      (else
        (if (>= $score 80)
          (Text (text "Grade: B"))
          (else
            (if (>= $score 70)
              (Text (text "Grade: C"))
              (else
                (Text (text "Grade: F")))))))))

  ; Section 3: Component usage
  (Container
    (Text (text "Counters (component instances):"))
    (Row
      (Counter (initialValue 0) (label "Counter 1") (step 1))
      (Counter (initialValue 10) (label "Counter 2") (step 5))
      (Counter (initialValue 100) (label "Counter 3") (step 10)))))
