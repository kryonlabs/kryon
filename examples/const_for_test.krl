; Test const_for loops

(const colors ["red" "green" "blue"])

(Column
  ; Simple iteration
  (const_for color in colors
    (Box (backgroundColor $color)))

  ; With index
  (const_for (i color) in colors
    (Text (text $color)))

  ; Range iteration
  (const_for i in (range 1 5)
    (Container (id $i))))
