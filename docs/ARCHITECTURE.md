# Kryon Architecture

## Overview

Kryon is a declarative UI framework for Nim with a modular backend system that separates windowing from rendering.

## Directory Structure

```
kryon/
├── src/
│   ├── kryon/              # Core framework
│   │   ├── core.nim        # Element types, DSL macros
│   │   ├── fonts.nim       # Font management
│   │   ├── components/     # Component extraction
│   │   ├── layout/         # Layout engine
│   │   ├── rendering/      # Rendering context
│   │   └── state/          # State management
│   │
│   ├── backends/           # Backend implementations
│   │   ├── windowing/      # Platform/windowing systems
│   │   │   ├── sdl2/
│   │   │   └── raylib/
│   │   ├── rendering/      # Rendering engines
│   │   │   └── skia/
│   │   └── integration/    # Complete backend integrations
│   │       ├── raylib.nim
│   │       ├── sdl2.nim
│   │       ├── sdl2_skia.nim
│   │       └── html.nim
│   │
│   └── cli/                # CLI tool
│       └── kryon.nim
│
├── examples/               # Example applications
├── build/                  # Compiled binaries
└── docs/                   # Documentation
```

## Core Components

### 1. Element System (`src/kryon/core.nim`)

All UI components are represented as `Element` objects:

```nim
type
  Element* = ref object
    kind*: ElementKind
    props*: Table[string, Value]
    children*: seq[Element]
    x*, y*, width*, height*: float
    eventHandlers*: Table[string, EventHandler]
```

### 2. DSL Macro (`src/kryon/core.nim`)

The `kryonApp` macro transforms declarative syntax into element trees:

```nim
let app = kryonApp:
  Body:
    Text:
      text = "Hello"
```

### 3. Layout Engine (`src/kryon/layout/`)

- **layoutEngine.nim** - Calculates positions and sizes for all elements
- **zindexSort.nim** - Sorts elements by z-index for proper rendering order

Supports:
- Column/Row/Center layouts
- Flexbox-style alignment
- Gap spacing
- Absolute positioning

### 4. Component Extraction (`src/kryon/components/`)

Extracts component data into pure data structures before rendering:
- `button.nim`, `text.nim`, `input.nim`, `checkbox.nim`, `dropdown.nim`
- `container.nim`, `containers.nim`, `tabs.nim`

This allows 90%+ code reuse across backends.

### 5. Backend System (`src/backends/`)

#### Windowing Layer
- **`windowing/raylib/raylib_ffi.nim`** - Raylib FFI bindings
- **`windowing/sdl2/sdl2_ffi.nim`** - SDL2 FFI bindings

#### Rendering Layer
- **`rendering/skia/skia_ffi.nim`** - Skia FFI bindings

#### Integration Layer
- **`integration/raylib.nim`** - Raylib windowing + Raylib rendering
- **`integration/sdl2.nim`** - SDL2 windowing + SDL2 rendering
- **`integration/sdl2_skia.nim`** - SDL2 windowing + Skia rendering
- **`integration/html.nim`** - Browser-based rendering

All backends implement 7 core primitives:
1. `drawRectangle()` - Filled rectangle
2. `drawRectangleBorder()` - Rectangle border
3. `drawText()` - Text rendering
4. `drawLine()` - Line drawing
5. `beginClipping()` - Start clipping region
6. `endClipping()` - End clipping region
7. `measureText()` - Text measurement

### 6. State Management (`src/kryon/state/`)

- **backendState.nim** - Shared state across backends (focus, input values, etc.)
- **inputManager.nim** - Input field state
- **checkboxManager.nim** - Checkbox state
- **dropdownManager.nim** - Dropdown state
- **reactivity.nim** - Reactive value tracking

### 7. CLI Tool (`src/cli/kryon.nim`)

Commands:
- `run` - Compile and run application
- `build` - Compile to executable
- `info` - Show file information
- `version` - Show version

## Data Flow

```
User Code (DSL)
    ↓
kryonApp macro
    ↓
Element Tree
    ↓
Layout Engine → calculates x, y, width, height
    ↓
Component Extraction → extracts rendering data
    ↓
Backend Primitives → draws to screen
    ↓
Event Handling → updates state
    ↓
(loop back to Layout Engine on state change)
```

## Backend Architecture

### Separation of Concerns

**Windowing** handles:
- Window creation and management
- Event loop
- Input events (mouse, keyboard)
- Cross-platform window APIs

**Rendering** handles:
- Drawing primitives
- Text rendering
- Graphics quality
- Performance characteristics

**Integration** combines:
- Specific windowing system
- Specific rendering engine
- Complete event handling
- Application lifecycle

### Adding a New Backend

To add a new backend (e.g., GLFW + Cairo):

1. Add FFI bindings in `windowing/glfw/` or `rendering/cairo/`
2. Create `integration/glfw_cairo.nim`
3. Implement 7 renderer primitives
4. Update `cli/kryon.nim` to support new backend
5. Test with examples

## Performance Characteristics

- **Layout calculation**: O(n) where n = number of elements
- **Rendering**: O(n) with z-index sorting O(n log n)
- **Event handling**: O(d) where d = tree depth
- **Memory**: ~100-200 bytes per element

## Extension Points

1. **Custom Components**: Create new element kinds
2. **Custom Layouts**: Extend layout engine
3. **Custom Backends**: Add new windowing/rendering combinations
4. **Custom Event Handlers**: Add new event types
5. **Custom State**: Extend state management

## Dependencies

- **Nim** 2.0+
- **cligen** (CLI argument parsing)
- **Backend-specific**:
  - Raylib: `raylib` library
  - SDL2: `libSDL2`, `libSDL2_ttf`
  - Skia: `libskia`, `libSDL2`
