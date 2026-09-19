# Kry Language Specification

Kry language version: `0.3`
KIR version: `0.1`

This is the stable public contract for `.kry` source accepted by Kryon tools.
Kry is intentionally C-close: expressions and types stay familiar to C, while
the frontend owns declarations, compile-time guards, UI frame structure, and
KIR emission.

The active pipeline is:

```text
.kry -> KIR -> C
.kry -> KIR -> Go
.kry -> KIR -> KRB
```

The old `.kry -> KIR -> JS` path is paused. It remains in the tree as
experimental reference material for the future web-native roadmap target, not
as part of the supported language contract.

Backends may accept `.kry` directly as a CLI convenience, but they must behave
as if the source was first lowered into KIR. Unsupported behavior must be
diagnosed or represented honestly as unsupported KIR; it must not silently
change semantics.

## Compatibility

- Patch-level language changes may add diagnostics, new KIR metadata, or new
  backend support for previously rejected forms.
- A minor language version bump is required when new source syntax becomes
  documented as stable.
- A major language version bump is required if valid version `0.x` source would
  be interpreted differently.
- Raw statement text remains part of KIR until structured expression lowering is
  complete enough for every backend that needs it.

## Lexical Model

Kry source is line-oriented with C-like tokens inside expressions. Whitespace
separates declarations and statements but is otherwise not semantic except
inside strings and raw text.

```ebnf
identifier      = letter , { letter | digit | "_" } ;
string          = '"' , { character | escape } , '"' ;
integer         = digit , { digit | "_" | "x" | "X" | "a".."f" | "A".."F" } ,
                  { "u" | "U" | "l" | "L" } ;
type-text       = C-compatible type text up to a grammar delimiter ;
expr-text       = C-close expression text up to a statement/block delimiter ;
raw-c-text      = text after the `c` statement marker ;
```

The frontend tokenizes expressions and parses them with C operator precedence.
Full C-close expression text is still preserved for backends. Unsupported forms
remain explicit `unknown` nodes; compound initializers retain their source text.

## Top-Level Grammar

```ebnf
source          = { top-level-form } ;

top-level-form  = module-decl
                | output-decl
                | import-decl
                | pragma-decl
                | error-decl
                | assert-decl
                | const-decl
                | type-decl
                | enum-decl
                | state-block
                | app-block
                | function-decl
                | guarded-top-level ;

module-decl     = "#module" , string ;
output-decl     = "#output" , string ;
import-decl     = "#import" , ( string | "<" , text , ">" ) , [ "#private" ]
                | identifier , "::" , "#import" , string ;
pragma-decl     = "#pragma" , string ;
error-decl      = "#error" , string ;
assert-decl     = "#assert" , expr-text , [ "," , string ] ;

const-decl      = identifier , "::" , const-expr ;
const-expr      = "#defined" , "(" , identifier , ")"
                | "#run" , integer-expr
                | expr-text ;

type-decl       = identifier , "::" , "struct" , "{" , { field-decl } , "}"
                | identifier , "::" , type-text , "#type" , [ "#private" ] ;
field-decl      = identifier , ":" , type-text , [ "=" , expr-text ] ;

enum-decl       = identifier , "::" , "enum" , "{" , { enum-item } , "}"
                | "#enum" , [ "#private" ] , "{" , { enum-item } , "}" ;
enum-item       = identifier , [ "=" , expr-text ] ;

state-block     = "state" , "{" , { field-decl } , "}" ;
app-block       = "app" , string , "{" , { app-property } , "}" ;
app-property    = "size" , integer , integer
                | "fps" , integer
                | "font" , identifier
                | "theme" , identifier , identifier ;

function-decl   = identifier , "::" , signature , modifiers , block ;
signature       = "(" , [ params ] , ")" , [ "->" , type-text ] ;
params          = param , { "," , param } ;
param           = identifier , ":" , type-text ;
modifiers       = { "#export" | "#private" | "#global" | "#extern" ,
                    [ string ] | "#intrinsic" , string | "#ui" | abi-tag } ;
abi-tag         = "#storage" , string | "#abi" , string | "#attr" , string ;
```

## Statement Grammar

```ebnf
block           = "{" , { statement } , "}" ;

statement       = local-decl
                | assignment
                | ui-node
                | call-statement
                | if-statement
                | while-statement
                | for-statement
                | switch-statement
                | guard-statement
                | defer-statement
                | return-statement
                | jump-statement
                | label-statement
                | raw-c-statement
                | unused-statement
                | guarded-statement ;

local-decl      = identifier , ":=" , expr-text
                | identifier , ":" , type-text , [ "=" , expr-text ] ;
assignment      = expr-text , assignment-op , expr-text ;
assignment-op   = "=" | "+=" | "-=" | "*=" | "/=" | "%=" | "&=" | "|=" | "^=" ;
call-statement  = identifier , "(" , [ expr-text ] , ")" ;
ui-node         = layout-node | widget-props-node ;
layout-node     = layout-widget , identifier , [ ":" ] , "{" ,
                  { ui-prop | statement } , "}" ;
widget-props-node = props-widget , [ identifier ] , [ ":" ] , "{" ,
                    { ui-prop } , "}" ;
ui-prop         = identifier , "=" , expr-text ;
if-statement    = "if" , expr-text , block ,
                  { "else" , "if" , expr-text , block } ,
                  [ "else" , block ] ;
while-statement = "while" , expr-text , block ;
for-statement   = "for" , expr-text , block ;
switch-statement= "switch" , expr-text , "{" ,
                  { "case" , expr-text , ":" , { statement } } ,
                  [ "default" , ":" , { statement } ] , "}" ;
guard-statement = "guard" , expr-text , block ;
defer-statement = "defer" , statement ;
return-statement= "return" , [ expr-text ] ;
jump-statement  = "goto" , identifier | "break" | "continue" ;
label-statement = identifier , ":" ;
raw-c-statement = "c" , raw-c-text ;
unused-statement= "unused" , expr-text ;
```

A named synchronous child-content signature uses `#slot`:

```kry
Content :: (bounds: Rectangle) #slot
DrawChild :: (bounds: Rectangle) {
    // Draw the child inside these bounds.
}
Compose :: (bounds: Rectangle, content: Content) {
    content(bounds)
}
Draw :: (bounds: Rectangle) {
    Compose(bounds, DrawChild)
}
```

A slot returns `void` and accepts typed scalar or record arguments. A matching
local or imported `.kry` function can initialize, replace, or supply a slot value;
conditional selection uses the same expected signature. Parameters must match
exactly, including the declaration identity of record types. A lexical binding
shadows a function with the same name. Each invocation evaluates the callable
before its arguments, and record arguments are copied by value. Function values
preserve their source module state and runtime receiver. Slots cannot be stored
in records, module state, globals, or returned from functions; local bindings
require an initializer. A named widget block may initialize the first props
record's fields and supply subsequent slot parameters by name. Every slot is
required, names cannot collide with props fields or other slots, and values
evaluate once in source order. Omitted props fields remain zero-initialized.
Block calls and ordinary calls invoke the same declaration, including declared
widgets that use built-in names.

An inline body initializes a slot with `child: Content = (bounds: Rectangle)
#slot { ... }`, or supplies a block property with `content = (bounds: Rectangle)
#slot { ... }` (the signature and `#slot` appear on one logical line). Its exact
parameter types must match the expected slot. Bodies may nest and read or mutate
captured scalar and record bindings. Captured retained instances preserve their
storage identity. A bare `return` exits only the inline body. Slot reassignment
is allowed only in the binding's declaring block; writes to an outer block's
slot or a captured slot are rejected. Host callbacks borrow these environments
and must complete synchronously without retaining them.

Plain call statements are UI declarations when they call Kryon widget/runtime
functions. Leaf property-block widgets lower to the existing `WidgetProps`
C-style call shape. Layout nodes (`Screen`, `Column`, `Row`, `Stack`) open
retained UI scopes and therefore emit a matching `End()`.

`Button` is also a child-bearing scope. The compiler lowers it through the
runtime's Button-content entry and emits `End()`. Its label property creates an
ordinary child `Text`, so these declarations are equivalent:

```kry
Button save: { label = "Save" }

Button save: {
    Text { text = "Save" }
}
```

Button may contain non-interactive visual/layout nodes such as `Text`, `Icon`,
`Image`, `Row`, `Column`, and `Spacer`. Nested interactive controls are not a
valid Button content tree.

## Tooling

`k2c`, `k2cpp`, and `k2go` accept `--strict` for active targets. The old `k2js` path is paused and not part of the supported language contract. This enables the shared
scalar type checker before output generation. It checks lexical bindings,
duplicate local declarations, scalar operand compatibility, assignments,
function argument counts/types, return value types, and boolean conditions.
Unresolved names, opaque expressions, and unsupported statements are errors with
source locations. Accepted non-extern functions must use the shared scalar
emitter; strict mode cannot silently fall back to backend text interpretation.
It currently excludes UI calls, raw C, `for` headers, `switch`, pointer-sized
types, and aggregate/pointer expressions. It does not yet prove that every
control-flow path returns a value. Existing app builds may omit the flag while
these areas are being migrated.

The checker also annotates known expression types in ordinary builds, without
rejecting unresolved imported C/runtime symbols. KIR dumps expose these types.
`make language-test` runs expression-tree tests and executes matching numeric
and cleanup fixtures through the active scalar targets, including arithmetic
traps and negative diagnostics. See [portable scalar programming](LANGUAGE_SCALARS.md)
for the exact numeric contract and build commands.

## Fixed arrays and string bytes

Strict portable functions may read and write elements of fixed-capacity array
record fields and read bytes of borrowed strings:

```kry
WindowBytes :: struct {
    data: [8]u32
    filled: i32
}

SumTextBytes :: (text: string) -> i32 #export {
    total: i32 = 0
    index: i32 = 0
    while index < text.length {
        total = total + (i32)text[index]
        index = index + 1
    }
    return total
}
```

Rules:

- `[N]element` is valid as a record field or local type. Elements can be
  portable scalars, enums, strings or records; nested array types are rejected.
  Records may themselves contain fixed arrays.
- `record.field[index]` reads and writes one element with value semantics.
  Array and string indices must be integers; floating-point indices are errors.
- Array locals without an initializer are recursively zero-initialized.
  `items: [3]i32 = {1, 2}` constructs an array with a zero final element.
  `([3]i32){1, 2}` is the explicitly typed expression form. Initializer elements
  execute left to right, use the declared element type, and cannot exceed the
  capacity. Array literals require resolved capacities and positional elements.
- Array initialization, assignment and conditional selection copy values,
  including nested record contents; changing a copy does not change its source.
  Copies require equal capacities and element types. Numeric capacities compare
  by value and scalar aliases compare by canonical type. Named constants resolve
  to the same numeric shape, including constants from directly imported modules,
  aliases of previously declared local constants and evaluated `#run` results.
  The bound evaluator accepts signed integer literals, parentheses, unary `+`/`-`
  and `+`, `-`, `*`, `/`, `%`; every intermediate must fit `i32`, and the final
  capacity must be 1 through 1,048,576. Unknown, ambiguous, cyclic, invalid and
  overflowing bounds are errors. Foreign header macros may remain opaque for
  existing host storage, but cannot initialize portable array literals.
- Arrays can be captured by existing synchronous borrowed `#slot` callbacks.
  Captures refer to the lexical array, so writes through a callback are visible
  to its caller. Existing callback escape restrictions still apply.
  Array arithmetic, comparisons, casts and compound assignments are rejected.
- `text[index]` reads one byte of a `string` as `u8`. String bytes and
  `text.length` are read-only; assigning to them is a strict error.
- Active backends lower these identically: C uses array members and `String`
  views, explicitly copying array values; Go uses fixed arrays and native string
  indexing. The paused JS
  path previously routed reads through a byte-aware helper; it is not current
  conformance evidence.
- C/C++ output lowers `record.field[index]` and `text[index]` through the
  `KRYON_INDEX` macro. Generated headers include `kry_bounds.h` directly,
  so indexing works without importing the UI umbrella header. Builds that define `KRYON_BOUNDS_CHECK` trap on
  out-of-range indexes with a diagnostic naming the array; release builds
  compile to plain indexing. Checks apply to both reads and writes, including
  arrays with symbolic capacities. Go bounds-checks natively.
- Strict stored-type validation rejects unknown types, zero-capacity arrays,
  recursive record value layouts, and stored borrowed slots. Slices and direct
  array/slice function parameters or returns are rejected until portable
  storage, ownership and value semantics are specified.

## Trust model

Compiling a `.kry` file executes no code from that file, but the generated
output is only as trustworthy as its source:

- The `c <raw C>` statement and any statement the frontend cannot type-check
  pass through to the generated code verbatim. A `.kry` file can therefore do
  anything C can. Only compile `.kry` sources you trust; treat fetched `.kry`
  like fetched C. Generated C marks each raw-C statement with a
  `/* kry: raw-c source:line */` provenance comment so injected code is
  auditable.
- `#import` targets are validated: quotes, `>`, and control bytes are rejected
  so a crafted path cannot escape an emitted `#include "..."` line.
- Source lines longer than the frontend's fixed line buffer are rejected with
  a located error instead of being silently split.

`kryon fmt [--check] file.kry...` formats Kry source with stable indentation
and simple spacing cleanup. `--check` exits non-zero when a file would change.

`kryon locale-check source.kry... -- locales/*.txt` validates `t("key")`
references against locale files that use `[key]` blocks. It reports missing
keys, unused keys, duplicate locale keys, and non-English locale entries that
copy the English text exactly.

## Compile-Time Forms

`#defined(SYMBOL)` expands to a preprocessor-style condition. It is useful for
guards and assertions that C should preserve.

`NAME :: #run INTEGER_EXPR` folds a small integer expression while parsing Kry.
It supports integer literals, parentheses, unary `!`, unary `+`, unary `-`,
arithmetic operators, relational operators, equality operators, `&&`, and `||`.
It is not a general compile-time function system.

`#assert CONDITION, "message"` records a KIR assertion. If the assertion is
unguarded, fully known, and false, the frontend fails immediately. C lowers
assertions to `#if` and `#error`. Go and KRB currently accept only
known-true assertions and reject guarded or unresolved assertions.

## Diagnostics

Diagnostics should include file and line whenever source spans are available.
Backends must reject unsupported compile-time behavior that could alter program
meaning. TODO comments in generated output are acceptable only for explicitly
best-effort backend areas and must be covered by conformance status.

## KIR Contract

KIR records modules, imports, defines, assertions, state fields, types,
functions, statements, and expression metadata. Statement records always keep
the normalized raw source text. Expression records currently cover:

| Kind | Meaning |
|---|---|
| `ident` | Simple identifier. |
| `int` | Integer literal text. |
| `float` | Floating-point literal text. |
| `string` | String literal text. |
| `call` | Call with child argument expressions; indirect calls also retain the callee expression. |
| `binary` | Top-level binary expression with operator, left child, and right child. |
| `unary` | Prefix unary expression with operator and right child. |
| `member` | `base.field` access with base expression and field name. |
| `pointer_member` | `base->field` access with base expression and field name. |
| `index` | `base[index]` access with base and index children. |
| `compound` | C-style compound literal text preserved as one expression. |
| `sizeof` | `sizeof(...)` expression metadata. |
| `char` | Character literal. |
| `cast` | Explicit C-style cast with target type and operand. |
| `conditional` | Condition, true arm, and false arm of `a ? b : c`. |
| `postfix` | Postfix increment or decrement. |
| `unknown` | Preserved expression text that the frontend did not structure yet. |

Backends may use raw statement text while structured expression KIR grows. They
must not require a second source-language path around KIR.

Declaration statements retain binding names and declared/inferred types.
Assignment statements retain separate destination and value expression roots.
Expressions include a resolved type when the scalar checker can establish it.
An empty type means unresolved; it must not be interpreted as a concrete type.

## Lexical Cleanup

`defer expression` or `defer assignment` registers a cleanup action in its
lexical block. Registered actions run in reverse order before leaving that
block, including `return`, `break`, and `continue`. A return expression is
evaluated exactly once into a temporary before cleanup begins. Deferred
expressions read their operands at cleanup time, not registration time.

Cleanup is lowered into ordinary KIR statements before backend emission. This
gives C, C++, and Go the same normal-control-flow behavior; it does not use
Go's function-scoped `defer`. Cleanup does not run after process termination,
foreign exceptions, or Go panics. Exceptional unwinding is not
yet part of this language contract.

Functions using cleanup currently reject raw C, conditional preprocessing,
`goto`, labels, and `guard` (use explicit `if`/`return`). A switch case must use an explicit nested block to register
cleanup. Declarations cannot shadow names referenced by active cleanup actions;
`for` headers cannot reuse those names. These forms require further binding and
control-flow lowering and are diagnosed instead of emitting incorrect cleanup.

## Backend Conformance

| Feature | KIR | C | Go | KRB |
|---|---|---|---|---|
| `.kry -> KIR` frontend | yes | source | source | source |
| Header imports | yes | yes | partial | partial |
| Module imports | yes | yes | partial | partial |
| State block | yes | yes | yes | yes |
| Structs and type aliases | yes | yes | partial | partial |
| Tagged extern declarations | yes | yes | host interface or direct Go package call | host/capability subset |
| `#defined` guards | yes | yes | partial | partial |
| `#run` integer constants | yes | yes | yes | yes |
| `#assert` | yes | `#if/#error` | known true only | known true only |
| Locals and assignments | yes | yes | yes | partial |
| Control flow | yes | yes | partial | partial |
| `defer` | shared lexical lowering | yes | yes | lowered subset; not behaviorally verified |
| Raw C lines | yes | yes | no | no |
| Structured scalar expression emission | yes | yes | yes | metadata only |

## Known Boundaries

- C is the broadest backend and the canonical ABI path.
- Go is a declarative app subset. Tagged extern targets that contain a full Go
  import path, such as `github.com/example/app.Generate`, are imported and
  called directly by generated Go; short targets such as `app.Generate` remain
  host-interface methods. Explicit C ABI targets such as `#extern "c.abs"`
  are rejected by Go with a source diagnostic. Generated Go never uses cgo;
  provide a native Go package or host implementation instead.
- KRB is a portable cartridge subset with explicit host/capability boundaries.
- `defer` is a shared KIR transform; the restrictions above apply to every target.
- Raw C lines are not portable.
- Checked scalar functions emit directly from structured KIR in C/C++/Go.
  App, aggregate, and other unconverted bodies still use target-specific text
  lowering outside strict mode. KRB has not adopted the scalar emitter. The
  paused JS emitter is not current conformance evidence.
- Kry is not yet a full C replacement. [Implementation status](LANGUAGE_IMPLEMENTATION.md)
  lists the remaining type, memory, ABI, ownership, and compile-time work.
