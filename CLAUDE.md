# Kryon UI Framework - Claude Code Guide

## Project Overview

Kryon is a cross-platform UI framework with an intermediate representation (IR) layer that compiles to multiple backends (SDL3, terminal, web). The Nim bindings provide a high-level DSL for building reactive UIs.

## Key Architecture Concepts

### IR (Intermediate Representation)
- Components are represented as `IRComponent` structs in C
- IR layer handles layout computation, event routing, and tree management
- Multiple renderers can consume the same IR tree

### Reactive System
- `ReactiveState[T]` - observable values that trigger UI updates
- `ReactiveConditional` - conditional components that show/hide based on state
- `ReactiveTextExpression` - text that updates when reactive values change

### C-Nim Bridge
- C functions are exposed via `{.importc, dynlib, header.}` pragmas
- Nim callbacks are registered with C using `{.exportc, cdecl, dynlib.}` pragmas
- Weak extern declarations allow optional callbacks in C

## Directory Structure

```
/ir/                      - IR core library (C)
  ir_builder.c            - Component creation, tree management
  ir_builder.h            - Builder API
  ir_layout.c             - Flexbox layout computation
  ir_serialization.c      - Binary IR format v2.0 (.kir files)
  ir_serialization.h      - Serialization API
  ir_validation.c         - 3-tier validation (format, structure, semantic)
  ir_reactive_manifest.c  - Reactive state preservation for hot reload
  ir_hot_reload.c         - Hot reload support
  ir_text_shaping.c       - Text shaping with HarfBuzz
  ir_core.h               - Core types and structures
  Makefile                - Builds libkryon_ir.a

/backends/desktop/        - Desktop backend (C)
  ir_desktop_renderer.c   - SDL3/Terminal rendering
  desktop_layout.c        - Layout computation
  desktop_rendering.c     - Rendering primitives
  desktop_fonts.c         - Font management
  desktop_input.c         - Input handling
  desktop_markdown.c      - Markdown support
  Makefile                - Builds libkryon_desktop.a

/backends/web/            - Web backend (future)
  css_generator.c         - CSS generation from IR

/bindings/nim/            - Nim language bindings
  ir_core.nim             - Core IR bindings
  ir_desktop.nim          - Desktop backend bindings
  ir_serialization.nim    - Serialization bindings
  runtime.nim             - Runtime, event handlers, tab groups
  reactive_system.nim     - Reactive state management
  kryon_dsl/              - DSL implementation
    impl.nim              - Main DSL macros (Container, Button, etc.)
    components.nim        - Component definitions
    core.nim              - Core DSL functionality
    layout.nim            - Layout properties
    properties.nim        - Style properties
    reactive.nim          - Reactive helpers
    animations.nim        - Animation DSL

/bindings/lua/            - Lua language bindings (in progress)
  kryon/                  - Lua module structure

/cli/                     - Kryon CLI tool
  main.nim                - CLI entry point and command router
  compile.nim             - Compilation pipeline with caching
  inspect.nim             - IR inspection and tree visualization
  diff.nim                - IR diff tool
  config.nim              - Configuration (kryon.toml/kryon.json)
  project.nim             - Project scaffolding
  build.nim               - Build system
  device.nim              - Device deployment

/examples/nim/            - Nim example applications
  button_demo.nim         - Interactive buttons
  animations_demo.nim     - Animations and transitions
  bidi_demo.nim           - BiDi text (Hebrew, Arabic)
  markdown_simple_test.nim - Markdown rendering
  tabs_demo.nim           - Tab groups
  text_shaping_demo.nim   - Text shaping examples
  ...

/build/                   - Build artifacts
  libkryon_ir.a           - IR core library (static)
  libkryon_ir.so          - IR core library (shared)
  libkryon_desktop.a      - Desktop backend library
  libkryon_web.a          - Web backend library

/bin/cli/kryon            - CLI executable (after nimble build)

/docs/                    - Documentation
  KIR_FORMAT_V2.md        - Binary IR format specification
  DEVELOPER_GUIDE.md      - Developer documentation
```

## Build Commands

```bash
# Build the IR libraries and desktop backend
make build

# Build and install CLI
make install
nimble build  # Or directly build CLI

# Run examples with run_example.sh
./run_example.sh <example_name>  # Uses SDL3 renderer by default
./run_example.sh <example_name> nim terminal  # Use terminal renderer

# Build with debug info
make build-debug
```

## CLI Tool (kryon)

The Kryon CLI provides professional tooling for IR compilation, inspection, and project management:

```bash
# Compile Nim/Lua/C to Kryon IR (.kir files)
kryon compile examples/nim/button_demo.nim
kryon compile app.nim --validate  # Compile and validate
kryon compile app.nim --no-cache  # Force recompilation

# Inspect IR files
kryon inspect-ir app.kir           # Quick metadata
kryon inspect-detailed app.kir      # Full analysis with stats
kryon inspect-detailed app.kir --tree  # Include tree visualization
kryon tree app.kir --max-depth=5    # Visual component tree

# Validate IR format
kryon validate app.kir

# Compare IR files
kryon diff old.kir new.kir

# Project management
kryon new <name>                    # Create project (kryon.json)
kryon init <name> --template=nim    # Create project (kryon.toml)
kryon config                        # Show config summary
kryon config show                   # Full JSON output
kryon config validate               # Validate configuration

# Run applications
kryon run app.nim                   # Compile and run
kryon dev app.nim                   # Hot reload mode
```

### IR Compilation Cache

The CLI uses hash-based caching in `.kryon_cache/`:
- `index.json` - Cache metadata
- `<hash>.kir` - Cached IR files
- Cache invalidation on source changes
- Instant recompilation on cache hit

## Common Patterns

### Creating Components (DSL)
```nim
Container:
  width = 100.percent
  height = auto
  layoutDirection = 1  # Column
  Button:
    title = "Click me"
    onClick = proc() = echo "Clicked!"
```

### Reactive State
```nim
var counter = initReactiveState(0)
Text:
  content = fmt"Count: {counter.value}"  # Updates automatically
Button:
  onClick = proc() = counter.value += 1
```

### Tab Groups
```nim
TabGroup:
  TabBar:
    Tab:
      title = "Tab 1"
    Tab:
      title = "Tab 2"
  TabContent:
    TabPanel:
      Text: content = "Panel 1"
    TabPanel:
      Text: content = "Panel 2"
```

## Key Files for Development

### C Core (`/ir/`)
- `ir_builder.c` - Creating/managing IR components
- `ir_layout.c` - Flexbox layout algorithm
- `ir_events.c` - Event handling and routing
- `kryon.h` - All type definitions

### Nim Bindings (`/bindings/nim/`)
- `runtime.nim` - Button/checkbox handlers, tab groups, C bridge
- `reactive_system.nim` - Reactive state, conditionals, text expressions
- `kryon_dsl/impl.nim` - DSL macros that transform to runtime calls

## Important Implementation Details

### Loop Variable Capture in DSL
When using for-loops inside components, the loop variable is captured by value using `transformLoopVariableReferences()`. This ensures closures (like onClick handlers) capture the correct value.

### Reactive Conditional Suspension
When tab panels are hidden, their `ReactiveConditional` objects are suspended (not re-initialized). This preserves state across tab switches.

### Handler Tables
Button and checkbox handlers are stored in global tables keyed by component ID. The `cleanupHandlersForSubtree()` function cleans up handlers when components are removed.

### Tab Group State
Tab groups track:
- `tabGroupTabOrder` - Order of Tab components
- `tabGroupPanelOrder` - Order of TabPanel components
- `tabGroupVisuals` - Visual state per tab
- `tabGroupStates` - Active tab index, panel visibility

## Debugging and Inspecting IR Output

### Using the CLI to Inspect IR

The recommended way to debug and inspect component trees is using the CLI tools:

```bash
# Run the example to generate Nim code
./run_example.sh bidi_demo

# Or use the CLI run command (when implemented)
kryon run examples/nim/bidi_demo.nim

# To inspect the generated IR:
# 1. Compile to .kir file
kryon compile examples/nim/bidi_demo.nim

# 2. Inspect the IR structure
kryon tree .kryon_cache/*.kir

# 3. Get detailed analysis
kryon inspect-detailed .kryon_cache/*.kir --tree

# 4. Validate the IR format
kryon validate .kryon_cache/*.kir
```

The `kryon tree` command shows:
- Component hierarchy with indentation
- Component types (CONTAINER, TEXT, etc.)
- Component IDs
- Text content (for Text components)
- Child count
- Tree depth

The `kryon inspect-detailed` command provides:
- Total component count
- Component type distribution
- Tree depth statistics
- Text content analysis
- Style usage tracking
- Reactive state information
- Warnings (large text, excessive children, deep nesting)

### Debug Renderer (Legacy)

You can also use the debug backend directly in code:
```nim
# Print tree to stdout
debug_print_tree(root)

# Print tree to file
debug_print_tree_to_file(root, "/tmp/tree_debug.txt")
```

### Environment Variables
- `KRYON_TRACE_LAYOUT=1` - Trace layout calculations
- `KRYON_TRACE_COMPONENTS=1` - Trace component creation
- `KRYON_TRACE_ZINDEX=1` - Trace z-index sorting
- `KRYON_TRACE_POLYGON=1` - Trace polygon rendering
- `KRYON_TRACE_SCROLLBAR=1` - Trace scrollbar updates
- `KRYON_TRACE_TABS=1` - Trace tab group state
- `KRYON_RENDERER=terminal` - Use terminal renderer
- `KRYON_RENDERER=sdl3` - Use SDL3 renderer (default)

## Common Issues and Solutions

### Tab Switching Crashes
**Cause**: Reactive conditionals being re-initialized instead of resumed
**Fix**: Use `suspended` field in ReactiveConditional/ReactiveTextExpression

### Handler Accumulation
**Cause**: Handlers added on every tab switch without cleanup
**Fix**: Check for existing handlers before creating events, clear tab group tables in finalizeTabGroup

### Wrong Values in Loop Closures
**Cause**: Loop variables captured by reference
**Fix**: Transform loop body to use captured copy via `transformLoopVariableReferences()`

## Testing

```bash
# Build and run an example
./run_example.sh habits

# Run with tracing
KRYON_TRACE_LAYOUT=1 ./run_example.sh habits

# Use terminal renderer
KRYON_RENDERER=terminal ./run_example.sh habits
```
