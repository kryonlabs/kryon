# Kryon UI Framework

A declarative UI framework for Nim that makes building desktop applications simple and clean.

## Quick Start

### Prerequisites

```bash
# Option 1: Nix (recommended)
cd kryon
nix-shell

# Option 2: Manual install
# Ubuntu/Debian: sudo apt install libsdl2-dev libsdl2-ttf-dev
# macOS: brew install sdl2 sdl2_ttf
# Install: nim, nimble, raylib
```

### Installation

```bash
cd kryon
nimble build
```

## Usage

### Run Examples

```bash
# With Raylib backend (default)
./bin/kryon run examples/hello_world.nim
./bin/kryon run examples/button_demo.nim
./bin/kryon run examples/dropdown.nim

# With SDL2 backend
./bin/kryon run examples/hello_world.nim --renderer sdl2
./bin/kryon run examples/interactive_sdl2.nim --renderer sdl2

# Auto-detect backend based on imports
./bin/kryon run examples/hello_world.nim --renderer auto
```

### Example Code

```nim
import ../src/kryon

let app = kryonApp:
  Header:
    width = 800
    height = 600
    title = "My App"

  Body:
    backgroundColor = "#2C3E50"
    contentAlignment = "center"
    gap = 20

    Text:
      text = "Hello Kryon!"
      fontSize = 24
      color = "#ECF0F1"

    Button:
      text = "Click Me"
      fontSize = 16
      backgroundColor = "#3498DB"
      onClick = proc() =
        echo "Button clicked!"
```

**Run it:**
```bash
./bin/kryon run my_app.nim --renderer raylib  # or sdl2
```

## Available Backends

- **Raylib** - Lightweight C library for games and multimedia
- **SDL2** - Cross-platform development library

## CLI Commands

```bash
# Run an app
./bin/kryon run <file> --renderer <raylib|sdl2|auto>

# Build an executable
./bin/kryon build <file> --renderer <raylib|sdl2> --output <name>

# Get info about a file
./bin/kryon info <file>

# Show version
./bin/kryon version
```

## UI Elements

- **Container** - Layout container with styling
- **Text** - Text display
- **Button** - Clickable button
- **Input** - Text input field
- **Checkbox** - Toggle checkbox
- **Dropdown** - Selection dropdown
- **Column** - Vertical layout
- **Row** - Horizontal layout
- **Center** - Centered layout
- **TabGroup/TabBar/Tab** - Tab interface
- **Grid** - Grid layout

## Features

- ✅ Declarative syntax
- ✅ Event handlers (onClick, onChange, etc.)
- ✅ Reactive state management
- ✅ Multiple backends (Raylib, SDL2)
- ✅ Layout engine (Column, Row, Center)
- ✅ Styling (colors, borders, padding)
- ✅ Text rendering with fonts
- ✅ Mouse and keyboard input
- ✅ Auto-detect backend from imports

## License

BSD-Zero-Clause