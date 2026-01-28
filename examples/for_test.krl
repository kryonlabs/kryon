; Test runtime for loops

(var items ["apple" "banana" "cherry"])

(Column
  ; Simple iteration
  (for item in items
    (Text (text $item)))

  ; With index
  (for (i item) in items
    (Row
      (Text (text $i))
      (Text (text $item)))))
