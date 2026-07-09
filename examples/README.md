# File UI Toolkit Examples

This directory contains example programs demonstrating File UI Toolkit library components.

## Available Examples

1. **01_file_dialog** - File dialog component with theme integration

## Requirements

- File UI Toolkit library must be built (`make` in parent directory)
- Raylib from File UI Toolkit's `vendor/raylib` submodule, or set `RAYLIB_DIR=/path/to/raylib/src`
- X11 libraries (Linux): `-lGL -lm -lpthread -ldl -lrt -lX11`

## Building Examples

```bash
# Build all examples
make

# Build specific example
make 01_file_dialog

# Clean examples
make clean
```

## Running Examples

```bash
# Interactive menu to select and run examples
make run

# Run directly (after building)
./01_file_dialog
```

## Example Features

### 01_file_dialog
- Demonstrates File UI Toolkit file dialog component
- Theme integration with all 6 File UI Toolkit themes
- Load and save dialog modes
- Hidden files filtering (checkbox)
- Full keyboard/mouse interaction
- Direct component testing without full application

## Usage

- Press **L** to open load file dialog
- Press **S** to open save file dialog  
- Press **ESC** to exit
- Toggle "Show hidden files" checkbox in dialogs
