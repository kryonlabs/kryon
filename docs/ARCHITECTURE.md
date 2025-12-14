# Kryon Architecture

This document describes the architecture of Kryon, a cross-platform UI framework with an intermediate representation (IR) layer.

## Overview

Kryon separates concerns into distinct layers:

```
┌─────────────────────────────────────────────────────────────────┐
│                         FRONTENDS                                │
│   .kry (declarative)  |  .nim (DSL)  |  .lua  |  .js/.ts        │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│                    INTERMEDIATE REPRESENTATION                   │
│   .kir (JSON IR)  ←→  .kirb (Binary IR)                         │
│   Components, styles, layout, reactive manifest, logic blocks   │
└─────────────────────────────────────────────────────────────────┘
                              │
              ┌───────────────┴───────────────┐
              ▼                               ▼
┌─────────────────────────┐     ┌─────────────────────────┐
│     NATIVE TARGET       │     │      VM TARGET          │
│  Transpile to C/Nim     │     │   .krb (bytecode)       │
│  Compile to binary      │     │   Kryon VM Runtime      │
└─────────────────────────┘     └─────────────────────────┘
                                              │
                                              ▼
                              ┌─────────────────────────────────┐
                              │         RENDERERS               │
                              │  SDL3  |  Terminal  |  Web      │
                              │       (pluggable)               │
                              └─────────────────────────────────┘
```

## File Formats

### Source Formats

| Format | Description | Example |
|--------|-------------|---------|
| `.kry` | Declarative syntax | `Button { title: "Click" }` |
| `.nim` | Nim DSL with macros | `Button: title = "Click"` |
| `.lua` | Lua bindings | `Button { title = "Click" }` |

### Intermediate Formats

| Format | Description | Use Case |
|--------|-------------|----------|
| `.kir` | JSON IR | Human-readable, debugging, tooling |
| `.kirb` | Binary IR | Optimized storage (5-10× smaller) |

Both `.kir` and `.kirb` contain the same information:
- Component tree (types, IDs, properties)
- Style definitions (colors, dimensions, layout)
- Reactive manifest (variables, conditionals)
- Logic blocks (event handlers, functions)

### Executable Format

| Format | Description | Runtime |
|--------|-------------|---------|
| `.krb` | VM bytecode | Kryon VM + pluggable renderer |

The `.krb` format is a compiled, executable bundle containing:
- UI component tree (from IR)
- Compiled bytecode for event handlers
- String constants and initial values
- Optional: debug symbols, reactive manifest

## Compilation Targets

### Native Target

For maximum performance, Kryon can transpile to native source code:

```
.kry → .kir → C source → gcc → native binary
.kry → .kir → Nim source → nim c → native binary
```

**Characteristics:**
- Fastest execution
- No runtime VM overhead
- Platform-specific binaries
- Larger file sizes

### VM Target

For portability and hot-reload, Kryon compiles to bytecode:

```
.kry → .kir → .krb (bytecode)
```

**Characteristics:**
- Single file runs anywhere
- Hot-reload support
- Pluggable renderers
- Smaller distribution size

## Component Model

### IR Component Structure

```c
typedef struct IRComponent {
    IRComponentType type;      // CONTAINER, TEXT, BUTTON, etc.
    uint32_t id;               // Unique identifier
    char* text_content;        // For TEXT components
    IRStyle* style;            // Visual styling
    IRLayout* layout;          // Flexbox layout
    IRComponent** children;    // Child components
    int child_count;
    IREvent** events;          // Event handlers
    // ... additional fields
} IRComponent;
```

### Component Types

| Type | Description |
|------|-------------|
| `CONTAINER` | Generic container (div) |
| `ROW` | Horizontal flex container |
| `COLUMN` | Vertical flex container |
| `TEXT` | Text display |
| `BUTTON` | Clickable button |
| `INPUT` | Text input |
| `CHECKBOX` | Toggle checkbox |
| `DROPDOWN` | Selection dropdown |
| `TAB_GROUP` | Tab container |
| `TAB_BAR` | Tab header bar |
| `TAB` | Individual tab |
| `TAB_CONTENT` | Tab content area |
| `TAB_PANEL` | Tab panel |

## Reactive System

### State Variables

```kry
state counter: int = 0
state name: string = "World"
```

Variables in the reactive manifest can be:
- Integers, strings, booleans
- Arrays (mixed types)
- Referenced in text: `"Count: {{counter}}"`
- Updated by event handlers

### Conditionals

```kry
if counter > 0 {
    Text { content: "Positive!" }
}
```

Conditionals control component visibility based on state.

### Event Handlers

```kry
Button {
    title: "Increment"
    onClick: {
        counter = counter + 1
    }
}
```

Handlers can be:
- Universal statements (transpilable)
- Embedded source code (language-specific)

## VM Architecture

### .krb File Structure

```
┌──────────────────────────────────────┐
│ Header (32 bytes)                    │
│   Magic: "KRBY"                      │
│   Version: 1.0                       │
│   Section offsets                    │
├──────────────────────────────────────┤
│ UI Section                           │
│   Component tree (binary encoded)    │
├──────────────────────────────────────┤
│ Code Section                         │
│   Bytecode for event handlers        │
├──────────────────────────────────────┤
│ Data Section                         │
│   String constants                   │
│   Initial values                     │
├──────────────────────────────────────┤
│ Meta Section (optional)              │
│   Debug symbols                      │
│   Source maps                        │
└──────────────────────────────────────┘
```

### Bytecode Instructions

The VM uses a stack-based instruction set:

| Category | Instructions |
|----------|--------------|
| Stack | `PUSH_INT`, `PUSH_STR`, `POP`, `DUP` |
| Variables | `LOAD_VAR`, `STORE_VAR`, `LOAD_GLOBAL`, `STORE_GLOBAL` |
| Arithmetic | `ADD`, `SUB`, `MUL`, `DIV`, `MOD`, `NEG` |
| Comparison | `EQ`, `NE`, `LT`, `LE`, `GT`, `GE` |
| Logic | `AND`, `OR`, `NOT` |
| Control | `JMP`, `JMP_IF`, `JMP_IF_NOT`, `CALL`, `RET` |
| UI | `SET_PROP`, `GET_PROP`, `SET_VISIBLE`, `TRIGGER_REDRAW` |

### Runtime Execution

```
┌─────────────────┐     ┌─────────────────┐
│   .krb Module   │────▶│  Kryon Runtime  │
└─────────────────┘     └────────┬────────┘
                                 │
                   ┌─────────────┼─────────────┐
                   │             │             │
                   ▼             ▼             ▼
            ┌──────────┐  ┌──────────┐  ┌──────────┐
            │   SDL3   │  │ Terminal │  │   Web    │
            │ Renderer │  │ Renderer │  │ Renderer │
            └──────────┘  └──────────┘  └──────────┘
```

The runtime:
1. Loads the `.krb` module
2. Initializes state from data section
3. Builds component tree from UI section
4. Attaches to a renderer
5. Enters main loop (render, handle events, execute bytecode)

## Renderer Architecture

Renderers are pluggable backends that display the UI:

```c
typedef struct IRRenderer {
    bool (*init)(int width, int height, const char* title);
    void (*shutdown)(void);
    void (*begin_frame)(void);
    void (*end_frame)(void);
    void (*render_component)(IRComponent* component);
    bool (*poll_events)(IREvent* event);
} IRRenderer;
```

### Available Renderers

| Renderer | Platform | Features |
|----------|----------|----------|
| SDL3 | Desktop | GPU-accelerated, full graphics |
| Terminal | Any | Text-based, no graphics |
| Web | Browser | HTML/CSS output (planned) |

## Directory Structure

```
kryon/
├── ir/                     # IR core (C)
│   ├── ir_core.h           # Type definitions
│   ├── ir_builder.c        # Component creation
│   ├── ir_layout.c         # Flexbox layout
│   ├── ir_serialization.c  # .kir/.kirb formats
│   ├── ir_executor.c       # Event dispatch
│   ├── ir_krb.c            # .krb format
│   └── ir_vm_exec.c        # VM execution
│
├── backends/
│   ├── desktop/            # SDL3 + Terminal
│   └── web/                # Web (planned)
│
├── bindings/
│   ├── nim/                # Nim DSL
│   ├── lua/                # Lua bindings
│   └── typescript/         # TypeScript (planned)
│
├── cli/                    # CLI tool
│   ├── main.nim            # Entry point
│   ├── compile.nim         # Compilation
│   ├── kry_parser.nim      # .kry parser
│   └── codegen.nim         # Code generation
│
└── examples/
    └── kry/                # Source examples
```

## Future Vision

### Kryon Launcher

A standalone application that can:
- Open `.krb` files directly (double-click)
- Auto-compile `.kry` files on open
- Hot-reload during development
- Like Love2D for `.love` files

### Additional VM Targets

- LuaJIT VM (for Lua frontends)
- QuickJS VM (for JS/TS frontends)
- Python VM (for Python frontends)

### Web Deployment

- Compile to `.krb` + WASM runtime
- Single-file web app distribution
- Progressive web app support
