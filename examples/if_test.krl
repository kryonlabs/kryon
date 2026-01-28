; Test if/else conditionals

(var count 5)

(Column
  ; Simple if
  (if (> $count 0)
    (Text (text "Positive")))

  ; if-else
  (if (< $count 0)
    (Text (text "Negative"))
    (else
      (Text (text "Not negative"))))

  ; Nested if with else
  (if (== $count 0)
    (Text (text "Zero"))
    (else
      (if (> $count 0)
        (Text (text "Positive"))
        (else
          (Text (text "Negative")))))))
