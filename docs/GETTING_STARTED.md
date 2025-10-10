# Getting Started with Kryon-Nim

Welcome to Kryon-Nim! This guide will help you set up your development environment and run your first Kryon application.

---

## Step 1: Install Nim

### Linux/macOS

```bash
curl https://nim-lang.org/choosenim/init.sh -sSf | sh
```

Or use your package manager:

```bash
# Ubuntu/Debian
sudo apt install nim

# Arch Linux
sudo pacman -S nim

# macOS (Homebrew)
brew install nim
```

### Windows

Download the installer from: https://nim-lang.org/install.html

Or use Chocolatey:

```powershell
choco install nim
```

### Verify Installation

```bash
nim --version
# Should show: Nim Compiler Version 2.0.0 or later

nimble --version
# Should show: nimble v0.14.0 or later
```

---

## Step 2: Install Dependencies

```bash
cd kryon-nim
nimble install -y
```

This will install:
- `cligen` - For CLI argument parsing

---

## Step 3: Run Examples

### Hello World

```bash
nim c -r examples/hello_world.nim
```

**Expected output**:
```
Kryon Raylib Backend
Window: 400x300 - Hello World - Kryon

Calculating layout...

Rendering:
Container at (0, 0) size 400x300
  BG: #191970FF
  Center at (0, 0)
    Text: 'Hello World from Kryon-Nim!' at (100, 125)
      Color: #FFD700FF

Note: This is a simplified rendering. Real raylib integration coming soon!
The layout has been calculated and the UI structure is valid.
```

### Button Demo

```bash
nim c -r examples/button_demo.nim
```

### Counter Example

```bash
nim c -r examples/counter.nim
```

---

## Step 4: Create Your First App

Create a new file `my_app.nim`:

```nim
import kryon
import backends/raylib_backend

# Define your UI
let app = kryonApp:
  Container:
    width: 800
    height: 600
    backgroundColor: "#2C3E50"

    Center:
      Column:
        gap: 20

        Text:
          text: "Welcome to Kryon!"
          color: "#ECF0F1"

        Button:
          width: 200
          height: 60
          text: "Get Started"
          backgroundColor: "#3498DB"
          color: "#FFFFFF"
          onClick: proc() =
            echo "Button clicked!"

# Run it
when isMainModule:
  var backend = newRaylibBackend(800, 600, "My First Kryon App")
  backend.run(app)
```

Run it:

```bash
nim c -r my_app.nim
```

---

## Step 5: Using the CLI Tool

Build the CLI once:

```bash
nimble build
```

Then use it to run or build your apps:

```bash
# Run an app (compiles and runs)
nix-shell --run "./bin/kryon runKryon --filename=my_app.nim"

# Build an executable (release mode by default)
nix-shell --run "./bin/kryon build --filename=my_app.nim --output=my_app"

# Build with specific renderer
nix-shell --run "./bin/kryon build --filename=my_app.nim --renderer=raylib"

# Get info about a file
nix-shell --run "./bin/kryon info --filename=my_app.nim"

# Show version
./bin/kryon version
```

**Renderer selection**:
- `--renderer=auto` (default) - Autodetects from imports
- `--renderer=raylib` - Force Raylib renderer
- `--renderer=html` - HTML renderer (coming soon)
- `--renderer=terminal` - Terminal UI (coming soon)

---

## Understanding the Syntax

### Properties

Properties use Nim's colon syntax:

```nim
Container:
  width: 200        # Integer
  height: 100.5     # Float
  text: "Hello"     # String
  visible: true     # Boolean
  backgroundColor: "#FF0000"  # Color (hex string)
```

### Nested Elements

Elements are nested using indentation:

```nim
Container:      # Parent
  Text:         # Child 1
    text: "Hello"

  Button:       # Child 2
    text: "Click"
```

### Layouts

Use layout elements to organize children:

```nim
Column:         # Vertical layout
  gap: 10       # Space between children

  Text:
    text: "First"

  Text:
    text: "Second"

Row:            # Horizontal layout
  gap: 20

  Button:
    text: "Left"

  Button:
    text: "Right"
```

### Event Handlers

Attach event handlers using procedures:

```nim
proc handleClick() =
  echo "Clicked!"

Button:
  text: "Click Me"
  onClick: handleClick
```

Or inline:

```nim
Button:
  text: "Click Me"
  onClick: proc() =
    echo "Clicked!"
```

### State Variables

Just use regular Nim variables:

```nim
var count = 0

proc increment() =
  count = count + 1
  echo "Count: ", count

Button:
  text: "Increment"
  onClick: increment

Text:
  text: "Count: " & $count
```

**Note**: Currently, state changes don't automatically trigger re-renders. Reactive state management is planned for Phase 5.

---

## File Organization

### Single-File Apps

For simple apps, put everything in one file:

```nim
# my_app.nim
import kryon
import backends/raylib_backend

var count = 0

proc increment() = count.inc()

let app = kryonApp:
  Container:
    # ... UI code ...

when isMainModule:
  var backend = newRaylibBackend(800, 600, "My App")
  backend.run(app)
```

### Multi-File Apps

For larger apps, organize into modules:

```
my_project/
├── app.nim           # Main file
├── components/
│   ├── header.nim    # Reusable components
│   └── footer.nim
└── state/
    └── app_state.nim # State management
```

**app.nim**:
```nim
import kryon
import backends/raylib_backend
import components/header
import components/footer

let app = kryonApp:
  Column:
    Header()
    # ... main content ...
    Footer()

when isMainModule:
  var backend = newRaylibBackend(1024, 768, "My App")
  backend.run(app)
```

**components/header.nim**:
```nim
import ../kryon

proc Header*(): Element =
  Container:
    height: 80
    backgroundColor: "#2C3E50"

    Text:
      text: "My Application"
      color: "#ECF0F1"
```

---

## Tips and Best Practices

### 1. Use Type Safety

Take advantage of Nim's type system:

```nim
# Good: Type-safe properties
Container:
  width: 200        # Nim knows this is an int
  backgroundColor: "#FF0000"  # Parsed as Color

# Bad: String for everything (loses type checking)
```

### 2. Organize by Feature

Group related components:

```
components/
├── buttons/
│   ├── primary_button.nim
│   └── icon_button.nim
├── forms/
│   ├── text_input.nim
│   └── checkbox.nim
└── layouts/
    ├── app_layout.nim
    └── card.nim
```

### 3. Reusable Components

Create reusable components as procs:

```nim
proc PrimaryButton(text: string, onClick: EventHandler): Element =
  Button:
    text: text
    onClick: onClick
    backgroundColor: "#3498DB"
    color: "#FFFFFF"
    width: 150
    height: 40

# Usage:
let ui = Container:
  PrimaryButton("Submit", handleSubmit)
  PrimaryButton("Cancel", handleCancel)
```

### 4. Use Constants for Colors/Sizes

```nim
const
  ColorPrimary = "#3498DB"
  ColorSecondary = "#2ECC71"
  ColorDanger = "#E74C3C"

  ButtonHeight = 40
  ButtonPadding = 12

Container:
  Button:
    backgroundColor: ColorPrimary
    height: ButtonHeight
```

---

## Troubleshooting

### "nim: command not found"

Make sure Nim is installed and in your PATH:

```bash
# Add to ~/.bashrc or ~/.zshrc
export PATH=$HOME/.nimble/bin:$PATH
```

### Import errors

Make sure you're running from the kryon-nim directory:

```bash
cd /path/to/kryon-nim
nim c -r examples/hello_world.nim
```

Or use absolute imports:

```nim
import kryon_nim/kryon
import kryon_nim/backends/raylib_backend
```

### Compilation is slow

Use `-d:release` for optimized builds:

```bash
nim c -d:release -r my_app.nim
```

Or `--hints:off` to reduce output:

```bash
nim c --hints:off -r my_app.nim
```

---

## Next Steps

1. **Try all examples** in `examples/` directory
2. **Read ROADMAP.md** to see what's coming
3. **Modify examples** to experiment with the DSL
4. **Create your own app** using the patterns above
5. **Check back for updates** as we add features

---

## Getting Help

- Read the [README.md](README.md)
- Check the [ROADMAP.md](ROADMAP.md)
- Look at examples in `examples/`
- Review Nim documentation: https://nim-lang.org/docs/

---

## What's Working (Week 1)

✅ Core type system
✅ DSL macros
✅ Basic elements (Container, Text, Button, Column, Row, Center)
✅ Properties and values
✅ Event handlers
✅ Layout calculation
✅ Examples

## What's Coming Next

🚧 Real raylib integration (FFI to C code)
🚧 Advanced layout engine
🚧 More UI elements
🚧 Reactive state management
🚧 Component system
🚧 Styling/themes

---

**Welcome to Kryon-Nim! Happy coding! 🚀**
