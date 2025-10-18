# Getting Started with Kryon

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

# Or use package manager:
# Ubuntu/Debian: sudo apt install nim
# Arch Linux: sudo pacman -S nim
# macOS: brew install nim

# Install dependencies
# Ubuntu/Debian:
sudo apt install libsdl2-dev libsdl2-ttf-dev libraylib-dev

# macOS:
brew install sdl2 sdl2_ttf raylib

# Build Kryon CLI
nimble build
```

## Your First App

Create `my_app.nim`:

```nim
import ../src/kryon
import ../src/backends/integration/raylib

let app = kryonApp:
  Header:
    width = 800
    height = 600
    title = "My First App"

  Body:
    backgroundColor = "#2C3E50"

    Center:
      Column:
        gap = 20

        Text:
          text = "Welcome to Kryon!"
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

Run it:

```bash
./build/bin/kryon run my_app.nim
```

## Basic Concepts

### Elements

UI is built from elements:

```nim
Container:      # Container element
  width = 400
  height = 300

  Text:         # Text element
    text = "Hello"

  Button:       # Button element
    text = "Click"
```

### Properties

Elements have properties:

```nim
Text:
  text = "Hello"          # String
  fontSize = 24           # Integer
  color = "#FF0000"       # Color (hex)
  visible = true          # Boolean
```

### Layout

Use layout elements to organize children:

```nim
Column:         # Stack vertically
  gap = 10
  Text:
    text = "First"
  Text:
    text = "Second"

Row:            # Stack horizontally
  gap = 20
  Button:
    text = "Left"
  Button:
    text = "Right"

Center:         # Center children
  Text:
    text = "Centered"
```

### Event Handlers

Attach event handlers to elements:

```nim
Button:
  text = "Click Me"
  onClick = proc() =
    echo "Clicked!"

Input:
  placeholder = "Type here"
  onChange = proc(value: string) =
    echo "Value: ", value

Checkbox:
  label = "Accept"
  onChange = proc(checked: string) =
    echo "Checked: ", checked
```

### State

Use regular Nim variables for state:

```nim
var count = 0

proc increment() =
  count += 1
  echo "Count: ", count

let app = kryonApp:
  Body:
    Button:
      text = "Increment"
      onClick = increment

    Text:
      text = "Count: " & $count
```

## Components

### Available Components

- **Container** - Generic container
- **Text** - Display text
- **Button** - Clickable button
- **Input** - Text input field
- **Checkbox** - Toggle checkbox
- **Dropdown** - Selection dropdown
- **Column** - Vertical layout
- **Row** - Horizontal layout
- **Center** - Centered layout
- **TabGroup/TabBar/Tab/TabContent** - Tabs

### Example: Counter

```nim
import ../src/kryon
import ../src/backends/integration/raylib

var count = 0

proc increment() = count += 1
proc decrement() = count -= 1

let app = kryonApp:
  Header:
    width = 400
    height = 300
    title = "Counter"

  Body:
    Center:
      Column:
        gap = 20

        Text:
          text = "Count: " & $count
          fontSize = 32

        Row:
          gap = 10

          Button:
            text = "-"
            onClick = decrement

          Button:
            text = "+"
            onClick = increment

when isMainModule:
  var backend = newRaylibBackendFromApp(app)
  backend.run(app)
```

### Example: Input Form

```nim
import ../src/kryon
import ../src/backends/integration/raylib

var name = ""
var email = ""

let app = kryonApp:
  Header:
    width = 600
    height = 400
    title = "Form"

  Body:
    Center:
      Column:
        gap = 15

        Input:
          placeholder = "Name"
          onChange = proc(value: string) =
            name = value

        Input:
          placeholder = "Email"
          onChange = proc(value: string) =
            email = value

        Button:
          text = "Submit"
          onClick = proc() =
            echo "Name: ", name
            echo "Email: ", email

when isMainModule:
  var backend = newRaylibBackendFromApp(app)
  backend.run(app)
```

## Backends

### Choosing a Backend

Import the backend you want to use:

```nim
# Raylib - Simple and easy
import ../src/backends/integration/raylib
var backend = newRaylibBackendFromApp(app)

# SDL2 - Cross-platform
import ../src/backends/integration/sdl2
var backend = newSDL2BackendFromApp(app)

# Skia - High-quality graphics
import ../src/backends/integration/sdl2_skia
var backend = newSkiaBackendFromApp(app)
```

### Backend Comparison

| Backend | Pros | Cons |
|---------|------|------|
| **raylib** | Easy setup, lightweight | Basic rendering |
| **sdl2** | Cross-platform, mature | More dependencies |
| **skia** | High-quality rendering | Complex setup |

## CLI Usage

### Run Application

```bash
./build/bin/kryon run my_app.nim
./build/bin/kryon run my_app.nim --renderer=raylib
./build/bin/kryon run my_app.nim --release
```

### Build Executable

```bash
./build/bin/kryon build --filename=my_app.nim
./build/bin/kryon build --filename=my_app.nim --output=dist/app
./build/bin/kryon build --filename=my_app.nim --renderer=sdl2
```

### Get Info

```bash
./build/bin/kryon info --filename=my_app.nim
```

## Tips

### Organize Code

Split into modules for larger apps:

```
my_project/
├── app.nim          # Main file
├── components/
│   ├── header.nim
│   └── footer.nim
└── state/
    └── app_state.nim
```

### Reusable Components

Create component functions:

```nim
proc PrimaryButton(text: string, onClick: EventHandler): Element =
  Button:
    text = text
    onClick = onClick
    backgroundColor = "#3498DB"
    fontSize = 16

# Usage:
Column:
  PrimaryButton("Submit", handleSubmit)
  PrimaryButton("Cancel", handleCancel)
```

### Use Constants

Define colors and sizes as constants:

```nim
const
  ColorPrimary = "#3498DB"
  ColorSecondary = "#2ECC71"
  ButtonHeight = 40

Button:
  backgroundColor = ColorPrimary
  height = ButtonHeight
```

## Next Steps

1. **Try examples** in `examples/` directory
2. **Read documentation**:
   - [ARCHITECTURE.md](ARCHITECTURE.md)
   - [BACKENDS.md](BACKENDS.md)
   - [CLI.md](CLI.md)
3. **Build your own app**
4. **Explore advanced features**: tabs, dropdowns, custom styling

## Troubleshooting

### Import Errors

Use correct relative paths:

```nim
# From examples directory:
import ../src/kryon
import ../src/backends/integration/raylib

# From root directory:
import src/kryon
import src/backends/integration/raylib
```

### Missing Dependencies

Install required libraries:

```bash
# Ubuntu/Debian
sudo apt install libsdl2-dev libsdl2-ttf-dev libraylib-dev

# macOS
brew install sdl2 sdl2_ttf raylib
```

### Slow Compilation

Use release mode for faster executables:

```bash
./build/bin/kryon run my_app.nim --release
```

## Getting Help

- Read the documentation in `docs/`
- Check examples in `examples/`
- Review Nim documentation: https://nim-lang.org/docs/
