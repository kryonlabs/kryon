# Kry Language Specification

Kry language version: `0.1`
KIR version: `0.1`

This is the stable public contract for `.kry` source accepted by Kryon tools.
Kry is intentionally C-close: expressions and types stay familiar to C, while
the frontend owns declarations, compile-time guards, UI frame structure, and
KIR emission.

The required pipeline is:

```text
.kry -> KIR -> C
.kry -> KIR -> Go
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

The frontend parses a conservative subset of expressions into KIR expression
metadata. Full C-close expression text is still preserved for backends.

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
ui-node         = identifier , identifier , [ ":" ] , "{" ,
                  { ui-prop | statement } , "}" ;
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
functions. The frontend does not need widget-specific keywords.

## Compile-Time Forms

`#defined(SYMBOL)` expands to a preprocessor-style condition. It is useful for
guards and assertions that C should preserve.

`NAME :: #run INTEGER_EXPR` folds a small integer expression while parsing Kry.
It supports integer literals, parentheses, unary `!`, unary `+`, unary `-`,
arithmetic operators, relational operators, equality operators, `&&`, and `||`.
It is not a general compile-time function system.

`#assert CONDITION, "message"` records a KIR assertion. If the assertion is
unguarded, fully known, and false, the frontend fails immediately. C lowers
assertions to `#if` and `#error`. Go and KRB currently accept only known-true
assertions and reject guarded or unresolved assertions.

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
| `string` | String literal text. |
| `call` | Simple `Name(args...)` call with child expressions for arguments. |
| `binary` | Top-level binary expression with operator, left child, and right child. |
| `unknown` | Preserved expression text that the frontend did not structure yet. |

Backends may use raw statement text while structured expression KIR grows. They
must not require a second source-language path around KIR.

## Backend Conformance

| Feature | KIR | C | Go | KRB |
|---|---|---|---|---|
| `.kry -> KIR` frontend | yes | source | source | source |
| Header imports | yes | yes | partial | partial |
| Module imports | yes | yes | partial | partial |
| State block | yes | yes | yes | yes |
| Structs and type aliases | yes | yes | partial | partial |
| Tagged extern declarations | yes | yes | host interface | host/capability subset |
| `#defined` guards | yes | yes | partial | partial |
| `#run` integer constants | yes | yes | yes | yes |
| `#assert` | yes | `#if/#error` | known true only | known true only |
| Locals and assignments | yes | yes | yes | partial |
| Control flow | yes | yes | partial | partial |
| `defer` | yes | yes | no | no |
| Raw C lines | yes | yes | no | no |
| Structured expression metadata | partial | metadata only | metadata only | metadata only |

## Known Boundaries

- C is the broadest backend and the canonical ABI path.
- Go is a declarative app subset with host interfaces for tagged externs.
- KRB is a portable cartridge subset with explicit host/capability boundaries.
- `defer` is currently a C-oriented compile-time transform.
- Raw C lines are not portable.
- Structured expression KIR is metadata today, not the sole lowering source.
