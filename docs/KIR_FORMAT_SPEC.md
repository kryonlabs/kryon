# KIR (Kryon Intermediate Representation) Format Specification

**Version**: 0.1.0
**Format**: JSON (RFC 8259 compliant)
**Purpose**: Mandatory intermediate representation in Kryon compilation pipeline

## Table of Contents

1. [Overview](#overview)
2. [Design Goals](#design-goals)
3. [File Structure](#file-structure)
4. [Node Types](#node-types)
5. [Value Types](#value-types)
6. [Expression Nodes](#expression-nodes)
7. [Directive Nodes](#directive-nodes)
8. [Component Nodes](#component-nodes)
9. [Metadata](#metadata)
10. [Examples](#examples)
11. [Round-Trip Guarantees](#round-trip-guarantees)
12. [Versioning](#versioning)

---

## Overview

KIR is the **mandatory intermediate format** in the Kryon compilation pipeline:

```
kry source → [Lex/Parse/Expand] → KIR → [Codegen] → krb binary
```

**Full bidirectional support**:
- Forward: `kry → kir → krb`
- Backward: `krb → kir → kry`
- Round-trip: `kry → kir → krb → kir → kry` (semantic preservation)

**Key Properties**:
- **Lossless**: Complete AST information for perfect reconstruction
- **Post-expansion**: Components, const_for, const_if all expanded
- **Machine-readable**: JSON format for AI/LLM compatibility
- **Self-contained**: All necessary information for compilation or decompilation

---

## Design Goals

1. **Mandatory Intermediate Format**: Every compilation goes through KIR (not optional)
2. **Perfect Round-Trips**: `kry → kir → krb → kir → kry` produces semantically identical code
3. **AI/LLM Compatibility**: JSON format enables machine analysis and debugging assistance
4. **Semantic Preservation**: Structure identical, formatting may differ
5. **Decompilation Support**: KRB binaries can be reconstructed to readable source
6. **Testing Foundation**: Enables validation of each compiler stage independently

---

## File Structure

### Top-Level Schema

```json
{
  "version": "0.1.0",
  "format": "kir-json",
  "metadata": {
    "sourceFile": "example.kry",
    "compiler": "kryon",
    "compilerVersion": "1.0.0",
    "timestamp": "2026-01-28T10:30:00Z",
    "expansionInfo": {
      "componentsExpanded": 3,
      "constForLoopsUnrolled": 2,
      "constIfBranchesEvaluated": 1
    }
  },
  "root": {
    "type": "ROOT",
    "nodeId": 1,
    "children": [...]
  }
}
```

### Required Fields

| Field | Type | Description |
|-------|------|-------------|
| `version` | string | KIR format version (semantic versioning) |
| `format` | string | Always "kir-json" |
| `metadata` | object | Compilation metadata |
| `root` | object | Root AST node (always type "ROOT") |

### Metadata Fields

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `sourceFile` | string | Yes | Original source file name |
| `compiler` | string | Yes | Compiler name ("kryon") |
| `compilerVersion` | string | Yes | Compiler version |
| `timestamp` | string | No | ISO 8601 timestamp |
| `expansionInfo` | object | No | Statistics about expansion |

---

## Node Types

All AST nodes follow this general structure:

```json
{
  "type": "NODE_TYPE_NAME",
  "nodeId": 123,
  "location": {
    "line": 42,
    "column": 5,
    "file": "example.kry"
  },
  // Type-specific fields...
}
```

### Common Node Fields

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `type` | string | Yes | AST node type (see below) |
| `nodeId` | integer | Yes | Unique node identifier |
| `location` | object | No | Source location information |

### Location Object

```json
{
  "line": 42,
  "column": 5,
  "file": "example.kry"
}
```

### Supported Node Types

#### Container Nodes

**ROOT** - Document root
```json
{
  "type": "ROOT",
  "nodeId": 1,
  "children": [...]
}
```

**ELEMENT** - UI element
```json
{
  "type": "ELEMENT",
  "nodeId": 7,
  "elementType": "Container",
  "location": {"line": 6, "column": 1, "file": "app.kry"},
  "properties": [...],
  "children": [...]
}
```

**PROPERTY** - Element or style property
```json
{
  "type": "PROPERTY",
  "nodeId": 8,
  "name": "width",
  "value": {
    "type": "LITERAL",
    "valueType": "INTEGER",
    "value": 200
  }
}
```

#### Style Nodes

**STYLE_BLOCK** - Single style definition
```json
{
  "type": "STYLE_BLOCK",
  "nodeId": 15,
  "name": "primaryButton",
  "parentName": null,
  "properties": [...]
}
```

**STYLES_BLOCK** - Multiple style definitions
```json
{
  "type": "STYLES_BLOCK",
  "nodeId": 20,
  "properties": [
    {"type": "STYLE_BLOCK", "name": "style1", ...},
    {"type": "STYLE_BLOCK", "name": "style2", ...}
  ]
}
```

#### Definition Nodes

**VARIABLE_DEFINITION** - @var
```json
{
  "type": "VARIABLE_DEFINITION",
  "nodeId": 25,
  "name": "message",
  "varType": "String",
  "value": {
    "type": "LITERAL",
    "valueType": "STRING",
    "value": "Hello"
  }
}
```

**CONST_DEFINITION** - @const
```json
{
  "type": "CONST_DEFINITION",
  "nodeId": 30,
  "name": "colors",
  "value": {
    "type": "ARRAY_LITERAL",
    "elements": [...]
  }
}
```

**FUNCTION_DEFINITION** - @function
```json
{
  "type": "FUNCTION_DEFINITION",
  "nodeId": 35,
  "language": "js",
  "name": "handleClick",
  "parameters": ["event", "index"],
  "code": "console.log('Clicked:', index);"
}
```

**STATE_DEFINITION** - @state
```json
{
  "type": "STATE_DEFINITION",
  "nodeId": 40,
  "name": "count",
  "value": {
    "type": "LITERAL",
    "valueType": "INTEGER",
    "value": 0
  }
}
```

#### Theme Nodes

**THEME_DEFINITION** - @theme
```json
{
  "type": "THEME_DEFINITION",
  "nodeId": 45,
  "groupName": "colors",
  "variables": [
    {
      "type": "VARIABLE_DEFINITION",
      "name": "primary",
      "value": {"type": "LITERAL", "valueType": "COLOR", "value": "#007BFFFF"}
    }
  ]
}
```

---

## Value Types

### LITERAL Node

```json
{
  "type": "LITERAL",
  "nodeId": 50,
  "valueType": "STRING|INTEGER|FLOAT|BOOLEAN|NULL|COLOR|UNIT",
  "value": <type-specific-value>,
  "unit": "px"  // Only for UNIT type
}
```

**Examples**:

```json
// String
{"type": "LITERAL", "valueType": "STRING", "value": "Hello World"}

// Integer
{"type": "LITERAL", "valueType": "INTEGER", "value": 42}

// Float
{"type": "LITERAL", "valueType": "FLOAT", "value": 3.14}

// Boolean
{"type": "LITERAL", "valueType": "BOOLEAN", "value": true}

// Null
{"type": "LITERAL", "valueType": "NULL", "value": null}

// Color (RGBA as hex string)
{"type": "LITERAL", "valueType": "COLOR", "value": "#FF5733FF"}

// Unit (with measurement)
{"type": "LITERAL", "valueType": "UNIT", "value": 16, "unit": "px"}
```

### VARIABLE Node

```json
{
  "type": "VARIABLE",
  "nodeId": 55,
  "name": "count"  // Variable name without $ prefix
}
```

### IDENTIFIER Node

```json
{
  "type": "IDENTIFIER",
  "nodeId": 60,
  "name": "parameterName"  // Component parameter or scope identifier
}
```

---

## Expression Nodes

### TEMPLATE Node

Template string with interpolation:

```json
{
  "type": "TEMPLATE",
  "nodeId": 65,
  "segments": [
    {"type": "LITERAL", "valueType": "STRING", "value": "Count: "},
    {"type": "VARIABLE", "name": "count"},
    {"type": "LITERAL", "valueType": "STRING", "value": " items"}
  ]
}
```

### BINARY_OP Node

```json
{
  "type": "BINARY_OP",
  "nodeId": 70,
  "operator": "+",
  "left": {"type": "VARIABLE", "name": "count"},
  "right": {"type": "LITERAL", "valueType": "INTEGER", "value": 1}
}
```

**Operators**: `+`, `-`, `*`, `/`, `%`, `==`, `!=`, `<`, `>`, `<=`, `>=`, `&&`, `||`

### UNARY_OP Node

```json
{
  "type": "UNARY_OP",
  "nodeId": 75,
  "operator": "!",
  "operand": {"type": "VARIABLE", "name": "isActive"}
}
```

**Operators**: `!`, `-`, `+`

### TERNARY_OP Node

```json
{
  "type": "TERNARY_OP",
  "nodeId": 80,
  "condition": {"type": "VARIABLE", "name": "isActive"},
  "trueExpr": {"type": "LITERAL", "valueType": "STRING", "value": "Active"},
  "falseExpr": {"type": "LITERAL", "valueType": "STRING", "value": "Inactive"}
}
```

### FUNCTION_CALL Node

```json
{
  "type": "FUNCTION_CALL",
  "nodeId": 85,
  "functionName": "max",
  "arguments": [
    {"type": "VARIABLE", "name": "a"},
    {"type": "VARIABLE", "name": "b"}
  ]
}
```

### MEMBER_ACCESS Node

```json
{
  "type": "MEMBER_ACCESS",
  "nodeId": 90,
  "object": {"type": "VARIABLE", "name": "user"},
  "member": "name"
}
```

### ARRAY_ACCESS Node

```json
{
  "type": "ARRAY_ACCESS",
  "nodeId": 95,
  "array": {"type": "VARIABLE", "name": "items"},
  "index": {"type": "LITERAL", "valueType": "INTEGER", "value": 0}
}
```

### ARRAY_LITERAL Node

```json
{
  "type": "ARRAY_LITERAL",
  "nodeId": 100,
  "elements": [
    {"type": "LITERAL", "valueType": "INTEGER", "value": 1},
    {"type": "LITERAL", "valueType": "INTEGER", "value": 2},
    {"type": "LITERAL", "valueType": "INTEGER", "value": 3}
  ]
}
```

### OBJECT_LITERAL Node

```json
{
  "type": "OBJECT_LITERAL",
  "nodeId": 105,
  "properties": [
    {
      "type": "PROPERTY",
      "name": "x",
      "value": {"type": "LITERAL", "valueType": "INTEGER", "value": 10}
    },
    {
      "type": "PROPERTY",
      "name": "y",
      "value": {"type": "LITERAL", "valueType": "INTEGER", "value": 20}
    }
  ]
}
```

---

## Directive Nodes

### FOR_DIRECTIVE Node (Runtime loop)

```json
{
  "type": "FOR_DIRECTIVE",
  "nodeId": 110,
  "indexVarName": "i",
  "varName": "item",
  "arrayName": "items",
  "body": [...]
}
```

### IF_DIRECTIVE Node (Runtime conditional)

```json
{
  "type": "IF_DIRECTIVE",
  "nodeId": 115,
  "isConst": false,
  "condition": {"type": "VARIABLE", "name": "isVisible"},
  "thenBody": [...],
  "elifConditions": [
    {"type": "BINARY_OP", "operator": "==", ...}
  ],
  "elifBodies": [
    [...]
  ],
  "elseBody": [...]
}
```

### Event Directives

**EVENT_DIRECTIVE** - @event
```json
{
  "type": "EVENT_DIRECTIVE",
  "nodeId": 120,
  "eventName": "click",
  "handlerName": "handleClick"
}
```

**WATCH_DIRECTIVE** - @watch
```json
{
  "type": "WATCH_DIRECTIVE",
  "nodeId": 125,
  "variableName": "count",
  "handlerCode": "console.log('Count changed:', count);"
}
```

**ON_MOUNT_DIRECTIVE** - on_mount
```json
{
  "type": "ON_MOUNT_DIRECTIVE",
  "nodeId": 130,
  "code": "console.log('Component mounted');"
}
```

**ON_UNMOUNT_DIRECTIVE** - on_unmount
```json
{
  "type": "ON_UNMOUNT_DIRECTIVE",
  "nodeId": 135,
  "code": "console.log('Component unmounted');"
}
```

### Import/Export Directives

**IMPORT_DIRECTIVE** - import
```json
{
  "type": "IMPORT_DIRECTIVE",
  "nodeId": 140,
  "path": "./components/Button.kry",
  "symbols": ["Button", "PrimaryButton"]
}
```

**EXPORT_DIRECTIVE** - export
```json
{
  "type": "EXPORT_DIRECTIVE",
  "nodeId": 145,
  "symbols": ["MyComponent", "myHelper"]
}
```

**INCLUDE_DIRECTIVE** - include (processed, may be expanded)
```json
{
  "type": "INCLUDE_DIRECTIVE",
  "nodeId": 150,
  "path": "./header.kry",
  "expanded": true
}
```

---

## Component Nodes

**IMPORTANT**: In KIR, component **definitions** are preserved, but component **instances** are **fully expanded** (parameters substituted, body inlined).

### COMPONENT Node (Definition)

Component definitions remain in KIR for reference:

```json
{
  "type": "COMPONENT",
  "nodeId": 200,
  "name": "Counter",
  "parameters": ["initialValue", "step"],
  "paramDefaults": ["0", "1"],
  "parentComponent": null,
  "overrideProps": [],
  "stateVars": [
    {
      "type": "STATE_DEFINITION",
      "name": "count",
      "value": {"type": "IDENTIFIER", "name": "initialValue"}
    }
  ],
  "functions": [
    {
      "type": "FUNCTION_DEFINITION",
      "language": "js",
      "name": "increment",
      "parameters": [],
      "code": "this.count += this.step;"
    }
  ],
  "onCreate": null,
  "onMount": null,
  "onUnmount": null,
  "bodyElements": [...]
}
```

### Expanded Component Instance

When a component is used, it's **expanded inline** (no COMPONENT_INSTANCE node):

```kry
# Original source
Counter { initialValue: 5 }
```

```json
# In KIR (expanded)
{
  "type": "ELEMENT",
  "nodeId": 205,
  "elementType": "Container",
  "location": {"line": 10, "column": 1},
  "properties": [...],
  "children": [
    {
      "type": "ELEMENT",
      "elementType": "Button",
      "properties": [
        {"name": "text", "value": {"type": "LITERAL", "value": "Increment"}},
        {"name": "onClick", "value": {"type": "IDENTIFIER", "name": "increment"}}
      ]
    }
  ],
  "metadata": {
    "expandedFrom": "Counter",
    "instanceParameters": {
      "initialValue": "5",
      "step": "1"
    }
  }
}
```

### Component Inheritance

Child components that extend parents:

```json
{
  "type": "COMPONENT",
  "nodeId": 210,
  "name": "PrimaryButton",
  "parentComponent": "Button",
  "overrideProps": [
    {
      "type": "PROPERTY",
      "name": "backgroundColor",
      "value": {"type": "LITERAL", "valueType": "COLOR", "value": "#007BFFFF"}
    }
  ],
  "bodyElements": [...]
}
```

---

## Metadata

### Node Metadata

Optional metadata can be attached to any node for decompilation hints:

```json
{
  "type": "ELEMENT",
  "nodeId": 300,
  "elementType": "Container",
  "metadata": {
    "expandedFrom": "Counter",
    "instanceParameters": {"initialValue": "5"},
    "originalLine": 42,
    "expansion": "const_for"
  },
  "children": [...]
}
```

### Top-Level Metadata

```json
{
  "metadata": {
    "sourceFile": "app.kry",
    "compiler": "kryon",
    "compilerVersion": "1.0.0",
    "timestamp": "2026-01-28T10:30:00Z",
    "expansionInfo": {
      "componentsExpanded": 3,
      "constForLoopsUnrolled": 2,
      "constIfBranchesEvaluated": 1,
      "includesProcessed": 1
    },
    "statistics": {
      "totalNodes": 157,
      "maxDepth": 8,
      "elementCount": 42
    }
  }
}
```

---

## Examples

### Example 1: Simple Button

**Source** (`button.kry`):
```kry
Button {
  text = "Click Me"
  backgroundColor = #007BFF
  onClick = handleClick
}

function "js" handleClick() {
  console.log("Button clicked");
}
```

**KIR** (`button.kir`):
```json
{
  "version": "0.1.0",
  "format": "kir-json",
  "metadata": {
    "sourceFile": "button.kry",
    "compiler": "kryon",
    "compilerVersion": "1.0.0"
  },
  "root": {
    "type": "ROOT",
    "nodeId": 1,
    "children": [
      {
        "type": "ELEMENT",
        "nodeId": 2,
        "elementType": "Button",
        "location": {"line": 1, "column": 1, "file": "button.kry"},
        "properties": [
          {
            "type": "PROPERTY",
            "nodeId": 3,
            "name": "text",
            "value": {
              "type": "LITERAL",
              "nodeId": 4,
              "valueType": "STRING",
              "value": "Click Me"
            }
          },
          {
            "type": "PROPERTY",
            "nodeId": 5,
            "name": "backgroundColor",
            "value": {
              "type": "LITERAL",
              "nodeId": 6,
              "valueType": "COLOR",
              "value": "#007BFFFF"
            }
          },
          {
            "type": "PROPERTY",
            "nodeId": 7,
            "name": "onClick",
            "value": {
              "type": "IDENTIFIER",
              "nodeId": 8,
              "name": "handleClick"
            }
          }
        ],
        "children": []
      },
      {
        "type": "FUNCTION_DEFINITION",
        "nodeId": 9,
        "language": "js",
        "name": "handleClick",
        "parameters": [],
        "code": "console.log(\"Button clicked\");"
      }
    ]
  }
}
```

### Example 2: Expanded Component

**Source** (`counter.kry`):
```kry
component Counter(initialValue = 0) {
  state count = initialValue

  function "js" increment() {
    this.count += 1;
  }

  Container {
    Text { text = "Count: ${count}" }
    Button {
      text = "Increment"
      onClick = increment
    }
  }
}

Counter { initialValue = 5 }
```

**KIR** (component instance **expanded**):
```json
{
  "version": "0.1.0",
  "format": "kir-json",
  "metadata": {
    "sourceFile": "counter.kry",
    "expansionInfo": {
      "componentsExpanded": 1
    }
  },
  "root": {
    "type": "ROOT",
    "nodeId": 1,
    "children": [
      {
        "type": "COMPONENT",
        "nodeId": 2,
        "name": "Counter",
        "parameters": ["initialValue"],
        "paramDefaults": ["0"],
        "stateVars": [
          {
            "type": "STATE_DEFINITION",
            "nodeId": 3,
            "name": "count",
            "value": {"type": "IDENTIFIER", "name": "initialValue"}
          }
        ],
        "functions": [
          {
            "type": "FUNCTION_DEFINITION",
            "nodeId": 4,
            "language": "js",
            "name": "increment",
            "parameters": [],
            "code": "this.count += 1;"
          }
        ],
        "bodyElements": [
          {
            "type": "ELEMENT",
            "elementType": "Container",
            "children": [...]
          }
        ]
      },
      {
        "type": "ELEMENT",
        "nodeId": 10,
        "elementType": "Container",
        "location": {"line": 15, "column": 1},
        "metadata": {
          "expandedFrom": "Counter",
          "instanceParameters": {"initialValue": "5"}
        },
        "children": [
          {
            "type": "ELEMENT",
            "nodeId": 11,
            "elementType": "Text",
            "properties": [
              {
                "type": "PROPERTY",
                "name": "text",
                "value": {
                  "type": "TEMPLATE",
                  "segments": [
                    {"type": "LITERAL", "valueType": "STRING", "value": "Count: "},
                    {"type": "VARIABLE", "name": "count"}
                  ]
                }
              }
            ]
          },
          {
            "type": "ELEMENT",
            "nodeId": 12,
            "elementType": "Button",
            "properties": [
              {"type": "PROPERTY", "name": "text", "value": {"type": "LITERAL", "value": "Increment"}},
              {"type": "PROPERTY", "name": "onClick", "value": {"type": "IDENTIFIER", "name": "increment"}}
            ]
          }
        ]
      }
    ]
  }
}
```

### Example 3: Const For Loop (Expanded)

**Source**:
```kry
const colors = ["red", "green", "blue"]

const_for color in colors {
  Button {
    text = color
    backgroundColor = color
  }
}
```

**KIR** (loop **unrolled**):
```json
{
  "root": {
    "type": "ROOT",
    "children": [
      {
        "type": "CONST_DEFINITION",
        "name": "colors",
        "value": {
          "type": "ARRAY_LITERAL",
          "elements": [
            {"type": "LITERAL", "valueType": "STRING", "value": "red"},
            {"type": "LITERAL", "valueType": "STRING", "value": "green"},
            {"type": "LITERAL", "valueType": "STRING", "value": "blue"}
          ]
        }
      },
      {
        "type": "ELEMENT",
        "elementType": "Button",
        "metadata": {"expansion": "const_for", "iteration": 0},
        "properties": [
          {"name": "text", "value": {"type": "LITERAL", "value": "red"}},
          {"name": "backgroundColor", "value": {"type": "LITERAL", "value": "red"}}
        ]
      },
      {
        "type": "ELEMENT",
        "elementType": "Button",
        "metadata": {"expansion": "const_for", "iteration": 1},
        "properties": [
          {"name": "text", "value": {"type": "LITERAL", "value": "green"}},
          {"name": "backgroundColor", "value": {"type": "LITERAL", "value": "green"}}
        ]
      },
      {
        "type": "ELEMENT",
        "elementType": "Button",
        "metadata": {"expansion": "const_for", "iteration": 2},
        "properties": [
          {"name": "text", "value": {"type": "LITERAL", "value": "blue"}},
          {"name": "backgroundColor", "value": {"type": "LITERAL", "value": "blue"}}
        ]
      }
    ]
  }
}
```

---

## Round-Trip Guarantees

### Semantic Equivalence

KIR guarantees **semantic equivalence** across round-trips:

```
original.kry → kir → krb → kir → roundtrip.kry

AST(original.kry) ≈ AST(roundtrip.kry)  // Semantically identical
```

**What is preserved**:
- All element types and hierarchy
- All properties and values
- All expressions and logic
- All component state and functions
- All directives (@for, @if, etc.)

**What may differ**:
- Whitespace and indentation
- Comment placement
- Property ordering
- Formatting style

### Expansion Preservation

**Components**: Definitions preserved, instances expanded inline
**const_for**: Unrolled (loop body repeated with substitutions)
**const_if**: Evaluated (only matching branch included)
**include**: Processed (file contents inlined)

### Lossless Guarantees

**Forward path** (`kry → kir → krb`):
- KIR contains complete post-expansion AST
- All expansion information preserved in metadata
- Location information maintained

**Backward path** (`krb → kir → kry`):
- KIR reconstructed from binary structure
- Approximate location information
- Expanded forms shown inline (not un-expanded)
- Comments added showing expansion source

---

## Versioning

### Format Version

KIR uses semantic versioning: `MAJOR.MINOR.PATCH`

```json
{
  "version": "0.1.0",
  "format": "kir-json"
}
```

**Version Compatibility Rules**:
- **MAJOR**: Breaking changes (incompatible AST structure)
- **MINOR**: New node types or fields (backward compatible)
- **PATCH**: Bug fixes, clarifications (fully compatible)

**Forward Compatibility**:
- Readers MUST ignore unknown fields
- Readers SHOULD warn on unknown node types
- Readers MAY skip unknown nodes with warning

**Version Checks**:
```c
bool is_compatible = (file_major == EXPECTED_MAJOR) &&
                     (file_minor >= EXPECTED_MINOR);
```

### Migration

When breaking changes occur:
1. Increment MAJOR version
2. Provide migration tool: `kryon kir-migrate old.kir new.kir`
3. Document changes in changelog

---

## Implementation Notes

### JSON Requirements

- **Encoding**: UTF-8
- **Max depth**: Unlimited (implementation may set limits)
- **Number precision**: 64-bit float for all numbers
- **String escaping**: Standard JSON escaping (\n, \t, \", \\, etc.)

### Performance

- **Streaming**: Writers should stream output (don't build full JSON in memory)
- **Incremental parsing**: Readers may parse incrementally
- **Memory**: Allocate AST nodes using Kryon's memory system

### Error Handling

**Writer errors** (should never happen if AST is valid):
- Invalid AST structure
- Out of memory

**Reader errors** (may happen with malformed KIR):
- JSON syntax errors
- Missing required fields
- Invalid node structure
- Version incompatibility

---

## Conclusion

KIR is the **canonical intermediate representation** for Kryon compilation. It provides:

1. **Transparency**: Full visibility into compiler transformations
2. **Decompilability**: Reverse engineering from binary to source
3. **Testing**: Independent validation of each compiler stage
4. **AI/LLM**: Machine-readable format for advanced tooling
5. **Modularity**: Clean separation of frontend and backend

Every Kryon compilation goes through KIR, making it the **source of truth** for the compilation pipeline.
