# KRL (KRyon Lisp) Implementation

## Overview

KRL is a Lisp-style S-expression syntax frontend for Kryon that compiles to KIR JSON. It provides a homoiconic alternative to the `.kry` syntax while maintaining full compatibility with the Kryon pipeline.

## Architecture

```
.krl (KRyon Lisp) → S-expr Parser → KIR JSON → KIR Reader → AST → KRB Binary
```

### Key Components

1. **KRL Lexer** (`src/compiler/krl/krl_lexer.c`)
   - Tokenizes S-expression syntax
   - Handles strings, numbers, symbols, keywords, variables
   - Supports comments (`;` to end of line)

2. **KRL Parser** (`src/compiler/krl/krl_parser.c`)
   - Parses tokens into S-expression AST
   - Creates typed S-expression nodes (symbols, strings, integers, floats, booleans, lists)

3. **KRL to KIR Converter** (`src/compiler/krl/krl_to_kir.c`)
   - Transpiles S-expressions directly to KIR JSON
   - Maps Lisp forms to KIR node types
   - Generates valid KIR JSON documents

4. **CLI Integration** (`src/cli/compile_command.c`)
   - Detects `.krl` file extension
   - Automatically transpiles to `.kir` before compilation
   - Seamless integration with existing pipeline

## KRL Syntax

### Elements

```lisp
(ElementType
  (property value)
  ...)
```

Example:
```lisp
(Container
  (width 400)
  (height 300)
  (Text
    (text "Hello World")))
```

### Styles

```lisp
(style "name"
  (property value)
  ...)
```

Example:
```lisp
(style "button"
  (backgroundColor "#007AFF")
  (color "white"))
```

### Functions

```lisp
(function "language" functionName ()
  "code")
```

Example:
```lisp
(function "rc" handleClick ()
  "print('Button clicked!')")
```

### Directives

#### For Loops
```lisp
(@for varName in arrayName
  ...)
```

#### Conditionals
```lisp
(@if condition
  ...)
```

#### Constants
```lisp
(@const name value)
```

#### Variables
```lisp
(@var name value)
```

#### State
```lisp
(@state name value)
```

### Variables

Reference variables with `$`:

```lisp
(Text (text $count))
```

### Expressions

#### Binary Operations
```lisp
(+ 10 20)
(> count 0)
(and condition1 condition2)
```

#### Member Access
```lisp
(. object property)
```

#### Array Access
```lisp
([] array index)
```

#### Function Calls
```lisp
(functionName arg1 arg2)
```

## Value Types

KRL automatically infers types:

- **Strings**: `"hello"`
- **Integers**: `42`
- **Floats**: `3.14`
- **Booleans**: `true`, `false`
- **Colors**: `#FF0000`, `#00FF00AA`
- **Units**: Will be parsed as strings like `"20px"`, `"50%"`, `"1.5em"`

## Examples

### Hello World
```lisp
(App
  (windowWidth 800)
  (windowHeight 600)
  (Center
    (Text (text "Hello, World!"))))
```

### Button with Event Handler
```lisp
(style "primary-btn"
  (backgroundColor "#007AFF")
  (color "white"))

(function "rc" handleClick ()
  "print('Clicked!')")

(App
  (windowWidth 600)
  (Button
    (text "Click Me!")
    (class "primary-btn")
    (onClick "handleClick")))
```

### Counter with State
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

## Usage

### Compile KRL to KRB

```bash
kryon compile hello.krl -o hello.krb
```

### Generate KIR Only

```bash
kryon compile hello.krl --no-krb -k hello.kir
```

### Run KRL Program

```bash
kryon compile button.krl -o button.krb
kryon run button.krb --renderer sdl2
```

## Implementation Details

### Transpilation Flow

1. **Lexing**: `krl_lexer.c` tokenizes the input
2. **Parsing**: `krl_parser.c` builds S-expression AST
3. **KIR Generation**: `krl_to_kir.c` generates KIR JSON
4. **KIR Reading**: Existing `kir_reader.c` parses JSON to AST
5. **Codegen**: Existing pipeline generates KRB

### Node Type Mapping

| KRL Form | KIR Node Type |
|----------|---------------|
| `(style ...)` | `STYLE_BLOCK` |
| `(function ...)` | `FUNCTION_DEFINITION` |
| `(ElementType ...)` | `ELEMENT` |
| `(@for ...)` | `FOR_DIRECTIVE` |
| `(@if ...)` | `IF_DIRECTIVE` |
| `(@const ...)` | `CONST_DEFINITION` |
| `(@var ...)` | `VARIABLE_DEFINITION` |
| `(@state ...)` | `STATE_DEFINITION` |
| `$variable` | `VARIABLE` |
| `(+ a b)` | `BINARY_OP` |
| `(. obj prop)` | `MEMBER_ACCESS` |

### Advantages of KRL

1. **Homoiconic**: Code is data, enabling programmatic manipulation
2. **Minimal syntax**: Consistent S-expression format
3. **Full compatibility**: Generates standard KIR JSON
4. **Zero semantic changes**: Maps 1:1 to existing KIR nodes
5. **Easy parsing**: Simple recursive descent parser

### Limitations

1. **Not a true Lisp**: No macros or first-class functions
2. **Syntax-only alternative**: Doesn't add new language features
3. **Limited metaprogramming**: Only existing `@const_for` and `@const_if`

## File Organization

```
include/krl_parser.h          # Public API
src/compiler/krl/
  ├── krl_lexer.c             # Tokenizer (~800 LOC)
  ├── krl_parser.c            # S-expression parser (~500 LOC)
  └── krl_to_kir.c            # KIR JSON generator (~1500 LOC)
examples/
  ├── hello-world.krl
  ├── button.krl
  └── counter.krl
```

## Testing

### Manual Testing

```bash
# Transpile KRL to KIR
kryon compile examples/hello-world.krl --no-krb -k hello.kir

# View generated KIR
cat hello.kir

# Compile to KRB
kryon compile hello.kir -o hello.krb

# Run
kryon run hello.krb --renderer sdl2
```

### Round-Trip Verification

```bash
# KRL → KIR
kryon compile button.krl --no-krb -k button-from-krl.kir

# KRY → KIR (for comparison)
kryon compile button.kry --no-krb -k button-from-kry.kir

# KIR → KRY (decompile)
kryon print button-from-krl.kir -o button-roundtrip.kry

# Compare semantics
diff button-from-krl.kir button-from-kry.kir
```

## Future Enhancements

Potential future additions (not in current implementation):

1. **Macros**: Compile-time code transformation
2. **List operations**: `map`, `filter`, `reduce` for UI generation
3. **Template syntax**: Quasiquotation for code generation
4. **REPL**: Interactive development environment
5. **Package system**: Module imports and namespaces

## Conclusion

KRL provides a Lisp-style frontend for Kryon that maintains full compatibility with the existing pipeline while offering a homoiconic syntax for those who prefer S-expressions. It's implemented as a lightweight transpiler (~2800 LOC) that generates standard KIR JSON.
