# Kryon Specification

Complete specification for the Kryon platform-agnostic UI framework.

## Overview

Kryon is a UI framework where View (pixels), Model (state), and Controller (logic) are decoupled via the 9P protocol. This allows the same application logic to run natively on 9front or as a standalone MirageOS unikernel.

### Key Design Principles

1. **9P as Universal Interface**: All UI state exposed as filesystem
2. **Language Agnostic**: Any language that speaks 9P can build UIs
3. **Platform Agnostic**: Same protocol works on 9front, MirageOS, and beyond
4. **Complete Widget Set**: 89 widgets covering HTML5, Tk, and Flutter widgets
5. **Multi-Window**: Full support for unlimited windows from the start

## Architecture

```
┌─────────────┐     9P      ┌─────────────┐     Events     ┌─────────────┐
│ Controller  │◄──────────►│   Model     │◄──────────────►│    View     │
│  (Logic)    │  Read/Write │ (9P Server) │   State Change │ (Renderer)  │
│  Lua/Scheme │             │             │                │  libdraw/   │
│     C/C++   │             │             │                │  framebuffer│
└─────────────┘             └─────────────┘                └─────────────┘
```

### Components

1. **Controller**: Application logic in any language (Lua, Scheme, C, etc.)
2. **Model**: 9P server exposing UI state as filesystem
3. **View**: Renderer drawing widgets to display (libdraw on 9front, MirageOS framebuffer)

## .kryon Layout Format

.kryon files are **simple layout descriptors** that declare widget hierarchies and assign IDs for controllers to reference. All application logic lives in controller code.

### Example .kryon file

```kryon
window "Counter" 300 200 {
    column gap=10 padding=20 {
        label lbl_count "Count: 0" align="center"
        button btn_inc "+"
        button btn_dec "-"
    }
}
```

### Example Lua controller

```lua
local count = 0
local lbl = kryon.widget("lbl_count")

kryon.widget("btn_inc").on_click(function()
    count = count + 1
    lbl.text = "Count: " .. count
end)

kryon.widget("btn_dec").on_click(function()
    count = count - 1
    lbl.text = "Count: " .. count
end)

kryon.run()
```

**Key Points:**
- `.kryon` files declare **what** the UI looks like (layout + widget IDs)
- Controllers declare **how** it behaves (event handlers, logic)
- Any language can be a controller (Lua, Scheme, C, Python, etc.)
- Controllers reference widgets by ID to read/write properties and attach handlers

See [layout-format.md](layout-format.md) for complete .kryon syntax.

## Filesystem Structure

```
/                              # Root
├── version                    # "Kryon 1.0"
├── ctl                        # Command pipe (create/destroy windows)
├── events                     # Global events
├── windows/                   # All windows
│   ├── 1/                    # Window 1
│   │   ├── title             # Window title
│   │   ├── rect              # Position and size
│   │   ├── widgets/          # Widget tree
│   │   │   └── [widget_id]/
│   │   │       ├── type      # Widget type
│   │   │       ├── rect      # Widget geometry
│   │   │       ├── text      # Widget content
│   │   │       └── event     # Widget events
│   │   └── event             # Window events
│   └── 2/                    # Window 2
└── themes/                    # Optional themes
```

## Specification Documents

### Protocol Layer

1. [Protocol Overview](protocol/00-overview.md) - Architecture and design philosophy
2. [9P Specification](protocol/01-9p-specification.md) - Complete 9P wire protocol
3. [Filesystem Namespace](protocol/02-filesystem-namespace.md) - Directory structure
4. [Permissions](protocol/03-permissions.md) - Security model
5. [Error Handling](protocol/04-error-handling.md) - Error codes and handling
6. [Multi-Window Architecture](protocol/05-multi-window.md) - Window management

### Widget Layer

1. [Base Widget Interface](widgets/00-base-widget.md) - Mandatory properties and lifecycle
2. [Property System](widgets/01-property-system.md) - Property types and validation
3. [Event System](widgets/02-event-system.md) - Event delivery mechanism
4. [Layout System](widgets/03-layout-system.md) - Layout algorithms
5. [Widget Catalog](widgets/04-widget-catalog.md) - All 89 widgets

### Layout Format

1. [.kryon Layout Format](layout-format.md) - Complete syntax for layout files

### Implementation Layer

1. [Compatibility](implementation/00-compatibility.md) - Cross-platform requirements
2. [9front Implementation](implementation/9front/) - 9front/C guide (TODO)
3. [MirageOS Implementation](implementation/mirageos/) - MirageOS/OCaml guide (TODO)

### Examples Layer

1. [Hello World](examples/01-hello-world.kryon) - Simple window with label
2. [Counter](examples/02-counter.kryon) - State and events
3. [Login Form](examples/03-form.kryon) - Form widgets and validation

### Testing Layer

1. [Protocol Tests](testing/protocol-tests.md) - 9P compliance (TODO)
2. [Widget Tests](testing/widget-tests.md) - Widget behavior (TODO)
3. [Integration Tests](testing/integration-tests.md) - End-to-end tests (TODO)
4. [Multi-Window Tests](testing/multi-window-tests.md) - Window management (TODO)

## Quick Start

### Creating a Window with 9P

```bash
# Create window
echo "create_window" > /mnt/kryon/ctl

# Set properties
echo "My App" > /mnt/kryon/windows/1/title
echo "100 100 800 600" > /mnt/kryon/windows/1/rect
echo "1" > /mnt/kryon/windows/1/visible

# Create button
echo "create_widget 1 button 0" > /mnt/kryon/ctl
echo "Click Me" > /mnt/kryon/windows/1/widgets/1/text
```

### Handling Events in Lua

```lua
-- Read from event pipe
local event_pipe = io.open("/mnt/kryon/windows/1/widgets/1/event", "r")

while true do
    local event = event_pipe:read("*line")
    if event == "clicked" then
        print("Button was clicked!")
    end
end
```

### Using .kryon Layout Files

**app.kryon:**
```kryon
window "My App" 800 600 {
    column gap=10 padding=20 {
        label "Welcome to Kryon!"
        textinput txt_input placeholder="Enter text"
        button btn_submit "Submit"
    }
}
```

**app.lua:**
```lua
local kryon = require("kryon")

-- Load layout
kryon.load("app.kryon")

-- Attach event handler
kryon.widget("btn_submit").on_click(function()
    local input = kryon.widget("txt_input").value
    print("Submitted: " .. input)
end)

kryon.run()
```

## Widget Catalog

Kryon includes 89 widgets organized by category:

### Input (24 widgets)
- **Text Inputs**: TextInput, PasswordInput, TextArea, SearchInput, NumberInput, EmailInput, UrlInput, TelInput
- **Selection**: Checkbox, RadioButton, Dropdown, MultiSelect, ListBox, Switch, Slider, RangeSlider, Rating, Spinbox
- **Date/Time**: DateInput, TimeInput, DateTimeInput, MonthInput, WeekInput
- **File**: FileInput

### Buttons (8 widgets)
- Button, SubmitButton, ResetButton, IconButton, FloatingActionButton, DropdownButton, SegmentedControl

### Display (11 widgets)
- Label, Heading, Paragraph, RichText, Code, Image, ProgressBar, ActivityIndicator, Badge, Divider, Tooltip

### Containers (11 widgets)
- Container, Box, Frame, Card, ScrollArea, SplitPane, TabContainer, Dialog, Modal, ActionSheet, Drawer

### Lists & Data (8 widgets)
- ListItem, ListTile, ListView, GridView, DataTable, TreeView, Chip, Tag

### Navigation (8 widgets)
- MenuBar, ToolBar, NavBar, NavRail, Breadcrumb, Pagination, Stepper, PageView

### Menus (4 widgets)
- DropdownMenu, ContextMenu, CascadingMenu, PopupMenu

### Forms (4 widgets)
- Form, FormGroup, ValidationMessage, InputGroup

### Windows (4 widgets)
- Window, DialogWindow, PageScaffold, Fullscreen

### Specialized (8 widgets)
- Canvas, Svg, Chart, ColorPicker, SignaturePad, Resizable, Calendar, TableView

See [Widget Catalog](widgets/04-widget-catalog.md) for complete details.

## Controller Languages

Kryon is language-agnostic. Any language that can:

1. Mount 9P filesystems
2. Read/write files
3. Read from pipes (for events)

can be a Kryon controller.

### Supported Languages

- **Lua**: Lightweight scripting
- **Scheme**: Functional programming
- **C**: High performance
- **Python**: Rapid development
- **Any**: Anything with 9P support

### Controller Runtime Library

Example Lua runtime API:

```lua
kryon = {
    -- Filesystem
    mount = function(server_path),
    unmount = function(),

    -- Window management
    create_window = function(title, width, height),
    destroy_window = function(window_id),
    get_window = function(window_id),

    -- Widget management
    widget = function(widget_id),           -- Look up widget by ID
    create_widget = function(type, parent),

    -- Event handling
    run = function(),                        -- Start event loop
    quit = function(),

    -- Utility
    load = function(kryon_file),             -- Load .kryon layout
}
```

Widget API:

```lua
widget = {
    -- Properties
    text = "...",
    value = "...",
    visible = true,
    enabled = true,

    -- Event handlers
    on_click = function(handler),
    on_change = function(handler),
    on_focus = function(handler),

    -- Methods
    focus = function(),
    show = function(),
    hide = function(),
}
```

## Implementation Status

### Specification
- [x] Protocol specification (6 documents)
- [x] Widget specification (5 documents)
- [x] Layout format specification (1 document)
- [x] Implementation guide (1 document)

### Examples
- [x] Hello World (with Lua controller)
- [x] Counter (with Lua controller)
- [x] Login Form (with Lua controller)

### TODO
- [ ] Platform implementation guides (9front, MirageOS)
- [ ] Runtime libraries (Lua, Scheme, C)
- [ ] Test suites
- [ ] Additional examples

## Version

**Current Version**: 1.0.0

### Version History

- **1.0.0** (2026-02-22): Initial specification
  - Complete 9P protocol specification
  - 89 widgets catalog
  - Multi-window architecture
  - .kryon layout format
  - Implementation compatibility guide

## Contributing

When implementing Kryon:

1. Follow the specification exactly
2. Pass all compliance tests
3. Document platform-specific differences
4. Maintain compatibility with existing controllers

## License

See LICENSE file in root directory.

## See Also

- [Parent README](../README.md) - Project overview
- [.kryon Layout Format](layout-format.md) - Layout syntax
- [9P Protocol](protocol/01-9p-specification.md) - Wire protocol details
- [Widget Catalog](widgets/04-widget-catalog.md) - All widgets
- [Examples](examples/) - Example applications
