# Kryon UI Framework

A declarative UI framework for Nim with multiple rendering backends.

## Quick Start

```nim
import ../src/kryon
import ../src/backends/integration/raylib

let app = kryonApp:
  Header:
    width = 800
    height = 600
    title = "Hello Kryon"

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

when isMainModule:
  var backend = newRaylibBackendFromApp(app)
  backend.run(app)
```

## Installation

### Using Nix (Recommended)

```bash
cd kryon
nix-shell
nimble build
```

### Manual Installation

```bash
# Install Nim
curl https://nim-lang.org/choosenim/init.sh -sSf | sh

# Install dependencies
# Ubuntu/Debian:
sudo apt install libsdl2-dev libsdl2-ttf-dev libraylib-dev

# macOS:
brew install nim sdl2 sdl2_ttf raylib

# Build
nimble build
```

## Usage

### Run Examples

```bash
./build/bin/kryon run examples/button_demo.nim
./build/bin/kryon run examples/counter.nim
./build/bin/kryon run examples/dropdown.nim
```

### Build Executable

```bash
./build/bin/kryon build --filename=my_app.nim --output=my_app
./my_app
```

### Available Backends

- **raylib** - Lightweight, easy to use
- **sdl2** - Cross-platform, hardware accelerated
- **skia** - High-quality 2D graphics
- **html** - Web applications

```bash
# Specify backend
./build/bin/kryon build --filename=app.nim --renderer=raylib
./build/bin/kryon build --filename=app.nim --renderer=sdl2
./build/bin/kryon build --filename=app.nim --renderer=skia
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

- **[ARCHITECTURE.md](docs/ARCHITECTURE.md)** - System architecture and design
- **[BACKENDS.md](docs/BACKENDS.md)** - Backend system and custom backends
- **[CLI.md](docs/CLI.md)** - CLI tool usage
- **[GETTING_STARTED.md](docs/GETTING_STARTED.md)** - Detailed getting started guide

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
