# Kryon Elements Reference

This document lists all Kryon UI elements with their hex values, organized by functionality.

## Table of Contents
- [Core Elements](#core-elements)
- [Interactive Elements](#interactive-elements)
- [Media Elements](#media-elements)
- [Form Controls](#form-controls)
- [Semantic Elements](#semantic-elements)
- [Table Elements](#table-elements)
- [Input Types](#input-types)
- [EmbedView Types](#embedview-types)
- [Interactive States](#interactive-states)

## Element Types

All Kryon UI elements organized by functionality with systematic hex value allocation.

### Core Elements (0x00-0x0F)

These elements form the foundation of Kryon UI. In theory, all other UI patterns could be built using just these core elements:

| Element | Hex Value | Description | Why Core? |
|---------|-----------|-------------|-----------|
| `App` | `0x00` | Root application container | Required root element for every Kryon app |
| `Container` | `0x01` | Layout container for grouping | Fundamental for layout and composition |
| `Text` | `0x02` | Text display element | Essential for displaying any text content |
| `EmbedView` | `0x03` | Platform-specific embedded content | Enables extending beyond Kryon's capabilities |

### Interactive Elements (0x10-0x1F)

Elements that handle user interaction:

| Element | Hex Value | Description | Can be built with |
|---------|-----------|-------------|-------------------|
| `Button` | `0x10` | Interactive button element | Basic interaction primitive |
| `Input` | `0x11` | User input field | Core user input mechanism |
| `Link` | `0x12` | Clickable link element | Button + Text |

### Media Elements (0x20-0x2F)

Elements for rich media content:

| Element | Hex Value | Description | Example |
|---------|-----------|-------------|---------|
| `Image` | `0x20` | Image display element | `Image { src: "icon.png" }` |
| `Video` | `0x21` | Video playback element | `Video { src: "video.mp4" }` |
| `Canvas` | `0x22` | Native 2D/3D drawing canvas | `Canvas { ... }` |
| `Svg` | `0x23` | SVG vector graphics element | `Svg { src: "icon.svg" }` |

### Form Controls (0x30-0x3F)

Specialized input elements that enhance user interaction:

| Element | Hex Value | Description | Can be built with |
|---------|-----------|-------------|-------------------|
| `Select` | `0x30` | Dropdown/combobox element | Container + Input + Button |
| `Slider` | `0x31` | Range slider control | Container + Input + Canvas |
| `ProgressBar` | `0x32` | Progress indicator | Container + Canvas/Svg |
| `Checkbox` | `0x33` | Checkbox control | Button + Text |
| `RadioGroup` | `0x34` | Radio button group | Container + Button + Text |
| `Toggle` | `0x35` | Switch/toggle control | Button + Container |
| `DatePicker` | `0x36` | Date input control | Input + Container + Button |
| `FilePicker` | `0x37` | File upload control | Input + Button |

### Semantic Elements (0x40-0x4F)

Elements that provide semantic structure and accessibility:

| Element | Hex Value | Description | Can be built with |
|---------|-----------|-------------|-------------------|
| `Form` | `0x40` | Form container | Container (with form semantics) |
| `List` | `0x41` | Ordered/unordered list | Container + Text |
| `ListItem` | `0x42` | List item element | Container + Text |

### Table Elements (0x50-0x5F)

Specialized elements for tabular data:

| Element | Hex Value | Description | Can be built with |
|---------|-----------|-------------|-------------------|
| `Table` | `0x50` | Table container | Container (grid layout) |
| `TableRow` | `0x51` | Table row | Container (row layout) |
| `TableCell` | `0x52` | Table cell | Container + Text |
| `TableHeader` | `0x53` | Table header cell | TableCell (with semantics) |

## Input Types

For `Input` elements, the `type` property supports:

| Type | Hex | Description |
|------|-----|-------------|
| `text` | `0x00` | Default text input |
| `password` | `0x01` | Password field |
| `email` | `0x02` | Email input |
| `number` | `0x03` | Numeric input |
| `tel` | `0x04` | Telephone input |
| `url` | `0x05` | URL input |
| `search` | `0x06` | Search field |
| `checkbox` | `0x10` | Checkbox |
| `radio` | `0x11` | Radio button |
| `range` | `0x20` | Range slider |
| `date` | `0x30` | Date picker |
| `datetime-local` | `0x31` | Date & time |
| `month` | `0x32` | Month picker |
| `time` | `0x33` | Time picker |
| `week` | `0x34` | Week picker |
| `color` | `0x40` | Color picker |
| `file` | `0x41` | File upload |
| `hidden` | `0x42` | Hidden field |
| `submit` | `0x50` | Submit button |
| `reset` | `0x51` | Reset button |
| `button` | `0x52` | Generic button |
| `image` | `0x53` | Image button |

## EmbedView Types

For `EmbedView` elements:

| Type | Hex | Aliases | Description |
|------|-----|---------|-------------|
| `webview` | `0x00` | `web` | Web content |
| `native_renderer` | `0x01` | `native` | Native rendering |
| `wasm_view` | `0x02` | `wasm` | WebAssembly |
| `uxn_view` | `0x03` | `uxn` | UXN virtual machine |
| `gba_view` | `0x04` | `gba` | Game Boy Advance |
| `dos_view` | `0x05` | `dos` | DOS emulation |
| `code_editor` | `0x06` | `editor` | Code editor |
| `terminal` | `0x07` | `term` | Terminal emulator |
| `model_viewer` | `0x08` | `model` | 3D model viewer |

## Interactive States

Element interaction states with their values:

| State | Value | Description |
|-------|-------|-------------|
| `Normal` | `0` | Default state |
| `Hover` | `1` | Mouse hover |
| `Active` | `2` | Mouse pressed |
| `Focus` | `4` | Keyboard focus |
| `Disabled` | `8` | Disabled state |
| `Checked` | `16` | Selected/checked |

## Layout System

### Layout Flags (Binary Encoded)

The `layout-flags` property uses binary encoding:

| Flag | Hex | Binary | Description |
|------|-----|--------|-------------|
| Row Layout | `0x00` | `00000000` | Horizontal flex layout (default) |
| Column Layout | `0x01` | `00000001` | Vertical flex layout |
| Absolute Layout | `0x02` | `00000010` | Absolute positioning |
| Center Alignment | `0x04` | `00000100` | Center content |
| Grow Flag | `0x20` | `00100000` | Allow element to grow |

### Layout Dimensions

Kryon supports three dimension types:
- **Pixels**: Fixed size (e.g., `100px`, `200`)
- **Percentage**: Responsive size (e.g., `50%`, `100%`)
- **Auto**: Content-based sizing (e.g., `auto`)

## Usage Examples

### Basic Container with Flexbox
```kry
Container {
    layout: "row"        // or use layout-flags: 0x00
    gap: 10
    padding: 20
    background-color: "#f0f0f0"
    
    Button {
        text: "Click Me"
        flex-grow: 1
    }
}
```

### Absolute Positioned Element
```kry
Container {
    layout: "absolute"   // or use layout-flags: 0x02
    
    Text {
        text: "Overlay"
        position: "absolute"
        top: 10
        left: 10
        z-index: 100
    }
}
```

### Responsive Layout
```kry
Container {
    width: 100%          // Percentage width
    height: auto         // Auto height
    max-width: 1200px    // Constraint
    
    Container {
        width: 50%       // Half width
        min-width: 300px // Minimum size
    }
}
```

## Notes

- All hex values are used in the binary KRB format for efficient storage
- The `Custom(u8)` element type allows for values beyond the predefined types
- Layout flags can be combined using bitwise OR operations
- Elements marked as "Can be built with" show that they could theoretically be constructed from core elements, but have dedicated implementations for efficiency and convenience