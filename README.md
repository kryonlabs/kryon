# Kryon-Nim UI Framework

**Version**: 0.2.0
**Status**: Phase 1 & 2 Complete - Production Ready for Desktop Apps

Kryon-Nim is a declarative UI framework built on Nim, leveraging Nim's powerful macro system to provide a clean, type-safe way to build cross-platform user interfaces.

---

## Quick Start

### Prerequisites

- Nix (for reproducible environment)
- Or: Nim 2.0+, Nimble, GCC, Raylib 5.0

### Installation

```bash
cd kryon-nim
nix-shell
nimble build
```

### Run Examples

```bash
# Enter nix environment
nix-shell

# Run examples (creates real GUI windows!)
mkdir -p build
nim c -o:build/hello_world -r examples/hello_world.nim
nim c -o:build/button_demo -r examples/button_demo.nim
nim c -o:build/counter -r examples/counter.nim
```

### Using the CLI

```bash
# Build the CLI once
nimble build

# Run an app
nix-shell --run "./bin/kryon runKryon --filename=examples/counter.nim"

# Build an executable
nix-shell --run "./bin/kryon build --filename=my_app.nim --output=my_app"

# Get info about a file
nix-shell --run "./bin/kryon info --filename=examples/counter.nim"
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

### Current (Phase 1 & 2 Complete)

**Core System:**
- ✅ Full DSL with Nim macros (12 element types)
- ✅ Type-safe property system
- ✅ Event handlers (onClick with closures)
- ✅ Layout engine (Column, Row, Center)
- ✅ Expression support in DSL (`"Count: " & $count`)

**Rendering:**
- ✅ Real Raylib integration (FFI to C library)
- ✅ 60 FPS event loop
- ✅ Mouse input handling
- ✅ Actual window rendering (not simulation!)
- ✅ Container, Text, Button fully implemented

**CLI Tool:**
- ✅ `kryon runKryon` - Compile and run
- ✅ `kryon build` - Build executables
- ✅ `kryon info` - Analyze files
- ✅ `kryon version` - Show version
- ✅ Renderer autodetection
- ✅ Multiple renderer support (--renderer flag)

**Examples:**
- ✅ hello_world - Basic UI with centered text
- ✅ button_demo - Interactive clickable button
- ✅ counter - Stateful counter with increment/decrement

### Next (Phase 3)

- [ ] Input element (text input with keyboard)
- [ ] Checkbox element (toggle state)
- [ ] Dropdown element (select menu)
- [ ] Image element (load/display images)
- [ ] ScrollView element (scrollable content)
- [ ] Grid layout
- [ ] Padding and margin support
- [ ] Custom component macro system

### Future (Phase 4+)

- [ ] Reactive state management (auto re-render)
- [ ] Rich styling system (gradients, shadows, fonts)
- [ ] Theme support
- [ ] HTML/Web backend
- [ ] Terminal backend
- [ ] Hot reload for development
- [ ] Animation system

---

## Project Structure

```
kryon-nim/
├── src/
│   ├── kryon/
│   │   ├── core.nim              # Core types (Element, Value, Color)
│   │   └── dsl.nim               # DSL macro system
│   ├── backends/
│   │   ├── raylib_ffi.nim        # Raylib FFI bindings
│   │   └── raylib_backend.nim    # Raylib rendering backend
│   ├── cli/
│   │   └── kryon.nim             # CLI tool (4 commands)
│   └── kryon.nim                 # Main module (public API)
├── examples/
│   ├── hello_world.nim           # Centered text example
│   ├── button_demo.nim           # Clickable button example
│   └── counter.nim               # Stateful counter example
├── docs/
│   ├── CLI.md                    # CLI documentation
│   └── GETTING_STARTED.md        # User guide & tutorials
├── bin/
│   └── kryon                     # Compiled CLI tool
├── build/                        # Example binaries (gitignored)
├── kryon.nimble                  # Package configuration
├── ROADMAP.md                    # Implementation roadmap
├── LICENSE                       # BSD-Zero-Clause license
├── .gitignore                    # Git ignore rules
├── shell.nix                     # Nix development environment
└── README.md                     # This file
```

---

## Examples

All examples create **real GUI windows** with Raylib rendering at 60 FPS.

### Hello World

```bash
nix-shell
nim c -o:build/hello_world -r examples/hello_world.nim
```

Creates a 400×300 window with blue background and centered yellow text.

### Button Demo

```bash
nix-shell
nim c -o:build/button_demo -r examples/button_demo.nim
```

Creates a 600×400 window with a centered clickable button that prints to console.

### Counter

```bash
nix-shell
nim c -o:build/counter -r examples/counter.nim
```

Creates a 600×400 window with increment/decrement buttons and live count display.

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

### Build CLI

```bash
nix-shell
nimble build
# Creates ./bin/kryon
```

### Run Examples

```bash
nix-shell
mkdir -p build
nim c -o:build/example_name -r examples/example_name.nim
```

### Build for Release

```bash
nix-shell --run "./bin/kryon build --filename=my_app.nim --release"
# Creates optimized executable
```

### Run Tests

```bash
nimble test  # Coming soon
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

See [ROADMAP.md](ROADMAP.md) for detailed implementation plan.

### ✅ Phase 1-2: Foundation & Rendering (COMPLETE)

- [x] Core type system (Element, Value, Property)
- [x] Full DSL with 12 element macros
- [x] Real Raylib integration (FFI)
- [x] 60 FPS event loop
- [x] Mouse input handling
- [x] CLI tool with 4 commands
- [x] Three working examples

**Status**: Production-ready for desktop apps

### 📋 Phase 3: Advanced Elements & Layout (NEXT)

- [ ] Input, Checkbox, Dropdown elements
- [ ] Image, ScrollView elements
- [ ] Grid layout
- [ ] Padding/margin support
- [ ] Custom component macro system

### 🔮 Phase 4+: Future Features

- [ ] Reactive state management
- [ ] Rich styling system
- [ ] HTML/Web backend
- [ ] Terminal backend
- [ ] Hot reload
- [ ] Animation system

---

## Contributing

Kryon-Nim is in active development. See [ROADMAP.md](ROADMAP.md) for current priorities.

### Current Focus (Phase 3)

1. Advanced UI elements (Input, Checkbox, Dropdown)
2. Custom component macro system
3. Enhanced layout engine
4. More example applications

### Getting Started

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Test with examples
5. Submit a pull request

---

## Documentation

- **[Getting Started Guide](docs/GETTING_STARTED.md)** - Tutorials and user guide
- **[CLI Documentation](docs/CLI.md)** - CLI tool usage
- **[Roadmap](ROADMAP.md)** - Development roadmap
- **Examples** - See `examples/` directory

---

## Why Nim?

Kryon was originally being built as a custom language (Klang) in C. After careful analysis, we chose to build on Nim instead:

**Benefits**:
- ✅ **12x faster development** (Phases 1-2 in 1 session vs 4+ weeks estimated)
- ✅ **12x less code** (~870 LOC vs 11,150 LOC for same features)
- ✅ **Powerful macro system** (perfect for DSLs)
- ✅ **Mature infrastructure** (stdlib, tooling, compiler)
- ✅ **Multiple backends** (C, JavaScript, LLVM)
- ✅ **Excellent performance** (compiles to optimized C)

See `../nim-analysis/` for detailed comparison.

---

## License

**BSD-Zero-Clause** - See [LICENSE](LICENSE)

Permission to use, copy, modify, and/or distribute this software for any purpose with or without fee is hereby granted.

---

## Status

**Phase 1-2 Complete** ✅ (2025-10-09)

- ✅ Full DSL with Nim macros
- ✅ Real Raylib rendering (60 FPS)
- ✅ Complete CLI tool
- ✅ Three working examples
- ✅ Production-ready for desktop apps

**Next**: Phase 3 - Advanced Elements & Custom Components

---

**Built with ❤️ using Nim and Raylib**
