# Kryon UI Framework

A declarative UI framework with multiple frontends (.kry, .nim) and rendering backends (SDL3, terminal).

## Quick Start

### Using `.kry` files (recommended for simple UIs)

```kry
// hello.kry
App {
    windowTitle = "Hello Kryon"
    windowWidth = 800
    windowHeight = 600

    Container {
        width = 200
        height = 100
        backgroundColor = "#191970"
        contentAlignment = "center"

        Text {
            text = "Hello World"
            color = "yellow"
        }
    }
}
```

Run with: `kryon run hello.kry`

### Using Nim DSL (for reactive apps)

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
          onClick = proc() =
            echo "Button clicked!"

app.run()
```

Run with: `kryon run hello.nim`

## Installation

### Using Nix (Recommended)

```bash
cd kryon
nix-shell
make build
make install   # Installs kryon CLI to ~/.local/bin
```

### Manual Installation

```bash
# Install Nim (>= 2.0)
curl https://nim-lang.org/choosenim/init.sh -sSf | sh

# Build C libraries and CLI
make build
make install
```

The `make install` command installs:
- `kryon` CLI to `~/.local/bin/`
- Shared libraries to `~/.local/lib/`

## Usage

### CLI Commands

```bash
# Run applications
kryon run examples/kry/hello_world.kry   # Run .kry file
kryon run examples/nim/button_demo.nim   # Run Nim app
kryon run app.kir                        # Run pre-compiled IR

# Parse .kry to .kir (JSON IR)
kryon parse hello.kry                    # Outputs hello.kir

# Inspect IR files
kryon tree app.kir                       # Show component tree
kryon inspect-detailed app.kir           # Full analysis

# Development mode with hot reload
kryon dev examples/nim/habits.nim
```

### Run Examples Script

```bash
./run_example.sh hello_world             # Run Nim example with SDL3
./run_example.sh button_demo nim terminal # Use terminal renderer
```

### Available Backends

- **SDL3** - Modern cross-platform, hardware accelerated (default)
- **Terminal** - Text-based UI using ANSI escape sequences

```bash
# Set renderer via environment variable
KRYON_RENDERER=terminal kryon run app.kry
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
