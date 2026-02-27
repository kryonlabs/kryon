# Kryon Examples

This directory contains example .kryon files that demonstrate various features of the Kryon Window Manager.

## Running Examples

### Default (minimal.kry)
```bash
# Terminal 1: Start Marrow
cd /mnt/storage/Projects/KryonLabs/marrow && nix-shell --run "./bin/marrow"

# Terminal 2: Start WM (loads examples/minimal.kry by default)
cd /mnt/storage/Projects/KryonLabs/kryon && nix-shell --run "./bin/kryon-wm"

# Terminal 3: Start View
cd /mnt/storage/Projects/KryonLabs/kryon && nix-shell --run "./bin/kryon-view"
```

### Load Specific File
```bash
./bin/kryon-wm --load examples/widgets.kry
```

### Load Example by Name
```bash
./bin/kryon-wm --example widgets
./bin/kryon-wm --example layouts
./bin/kryon-wm --example nested
./bin/kryon-wm --example minimal
```

### List Available Examples
```bash
./bin/kryon-wm --list-examples
```

## Example Files

### minimal.kry
Simple button and label demo. This is the default example that loads when no arguments are provided.

**Features:**
- Single window with title
- VBox layout (vertical arrangement)
- Button and label widgets
- Yellow color on label

### widgets.kry
Showcase of all implemented widget types.

**Features:**
- All widget types: button, label, switch, slider, textinput, checkbox, radiobutton, dropdown, progressbar, heading, paragraph
- Various color demonstrations
- VBox layout for organization

### layouts.kry
Demonstration of different layout types.

**Features:**
- VBox (vertical box) layout
- HBox (horizontal box) layout
- Absolute layout (manual positioning)
- Nested layouts (VBox inside HBox)
- Gap and padding parameters

### nested.kry
Multiple top-level windows.

**Features:**
- Two separate windows defined in one file
- Shows how to organize multi-window applications

## .kryon File Format

### Window Syntax
```kryon
window id "Title" width height {
    layout_type params {
        widgets...
    }
}
```

### Layout Types
- `vbox gap=N padding=N align=TYPE` - Vertical layout
- `hbox gap=N padding=N justify=TYPE` - Horizontal layout
- `grid cols=N rows=N gap=N` - Grid layout
- `absolute` - Manual positioning via `rect` property
- `stack` - Layered widgets (only top visible)

### Widget Syntax
```kryon
# Basic widget with text
button id "Display Text"

# Widget with properties
label id "Text" { color "#RRGGBB" }

# Widget with manual positioning
button id "Text" { rect "x y w h" }
```

### Widget Properties
- `text` - Display text (default property)
- `rect` - Position and size "x y w h"
- `color` - Color "#RRGGBB" or "#RRGGBBAA"
- `placeholder` - Hint text for inputs

### Layout Properties
- `gap` - Spacing between children
- `padding` - Internal spacing
- `align` - Alignment: start, center, end, stretch
- `cols` - Number of columns (grid)
- `rows` - Number of rows (grid)

## Creating Your Own Examples

Create a new .kryon file in this directory:

```kryon
# myexample.kry
window myapp "My Application" 800 600 {
    vbox gap=10 padding=20 {
        label title "Welcome to My App" { color "#00FF00" }
        button btn_start "Start"
        button btn_quit "Quit"
    }
}
```

Run it:
```bash
./bin/kryon-wm --load examples/myexample.kry
```

## Notes

- .kryon files use a simple declarative syntax
- Comments start with `#`
- Strings can be quoted with `"` or `'`
- Properties can be inline (`gap=10`) or in blocks (`{ gap=10 }`)
- The parser is written in C89 for maximum compatibility
