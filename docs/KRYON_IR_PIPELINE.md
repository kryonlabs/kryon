# Kryon IR Pipeline Reference v0.1
## Smart Hybrid System Compilation

## Pipeline Overview

```
KRY Source → Lexer → Parser → Hybrid AST → Optimizer → Generator → KRB Binary
```

## 1. Lexer Phase

### Token Types

| Token | Value | Description |
|-------|-------|-------------|
| WIDGET_IDENTIFIER | - | Widget type names (Button, Column, etc.) |
| STYLE_IDENTIFIER | - | Style names |
| PROPERTY_IDENTIFIER | - | Property names |
| THEME_IDENTIFIER | - | Theme variable names |
| STRING | - | Quoted string literals |
| NUMBER | - | Numeric literals with optional units (px, %, em, rem, vw, vh, etc.) |
| BOOLEAN | true/false | Boolean literals |
| COLOR | - | Color values (#RRGGBBAA, rgb(), hsl()) |
| VARIABLE_IDENTIFIER | - | Variable names (colors, userName, count) |
| STYLE | style | Style declaration keyword |
| THEME | @theme | Theme declaration keyword |
| EXTENDS | extends | Style inheritance keyword |
| LBRACE | { | Left brace |
| RBRACE | } | Right brace |
| LBRACKET | [ | Left bracket |
| RBRACKET | ] | Right bracket |
| LPAREN | ( | Left parenthesis |
| RPAREN | ) | Right parenthesis |
| COLON | : | Property separator |
| COMMA | , | List separator |
| DOT | . | Member access for variables |
| DOLLAR_BRACE | ${ | String interpolation start |
| AT | @ | Directive prefix |
| COMMENT | #, // | Comments |
| EOF | - | End of file |

### Lexer Rules

1. **Whitespace**: Spaces, tabs, newlines are ignored (except in strings)
2. **Comments**: `#` or `//` to end of line
3. **Strings**: Always quoted with `"`, supports escape sequences
4. **Numbers**: Integer or float, with optional units (px, %, em, rem, vw, vh, etc.)
5. **Widget Identifiers**: Widget type names (Button, Column, Text, etc.)
6. **Variables**: Direct usage (colors.primary, spacing.md) and string interpolation (${count})
7. **Style References**: String references to style definitions
8. **Keywords**: Reserved words (true, false, style, extends, @theme)

## 2. Parser Phase

### Hybrid AST Node Types

```
HybridASTNode
├── Program
│   ├── styleDefinitions: StyleDefinition[]
│   ├── themeDefinitions: ThemeDefinition[]
│   ├── variables: VariableDefinition[]
│   └── widgets: WidgetDeclaration[]
├── StyleDefinition
│   ├── name: string
│   ├── parent: string (for inheritance)
│   └── properties: StyleProperty[]
├── ThemeDefinition
│   ├── groupName: string ("colors", "spacing", etc.)
│   └── variables: ThemeVariable[]
├── WidgetDeclaration
│   ├── type: WidgetType
│   ├── properties: WidgetProperty[]
│   ├── styleReference: string
│   └── children: WidgetDeclaration[]
├── StyleProperty
│   ├── name: string
│   ├── value: StyleExpression
│   └── responsive: ResponsiveValue
├── WidgetProperty
│   ├── name: string
│   ├── value: WidgetExpression
│   └── responsive: ResponsiveValue
├── StyleExpression
│   ├── Literal (string, number, boolean, color)
│   ├── ThemeVariableReference
│   ├── ResponsiveExpression
│   └── ConditionalExpression
├── ThemeVariable
│   ├── name: string
│   ├── type: ThemeVariableType
│   └── value: Expression
└── ResponsiveValue
    ├── mobile: Expression
    ├── tablet: Expression
    └── desktop: Expression
```

### Parser Rules

1. **Program**: Collection of style definitions, theme definitions, and widget declarations
2. **Style**: `style "name" { properties }` or `style "name" extends "parent" { properties }`
3. **Theme**: `@theme groupName { variables }`
4. **Widget**: `WidgetType { properties and children }`
5. **Property**: `name: value` with optional responsive values
6. **Variable**: `variableName` or `groupName.variableName` (used directly, no $ prefix)

## 3. Semantic Analysis

### Hybrid System Type Checking

1. **Style Validation**: Ensure style names are unique and inheritance chains are valid
2. **Theme Variable Validation**: Check theme variable references are valid
3. **Widget Property Validation**: Ensure properties are valid for widget type
4. **CSS Property Validation**: Validate CSS-like property names and values
5. **Theme Variable Resolution**: Resolve theme variable references
6. **Style Inheritance Resolution**: Resolve style inheritance chains
7. **Responsive Value Validation**: Check responsive breakpoint definitions

### Scope Management

```
GlobalScope
├── StyleScope
│   ├── StyleDefinitions
│   └── StyleInheritance
├── ThemeScope
│   ├── ThemeGroups ("colors", "spacing", etc.)
│   └── ThemeVariables
├── WidgetScope
│   ├── WidgetDefinitions
│   ├── PropertyScope
│   └── ChildrenScope
└── VariableScope
    └── RegularVariables
```

## 4. Optimization Phase

### Hybrid System Optimization Passes

1. **Theme Variable Resolution**: Resolve theme variables to concrete values where beneficial
   ```
   background: colors.primary  →  background: var(--kryon-colors-primary)
   ```

2. **Style Inheritance Flattening**: Flatten style inheritance chains
   ```
   style "primaryButton" extends "button"
   →  primaryButton with all inherited + override properties
   ```

3. **CSS Property Optimization**: Optimize CSS-like properties
   ```
   padding: "8px 16px"  →  optimized padding values
   ```

4. **Style Deduplication**: Merge identical style definitions
   ```
   style1: { background: "#007AFF" }
   style2: { background: "#007AFF" }  →  (shared style definition)
   ```

5. **Theme Variable Grouping**: Optimize theme variable organization
   ```
   Multiple color references  →  Grouped color variable table
   ```

6. **Responsive Value Optimization**: Optimize responsive breakpoints
   ```
   width: { mobile: 100px, tablet: 200px }
   →  Optimized media query conditions
   ```

7. **Widget Layout Optimization**: Optimize widget layout properties
   ```
   Column with spacing  →  Optimized flexbox properties
   ```

8. **Dead Style Elimination**: Remove unused styles and theme variables

## 5. Code Generation

### Hybrid System Code Generation

#### Style Definition Table Generation

```
Style ID Assignment:
primaryButton: 0x0001
button: 0x0002
card: 0x0003
```

#### Style Property Encoding

1. **CSS Property Resolution**: CSS property names → Property IDs
2. **Theme Variable Linking**: Theme variable references → Theme variable IDs
3. **Style Inheritance**: Parent style references → Parent style IDs
4. **Responsive Values**: Breakpoint values → Responsive value definitions

#### Theme Variable Table Generation

```
Theme Group Assignment:
colors: 0x0001
spacing: 0x0002
typography: 0x0003

Theme Variable Assignment:
colors.primary: 0x0001
colors.background: 0x0002
spacing.sm: 0x0003
spacing.md: 0x0004
```

#### Widget Instance Generation

1. **Widget ID Assignment**: Widget instances → Instance IDs
2. **Style Reference**: Style name → Style ID
3. **Property Encoding**: Widget properties → Property definitions
4. **Theme Variable References**: Theme variables → Theme variable IDs

#### String Table Generation (Enhanced)

1. Collect all unique strings (widget names, style names, theme group names, property names)
2. Sort by frequency for better compression
3. Assign 16-bit indices for efficiency
4. Encode UTF-8 data with length prefixes

## 6. Binary Emission

### Write Order

1. **Header**: Magic, version, style counts, theme counts, widget counts, offsets
2. **String Table**: All strings (style names, theme names, property names)
3. **Style Definition Table**: Style definitions with inheritance chains
4. **Theme Variable Table**: Theme groups and variables
5. **Widget Definition Table**: Widget types and property definitions
6. **Widget Instances**: Widget tree in depth-first order
7. **Script Section**: Embedded event handlers and functions
8. **Resource Section**: Embedded assets (images, fonts, etc.)
9. **Checksum**: CRC32 of all data

## Error Handling

### Hybrid System Error Categories

| Category | Examples |
|----------|----------|
| Lexical | Invalid character, unterminated string |
| Syntax | Missing brace, invalid style syntax |
| Semantic | Unknown widget type, invalid CSS property |
| Style | Unknown style reference, invalid inheritance |
| Theme | Unknown theme variable, invalid theme group |
| Responsive | Invalid breakpoint definition |
| Reference | Undefined style, unknown theme variable |

### Enhanced Error Format

```
[filename]:[line]:[column]: [severity]: [message]
[context information if applicable]
  |
  v [code snippet with syntax highlighting]
    ^--- [pointer to error with suggestions]
```

Examples:
```
button.kry:15:10: error: Unknown CSS property 'backgroundColor' in style definition
  Did you mean 'background'?
15 |   style "button" { backgroundColor: "#007AFF" }
   |                    ^^^^^^^^^^^^^^^

theme.kry:8:15: error: Unknown theme variable 'colors.primaryDark'
  Available variables in 'colors': primary, secondary, background, text
8  |   background: colors.primaryDark
   |               ^^^^^^^^^^^^^^^^^^
   |               Did you mean 'colors.primary'?

widget.kry:22:5: error: Style 'unknownStyle' not found
  Available styles: button, primaryButton, card
22 |   Button { style: "unknownStyle" }
   |                   ^^^^^^^^^^^^^
```

## Compilation Flags

| Flag | Description |
|------|-------------|
| `--optimize` | Enable style and theme optimizations |
| `--debug` | Include style and theme debug information |
| `--source-map` | Generate style and widget source mapping |
| `--compress` | Compress output with style-aware compression |
| `--validate` | Strict style inheritance and theme validation |
| `--target` | Target platform (web, native, mobile) |
| `--responsive` | Enable responsive design features |
| `--theme-validation` | Strict theme variable validation |

## Metadata Generation

Compiler automatically generates:

1. **Style Metadata**: Style definitions, inheritance chains, property mappings
2. **Theme Metadata**: Theme variable tables, theme switching definitions
3. **Widget Metadata**: Widget types, property definitions, layout hints
4. **Responsive Metadata**: Breakpoint definitions, media query conditions
5. **CSS Metadata**: CSS property mappings, optimization hints
6. **Script Metadata**: Event handlers, function signatures
7. **Resource Metadata**: Asset references, sizes, optimization data

## Platform-Specific Transformations

### Web Target
- Convert styles to CSS classes with proper prefixing
- Generate CSS custom properties for theme variables
- Map widget layout to HTML elements with flexbox CSS
- Generate JavaScript for theme switching
- Create responsive CSS media queries
- Optimize CSS output for web performance

### Native Target (Desktop/Mobile)
- Convert styles to platform-specific styling systems
- Map theme variables to platform color schemes
- Generate native layout constraints from widget definitions
- Create platform-specific responsive behaviors
- Map CSS-like properties to native equivalents

### Cross-Platform
- Generate style compatibility matrices
- Create platform-specific property overrides
- Generate responsive breakpoint definitions for each platform
- Create theme adaptation rules per platform

## Performance Considerations

### Compilation Optimizations
1. **Style String Interning**: Reuse string table indices for style names, CSS properties
2. **Theme Variable Caching**: Pre-compute theme variable resolution chains
3. **CSS Property Mapping**: Pre-map CSS properties to platform equivalents
4. **Style Inheritance Flattening**: Pre-flatten style inheritance for runtime performance
5. **Responsive Value Pre-calculation**: Pre-calculate responsive breakpoint conditions

### Runtime Optimizations
1. **Style Deduplication**: Share identical style definitions between widgets
2. **Theme Variable Pooling**: Pool theme variable resolution results
3. **CSS Class Generation**: Generate optimized CSS classes for web target
4. **Widget Layout Caching**: Cache widget layout calculations
5. **Responsive Calculation**: Pre-calculate responsive value transitions

### Binary Optimizations
1. **Style-Aware Compression**: Use style structure knowledge for better compression
2. **Theme Variable Delta Encoding**: Store only theme variable differences
3. **CSS Property Compression**: Compress CSS property names using frequency analysis
4. **Style Reference Compression**: Compress style references using dependency analysis
5. **Responsive Value Compression**: Compress responsive breakpoint data efficiently

This compilation pipeline supports the Smart Hybrid System by efficiently processing both CSS-like styling and widget-based layout, with comprehensive theme support and cross-platform optimization.