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

### IR Pipeline (Universal)
All frontends go through the same pipeline:
```
.kry  → .kir (JSON) → renderer
.nim  → .kir (JSON) → renderer
.kirb → renderer (binary, pre-compiled)
```

- **`.kry`** - Simple declarative syntax, parsed by `kry_parser.nim`
- **`.nim`** - Nim DSL with reactive state, compiled to binary that serializes IR
- **`.kir`** - JSON IR format (human-readable, named colors supported)
- **`.kirb`** - Binary IR format (5-10× smaller, optimized)

The CLI uses `renderIRFile()` helper (cli/main.nim:128) for unified rendering across all paths.

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
  kry_lexer.nim           - .kry tokenizer
  kry_parser.nim          - .kry parser (produces AST)
  kry_to_kir.nim          - .kry AST to .kir JSON transpiler
  kry_ast.nim             - .kry AST types

/examples/kry/            - .kry example applications (SOURCE OF TRUTH)
  hello_world.kry         - Simple hello world
  button_demo.kry         - Interactive buttons
  animations_demo.kry     - Animations and transitions
  tabs_reorderable.kry    - Reorderable tabs demo
  ... (14 total examples)

/examples/nim/            - Generated Nim examples (NOT IN GIT)
  (Auto-generated from .kry files via scripts/generate_examples.sh)
  button_demo.nim         - Generated from button_demo.kry
  animations_demo.nim     - Generated from animations_demo.kry
  ... (regenerate with: make generate-examples)

/build/                   - Build artifacts
  libkryon_ir.a           - IR core library (static)
  libkryon_ir.so          - IR core library (shared)
  libkryon_desktop.a      - Desktop backend library
  libkryon_web.a          - Web backend library

/bin/cli/kryon            - CLI executable (after nimble build)

/docs/                    - Documentation
  KIR_FORMAT_V2.md        - Binary IR format specification
  DEVELOPER_GUIDE.md      - Developer documentation

/scripts/                 - Build and generation scripts
  generate_examples.sh    - Generate/validate all examples from .kry
  compare_kir.sh          - Semantic .kir comparison
  normalize_kir.jq        - JSON normalization filter
```

## Examples Workflow

**IMPORTANT:** Only `.kry` files in `examples/kry/` are checked into git. All other formats (`.nim`, `.lua`, etc.) are auto-generated on demand.

### Working with Examples

**Do NOT edit** files in `examples/nim/` - they are generated and will be overwritten!

**DO edit** files in `examples/kry/` - these are the source of truth.

### Generating Examples

```bash
# Generate all examples from .kry sources
make generate-examples
# Or directly:
./scripts/generate_examples.sh

# Generate specific example
./scripts/generate_examples.sh button_demo

# Validate round-trip transpilation (must be 100%)
make validate-examples
# Or directly:
./scripts/generate_examples.sh --validate

# Clean generated files
make clean-generated
```

### How Auto-Generation Works

The examples pipeline ensures perfect round-trip transpilation:

1. **`.kry → .kir`** - Parse .kry file to JSON IR (with `--preserve-static` for codegen)
2. **`.kir → .nim`** - Generate idiomatic Nim DSL code from IR
3. **`.nim → .kir`** - Compile generated Nim back to IR (round-trip test)
4. **Validation** - Compare original and round-trip `.kir` files semantically

If validation fails, it indicates a transpilation bug that must be fixed.

### Auto-Generation on Run

The `run_example.sh` script automatically generates missing `.nim` files:

```bash
# If button_demo.nim doesn't exist, it's generated from button_demo.kry
./run_example.sh button_demo
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
# Compile Nim/Lua/C to Kryon IR
kryon compile examples/nim/button_demo.nim  # Outputs .kir (JSON)
kryon compile app.nim --format=binary       # Outputs .kirb (binary)
kryon compile app.nim --validate            # Compile and validate
kryon compile app.nim --no-cache            # Force recompilation

# Convert .kir (JSON) to .kirb (binary)
kryon convert app.kir app.kirb     # Manual conversion
# Binary format is 5-10× smaller and optimized for backends

# Inspect IR files
kryon inspect-ir app.kir           # Quick metadata
kryon inspect-detailed app.kir      # Full analysis with stats
kryon inspect-detailed app.kir --tree  # Include tree visualization
kryon tree app.kir --max-depth=5    # Visual component tree

# Validate IR format (works with both .kir and .kirb)
kryon validate app.kir
kryon validate app.kirb

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

**Enhanced Options**:
```bash
# Basic tree
kryon tree <file>.kir

# Show dimensions and colors
kryon tree <file>.kir --visual

# Show only colors
kryon tree <file>.kir --with-colors

# Show only dimensions/positions
kryon tree <file>.kir --with-positions

# Limit depth
kryon tree <file>.kir --max-depth=5
```

Example output with `--visual`:
```
Column #1 (800.0px×600.0px) [bg:#2c2c2c] (1 children)
└── Column #2 (3 children)
    ├── Text #3 [color:#ffffff] "Reorderable Tabs Demo"
    └── TabGroup #5 (760.0px×400.0px) (2 children)
        ├── TabBar #6 [bg:#1a1a1a] (4 children)
        │   ├── Tab #7 [bg:#3d3d3d] [Home]
        │   ├── Tab #8 [bg:#3d3d3d] [Profile]
```

The `kryon inspect-detailed` command provides:
- Total component count
- Component type distribution
- Tree depth statistics
- Text content analysis
- Style usage tracking
- Reactive state information
- Warnings (large text, excessive children, deep nesting)

### Visual Debugging with Screenshots

**IMPORTANT**: When debugging UI rendering issues, ALWAYS use the visual debugging tools first. See `.claude/skills/kryon-visual-debug/` for detailed workflow.

#### Screenshot Capture
Capture the actual rendered output to see what's really happening:

```bash
# Basic screenshot (headless, auto-exit after 3 frames)
env LD_LIBRARY_PATH=build:$LD_LIBRARY_PATH \
    KRYON_SCREENSHOT=/tmp/debug.png \
    KRYON_SCREENSHOT_AFTER_FRAMES=3 \
    KRYON_HEADLESS=1 \
    timeout 2 ~/.local/bin/kryon run <file>
```

#### Debug Overlay
Show component boundaries, IDs, and dimensions:

```bash
# Screenshot with debug overlay
env LD_LIBRARY_PATH=build:$LD_LIBRARY_PATH \
    KRYON_SCREENSHOT=/tmp/debug_overlay.png \
    KRYON_DEBUG_OVERLAY=1 \
    KRYON_HEADLESS=1 \
    timeout 2 ~/.local/bin/kryon run <file>
```

Debug overlay color coding:
- **Blue**: Containers, Columns, Rows
- **Green**: Tab components (TabGroup, TabBar, Tab, etc.)
- **Yellow**: Text
- **Red**: Buttons
- **Orange**: Inputs
- **Purple**: Checkboxes
- **Cyan**: Dropdowns

#### Visual Debugging Environment Variables
- `KRYON_SCREENSHOT=/path/to/output.png` - Enable screenshot capture
- `KRYON_SCREENSHOT_AFTER_FRAMES=N` - Capture after N frames (default: 5)
- `KRYON_HEADLESS=1` - Run without window, exit after screenshot
- `KRYON_DEBUG_OVERLAY=1` - Show component boundaries and labels

#### Claude Code Skills for Visual Debugging

Three skills are available to automate visual debugging workflows:

1. **kryon-visual-debug** (`.claude/skills/kryon-visual-debug/`)
   - Auto-activates when user reports UI rendering issues
   - Takes screenshot BEFORE making code changes
   - Uses debug overlay to see component dimensions
   - Verifies fixes with after screenshots

2. **kryon-layout-inspector** (`.claude/skills/kryon-layout-inspector/`)
   - Deep-dives into layout calculations
   - Uses `KRYON_TRACE_LAYOUT=1` for detailed traces
   - Identifies flex, sizing, and positioning issues
   - Provides targeted fix recommendations

3. **kryon-screenshot-diff** (`.claude/skills/kryon-screenshot-diff/`)
   - Compares before/after screenshots
   - ALWAYS used after UI fixes to verify success
   - Catches regressions before user reports them
   - Builds confidence in claimed fixes

**Best Practice**: When user says "component not showing" or "layout is wrong", use the skills in order:
1. `kryon-visual-debug` → Take screenshot to SEE the problem
2. `kryon-layout-inspector` → Analyze layout if needed
3. Make fix
4. `kryon-screenshot-diff` → Verify fix worked

This approach reduces debugging time from hours to minutes and eliminates blind guessing.

### Performance and Freezing Debug Skill

**IMPORTANT**: For freezing, memory, or performance issues, do NOT use screenshots. Screenshots only capture visual state, not performance problems. Use these tools instead:

#### Quick Freeze Detection
```bash
# If exit code is 124, app froze
timeout 5 ./run_example.sh example_name
echo "Exit code: $?"
```

Exit codes:
- `124` = Timeout (app froze/hung)
- `137` = SIGKILL (force killed)
- `0` = Normal exit

#### Layout Trace (Most Common Issue)
```bash
# High line count = layout computed every frame = BUG
KRYON_TRACE_LAYOUT=1 timeout 2 ./run_example.sh example_name 2>&1 | wc -l
```

What to look for:
- Same layout trace repeating every frame = layout not cached
- Expensive operations (table layout, deep trees) running repeatedly

#### Memory Leak Detection
```bash
nix-shell --run "make -C ir asan-ubsan"
ASAN_OPTIONS=detect_leaks=1 ./run_example.sh example_name
```

#### Common Performance Issues and Fixes

1. **Layout Recomputed Every Frame**
   - Symptom: App slows down over time, eventually freezes
   - Fix: Add caching - only recompute when dimensions change

2. **Event Queue Starvation**
   - Symptom: ESC key and window close don't work
   - Fix: Cache expensive computations, handle ESC on KEY_DOWN

3. **Memory Leaks**
   - Symptom: Memory usage grows over time
   - Fix: Check component cleanup, handler tables, texture caches

**Key Rule**: When user reports "app freezing" or "can't exit", use timeout + trace, NOT screenshots.

### Debug Renderer (Legacy)

You can also use the debug backend directly in code:
```nim
# Print tree to stdout
debug_print_tree(root)

# Print tree to file
debug_print_tree_to_file(root, "/tmp/tree_debug.txt")
```

### Environment Variables

**Visual Debugging**:
- `KRYON_SCREENSHOT=/path/to/output.png` - Enable screenshot capture
- `KRYON_SCREENSHOT_AFTER_FRAMES=N` - Capture after N frames (default: 5)
- `KRYON_HEADLESS=1` - Run without window, exit after screenshot
- `KRYON_DEBUG_OVERLAY=1` - Show component boundaries and labels

**Trace Debugging**:
- `KRYON_TRACE_LAYOUT=1` - Trace layout calculations
- `KRYON_TRACE_COMPONENTS=1` - Trace component creation
- `KRYON_TRACE_ZINDEX=1` - Trace z-index sorting
- `KRYON_TRACE_POLYGON=1` - Trace polygon rendering
- `KRYON_TRACE_SCROLLBAR=1` - Trace scrollbar updates
- `KRYON_TRACE_TABS=1` - Trace tab group state

**Renderer Selection**:
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

### TabBar Components Not Laying Out Horizontally (FIXED)
**Root Cause**: Desktop renderer didn't include `IR_COMPONENT_TAB_BAR` in row layout checks
**Symptoms**: Tabs stacked vertically (on top of each other) instead of horizontally side-by-side
**Fix Locations**:
- `backends/desktop/desktop_rendering.c:996-999` - Added `IR_COMPONENT_TAB_BAR` to `is_row_layout` check
- `backends/desktop/desktop_rendering.c:1202` - Added TabBar to space distribution
- `backends/desktop/desktop_rendering.c:1429` - Added TabBar to cross-axis alignment
- `backends/desktop/desktop_rendering.c:1478` - Added TabBar to flex_grow handling
- `backends/desktop/desktop_rendering.c:1617` - Added TabBar to horizontal stacking (X-position advancement)
**Also Fixed**:
- `cli/kry_to_kir.nim:443-445` - Transpiler adds `flexDirection: "row"` for TabBar
- `ir/ir_json_v2.c:1920-1929` - Prevent default `flex.direction` from overwriting JSON values
**Testing**: Use `./test_visual.sh tabs_reorderable` to verify tabs render horizontally

### TabGroup Opens on Wrong Tab (.kir Files) (FIXED)
**Root Cause**: When loading `.kir` files from disk, TabGroups are deserialized but never have their `TabGroupState` runtime state created or finalized
**Symptoms**:
- Applications open on the last tab (e.g., "About") instead of the first tab ("Home")
- Only affects `.kir` files loaded via `ir_read_json_v2_file()`
- Nim DSL apps work correctly because they explicitly create and finalize TabGroupState
**Fix Location**: `ir/ir_json_v2.c:2095-2171`
**Solution**: Added `ir_finalize_tabgroups_recursive()` function that:
1. Finds TabGroup components in the deserialized tree
2. Creates `TabGroupState` via `ir_tabgroup_create_state()` if not already present
3. Registers all Tab components from TabBar children via `ir_tabgroup_register_tab()`
4. Registers all TabPanel components from TabContent children via `ir_tabgroup_register_panel()`
5. Calls `ir_tabgroup_finalize()` to set initial panel visibility based on `selectedIndex`
**Key Insight**: `IRTabData` (simple metadata) is populated during deserialization, but `TabGroupState` (runtime state in `custom_data`) must be created explicitly
**Testing**: Screenshot verification shows "Welcome to the Home Panel!" content instead of "About This App"

## Testing

```bash
# Build and run an example (ALWAYS use this for testing!)
./run_example.sh <example_name>

# Examples:
./run_example.sh tabs_reorderable
./run_example.sh habits

# Run with tracing (warning: generates huge output, use with timeout)
KRYON_TRACE_LAYOUT=1 timeout 1 ./run_example.sh <example_name>

# Use terminal renderer (faster for testing, auto-exits)
KRYON_RENDERER=terminal timeout 1 ./run_example.sh <example_name>
```

**IMPORTANT**: Always use `./run_example.sh` for testing, not `kryon run` directly. The script ensures proper compilation and handles the full pipeline (.kry → .kir → execution).

### Visual Testing with Screenshots

**CRITICAL**: Before claiming a visual issue is fixed, ALWAYS capture a screenshot to verify!

```bash
# Quick visual test with screenshot
./test_visual.sh <example_name>

# Custom output path
./test_visual.sh tabs_reorderable /tmp/my_test.png
```

This script:
- Kills existing kryon processes
- Runs with screenshot capture enabled
- Uses debug overlay to show component boundaries
- Saves to `/tmp/kryon_test_<example>.png` by default
- Verifies screenshot was actually created
