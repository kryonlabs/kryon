# Kry Language Specification

Kry language version: `0.2`
KIR version: `0.1`

This is the stable public contract for `.kry` source accepted by Kryon tools.
Kry is intentionally C-close: expressions and types stay familiar to C, while
the frontend owns declarations, compile-time guards, UI frame structure, and
KIR emission.

The required pipeline is:

```text
.kry -> KIR -> C
.kry -> KIR -> Go
.kry -> KIR -> JS
.kry -> KIR -> KRB
```

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

Plain call statements are UI declarations when they call Kryon widget/runtime
functions. Property-block widgets lower to the existing `WidgetProps` C-style
call shape, so `Button { label = "Save" ... }` is equivalent to
`Button((ButtonProps){.label = "Save", ...})`. Layout nodes (`Screen`,
`Column`, `Row`, `Stack`) still open retained UI scopes and therefore emit a
matching `End()`.

## Tooling

`k2c`, `k2cpp`, `k2go`, and `k2js` accept `--strict`. This enables the shared
scalar type checker before output generation. It checks lexical bindings,
duplicate local declarations, scalar operand compatibility, assignments,
function argument counts/types, return value types, and boolean conditions.
Unresolved names, opaque expressions, and unsupported statements are errors with
source locations. This is an incremental checking mode, not a guarantee of
complete backend support or portable numeric behavior. It currently excludes
UI calls, raw C, `for` headers, and aggregate/pointer expressions. Existing app
builds may omit the flag while these areas are being migrated.

The checker also annotates known expression types in ordinary builds, without
rejecting unresolved imported C/runtime symbols. KIR dumps expose these types.
`make language-test` runs expression-tree tests and executes matching cleanup
fixtures through C, C++, Go, and JavaScript, including negative diagnostics.

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
assertions to `#if` and `#error`. Go, JS, and KRB currently accept only
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
gives C, C++, Go, and JS the same normal-control-flow behavior; it does not use
Go's function-scoped `defer`. Cleanup does not run after process termination,
foreign exceptions, Go panics, or JS exceptions. Exceptional unwinding is not
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
| Structured expression metadata | partial | metadata only | metadata only | metadata only |

## Known Boundaries

- C is the broadest backend and the canonical ABI path.
- Go is a declarative app subset. Tagged extern targets that contain a full Go
  import path, such as `github.com/example/app.Generate`, are imported and
  called directly by generated Go; short targets such as `app.Generate` remain
  host-interface methods. Explicit C targets use the `c.symbol` form, such as
  `#extern "c.abs"`; only those declarations generate an isolated cgo bridge.
- KRB is a portable cartridge subset with explicit host/capability boundaries.
- `defer` is a shared KIR transform; the restrictions above apply to every target.
- Raw C lines are not portable.
- Structured expression KIR is metadata today, not the sole lowering source.
