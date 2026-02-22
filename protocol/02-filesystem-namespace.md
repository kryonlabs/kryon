# File System Namespace

## Table of Contents

1. [Root Structure](#root-structure)
2. [Window Directories](#window-directories)
3. [Widget Directories](#widget-directories)
4. [Control Pipe](#control-pipe)
5. [Event Pipes](#event-pipes)
6. [ID Allocation](#id-allocation)
7. [Path Resolution](#path-resolution)

---

## Root Structure

```
/                              # Root directory
├── version                    # Read-only: "Kryon 1.0"
├── ctl                        # Write-only: global commands
├── events                     # Read-only: global events
├── windows/                   # Directory of all windows
│   ├── 1/                    # Window ID 1
│   ├── 2/                    # Window ID 2
│   └── ...
└── themes/                    # (Optional) Theme configurations
    └── default/              # Default theme
        ├── colors/           # Color definitions
        └── fonts/            # Font definitions
```

### Root Files

#### `/version`

- **Type**: Regular file
- **Permissions**: `0444` (read-only)
- **Content**: `"Kryon 1.0\n"` (or current version)
- **Purpose**: Version discovery for clients

#### `/ctl`

- **Type**: Named pipe (write-only)
- **Permissions**: `0222` (write-only)
- **Purpose**: Global commands (create/destroy windows, widgets)
- **Format**: One command per line (newline-terminated)
- **See**: [Control Pipe](#control-pipe)

#### `/events`

- **Type**: Named pipe (read-only)
- **Permissions**: `0444` (read-only)
- **Purpose**: Global events (window created/destroyed/focus changes)
- **Format**: One event per line (space-separated)
- **See**: [Event Pipes](#event-pipes)

#### `/windows/`

- **Type**: Directory
- **Permissions**: `0755` (listable)
- **Purpose**: Container for all windows
- **Dynamic**: Windows appear/disappear based on lifecycle

#### `/themes/` (Optional)

- **Type**: Directory
- **Permissions**: `0755` (listable)
- **Purpose**: Shared theme resources (colors, fonts, styles)
- **Future**: May be used for skinning

---

## Window Directories

Each window is represented as a directory under `/windows/`:

```
/windows/<window_id>/
├── type               # Read-only: "window"
├── title              # Read/write: window title
├── rect               # Read/write: "x y width height"
├── visible            # Read/write: 0/1
├── focused            # Read-only: 0/1
├── state              # Read/write: "normal|minimized|maximized|fullscreen"
├── resizable          # Read/write: 0/1
├── decorations        # Read/write: 0/1 (show title bar)
├── always_on_top      # Read/write: 0/1
├── modal              # Read-only: 0/1
├── parent_window      # Read-only: parent window ID (for modals)
├── widgets/           # Directory of widgets in this window
│   ├── 1/            # Widget ID 1
│   ├── 2/            # Widget ID 2
│   └── ...
└── event              # Read-only: window event pipe
```

### Window Properties

| Property   | Type    | Read/Write | Default      | Description                              |
|------------|---------|------------|--------------|------------------------------------------|
| type       | string  | Read-only  | "window"     | Type identifier                          |
| title      | string  | Read/write | ""           | Window title (displayed in title bar)    |
| rect       | rect    | Read/write | "100 100 800 600" | Position and size              |
| visible    | bool    | Read/write | 1            | Window visibility (0=hidden, 1=shown)    |
| focused    | bool    | Read-only  | 0/1          | Window has keyboard focus                |
| state      | enum    | Read/write | "normal"     | Window state: normal, minimized, maximized, fullscreen |
| resizable  | bool    | Read/write | 1            | Allow user resize                        |
| decorations| bool    | Read/write | 1            | Show title bar and borders               |
| always_on_top| bool  | Read/write | 0            | Keep window above all others             |
| modal      | bool    | Read-only  | 0/1          | Is this a modal dialog?                  |
| parent_window| int   | Read-only  | 0 or parent_id | Parent window ID (0 if none)         |

### Property Formats

#### `rect` (Position and Size)

**Format**: `"x y width height"`

- `x`: X coordinate of top-left corner (screen-relative)
- `y`: Y coordinate of top-left corner (screen-relative)
- `width`: Window width in pixels
- `height`: Window height in pixels

**Example**:
```bash
echo "100 100 800 600" > /windows/1/rect  # Position at (100,100), size 800x600
cat /windows/1/rect                       # Read current rect
```

#### `state` (Window State)

**Valid Values**:
- `"normal"`: Regular window
- `"minimized"`: Hidden from screen, shown in taskbar/dock
- `"maximized"`: Fills screen (minus decorations)
- `"fullscreen"`: Fills entire screen (no decorations)

**Example**:
```bash
echo "maximized" > /windows/1/state
```

#### `title` (Window Title)

**Format**: UTF-8 string, max 256 characters

**Example**:
```bash
echo "My Application" > /windows/1/title
```

---

## Widget Directories

Each widget is represented as a directory under `/windows/<window_id>/widgets/`:

```
/windows/<window_id>/widgets/<widget_id>/
├── type               # Read-only: widget type (e.g., "button")
├── rect               # Read/write: "x0 y0 x1 y1" (widget geometry)
├── visible            # Read/write: 0/1
├── enabled            # Read/write: 0/1
├── focused            # Read-only: 0/1
├── [properties]       # Widget-specific properties
└── event              # Read-only: widget event pipe
```

### Base Widget Properties

All widgets have these mandatory properties:

| Property | Type    | Read/Write | Default | Description                              |
|----------|---------|------------|---------|------------------------------------------|
| type     | string  | Read-only  | (varies)| Widget type identifier                   |
| rect     | rect    | Read/write | "0 0 100 100" | Widget bounding box              |
| visible  | bool    | Read/write | 1       | Widget visibility                        |
| enabled  | bool    | Read/write | 1       | Widget can receive input                 |
| focused  | bool    | Read-only  | 0/1     | Widget has keyboard focus                |
| event    | pipe    | Read-only  | -       | Widget event pipe                        |

### Widget-Specific Properties

Each widget type has additional properties:

**Button** (`type = "button"`):
```
/windows/1/widgets/5/
├── text           # Read/write: button label
├── style          # Read/write: "default", "primary", "secondary", "danger"
├── icon           # Read/write: icon name (optional)
└── ...
```

**TextInput** (`type = "textinput"`):
```
/windows/1/widgets/10/
├── text           # Read/write: current text content
├── placeholder    # Read/write: placeholder text
├── max_length     # Read/write: maximum character count
├── password       # Read/write: 0/1 (obscure text)
└── ...
```

**See**: [Widget Catalog](../widgets/) for complete property lists.

---

## Control Pipe

The `/ctl` file accepts commands to create and destroy windows and widgets.

### Command Format

One command per line, space-separated:

```
<command> <arguments...>
```

### Commands

#### Window Management

**Create Window**:
```
create_window
```
- **Creates**: New window directory under `/windows/`
- **Returns**: Window ID (via response)
- **Initial State**: Window created but not visible (visible=0)

**Destroy Window**:
```
destroy_window <window_id>
```
- **Deletes**: Window directory and all widgets
- **Effect**: Closes window, frees resources

**Focus Window**:
```
focus_window <window_id>
```
- **Effect**: Brings window to front, gives keyboard focus

#### Widget Management

**Create Widget**:
```
create_widget <window_id> <type> <parent_id> [properties...]
```
- **window_id**: Target window
- **type**: Widget type (e.g., "button", "textinput")
- **parent_id**: Parent widget ID (0 for root-level)
- **properties**: Initial properties as `key=value` pairs

**Example**:
```bash
echo "create_widget 1 button 0 text=Hello rect='10 10 100 50'" > /ctl
```

**Destroy Widget**:
```
destroy_widget <window_id> <widget_id>
```
- **Deletes**: Widget directory and all children
- **Effect**: Removes widget from window

**Reparent Widget**:
```
reparent_widget <window_id> <widget_id> <new_parent_id>
```
- **Moves**: Widget to new parent
- **Preserves**: Widget properties and children

#### Global State

**Quit**:
```
quit
```
- **Effect**: Shuts down Kryon server

**Refresh**:
```
refresh
```
- **Effect**: Force redraw all windows

### Command Response

Commands respond via the 9P write response:

- **Success**: Returns bytes written
- **Failure**: Returns Rerror with error message

For `create_window`, the response includes the new window ID (implementation-specific).

---

## Event Pipes

Event pipes deliver events to controllers.

### Global Events Pipe (`/events`)

Global events related to windows:

```
<event_type> <window_id> [params...]
```

**Event Types**:

| Event            | Parameters                     | Description                              |
|------------------|-------------------------------|------------------------------------------|
| window_created   | window_id                     | New window created                       |
| window_destroyed | window_id                     | Window destroyed                         |
| window_focus_gained | window_id                 | Window received keyboard focus           |
| window_focus_lost | window_id                  | Window lost keyboard focus               |
| window_resized   | window_id width height        | User resized window                      |
| window_moved     | window_id x y                 | User moved window                        |
| window_state_changed | window_id state          | Window state changed                     |

**Examples**:
```
window_created 1
window_focus_gained 1
window_resized 1 1024 768
```

### Window Events Pipe (`/windows/<id>/event`)

Window-specific events:

```
<event_type> [params...]
```

**Event Types**:

| Event           | Parameters        | Description                              |
|-----------------|-------------------|------------------------------------------|
| close_request   | -                 | User requested close (X button)          |
| resize          | width height      | User resized window                      |
| move            | x y               | User moved window                        |
| focus_gained    | -                 | Window gained focus                      |
| focus_lost      | -                 | Window lost focus                        |

**Examples**:
```
close_request
resize 1024 768
move 100 100
```

### Widget Events Pipe (`/windows/<id>/widgets/<wid>/event`)

Widget-specific events:

```
<event_name> [key=value] [key=value]...
```

**Common Widget Events**:

| Event      | Parameters              | Description                              |
|------------|-------------------------|------------------------------------------|
| clicked    | x, y, button            | Mouse button clicked                     |
| changed    | value                   | Value changed (for inputs)               |
| focused    | -                       | Widget received focus                    |
| blurred    | -                       | Widget lost focus                        |

**Examples**:
```
clicked x=150 y=32 button=1
changed value=42
focused
```

**See**: [Event System](../widgets/02-event-system.md) for complete event list.

---

## ID Allocation

### Window IDs

- **Range**: 1 to 2^31-1 (signed 32-bit)
- **Allocation**: Sequential starting from 1
- **Reuse**: IDs reusable after window destroyed
- **Maximum**: Platform-dependent (recommended: 1024 windows)

**Algorithm**:
```
next_window_id = 1
while windows[next_window_id] exists:
    next_window_id += 1
if next_window_id > MAX_WINDOWS:
    return error EMAXWINDOWS
```

### Widget IDs

- **Range**: Per-window, 1 to 2^31-1
- **Allocation**: Per-window sequential starting from 1
- **Reuse**: IDs reusable after widget destroyed
- **Maximum**: Platform-dependent (recommended: 65536 widgets per window)

**Algorithm**:
```
next_widget_id[window_id] = 1
while widgets[window_id][next_widget_id] exists:
    next_widget_id += 1
if next_widget_id > MAX_WIDGETS:
    return error EMAXWIDGETS
```

### Special IDs

- **0**: Reserved (no widget/window)
- **Negative**: Reserved (for internal use)

---

## Path Resolution

### Absolute Paths

All paths are absolute from root:

```
/windows/1/widgets/5/text
/windows/2/rect
/ctl
/events
```

### Relative Paths

Not supported (must use absolute paths from root).

### Path Limits

- **Maximum path length**: 4096 characters
- **Maximum component length**: 255 characters

### Path Walk Example

To read button text:

```c
// 9P client code
fid = walk("/windows/1/widgets/5");
open(fid, OREAD);
read(fid, buffer, size);
close(fid);
```

---

## Lifecycle Examples

### Creating a Window

```bash
# 1. Create window
echo "create_window" > /ctl
# Response: window_id = 1

# 2. Set window properties
echo "My App" > /windows/1/title
echo "100 100 800 600" > /windows/1/rect
echo "1" > /windows/1/visible

# 3. Create button widget
echo "create_widget 1 button 0 text='Click Me' rect='10 10 100 40'" > /ctl

# 4. Wait for events
cat /windows/1/widgets/1/event  # Blocks until event
# Output: "clicked x=50 y=20 button=1"
```

### Destroying a Window

```bash
# 1. Hide window (optional)
echo "0" > /windows/1/visible

# 2. Destroy window
echo "destroy_window 1" > /ctl

# 3. Window directory removed
ls /windows/
# (window 1 no longer listed)
```

---

## See Also

- [9P Specification](01-9p-specification.md) - Wire protocol
- [Multi-Window Architecture](05-multi-window.md) - Window management details
- [Widget Catalog](../widgets/) - Widget specifications
