# Kryon Architecture

## Core Concept

**Write UI once, run everywhere** through a universal intermediate representation.

```
Source (.tsx, .kry, .lua, .html) → KIR (JSON) → Target (.tsx, .c, .lua, .nim) → Runtime
```

## KIR Format

KIR (Kryon Intermediate Representation) is JSON with three sections:

```json
{
  "root": { /* Component tree */ },
  "logic_block": { /* Event handlers */ },
  "reactive_manifest": { /* State & hooks */ }
}
```

### Root - Component Tree

```json
{
  "id": 1,
  "type": "Column",
  "children": [
    {"id": 2, "type": "Text", "text": "Hello"},
    {"id": 3, "type": "Button", "events": [...]}
  ]
}
```

### Logic Block - Event Handlers

```json
{
  "functions": [
    {
      "name": "handler_1_click",
      "universal": {
        "statements": [{"type": "assign", "target": "count", "expr": {...}}]
      },
      "sources": [{"language": "typescript", "source": "..."}]
    }
  ],
  "event_bindings": [
    {"component_id": 3, "event_type": "click", "handler_name": "handler_1_click"}
  ]
}
```

### Reactive Manifest - State & Hooks

```json
{
  "variables": [
    {"name": "count", "type": "number", "initial_value": "0", "setter_name": "setCount"}
  ],
  "hooks": [
    {"type": "useEffect", "callback": "...", "dependencies": "[]"}
  ]
}
```

## Architecture Layers

### 1. Parsers (Source → KIR)

- **TSX** (`ir/parsers/tsx/`, `ir/parsers/tsx/`) - TypeScript/React with hooks
- **Kry** (`ir/parsers/kry/`) - Custom DSL
- **Lua** (`ir/parsers/lua/`) - Lua-based UI (perfect roundtrip)
- **HTML** (`ir/parsers/html/`) - Standard HTML with CSS support
- **Markdown** (`ir/parsers/markdown/`) - Documents
- **C** (`ir/parsers/c/`) - C-based UI

All parsers output the same KIR format.

### 2. Codegens (KIR → Target)

- **TSX** (`codegens/tsx/`, `codegens/react_common.c`) - React code
- **Lua** (`codegens/lua/`) - Lua code
- **Nim** (`codegens/nim/`) - Nim code
- **C** (`codegens/c/`) - C code (DSL-style, incomplete)

### 3. Runtimes

- **Desktop** (`backends/desktop/`) - SDL3 native windows
- **IR Executor** (`ir/ir_executor.c`) - Runs KIR directly via universal logic

## Universal Logic

Simple handlers convert to language-independent IR that executes without runtime:

```typescript
// Source
onClick={() => setCount(count + 1)}

// Universal Logic
{
  "type": "assign",
  "target": "count",
  "expr": {"op": "add", "left": {"var": "count"}, "right": {"value": 1}}
}
```

Complex handlers remain as source strings and need language runtime.

## Components

- **Layout**: Column, Row, Container
- **UI**: Text, Button, Input
- **More**: See implementation guide

## Events

All standard DOM events with React-style camelCase:
- onClick, onChange, onSubmit, onFocus, onBlur
- onKeyDown, onKeyUp, onMouseEnter, onMouseLeave, etc.

## State Management

Kryon supports direct variable reactivity and React-style hooks:

- **Direct Variables** - Native to .kry format
- **React Hooks** - useState, useEffect, useCallback, useMemo, useReducer
- **Type Inference** - Automatic from initial values

## Workflow

```bash
# Parse to KIR
bun ir/parsers/tsx/tsx_to_kir.ts app.tsx > app.kir

# Run on desktop
kryon run app.kir

# Generate target code
kryon codegen tsx app.kir app_gen.tsx
kryon codegen lua app.kir app.lua
```

## File Organization

```
kryon/
├── cli/           # Command-line tool (C + TypeScript)
├── ir/            # Core IR library (C)
├── core/          # Core runtime components
├── codegens/      # Code generators (TSX, Lua, Nim, C)
├── backends/      # Runtime backends (Desktop/SDL3)
├── renderers/     # Rendering systems
├── platforms/     # Platform-specific code
├── bindings/      # Language bindings (C, Nim, TypeScript)
├── examples/      # Example applications
├── tests/         # Test files
├── docs/          # Documentation
├── packages/      # Package components
├── scripts/       # Build/utility scripts
└── third_party/   # External dependencies
```

## Known Limitations

1. Complex JSX expressions evaluated at parse time
2. Custom hooks not yet supported
3. Component props system needs enhancement
4. Conditional rendering evaluated at parse time
5. Array mapping evaluated at parse time

## Extension Points

See `IMPLEMENTATION_GUIDE.md` for:
- Adding parsers
- Adding codegens
- Adding components
- Adding events
- Adding hooks

---

**Version**: 1.0.0  
**Status**: Production ready - multiple parsers with perfect roundtrip (Lua), universal logic execution
