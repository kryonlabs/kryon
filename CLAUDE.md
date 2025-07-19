# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with the Kryon Renderer project.

## Project Overview

The Kryon Renderer is a multi-backend rendering engine for KRB (Kryon Binary) files, supporting desktop (WGPU), terminal (Ratatui), and graphics (Raylib) backends. It provides a complete runtime system with layout engine, script integration, and DOM API for building cross-platform applications.

## Essential Commands

### Building
```bash
# Build all backends
cargo build --workspace

# Build specific backends
cargo build --no-default-features --features wgpu
cargo build --no-default-features --features ratatui  
cargo build --no-default-features --features raylib

# Release builds
cargo build --release --workspace
```

### Testing
```bash
# Core testing command (snapshot testing)
cargo test -p kryon-ratatui
cargo insta review              # Review visual differences

# Run all tests
cargo test

# Test specific backends
cargo test -p kryon-wgpu
cargo test -p kryon-raylib
```

### Running Applications
```bash
# Run with different backends
./target/debug/kryon-renderer-wgpu examples/hello_world.krb
./target/debug/kryon-renderer-ratatui examples/hello_world.krb
./target/debug/kryon-renderer-raylib examples/hello_world.krb

# Debug output
RUST_LOG=debug ./target/debug/kryon-renderer-raylib examples/tabbar_left_demo.krb
```

## Architecture Overview

### Workspace Structure
- **`kryon-core`**: Core types, KRB parser, element definitions, property system
- **`kryon-layout`**: Layout engine with flexbox and absolute positioning
- **`kryon-render`**: Rendering abstractions and command system
- **`kryon-runtime`**: App management, event system, script integration, DOM API
- **`kryon-wgpu`**: GPU rendering backend (high-performance desktop)
- **`kryon-ratatui`**: Terminal UI backend (text-based, snapshot testing)
- **`kryon-raylib`**: Simple 2D/3D graphics backend

### Key Features
- ✅ **Multi-Backend Rendering**: WGPU, Ratatui, Raylib support
- ✅ **Layout Engine**: Flex and absolute positioning with proper layout flags
- ✅ **DOM API**: Complete script integration with element manipulation
- ✅ **Event System**: Click, hover, focus events with script callbacks
- ✅ **Property Resolution**: Style application and element property management
- ✅ **SVG Rendering**: Native SVG support with resvg library and Arc<> caching
- ✅ **Transform System**: CSS-like transforms with DOM API integration
- ✅ **LayoutDimension System**: Native percentage support with type safety
- 🔴 **Animation System**: Planned for future implementation

## Core Testing Principle

This project's quality is maintained through **snapshot testing** using the `kryon-ratatui` backend as the "source of truth":

### Testing Workflow
1. **Run tests**: `cargo test -p kryon-ratatui`
2. **Review diffs**: `cargo insta review` (shows visual differences)
3. **Accept/reject**: Interactive tool to approve changes
4. **Goal**: Make the diff disappear by fixing code

This text-based approach provides deterministic visual verification without image comparison complexity.

## Script System & DOM API

### Core DOM API (✅ IMPLEMENTED)
The script system provides comprehensive element manipulation:

```lua
-- Element access and modification
local element = getElementById("my_element")
element:setText("New text")
element:setVisible(false)
element:setChecked(true)
element:setStyle("new_style_name")

-- Transform manipulation
element:setTransform({
    translateX = 100, translateY = 50, translateZ = 0,
    scaleX = 1.5, scaleY = 1.5, scaleZ = 1.0,
    rotateX = 0, rotateY = 0, rotateZ = 45
})
element:setTranslation(100, 50, 0)
element:setScale(1.5, 1.5, 1.0)
element:setRotation(45, "z")
element:resetTransform()

-- DOM traversal
local parent = element:getParent()
local children = element:getChildren()
local nextSibling = element:getNextSibling()

-- Query methods
local buttons = getElementsByTag("Button")
local styledElements = getElementsByClass("my_style")
local element = querySelector("#my_id")
```

### Implementation Details
- **Location**: `crates/kryon-runtime/src/script_system.rs`
- **Initialization**: `register_dom_functions()` called after `setup_bridge_functions()`
- **Languages**: Lua, JavaScript, Python, Wren support
- **Memory Model**: Element data cloned into script context

## Optimized Layout Engine

### Layout System (✅ FULLY OPTIMIZED)
- **OptimizedTaffyLayoutEngine**: Production-ready layout engine with caching
- **LayoutDimension System**: Native percentage support with type safety
- **Incremental Updates**: Invalidation regions for partial layout updates
- **Property Caching**: O(1) access with cache-friendly data structures
- **Performance Monitoring**: Built-in profiling and benchmarking tools

### Layout Architecture
- **Core**: `crates/kryon-layout/src/optimized_taffy_engine.rs`
- **Property Cache**: `crates/kryon-core/src/optimized_property_cache.rs`
- **Dimension Types**: `crates/kryon-core/src/layout_units.rs`
- **Performance Tools**: `crates/kryon-layout/src/performance.rs`

### Layout Flag Values
- `0x00`: Row layout (default)
- `0x01`: Column layout
- `0x02`: Absolute layout (critical for overlays)
- `0x04`: Center alignment
- `0x20`: Grow flag

### LayoutDimension Types
- **Pixels**: Fixed pixel values for precise positioning
- **Percentage**: Responsive sizing (0.0 to 1.0)
- **Auto**: Content-driven automatic sizing
- **MinPixels/MaxPixels**: Constraint-based sizing

### Performance Optimizations
- ✅ **Debug-free hot paths**: All eprintln! statements removed from layout computation
- ✅ **Node caching**: Taffy nodes reused across layout cycles
- ✅ **Incremental updates**: Only recompute changed layout subtrees
- ✅ **Property caching**: O(1) access to element properties
- ✅ **Layout diffing**: Minimal re-renders through result comparison

### Common Issues Fixed
- ✅ **Layout flag compilation**: Styles now correctly generate absolute positioning flags
- ✅ **Overlay positioning**: Content panels properly overlap instead of stacking
- ✅ **DOM API availability**: Functions available when scripts execute
- ✅ **Legacy system removal**: All Vec2 fields removed, LayoutDimension system unified

## Development Status

### Completed Features
- ✅ **KRB parsing**: Complete binary format support with debug output
- ✅ **Multi-backend rendering**: WGPU, Ratatui, Raylib implementations
- ✅ **Optimized layout engine**: Production-ready with caching and incremental updates
- ✅ **LayoutDimension system**: Native percentage support with type safety
- ✅ **Script integration**: Full DOM API with element manipulation
- ✅ **Event system**: Click handlers and state management
- ✅ **Property system**: Optimized style application and element property resolution
- ✅ **SVG rendering**: resvg integration with Arc<> caching for external files
- ✅ **Transform system**: CSS-like transforms with matrix operations and DOM API
- ✅ **Performance optimization**: Debug-free hot paths, property caching, layout diffing

### Pending Features
- 🟡 **Resource management**: Image and font loading system
- 🟡 **Layout diffing integration**: Minimal re-renders with renderer backends
- 🟡 **SVG animations**: Interactive SVG elements with script integration
- 🔴 **Animation system**: Transitions and keyframe animations
- 🔴 **Accessibility**: Screen reader and keyboard navigation support
- 🔴 **Spatial indexing**: Accelerated hit testing for large element counts

## Development Workflow

1. **Make changes** to renderer code
2. **Run tests**: `cargo test -p kryon-ratatui`
3. **Review snapshots**: `cargo insta review` for visual diffs
4. **Test backends**: Verify changes work across WGPU, Ratatui, Raylib
5. **Debug output**: Use debug builds to trace layout and rendering issues
6. **Update examples**: Ensure example KRB files work correctly

## Troubleshooting

### Common Issues
- **Layout problems**: Check layout flags in debug output (`[STYLE_LAYOUT]`)
- **Script errors**: Verify DOM API initialization order in runtime
- **Rendering issues**: Compare behavior across different backends
- **Property resolution**: Check style application in KRB parser debug output

### Debug Commands
```bash
# Enable debug logging
RUST_LOG=debug ./target/debug/kryon-renderer-raylib file.krb

# Test specific layout scenarios
cargo test -p kryon-ratatui test_layout
cargo insta review

# Check KRB file structure
cargo run --bin debug_krb file.krb
```

Focus on making tests pass, particularly the snapshot tests which provide the definitive measure of rendering correctness.