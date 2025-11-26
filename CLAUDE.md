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
/ir/                   - IR core (C)
  ir_builder.c         - Component creation, tree management
  ir_layout.c          - Flexbox layout computation
  ir_events.c          - Event system
  kryon.h             - Main header with all IR types

/backends/desktop/     - Desktop backends (C)
  sdl_backend.c        - SDL3 rendering
  debug_backend.c      - Text-based debug renderer

/bindings/nim/         - Nim language bindings
  runtime.nim          - Main runtime, event handlers, tab groups
  reactive_system.nim  - Reactive state management
  kryon_dsl/impl.nim   - DSL macros (Container, Button, etc.)

/renderers/            - Platform-specific renderers
  sdl3/               - SDL3 graphics renderer
  terminal/           - Terminal/console renderer

/cli/                  - CLI tool for running Kryon apps
/examples/nim/         - Example Nim applications
```

## Build Commands

```bash
# Build the main library
make build

# Build and install CLI
make install

# Run examples
./run_example.sh <example_name>

# Build with debug info
make build-debug
```

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

## Debugging

### Debug Renderer
Use the debug backend to see IR tree structure with all properties:
```nim
# Print tree to stdout
debug_print_tree(root)

# Print tree to file
debug_print_tree_to_file(root, "/tmp/tree_debug.txt")
```

The debug output includes:
- Component type and ID
- Text content (truncated)
- Rendered bounds [x, y, width, height]
- Size (width × height dimensions)
- Background color (if set)
- Padding and margin
- Gap between children
- Alignment (justify, align)
- Events (if any handlers attached)
- Visibility and z-index
- Child count

### Environment Variables
- `KRYON_TRACE_LAYOUT=1` - Trace layout calculations
- `KRYON_TRACE_COMPONENTS=1` - Trace component creation
- `KRYON_TRACE_ZINDEX=1` - Trace z-index sorting
- `KRYON_RENDERER=terminal` - Use terminal renderer

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
