# Kryon UI Framework

A declarative UI framework with multiple rendering backends and C core architecture.

## Quick Start

Kryon provides a DSL-based UI framework with a C core that supports multiple backends:

```nim
import kryon_dsl

let app = kryonApp:
  Header:
    windowWidth = 800
    windowHeight = 600
    windowTitle = "Hello Kryon"

  Body:
    backgroundColor = "#2C3E50"

    Center:
      Column:
        gap = 20

        Text:
          text = "Hello Kryon!"
          fontSize = 32
          color = "#ECF0F1"

        Button:
          text = "Click Me"
          fontSize = 16
          onClick = proc() =
            echo "Button clicked!"

app.run()
```

## Installation

### Using Nix (Recommended)

```bash
cd kryon
nix-shell
make build
```

### Manual Installation

```bash
# Install Nim
curl https://nim-lang.org/choosenim/init.sh -sSf | sh

# Build C Core
cd core && make
cd ../renderers/sdl3 && make
cd ../terminal && make

# Build CLI tool
cd ../cli && make

# Build
nimble build
```

## Usage

### Run Examples

```bash
./run_example.sh hello_world nim sdl3
./run_example.sh button_demo nim terminal
./run_example.sh checkbox nim framebuffer
```

### Build Executable

```bash
# Build with specific renderer
nim c -r examples/hello_world.nim --renderer:terminal
nim c -r examples/button_demo.nim --renderer:sdl3
```

### Available Backends

- **SDL3** - Modern cross-platform, hardware accelerated
- **Terminal** - Text-based UI using ANSI escape sequences
- **Framebuffer** - Direct framebuffer rendering (Linux)
- **Web** - WebAssembly/JavaScript backend (planned)

```bash
# Specify backend in build command
nim c -r app.nim --renderer:terminal
nim c -r app.nim --renderer:sdl3
```

## Features

- ✅ Declarative DSL syntax
- ✅ Multiple rendering backends
- ✅ Event handlers (onClick, onChange, onSubmit, etc.)
- ✅ Reactive state management
- ✅ Flexible layout system (Column, Row, Center, Grid)
- ✅ Rich components (Button, Input, Checkbox, Dropdown, Tabs)
- ✅ Styling (colors, borders, padding, fonts)
- ✅ Text rendering with custom fonts
- ✅ Mouse and keyboard input

## Components

- **Container** - Generic container with styling
- **Text** - Text display
- **Button** - Clickable button with hover states
- **Input** - Text input field with focus and cursor
- **Checkbox** - Toggle checkbox with label
- **Dropdown** - Selection dropdown with keyboard navigation
- **Column** - Vertical layout with alignment options
- **Row** - Horizontal layout with alignment options
- **Center** - Centered layout
- **TabGroup/TabBar/Tab/TabContent** - Tab-based navigation
- **Grid** - Grid layout (planned)

## Documentation

- **[ARCHITECTURE.md](ARCHITECTURE.md)** - System architecture and design
- **[Installation Guide](examples/)** - See examples directory for usage
- **Build System** - See Makefile for build options

## Examples

See `examples/` directory for complete applications:
- `hello_world.nim` - Basic hello world
- `button_demo.nim` - Button interactions
- `counter.nim` - State management
- `dropdown.nim` - Dropdown component
- `checkbox.nim` - Checkbox component
- `tabs.nim` - Tab navigation
- And more...

## License

BSD-Zero-Clause
