# Kryon Examples

This directory contains `.kry` example programs demonstrating Kryon library
components. The C files used for native and web builds are generated into
`../build/examples/codegen` and are not source files.

## Available Examples

1. **01_file_dialog** - File dialog component with theme integration
2. **02_buttons** - Button styles and click handling
3. **03_theme** - Theme switching
4. **04_modal** - Modal dialogs
5. **05_color** - Color helpers
6. **06_scaling** - UI scaling
7. **07_layout** - Existing layout helpers
8. **09_geometry** - Packed frames, grid cells, placement, separators
9. **10_menus** - Menubar and menu items
10. **11_basic_controls** - Radio, progress, spinbox, combobox, label frame
11. **12_collections** - Listbox, tree view, table view
12. **13_text_editor** - Text area plus clipboard helpers
13. **14_canvas** - Canvas clipping, grid, and hit testing
14. **15_containers** - Notebook, paned view, collapsible section
15. **16_dialogs** - Message, confirm, prompt, and color picker
16. **17_keyboard_platform** - Accelerators and clipboard
17. **18_accessibility** - Accessibility/debug node overlay

## Requirements

- Kryon library must be built (`make` in parent directory)
- Edit the `.kry` files; generated C/H in `../build/examples/codegen` is disposable
- X11 libraries (Linux): `-lGL -lm -lpthread -ldl -lrt -lX11`

## Building Examples

```bash
# Build all examples
make

# Build specific example
make ../build/examples/bin/01_file_dialog

# Clean examples
make clean
```

Each build transpiles `NN_name.kry` with `kc`, writes generated C/H under
`../build/examples/codegen`, then compiles the standalone example binary under
`../build/examples/bin`.

## Running Examples

```bash
# Interactive menu to select and run examples
make run

# Run directly (after building)
../build/examples/bin/01_file_dialog
```

## IDE Preview

Open this directory with Kryon:

```bash
kryon .
```

The IDE automatically discovers `.kry` screens, builds a live preview host under
`build/kryon`, and reloads when `.kry` sources change. A `project.kryon` file is
only needed for full projects that want custom build or run targets.

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
