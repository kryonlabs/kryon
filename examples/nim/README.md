# Kryon Framework

**A modern, efficient UI framework for embedded and desktop applications**

## Overview

Kryon is a **cross-platform UI framework** that provides:

- **🚀 Performance** - Optimized for microcontrollers and desktop
- **🎨 Declarative DSL** - Clean, type-safe UI definitions
- **📱 Cross-platform** - Same code runs everywhere
- **🎯 Canvas System** - Love2D-inspired drawing API
- **📝 Markdown Support** - Rich text formatting
- **🧩 Component System** - Reusable UI components

## Quick Start

### Installation

```nim
# Using Nimble
nimble install kryon
```

### Basic Example

```nim
import kryon_dsl

let app = kryonApp:
  Header:
    windowWidth = 800
    windowHeight = 600
    windowTitle = "Hello Kryon"

  Body:
    Container:
      width = 800
      height = 600
      backgroundColor = "#f0f0f0"

      Text:
        text = "Hello, World!"
        color = "#333333"
        fontSize = 24
```

## Features

### Canvas Drawing

The canvas system provides **Love2D-inspired immediate mode drawing**:

```nim
import canvas

proc drawCanvas() =
  background(25, 25, 35)
  fill(255, 100, 100, 200)
  rectangle(Fill, 50, 50, 100, 80)
  circle(Fill, 200, 90, 40)

let app = kryonApp:
  Body:
    Canvas:
      width = 800
      height = 600
      onDraw = drawCanvas
```

### Markdown Rendering

Rich text formatting with **full CommonMark compliance**:

```nim
import markdown

let app = kryonApp:
  Body:
    Markdown:
      source = """
      # Documentation

      This is **formatted text** with `code` and [links](https://example.com).

      ## Features
      - Easy to use
      - High performance
      - Cross-platform
      """
      theme = darkTheme
```

### Component System

Built-in components for common UI patterns:

```nim
Container:
  width = 400
  height = 300

  Row:
    Button:
      text = "Click me!"
      onClick = proc() = echo "Clicked!"

    Text:
      text = "Status: Ready"
      color = "#666666"

  Column:
    Input:
      placeholder = "Enter text..."
      onChange = proc(text: string) = echo "Input: ", text

    Checkbox:
      text = "Enable notifications"
      checked = false
      onChange = proc(checked: bool) = echo "Notifications: ", checked
```

## Platform Support

| Platform | Status | Features |
|----------|--------|----------|
| STM32F4 | ✅ Complete | Core UI, Canvas, Limited Markdown |
| ESP32 | ✅ Complete | Full feature set |
| Desktop | ✅ Complete | All features including advanced themes |
| Terminal | ✅ Complete | TUI mode with color support |
| Web | 🔄 In Progress | WASM compilation |

## Performance

- **STM32F4**: <8KB ROM, <4KB RAM
- **Desktop**: Fast compilation, efficient rendering
- **Memory**: No heap allocations in core rendering path
- **Speed**: 60fps rendering with hundreds of components

## Architecture

The framework uses a **layered architecture**:

```
Application Layer (Nim/Lua/JS/C++)
    ↓
Interface Layer (Language Bindings)
    ↓
Core Layer (C99 ABI - UI, Layout, Events)
    ↓
Renderer Abstraction (Canvas, Fonts, Text)
    ↓
Platform Backends (SDL3, Framebuffer, Terminal)
```

## Examples

See the `examples/` directory for complete examples:

- `hello_world.nim` - Basic application
- `canvas_shapes_demo.nim` - Canvas drawing
- `markdown_demo.nim` - Rich text formatting
- `button_demo.nim` - Interactive components

## Documentation

- `CANVAS.md` - Canvas system documentation
- `MARKDOWN.md` - Markdown component documentation
- `ARCHITECTURE.md` - Framework architecture

## License

**0BSD License** - Permissive, business-friendly open source license.

## Contributing

Contributions are welcome! Please read the contribution guidelines and follow the code style.

---

*"Measure twice, cut once. The framework's true test isn't how much it can do on a desktop — it's how little it needs to run on a $2 microcontroller."*