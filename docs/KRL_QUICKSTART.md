# KRL Quick Start

## What is KRL?

KRL is a Lisp-style S-expression syntax for Kryon. Files use `.krl` extension.

## Installation

Built into `kryon` compiler. No extra setup needed.

```bash
make
```

## Basic Usage

```bash
# Compile
kryon compile hello.krl -o hello.krb

# View KIR
kryon compile hello.krl --no-krb -k output.kir

# Run
kryon run hello.krb --renderer sdl2
```

## Examples

### Hello World

```lisp
(App
  (windowWidth 800)
  (windowHeight 600)
  (Center
    (Text (text "Hello, World!"))))
```

### Counter with State

```lisp
(var count 0)
(function "rc" increment () "count++;")
(function "rc" decrement () "count--;")

(App
  (windowWidth 400)
  (Column
    (Text (text $count) (fontSize 48))
    (Row
      (Button (text "-") (onClick decrement))
      (Button (text "+") (onClick increment)))))
```

### Arrays and Loops

```lisp
(const colors ["red" "green" "blue"])

(App
  (Column
    ; Runtime loop
    (var items ["A" "B" "C"])
    (for (i item) in items
      (Text (text $item)))))
```

### Conditionals

```lisp
(var score 85)

(App
  (if (>= $score 90)
    (Text (text "A"))
    (else
      (if (>= $score 80)
        (Text (text "B"))
        (else
          (Text (text "C")))))))
```

### Reusable Components

```lisp
(component Counter ((initialValue 0) (label "Count"))
  (var count initialValue)
  (function "rc" inc () "count++;")
  (function "rc" dec () "count--;")

  (Column
    (Text (text $label))
    (Text (text $count))
    (Row
      (Button (text "-") (onClick dec))
      (Button (text "+") (onClick inc)))))

(App
  (Row
    (Counter (initialValue 0) (label "First"))
    (Counter (initialValue 10) (label "Second"))))
```

## Syntax Quick Reference

```lisp
; Elements & Properties
(ElementType (prop value) (Child ...))

; Variables
(const name value)
(var name initialValue)
$varName

; Arrays
["item1" "item2"]

; Loops
(for item in array ...)

; Conditionals
(if condition ... (else ...))

; Components
(component Name ((param default)) ...)

; Functions & Styles
(function "rc" name () "code")
(style "name" (prop value) ...)

; Expressions
(+ a b)  (> a b)  (. obj prop)

; Comments
; Line comment
```

## More Info

See `docs/KRL_IMPLEMENTATION.md` for detailed documentation.
