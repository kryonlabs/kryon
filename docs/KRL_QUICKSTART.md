# KRL (KRyon Lisp) Quick Start

## What is KRL?

KRL is a Lisp-style S-expression syntax for Kryon that compiles to KIR JSON. It provides an alternative frontend to the `.kry` syntax while maintaining full compatibility with the Kryon compilation pipeline.

## Installation

KRL support is built directly into the `kryon` compiler. No additional installation needed.

```bash
make clean && make
```

## Basic Usage

### Compile KRL to KRB

```bash
kryon compile hello.krl -o hello.krb
```

### Generate KIR only

```bash
kryon compile hello.krl --no-krb -k output.kir
```

### View generated KIR

```bash
kryon compile hello.krl --no-krb -k /tmp/output.kir
cat /tmp/output.kir
```

## Example: Hello World

**hello-world.krl:**
```lisp
; Hello World in KRL
(App
  (windowWidth 800)
  (windowHeight 600)
  (windowTitle "Hello World")

  (Center
    (Text
      (text "Hello, World!")
      (fontSize 32))))
```

**Compile and view:**
```bash
kryon compile examples/hello-world.krl -o hello.krb
```

## Example: Button with Event Handler

**button.krl:**
```lisp
(style "base"
  (backgroundColor "#F5F5F5"))

(function "rc" handleClick ()
  "print('Button clicked!')")

(App
  (windowWidth 600)
  (windowHeight 400)
  (Button
    (text "Click Me!")
    (onClick "handleClick")))
```

## Example: Counter

**counter.krl:**
```lisp
(function "rc" increment () "count = count + 1")
(function "rc" decrement () "count = count - 1")

(App
  (windowWidth 400)
  (Center
    (Column
      (gap 20)
      (Text (text $count) (fontSize 48))
      (Row
        (gap 10)
        (Button (text "-") (onClick "decrement"))
        (Button (text "+") (onClick "increment"))))))
```

## Syntax Reference

### Elements
```lisp
(ElementType
  (property value)
  (childElement ...))
```

### Properties
```lisp
(ElementType
  (width 400)
  (height 300)
  (text "Hello"))
```

### Variables
```lisp
(Text (text $variableName))
```

### Functions
```lisp
(function "rc" functionName ()
  "code here")
```

### Styles
```lisp
(style "styleName"
  (backgroundColor "#007AFF")
  (color "white"))
```

### Directives
```lisp
; For loop
(@for item in items
  (Text (text item)))

; Conditional
(@if (> count 0)
  (Text (text "Positive")))

; Constants
(@const maxWidth 800)
```

## File Structure

- `examples/*.krl` - Example KRL files
- `examples/.kryon_cache/*.kir` - Generated KIR files
- `src/compiler/krl/` - KRL implementation
- `include/krl_parser.h` - KRL API

## Implementation

The KRL implementation consists of:

1. **Lexer** (`krl_lexer.c`) - Tokenizes S-expressions
2. **Parser** (`krl_parser.c`) - Builds S-expression AST
3. **KIR Generator** (`krl_to_kir.c`) - Generates KIR JSON
4. **CLI Integration** (`compile_command.c`) - Auto-detects `.krl` files

## Compilation Pipeline

```
.krl → S-expr Parser → KIR JSON → KIR Reader → AST → KRB Binary
```

The `.krl` file is automatically transpiled to KIR JSON, then processed through the existing Kryon pipeline.

## Advantages

- **Homoiconic**: Code structure is explicit in S-expressions
- **Consistent syntax**: Everything is a list
- **Full compatibility**: Generates standard KIR JSON
- **Zero overhead**: No runtime cost, compiles to same KRB

## More Information

See `docs/KRL_IMPLEMENTATION.md` for detailed implementation documentation.
