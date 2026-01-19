# Kryon Architecture

## Executive Summary

Kryon is a universal UI framework that enables **write-once, run-anywhere** user interfaces through a three-stage compilation pipeline:

```
Source (.tsx, .kry, .html, .lua) → KIR (JSON IR) → Target (.tsx, .c, .lua) → Runtime
```

The core innovation is **KIR (Kryon Intermediate Representation)**, a JSON-based intermediate representation that captures UI structure, styling, layout, and behavior in a platform-agnostic format.

## Core Philosophy

1. **Separation of Concerns**: UI definition (source) is separate from rendering (runtime)
2. **Platform Agnostic**: KIR can be rendered on any platform with a runtime implementation
3. **Incremental Adoption**: Use Kryon for individual components or entire applications
4. **Zero Runtime Cost**: Kryon compiles to native platform code, not an interpreter

## System Architecture

### Three-Stage Pipeline

```
┌─────────────────┐    ┌──────────────┐    ┌─────────────────┐    ┌──────────┐
│  Source Files   │ →  │     KIR      │ →  │  Target Code    │ →  │ Runtime  │
│                 │    │  (JSON IR)   │    │                 │    │          │
│ • .tsx          │    │              │    │ • .tsx (React)  │    │ • SDL3   │
│ • .kry          │    │ • Components │    │ • .c (Native)   │    │ • Raylib │
│ • .html         │    │ • Styles     │    │ • .lua (Lua)    │    │ • Terminal│
│ • .lua          │    │ • Layout     │    │ • .html (Web)   │    │ • Web    │
└─────────────────┘    └──────────────┘    └─────────────────┘    └──────────┘
     Parser                 Serialization         Codegen            Backend
```

### Stage 1: Source → KIR (Parsing)

**Location**: `ir/parsers/`

Each language has a parser that converts source code into KIR JSON:

- **TSX Parser** (`parsers/tsx/tsx_to_kir.ts`): TypeScript/JSX → KIR
- **KRY Parser** (`parsers/kry/kry_parser.c`): Kryon DSL → KIR
- **HTML Parser** (`parsers/html/html_parser.c`): HTML → KIR
- **Lua Parser** (`parsers/lua/lua_parser.c`): Lua → KIR

**Output**: KIR JSON file (`.kir`) with this structure:

```json
{
  "version": "1.0",
  "component": {
    "type": "container",
    "children": [...],
    "style": {...},
    "layout": {...}
  }
}
```

### Stage 2: KIR → Target (Code Generation)

**Location**: `codegens/`

Code generators convert KIR into platform-specific target code:

- **TSX Codegen** (`codegens/tsx/`): KIR → React TSX
- **C Codegen** (`codegens/c/`): KIR → C with Kryon library
- **Lua Codegen** (`codegens/lua/`): KIR → Lua with Kryon library
- **Kotlin Codegen** (`codegens/kotlin/`): KIR → Kotlin/Jetpack Compose
- **Markdown Codegen** (`codegens/markdown/`): KIR → Markdown documentation
- **Web Codegen** (`codegens/web/`): KIR → HTML/CSS/JS

**Strategy**: Code generators use shared utilities from `codegen_common.c`

### Stage 3: Target → Runtime (Execution)

**Location**: `runtime/`

Runtime backends execute the generated code on specific platforms:

- **Desktop** (`runtime/desktop/`): SDL3-based desktop renderer
- **Android** (`runtime/android/`): Android native runtime
- **Plan 9** (`runtime/plan9/`): Plan 9/9front native runtime

**Rendering Systems** (`renderers/`):

- **SDL3** (`renderers/sdl3/`): SDL3 renderer for desktop
- **Raylib** (`renderers/raylib/`): Raylib renderer for desktop
- **Terminal** (`renderers/terminal/`): Terminal-based renderer
- **Framebuffer** (`renderers/framebuffer/`): Direct framebuffer renderer

## Component Architecture

### IR Core (`ir/`)

The IR library defines the component tree structure:

**Key Files**:
- `ir_core.h`: Component tree structures (`IRComponent`)
- `ir_logic.h`: Logic blocks (event handlers, state)
- `ir_executor.h`: Universal logic executor
- `ir_builder.h`: Component builder API
- `ir_serialization.c`: KIR JSON parsing/generation

**Component Types** (31 types total):

```c
typedef enum {
    IR_COMPONENT_CONTAINER,
    IR_COMPONENT_BUTTON,
    IR_COMPONENT_TEXT,
    IR_COMPONENT_IMAGE,
    IR_COMPONENT_TEXT_INPUT,
    IR_COMPONENT_SCROLL_VIEW,
    // ... 26 more types
} IRComponentType;
```

**Component Structure**:

```c
typedef struct IRComponent {
    IRComponentType type;
    char* id;
    IRStyle* style;
    IRLayout* layout;
    IRLogicBlock* logic;
    struct IRComponent** children;
    size_t child_count;
    // ... properties map for arbitrary data
} IRComponent;
```

### Styling System

**Location**: `ir/ir_style_builder.c`

Styles use CSS-like properties:

```c
IRComponent* button = ir_create_component(IR_COMPONENT_BUTTON);
ir_set_width(button, 200);
ir_set_height(button, 40);
ir_set_background_color(button, 0x3B82F6);  // Blue
ir_set_border_radius(button, 8);
```

**Supported Properties**:
- Layout: `width`, `height`, `padding`, `margin`
- Visual: `background_color`, `color`, `border`, `opacity`
- Typography: `font_size`, `font_weight`, `text_alignment`
- Transform: `rotate`, `scale`, `translate`

### Layout System

**Location**: `ir/layout/`

Layout strategies implement flexbox-like behavior:

- `strategy_container.c`: Flex container (row/column)
- `strategy_row.c`: Horizontal layout
- `strategy_column.c`: Vertical layout
- `strategy_text.c`: Text measurement and wrapping
- `strategy_image.c`: Image sizing and positioning
- `strategy_canvas.c`: Canvas drawing primitives

**Layout Process**:
1. Measure: Calculate preferred sizes
2. Arrange: Position children within available space
3. Align: Apply alignment constraints

### Event Handling

**Location**: `ir/ir_event_builder.c`

Events are captured as logic blocks in KIR:

```json
{
  "events": {
    "onClick": [
      {"type": "setState", "state": "count", "value": "count + 1"}
    ]
  }
}
```

**Universal Logic Executor** (`ir_executor.c`):
- Executes simple patterns (arithmetic, state updates)
- Platform-specific handlers for complex behavior

## Data Flow

### End-to-End Flow

```
1. User writes .tsx file
   └─> Contains JSX components and React hooks

2. TSX Parser (tsx_to_kir.ts)
   └─> Parses TSX, extracts components
   └─> Generates KIR JSON

3. KIR Serialization (ir_serialization.c)
   └─> Validates KIR structure
   └─> Builds IR component tree

4. Code Generator (e.g., tsx_codegen.c)
   └─> Traverses IR tree
   └─> Generates target TSX code

5. Target Execution
   └─> React renders the TSX
   └─> User interacts with UI

6. Runtime Backend (optional)
   └─> Desktop runtime intercepts events
   └─> Executes via SDL3/Raylib/terminal
```

### Hot Reload Flow

```
1. File watcher detects .tsx change
   └─> cli/src/utils/file_watcher.c

2. Re-compile to KIR
   └─> tsx_to_kir.ts

3. Hot reload system updates component tree
   └─> ir/ir_hot_reload.c

4. Runtime re-renders without restart
   └─> runtime/desktop/
```

## Module Responsibilities

### CLI (`cli/`)

**Purpose**: Command-line interface for developers

**Key Commands**:
- `kryon compile <source>`: Source → KIR
- `kryon codegen <target> <kir>`: KIR → Target code
- `kryon run <kir>`: Execute KIR on desktop runtime
- `kryon inspect <kir>`: View KIR structure
- `kryon diff <kir1> <kir2>`: Compare KIR files

**Key Files**:
- `cli/src/commands/cmd_compile.c`: Compile command
- `cli/src/commands/cmd_codegen.c`: Codegen command
- `cli/src/commands/cmd_run.c`: Run command (desktop runtime)
- `cli/src/utils/file_watcher.c`: Hot reload file watcher
- `cli/src/utils/ws_reload.c`: WebSocket live reload

### IR Library (`ir/`)

**Purpose**: Core IR data structures and utilities

**Key Modules**:
- **Component Tree**: `ir_core.h`, `ir_component_factory.c`
- **Serialization**: `ir_serialization.c` (KIR JSON parsing)
- **Logic**: `ir_logic.h`, `ir_executor.c` (event handlers)
- **Builders**: `ir_builder.c`, `ir_style_builder.c`, `ir_layout_builder.c`
- **State**: `ir_state_manager.c` (reactive state)
- **Hot Reload**: `ir_hot_reload.c`

### Code Generators (`codegens/`)

**Purpose**: Convert KIR to platform-specific code

**Shared Utilities** (`codegens/codegen_common.c`):
- Component tree traversal
- Style property generation
- Layout code generation

**Individual Generators**:
- `codegens/tsx/`: React TSX generation
- `codegens/c/`: C with Kryon library
- `codegens/lua/`: Lua with Kryon library
- `codegens/kotlin/`: Kotlin/Jetpack Compose
- `codegens/web/`: HTML/CSS/JS
- `codegens/markdown/`: Markdown documentation

### Runtime Backends (`runtime/`)

**Purpose**: Execute Kryon applications on specific platforms

**Desktop Backend** (`runtime/desktop/`):
- `desktop_renderer.c`: Main renderer
- `desktop_input.c`: Event handling
- `desktop_platform.c`: Platform abstraction
- `ir_to_commands.c`: IR → Command buffer translation

**Android Backend** (`runtime/android/`):
- JNI bridge to Java/Kotlin
- Android lifecycle integration
- Touch event handling

**Plan 9 Backend** (`runtime/plan9/`):
- Plan 9 graphics integration
- Plan 9 event loop

### Rendering Systems (`renderers/`)

**Purpose**: Low-level rendering primitives

**Common** (`renderers/common/`):
- Shared rendering utilities
- Command buffer structures
- Drawing primitives

**SDL3** (`renderers/sdl3/`):
- SDL3 rendering backend
- Hardware acceleration

**Raylib** (`renderers/raylib/`):
- Raylib rendering backend
- 3D rendering support

**Terminal** (`renderers/terminal/`):
- ASCII/Unicode rendering
- Terminal event handling

### Bindings (`bindings/`)

**Purpose**: Language-specific APIs for using Kryon

**C Binding** (`bindings/c/`):
- `kryon.h`: C API
- `kryon_dsl.h`: C macros for component building
- Static/dynamic library

**TypeScript Binding** (`bindings/typescript/`):
- Bun FFI wrapper
- TypeScript type definitions

**Lua Binding** (`bindings/lua/`):
- Lua API
- Lua module loader

## Design Decisions

### Why JSON for KIR?

1. **Human-readable**: Easy to debug and inspect
2. **Language-agnostic**: Every language can parse JSON
3. **Schema-validatable**: Can validate against JSON schema
4. **Diffable**: Standard diff tools work

### Why Separate Runtimes?

1. **Native Performance**: Each runtime is optimized for its platform
2. **Platform Integration**: Deep integration with platform APIs
3. **Independent Evolution**: Runtimes can evolve independently

### Why C for Core?

1. **Portability**: C runs everywhere
2. **Performance**: Zero-cost abstractions
3. **Embeddability**: Easy to embed in other languages
4. **Stable ABI**: C ABI is stable across compilers

### Why Multiple Languages?

1. **Developer Choice**: Use the language you know
2. **Ecosystem Access**: Leverage existing ecosystems (React, Lua, Kotlin)
3. **Incremental Adoption**: Adopt Kryon without rewriting everything

## Dependency Graph

```
┌─────────────────────────────────────────────────────────┐
│                      CLI Tool                           │
│                   (cli/src/main.c)                      │
└──────────────┬──────────────────────────────────────────┘
               │
       ┌───────┴────────┐
       │                │
┌──────▼──────┐  ┌──────▼──────┐
│ IR Library  │  │ Codegens    │
│  (ir/*.c)   │  │(codegens/*) │
└──────┬──────┘  └──────┬──────┘
       │                │
       │         ┌──────┴────────┐
       │         │               │
┌──────▼──────┐ │   ┌───────────▼──┐
│  Parsers    │ │   │ Renderers    │
│(ir/parsers) │ │   │(renderers/*) │
└─────────────┘ │   └───────┬──────┘
               │           │
               │   ┌───────┴───────┐
               │   │               │
         ┌─────▼───▼────┐    ┌────▼────┐
         │   Runtimes   │    │ Bindings│
         │ (runtime/*)  │    │(bindings)│
         └──────────────┘    └─────────┘
```

## Build System

**Location**: `Makefile` (root)

**Key Targets**:
- `make all`: Build CLI and library
- `make build-desktop`: Desktop backend (SDL3)
- `make build-terminal`: Terminal backend only
- `make build-web`: Web codegen only
- `make test`: Run test suite
- `make install`: Install to `~/.local`

**Module Makefiles**:
- `cli/Makefile`: CLI build configuration
- `ir/Makefile`: IR library build
- `codegens/*/Makefile`: Individual codegen builds
- `runtime/desktop/Makefile`: Desktop backend build

## Testing

**Location**: `tests/`

**Test Structure**:
- `tests/unit/ir/`: IR unit tests
- `tests/unit/codegens/`: Codegen unit tests
- `tests/integration/parsers/`: Parser integration tests
- `tests/integration/serialization/`: Serialization tests
- `tests/functional/interactive/`: Interactive tests

**Running Tests**:
```bash
make test                           # Run all tests
make test-serialization             # Run serialization tests
make test-modular                   # Run modular tests
./tests/unit/ir/test_expression_parser  # Run specific test
```

## Performance Considerations

### Compilation Performance

- **Parser Caching**: TSX parser caches AST nodes
- **Lazy Evaluation**: Expression evaluation is deferred
- **Incremental Compilation**: Only changed components are recompiled

### Runtime Performance

- **Zero-Copy**: Component tree uses pointers, no copying
- **Dirty Tracking**: Only changed components are re-rendered
- **Command Buffers**: Rendering commands are batched
- **Hardware Acceleration**: SDL3/Raylib use GPU when available

## Future Architecture

### Planned Enhancements

1. **WebAssembly Runtime**: Compile KIR to WebAssembly for web
2. **Plugin System**: Dynamic component loading
3. **Hot Swapping**: Replace components at runtime without reload
4. **Visual Editor**: Drag-and-drop UI builder
5. **Multi-threading**: Parallel layout and rendering

### Extensibility Points

1. **Custom Components**: User-defined component types
2. **Custom Layouts**: User-defined layout strategies
3. **Custom Renderers**: User-defined rendering backends
4. **Custom Parsers**: User-defined source languages

## Conclusion

Kryon's architecture enables true cross-platform UI development through:

1. **Clean Separation**: Source, IR, and runtime are independent
2. **Modular Design**: Each module has a single responsibility
3. **Extensibility**: Easy to add new languages, platforms, and renderers
4. **Performance**: Zero-cost abstractions and native execution

The three-stage pipeline (Source → KIR → Target → Runtime) is the key innovation that makes Kryon universal yet performant.

---

**Last Updated**: January 18, 2026
**Kryon Version**: 1.0.0
