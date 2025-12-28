# Kryon Architecture Documentation

## Overview

Kryon is a universal UI framework that uses an intermediate representation (KIR) to enable cross-platform development. The architecture follows a three-stage pipeline:

```
Source Language → Parser → KIR → Codegen → Target Language/Runtime
```

This design allows writing UI code once in any supported source language (TSX, Kry, HTML, Markdown, Lua) and generating output for any target platform (React, native desktop, mobile, etc.).

## Architecture Layers

### 1. Parsers (Source → KIR)

Parsers convert source languages into KIR format:

- **TSX Parser** (`cli/tsx_parser/tsx_to_kir.ts`) - TypeScript React with hooks
- **Kry Parser** (`ir/ir_kry_parser.c`) - Custom Kryon language
- **HTML Parser** (`ir/ir_html_parser.c`) - Standard HTML5
- **Markdown Parser** (`ir/ir_markdown_parser.c`) - Markdown documents
- **Lua Parser** (`parsers/lua/ir_lua_parser.c`) - Lua-based UI definitions
- **C Parser** (`parsers/c/ir_c_parser.c`) - C-based UI definitions

### 2. KIR (Kryon Intermediate Representation)

JSON-based format with three main sections:

```json
{
  "format": "kir",
  "metadata": {
    "source_language": "tsx",
    "compiler_version": "kryon-1.0.0"
  },
  "root": { /* Component tree */ },
  "logic_block": { /* Event handlers and functions */ },
  "reactive_manifest": { /* State variables and hooks */ }
}
```

### 3. Codegens (KIR → Target)

Code generators convert KIR to various output formats:

- **React/TSX Codegen** (`codegens/tsx/tsx_codegen.c`, `codegens/react_common.c`) - React with TypeScript
- **C Codegen** (`ir/codegens/ir_c_codegen.c`) - Native C code
- **Lua Codegen** (`codegens/lua/lua_codegen.c`) - Lua code generation
- **KRB Format** (`ir/ir_krb.c`) - Kryon binary format

### 4. Renderers (Target → UI)

Runtime systems that execute the generated code:

- **Desktop Renderer** (`backends/desktop/`) - SDL3-based native desktop
- **React Renderer** - Web/mobile through React
- **Terminal Renderer** - Text-based UI (planned)

## KIR Format Specification

### Root Component Tree

The `root` section describes the UI component hierarchy:

```json
{
  "id": 1,
  "type": "Column",
  "flexDirection": "column",
  "children": [
    {
      "id": 2,
      "type": "Text",
      "text": "Hello World"
    },
    {
      "id": 3,
      "type": "Button",
      "events": [
        {
          "type": "click",
          "logic_id": "handler_1_click",
          "handler_data": "() => console.log('clicked')"
        }
      ],
      "children": [/* button content */]
    }
  ]
}
```

### Logic Block

The `logic_block` section contains event handlers and business logic:

```json
{
  "functions": [
    {
      "name": "handler_1_click",
      "sources": [
        {
          "language": "typescript",
          "source": "() => console.log('clicked')"
        }
      ]
    }
  ],
  "event_bindings": [
    {
      "component_id": 3,
      "event_type": "click",
      "handler_name": "handler_1_click"
    }
  ]
}
```

### Reactive Manifest

The `reactive_manifest` section defines state and reactive hooks:

```json
{
  "variables": [
    {
      "id": 1,
      "name": "count",
      "type": "number",
      "initial_value": "0",
      "setter_name": "setCount"
    }
  ],
  "hooks": [
    {
      "type": "useEffect",
      "callback": "() => console.log('mounted')",
      "dependencies": "[]"
    },
    {
      "type": "useMemo",
      "callback": "() => count * 2",
      "dependencies": "[count]",
      "name": "doubled"
    },
    {
      "type": "useCallback",
      "callback": "() => setCount(c => c + 1)",
      "dependencies": "[]",
      "name": "handleIncrement"
    }
  ]
}
```

## Component Types

### Layout Components

- **Column** - Vertical layout (`flexDirection: "column"`)
- **Row** - Horizontal layout (`flexDirection: "row"`)
- **Container** - Generic container
- **Grid** - Grid layout (planned)
- **Stack** - Layered layout (planned)

### UI Components

- **Text** - Text display with optional interpolation
- **Button** - Interactive button
- **Input** - Text input field
- **Image** - Image display (planned)
- **Video** - Video player (planned)

### Special Text Handling

Text components support JSX-style interpolation:

```tsx
<Text>Count: {count}</Text>
```

This is preserved through the KIR pipeline using the `text_expression` field:

```json
{
  "type": "Text",
  "children": [
    {"type": "Text", "text": "Count: "},
    {"type": "Text", "text": "0"}
  ],
  "text_expression": "{count}"
}
```

## Event System

### Supported Event Types

All standard DOM events are supported with React-style camelCase naming:

- **Mouse Events**: `onClick`, `onDoubleClick`, `onMouseEnter`, `onMouseLeave`, `onMouseDown`, `onMouseUp`
- **Input Events**: `onChange`, `onInput`, `onSubmit`
- **Focus Events**: `onFocus`, `onBlur`
- **Keyboard Events**: `onKeyDown`, `onKeyUp`, `onKeyPress`

### Event Handler Storage

Event handlers are stored in two places:

1. **Component level** - Direct reference in `events` array with `handler_data`
2. **Logic block** - Centralized functions with unique names

This dual storage enables both inline handlers and reusable functions.

## State Management

### State Variables

State is tracked through the reactive manifest with automatic type inference:

- **number** - Inferred from numeric literals: `0`, `42`, `-1.5`
- **string** - Inferred from quoted literals: `'text'`, `"text"`, `` `text` ``
- **boolean** - Inferred from: `true`, `false`
- **array** - Inferred from: `[]`, `[1, 2, 3]`
- **object** - Inferred from: `{}`, `{key: value}`
- **any** - Default fallback type

### Supported Hooks

#### useState
```typescript
const [count, setCount] = useState(0);
const [name, setName] = useState('Alice');
```

Generates:
```json
{
  "name": "count",
  "type": "number",
  "initial_value": "0",
  "setter_name": "setCount"
}
```

#### useEffect
```typescript
useEffect(() => {
  console.log('Component mounted');
}, []);

useEffect(() => {
  console.log('Count changed:', count);
}, [count]);
```

#### useCallback
```typescript
const handleIncrement = useCallback(() => {
  setCount(c => c + 1);
}, []);
```

Variable name (`handleIncrement`) is preserved through the pipeline.

#### useMemo
```typescript
const doubled = useMemo(() => count * 2, [count]);
```

Variable name (`doubled`) and dependencies are preserved.

#### useReducer
```typescript
const [state, dispatch] = useReducer(reducer, initialState);
```

Supported with full state/dispatch pair tracking.

## TSX Parser Implementation

### Technology Stack

The TSX parser uses **Bun** (JavaScript runtime) with built-in TypeScript transpilation:

**File**: `cli/tsx_parser/tsx_to_kir.ts`

### Parse Strategy

1. **Hook Detection** - Regex-based scanning before eval:
   ```typescript
   const useStatePattern = /const\s+\[([a-zA-Z_$][a-zA-Z0-9_$]*)\s*,\s*([a-zA-Z_$][a-zA-Z0-9_$]*)\]\s*=\s*useState(?:<[^>]+>)?\(([^)]*)\)/g;
   ```

2. **Text Expression Detection** - Scan for JSX interpolation before transpilation:
   ```typescript
   const jsxTextPattern = />([^<]*\{[^}]+\}[^<]*)</g;
   ```

3. **Transpile & Eval** - Use Bun's transpiler to convert TSX to executable JavaScript

4. **Component Traversal** - Walk the evaluated React element tree

5. **KIR Generation** - Build JSON structure with all metadata

### Why Regex Instead of AST?

- **Simplicity** - Regex patterns are straightforward and maintainable
- **Performance** - Fast execution for hook detection patterns
- **Reliability** - Works consistently across various TSX patterns
- **Bun Integration** - Leverages Bun's built-in transpiler for the heavy lifting

The regex approach captures hooks before Bun evaluates the code, preserving source-level information that would be lost after transpilation.

## Code Generation Details

### React/TSX Generation

**File**: `codegens/react_common.c`

Key features:

1. **Event Handler Mapping** - Converts KIR event types to React camelCase:
   ```c
   "click" → "onClick"
   "keydown" → "onKeyDown"
   "mouseenter" → "onMouseEnter"
   ```

2. **Hook Generation** - Outputs TypeScript-typed hooks:
   ```c
   const [count, setCount] = useState<number>(0);
   const doubled = useMemo(() => count * 2, [count]);
   ```

3. **Text Flattening** - Nested Text children become inline content:
   ```c
   // Instead of: <Text><Text>Hello</Text></Text>
   // Generate: <Text>Hello</Text>
   ```

4. **Expression Reconstruction** - Rebuilds JSX interpolation from metadata:
   ```c
   // From text_expression field, reconstruct:
   <Text>Count: {count}</Text>
   ```

### C Code Generation

**File**: `ir/codegens/ir_c_codegen.c`

Generates native C code using the desktop renderer API:
```c
KryComponent* button = kry_create_component("Button");
kry_component_set_event(button, "click", handler_1_click);
```

### Lua Code Generation

**File**: `codegens/lua/lua_codegen.c`

Generates Lua code for Lua-based runtimes.

## File Organization

```
kryon/
├── cli/                      # Command-line interface
│   ├── src/
│   │   └── commands/         # CLI commands (compile, diff, inspect)
│   └── tsx_parser/           # TypeScript/TSX parser (Bun-based)
│       └── tsx_to_kir.ts
├── ir/                       # Core IR library
│   ├── ir_kry_parser.c       # Kry language parser
│   ├── ir_html_parser.c      # HTML parser
│   ├── ir_markdown_parser.c  # Markdown parser
│   ├── ir_serialization.c    # KIR serialization
│   ├── ir_executor.c         # IR execution engine
│   └── codegens/
│       └── ir_c_codegen.c    # C code generator
├── parsers/                  # Additional language parsers
│   ├── c/
│   │   └── ir_c_parser.c     # C-based UI parser
│   └── lua/
│       └── ir_lua_parser.c   # Lua parser
├── codegens/                 # Code generators
│   ├── react_common.c        # Shared React/TSX generation
│   ├── tsx/
│   │   └── tsx_codegen.c     # TSX-specific generation
│   └── lua/
│       └── lua_codegen.c     # Lua code generation
├── backends/                 # Runtime backends
│   └── desktop/              # SDL3-based desktop renderer
│       ├── desktop_window.c
│       ├── desktop_input.c
│       └── desktop_renderer.c
└── bindings/                 # Language bindings
    ├── c/
    ├── nim/
    └── (others planned)
```

## Workflow Examples

### TSX → Desktop Native

```bash
# Parse TSX to KIR
bun cli/tsx_parser/tsx_to_kir.ts input.tsx > output.kir

# Generate C code from KIR
./kryon compile output.kir --target c --output output.c

# Compile and run
gcc output.c -lkryon_desktop -o app && ./app
```

### Round-Trip Verification

```bash
# TSX → KIR
bun tsx_to_kir.ts input.tsx > test1.kir

# KIR → TSX
./kryon compile test1.kir --target tsx --output generated.tsx

# TSX → KIR again
bun tsx_to_kir.ts generated.tsx > test2.kir

# Compare KIR files (should be semantically identical)
diff test1.kir test2.kir
```

Successfully verified with:
- 4 hooks (2 useEffect, 1 useCallback, 1 useMemo)
- 4 state variables (count, name, enabled, items)
- 8 event bindings (click, change, focus, blur, mouseenter, mouseleave)
- Text interpolation preserved (`{count}`, `{name}`, `{doubled}`)

### HTML → React

```bash
# Parse HTML to KIR
./kryon compile input.html --target kir --output output.kir

# Generate React from KIR
./kryon compile output.kir --target tsx --output output.tsx
```

## Capabilities Matrix

| Feature | TSX Parser | Kry Parser | HTML Parser | Markdown Parser | Lua Parser |
|---------|-----------|-----------|-------------|----------------|-----------|
| Layout Components | ✅ | ✅ | ✅ | ✅ | ✅ |
| Text/Button/Input | ✅ | ✅ | ✅ | ⚠️ (limited) | ✅ |
| Event Handlers | ✅ (all types) | ✅ | ⚠️ (basic) | ❌ | ✅ |
| useState | ✅ | ❌ | ❌ | ❌ | ⚠️ (TBD) |
| useEffect | ✅ | ❌ | ❌ | ❌ | ⚠️ (TBD) |
| useCallback | ✅ | ❌ | ❌ | ❌ | ⚠️ (TBD) |
| useMemo | ✅ | ❌ | ❌ | ❌ | ⚠️ (TBD) |
| useReducer | ✅ | ❌ | ❌ | ❌ | ⚠️ (TBD) |
| Text Interpolation | ✅ | ⚠️ (TBD) | ❌ | ❌ | ⚠️ (TBD) |
| Type Inference | ✅ | ❌ | ❌ | ❌ | ⚠️ (TBD) |

| Target | Code Quality | Maturity | Round-Trip |
|--------|-------------|----------|------------|
| TSX Codegen | ✅ High | ✅ Complete | ✅ Verified |
| C Codegen | ✅ High | ✅ Complete | ⚠️ Partial |
| Lua Codegen | ⚠️ Basic | ⚠️ In Progress | ❌ No |
| KRB Binary | ✅ High | ✅ Complete | ✅ Yes |

## Performance Characteristics

### Parser Performance

- **TSX Parser**: ~10-50ms for typical components (Bun transpilation overhead)
- **C Parsers**: <1ms for simple UIs (native code, no transpilation)
- **HTML Parser**: ~5-20ms (DOM tree construction)

### Codegen Performance

- **TSX Codegen**: ~5-10ms (string building in C)
- **C Codegen**: ~5-10ms
- **KIR Serialization**: <1ms (cJSON)

### Memory Usage

- Typical KIR file: 2-10KB for small apps, 50-200KB for complex apps
- In-memory IR: ~100-500 bytes per component
- Desktop renderer: ~5-20MB total memory for typical apps

## Known Limitations

1. **JSX Expression Complexity**: Only simple variable interpolation is preserved (e.g., `{count}`). Complex expressions like `{count + 1}` or `{user.name}` are evaluated and lost during parsing.

2. **Hook Dependencies**: Dependencies are stored as strings, not analyzed. Incorrect dependencies won't be caught.

3. **Custom Hooks**: Not yet supported. Only built-in React hooks (useState, useEffect, useCallback, useMemo, useReducer) are recognized.

4. **Component Props**: Props system is limited. Component-to-component data passing needs enhancement.

5. **Conditional Rendering**: Ternaries and && operators in JSX are evaluated during parsing. The KIR represents the resolved state, not the condition logic.

6. **Loops/Maps**: Array mapping (`.map()`) is evaluated at parse time. Dynamic lists aren't fully supported in KIR yet.

## Extension Points

### Adding a New Source Parser

1. Create parser in `parsers/<language>/`
2. Implement `parse_<language>_to_kir()` function
3. Output KIR JSON format
4. Add CLI command in `cli/src/commands/`
5. Update capability matrix

### Adding a New Target Codegen

1. Create codegen in `codegens/<target>/`
2. Implement KIR → target language conversion
3. Handle all KIR sections (root, logic_block, reactive_manifest)
4. Add CLI output option
5. Test round-trip if applicable

### Adding a New Component Type

1. Define component in KIR spec (update this doc)
2. Add to all relevant parsers
3. Add to all relevant codegens
4. Implement renderer support in backends
5. Add tests

### Adding a New Hook Type

1. Add detection regex to TSX parser
2. Add to `reactive_manifest.hooks` array
3. Update codegen to output the hook
4. Add to capability matrix
5. Test round-trip preservation

## Development Guidelines

### Testing Strategy

1. **Unit Tests**: Test individual parsers/codegens in isolation
2. **Round-Trip Tests**: Verify source → KIR → source preservation
3. **Integration Tests**: Full pipeline tests with multiple stages
4. **Golden Files**: Reference KIR files for regression testing

### Code Style

- **C Code**: Follow Linux kernel style (indentation, naming)
- **TypeScript**: Use TypeScript strict mode, ESLint
- **File Naming**: `ir_<module>.c` for IR library, `<lang>_codegen.c` for codegens

### Documentation Requirements

- Public APIs must have header comments
- Complex algorithms need inline explanation
- Update this architecture doc when adding major features
- Keep capability matrix current

## Future Roadmap

### Short Term (Next Month)

- [ ] Complete Lua parser round-trip support
- [ ] Add Grid layout component
- [ ] Implement component props system
- [ ] Add custom hook support to TSX parser
- [ ] Improve error messages across parsers

### Medium Term (Next Quarter)

- [ ] Add Image and Video components
- [ ] Implement conditional rendering in KIR
- [ ] Add array mapping support
- [ ] Create web-based KIR visualizer
- [ ] Add performance profiling tools

### Long Term (Next Year)

- [ ] Add terminal/TUI renderer backend
- [ ] Support mobile platforms (iOS/Android)
- [ ] Create visual editor for KIR
- [ ] Add hot-reload development mode
- [ ] Build plugin system for custom components

## Contributing

See individual component documentation in source files for specific contribution guidelines. All major changes should:

1. Update this architecture documentation
2. Add tests (unit + round-trip where applicable)
3. Update capability matrix
4. Ensure backward compatibility with existing KIR files

## Version History

- **1.0.0** (Current) - TSX parser complete with full hook support, round-trip verified
- **0.9.0** - HTML/Markdown/Kry parsers functional
- **0.8.0** - Desktop renderer with SDL3
- **0.7.0** - Initial KIR format and C codegen
