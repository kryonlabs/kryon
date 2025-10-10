# Kryon-Nim UI Framework

**Version**: 0.1.0
**Status**: Early Development (Week 1)

Kryon-Nim is a declarative UI framework built on Nim, leveraging Nim's powerful macro system to provide a clean, type-safe way to build cross-platform user interfaces.

---

## Quick Start

### Prerequisites

- Nim 2.0 or later
- Nimble package manager

### Installation

```bash
cd kryon-nim
nimble install
```

### Run Hello World

```bash
nim c -r examples/hello_world.nim
```

---

## Example

```nim
import kryon
import backends/raylib_backend

let app = kryonApp:
  Container:
    width: 400
    height: 300
    backgroundColor: "#191970FF"

    Center:
      Text:
        text: "Hello World from Kryon!"
        color: "#FFD700"

# Run the application
var backend = newRaylibBackend(400, 300, "My App")
backend.run(app)
```

---

## Features

### Current (Week 1)

- ✅ Core type system (Element, Property, Value)
- ✅ DSL macros for declarative UI
- ✅ Basic elements: Container, Text, Button, Column, Row, Center
- ✅ Property system with type safety
- ✅ Event handlers (onClick, etc.)
- ✅ Basic layout engine
- ✅ Simplified raylib backend (rendering simulation)
- ✅ CLI tool skeleton

### Planned

- [ ] Real raylib integration via FFI
- [ ] HTML/Web backend
- [ ] State management with reactivity
- [ ] Component system
- [ ] Styling system with themes
- [ ] Hot reload for development
- [ ] Advanced layout (Grid, Flexbox-like)
- [ ] Form elements (Input, Checkbox, Dropdown)
- [ ] Animation system

---

## Project Structure

```
kryon-nim/
├── src/
│   ├── kryon/
│   │   ├── core.nim           # Core types and utilities
│   │   └── dsl.nim            # DSL macro system
│   ├── backends/
│   │   └── raylib_backend.nim # Raylib rendering backend
│   ├── cli/
│   │   └── kryon.nim          # CLI tool
│   └── kryon.nim              # Main module (exports public API)
├── examples/
│   ├── hello_world.nim        # Simple hello world
│   ├── button_demo.nim        # Interactive button
│   └── counter.nim            # Stateful counter
├── tests/                     # (Coming soon)
├── docs/                      # (Coming soon)
├── kryon.nimble               # Package configuration
├── ROADMAP.md                 # Detailed implementation roadmap
└── README.md                  # This file
```

---

## Examples

### Hello World

```bash
nim c -r examples/hello_world.nim
```

### Button Demo

```bash
nim c -r examples/button_demo.nim
```

### Counter (State Management)

```bash
nim c -r examples/counter.nim
```

---

## Syntax Comparison

### Old Kryon (.kry)

```kry
Container {
    width = 200
    height = 100
    backgroundColor = "#191970FF"

    Text {
        text = "Hello"
        color = "yellow"
    }
}
```

### New Kryon-Nim (.nim)

```nim
Container:
  width: 200
  height: 100
  backgroundColor: "#191970FF"

  Text:
    text: "Hello"
    color: "yellow"
```

**Key changes**:
- `=` → `:` (Nim syntax)
- `{ }` → indentation (Nim style)
- Cleaner, more Pythonic syntax

---

## Development

### Running Tests

```bash
nimble test
```

### Building CLI Tool

```bash
nimble build
./bin/kryon run examples/hello_world.nim
```

### Generating Documentation

```bash
nimble docs
# Open docs/index.html
```

---

## Architecture

### DSL Macro System

Kryon uses Nim macros to transform declarative syntax into efficient Element trees at compile-time:

```
User Code (DSL)
     ↓
Nim Macros (compile-time transformation)
     ↓
Element Tree (runtime data structure)
     ↓
Backend Renderer (raylib, HTML, etc.)
     ↓
Screen Output
```

### Backends

Kryon supports multiple rendering backends:

- **Raylib** (native desktop) - Current focus
- **HTML/Web** (browser) - Planned
- **SDL2** (native desktop) - Future
- **Terminal** (text UI) - Future

---

## Roadmap

See [ROADMAP.md](ROADMAP.md) for detailed 16-week implementation plan.

### Phase 1: Foundation (Weeks 1-2) ✓ IN PROGRESS

- [x] Core type system
- [x] DSL macros
- [x] Basic elements
- [x] Simplified backend
- [ ] Complete layout engine
- [ ] Event handling integration

### Phase 2: Rendering (Weeks 3-4)

- [ ] Real raylib integration via FFI
- [ ] Complete element rendering
- [ ] Event loop with 60 FPS
- [ ] Advanced layout algorithms

### Phase 3: State & Events (Weeks 5-6)

- [ ] Reactive state management
- [ ] Full event system
- [ ] Component-local state

### Phase 4: Components & Styling (Weeks 7-8)

- [ ] Component composition system
- [ ] Styling with CSS-like properties
- [ ] Theme support

### Coming: Multi-backend, CLI, Polish (Weeks 9-16)

---

## Contributing

Kryon-Nim is in early development. Contributions welcome!

### Current Priorities

1. Real raylib FFI integration
2. Complete layout engine
3. Event handling
4. State management
5. More UI elements

---

## Why Nim?

Kryon was originally being built as a custom language (Klang) in C. After careful analysis, we chose to build on Nim instead:

**Benefits**:
- ✅ **6x faster development** (4 months vs 24 months)
- ✅ **Powerful macro system** (perfect for DSLs)
- ✅ **Mature infrastructure** (stdlib, tooling, compiler)
- ✅ **Multiple backends** (C, JavaScript, LLVM)
- ✅ **Excellent performance** (compiles to optimized C)
- ✅ **Lower risk** (proven technology)

See `../nim-analysis/` for detailed comparison.

---

## License

MIT License (TBD)

---

## Status

**Week 1 Complete** ✓

- Core infrastructure working
- DSL macros functional
- Basic examples running
- CLI tool skeleton ready

**Next**: Week 2 - Complete layout engine, real raylib integration

---

**Built with ❤️ using Nim**
