# KRL (KRyon Lisp) Implementation

## Overview

KRL is a Lisp-style S-expression syntax for Kryon that compiles to KIR JSON.

**Pipeline:** `.krl → S-expr Parser → KIR JSON → AST → KRB`

## Syntax Reference

### Basic Forms

```lisp
; Elements
(ElementType (prop value) (Child ...))

; Variables
$varName

; Arrays
["item1" "item2" "item3"]

; Constants
(const name value)

; State
(var name initialValue)
```

### Control Flow

```lisp
; Compile-time loops
(const_for item in array ...)
(const_for (i item) in array ...)
(const_for i in (range 1 10) ...)

; Runtime loops
(for item in array ...)
(for (i item) in array ...)

; Conditionals
(if condition
  then-elements...
  (else else-elements...))
```

### Components

```lisp
(component Name ((param default))
  (var state initialValue)
  (function "rc" funcName () "code")
  (ElementRoot ...))
```

### Expressions

```lisp
(+ a b)                ; Binary ops: +, -, *, /, >, <, ==, !=, >=, <=, &&, ||
(. object property)    ; Member access
([] array index)       ; Array access
(functionName args)    ; Function call
```

### Other

```lisp
(style "name" (prop value) ...)
(function "language" name () "code")
; Comment to end of line
```

## Complete Example

```lisp
(component Counter ((initialValue 0))
  (var count initialValue)
  (function "rc" increment () "count++;")

  (Column
    (Text (text $count))
    (Button (text "+") (onClick increment))))

(const colors ["red" "green" "blue"])

(App
  (windowWidth 800)
  (const_for color in colors
    (Box (backgroundColor $color)))
  (Counter (initialValue 10)))
```

## Usage

```bash
# Compile to KRB
kryon compile file.krl -o file.krb

# Generate KIR only
kryon compile file.krl --no-krb -k file.kir

# Run
kryon run file.krb --renderer sdl2
```

## Implementation

**Files:**
- `include/krl_parser.h` - API
- `src/compiler/krl/krl_lexer.c` - Tokenizer
- `src/compiler/krl/krl_parser.c` - S-expr parser
- `src/compiler/krl/krl_to_kir.c` - KIR generator
- `src/compiler/kir/kir_reader.c` - KIR node handlers

**Features:**
- Full `.kry` feature parity
- Zero runtime overhead
- Standard KIR JSON output
- Homoiconic syntax
