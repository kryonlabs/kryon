# Kryon IR Pipeline Reference

## Pipeline Overview

```
KRY Source → Lexer → Parser → AST → Optimizer → Code Generator → KRB Binary
```

## 1. Lexer Phase

### Token Types

| Token | Value | Description |
|-------|-------|-------------|
| IDENTIFIER | - | Element names, property names |
| STRING | - | Quoted string literals |
| NUMBER | - | Numeric literals |
| BOOLEAN | true/false | Boolean literals |
| COLOR | - | Color values (#RRGGBBAA) |
| LBRACE | { | Left brace |
| RBRACE | } | Right brace |
| LBRACKET | [ | Left bracket |
| RBRACKET | ] | Right bracket |
| LPAREN | ( | Left parenthesis |
| RPAREN | ) | Right parenthesis |
| COLON | : | Property separator |
| COMMA | , | List separator |
| DOT | . | Member access |
| DOLLAR | $ | Variable prefix |
| AT | @ | Directive prefix |
| PLUS | + | Addition |
| MINUS | - | Subtraction |
| MULTIPLY | * | Multiplication |
| DIVIDE | / | Division |
| MODULO | % | Modulus |
| EQUALS | == | Equality |
| NOT_EQUALS | != | Inequality |
| LESS_THAN | < | Less than |
| GREATER_THAN | > | Greater than |
| LESS_EQUAL | <= | Less or equal |
| GREATER_EQUAL | >= | Greater or equal |
| AND | && | Logical AND |
| OR | \|\| | Logical OR |
| NOT | ! | Logical NOT |
| QUESTION | ? | Ternary operator |
| ASSIGN | = | Assignment |
| ARROW | -> | Function arrow |
| COMMENT | #, // | Comments |
| EOF | - | End of file |

### Lexer Rules

1. **Whitespace**: Spaces, tabs, newlines are ignored (except in strings)
2. **Comments**: `#` or `//` to end of line
3. **Strings**: Always quoted with `"`, supports escape sequences
4. **Numbers**: Integer or float, with optional units (px, %, em, etc.)
5. **Identifiers**: Start with letter or underscore, contain alphanumeric
6. **Keywords**: Reserved words (true, false, null, undefined)

## 2. Parser Phase

### AST Node Types

```
ASTNode
├── Program
│   └── statements: Statement[]
├── Statement
│   ├── ElementDeclaration
│   ├── StyleDeclaration
│   ├── DirectiveStatement
│   └── ExpressionStatement
├── ElementDeclaration
│   ├── type: string
│   ├── properties: Property[]
│   └── children: ElementDeclaration[]
├── Property
│   ├── name: string
│   └── value: Expression
├── Expression
│   ├── Literal (string, number, boolean, color)
│   ├── Identifier
│   ├── MemberExpression
│   ├── BinaryExpression
│   ├── UnaryExpression
│   ├── TernaryExpression
│   ├── ArrayExpression
│   ├── ObjectExpression
│   └── FunctionCall
└── Directive
    ├── VariableDirective
    ├── IfDirective
    ├── ForDirective
    ├── FunctionDirective
    ├── ComponentDirective
    ├── IncludeDirective
    └── MetadataDirective
```

### Parser Rules

1. **Program**: Collection of top-level statements
2. **Element**: `Type { properties and children }`
3. **Property**: `name: value`
4. **Expression**: Literals, variables, operations
5. **Directive**: `@directiveName args { body }`

## 3. Semantic Analysis

### Type Checking

1. **Property Validation**: Ensure properties exist for element type
2. **Type Compatibility**: Check value types match property types
3. **Variable Resolution**: Resolve variable references
4. **Function Validation**: Check function signatures

### Scope Management

```
GlobalScope
├── StyleScope
├── ComponentScope
│   └── PropsScope
└── FunctionScope
    └── LocalScope
```

## 4. Optimization Phase

### Optimization Passes

1. **Constant Folding**: Evaluate constant expressions
   ```
   width: 100 + 50  →  width: 150
   ```

2. **Dead Code Elimination**: Remove unreachable code
   ```
   @if false { Element { } }  →  (removed)
   ```

3. **Variable Inlining**: Replace constant variables
   ```
   @var size = 20
   width: $size  →  width: 20
   ```

4. **Style Merging**: Combine duplicate styles
   ```
   style "a" { color: "red" }
   style "b" { color: "red" }  →  (merged)
   ```

5. **String Deduplication**: Share identical strings

## 5. Code Generation

### Element ID Assignment

```
Root: 0
├── Child1: 1
│   ├── Grandchild1: 2
│   └── Grandchild2: 3
└── Child2: 4
```

### Property Encoding

1. **Property Resolution**: Name → Property ID
2. **Value Encoding**: Type-specific binary encoding
3. **Reference Resolution**: Variable/element references → IDs

### String Table Generation

1. Collect all unique strings
2. Sort by frequency (optional)
3. Assign indices
4. Encode UTF-8 data

## 6. Binary Emission

### Write Order

1. **Header**: Magic, version, counts, offsets
2. **String Table**: All strings used in file
3. **Elements**: Depth-first order
4. **Metadata**: JSON-encoded metadata
5. **Scripts**: Embedded script code
6. **Resources**: Embedded resources
7. **Checksum**: CRC32 of all data

## Error Handling

### Error Categories

| Category | Examples |
|----------|----------|
| Lexical | Invalid character, unterminated string |
| Syntax | Missing brace, invalid property |
| Semantic | Unknown property, type mismatch |
| Reference | Undefined variable, unknown function |

### Error Format

```
[filename]:[line]:[column]: [severity]: [message]
  |
  v [code snippet]
    ^--- [pointer to error]
```

Example:
```
button.kry:15:10: error: Unknown property 'onClick' for element 'Text'
15 |   Text { onClick: "handler" }
   |          ^^^^^^^
```

## Compilation Flags

| Flag | Description |
|------|-------------|
| `--optimize` | Enable optimizations |
| `--debug` | Include debug information |
| `--source-map` | Generate source mapping |
| `--compress` | Compress output |
| `--validate` | Strict validation mode |
| `--target` | Target platform (web, native) |

## Metadata Generation

Compiler automatically generates:

1. **Element Metadata**: Type information, property defaults
2. **Style Metadata**: Computed styles, inheritance
3. **Script Metadata**: Function signatures, dependencies
4. **Resource Metadata**: Asset references, sizes

## Platform-Specific Transformations

### Web Target
- Convert dimensions to CSS units
- Generate HTML-compatible event names
- Include polyfills for missing features

### Native Target
- Convert colors to platform format
- Map to native UI elements
- Generate platform-specific layouts

## Performance Considerations

1. **String Interning**: Reuse string table indices
2. **Property Packing**: Bit-pack boolean properties
3. **Layout Hints**: Pre-calculate layout properties
4. **Dead Code**: Remove unused styles/components
5. **Compression**: Optional zlib/lz4 compression