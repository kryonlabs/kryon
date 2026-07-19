# Flint Examples

This directory contains example programs demonstrating Flint library components.

## Available Examples

1. **01_file_dialog** - File dialog component with theme integration
2. **02_buttons** - Button styles and click handling
3. **03_theme** - Theme switching
4. **04_modal** - Modal dialogs
5. **05_color** - Color helpers
6. **06_scaling** - UI scaling
7. **07_layout** - Existing layout helpers
8. **08_editor_gallery** - Editor overlay/gallery
9. **09_geometry** - Packed frames, grid cells, placement, separators
10. **10_menus** - Menubar and menu items
11. **11_basic_controls** - Radio, progress, spinbox, combobox, label frame
12. **12_collections** - Listbox, tree view, table view
13. **13_text_editor** - Text area plus clipboard helpers
14. **14_canvas** - Canvas clipping, grid, and hit testing
15. **15_containers** - Notebook, paned view, collapsible section
16. **16_dialogs** - Message, confirm, prompt, and color picker
17. **17_keyboard_platform** - Accelerators and clipboard
18. **18_accessibility** - Accessibility/debug node overlay

## Requirements

- Flint library must be built (`make` in parent directory)
- Include Flint with `#include "flint.h"`; Flint provides the Raylib-compatible public API surface
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

### Toolkit Examples

The toolkit examples use one direct API shape per widget: fill the struct that
describes the widget state and call the matching `DrawUI...` function. Geometry
uses `BeginUIFrameBox`, `UIFramePack`, `UIGridCell`, and `UIPlace`; canvas uses
`BeginUICanvas` / `EndUICanvas`.

## Usage

- Press **L** to open load file dialog
- Press **S** to open save file dialog  
- Press **ESC** to exit
- Toggle "Show hidden files" checkbox in dialogs
