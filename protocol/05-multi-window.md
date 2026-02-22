# Multi-Window Architecture

## Table of Contents

1. [Window Lifecycle](#window-lifecycle)
2. [Window Properties](#window-properties)
3. [Z-Order and Focus](#z-order-and-focus)
4. [Cross-Window Communication](#cross-window-communication)
5. [Platform Considerations](#platform-considerations)

---

## Window Lifecycle

### Creation

**Step 1: Create Window**
```bash
echo "create_window" > /ctl
```
- Server allocates new window ID
- Creates `/windows/<id>/` directory
- Creates mandatory files (type, title, rect, etc.)
- Returns window ID

**Step 2: Configure Window**
```bash
echo "My App" > /windows/1/title
echo "100 100 800 600" > /windows/1/rect
echo "1" > /windows/1/resizable
```

**Step 3: Show Window**
```bash
echo "1" > /windows/1/visible
```
- Window appears on screen
- Renderer draws window frame and widgets

### Modification

**Move Window**:
```bash
echo "200 200 800 600" > /windows/1/rect
```

**Resize Window**:
```bash
echo "100 100 1024 768" > /windows/1/rect
```

**Change State**:
```bash
echo "maximized" > /windows/1/state
```

### Destruction

**Step 1: Hide Window** (Optional)
```bash
echo "0" > /windows/1/visible
```

**Step 2: Destroy Window**
```bash
echo "destroy_window 1" > /ctl
```
- Server deletes `/windows/1/` directory
- All widgets under `/windows/1/widgets/` deleted
- Window ID becomes reusable

---

## Window Properties

### Complete Property List

```c
struct WindowProps {
    // Identity
    int window_id;           // Read-only: Window ID

    // Appearance
    char* title;             // Window title (max 256 chars)
    int decorations;         // Show title bar and borders (0/1)

    // Geometry
    struct Rect rect;        // Position and size
    char* state;             // "normal", "minimized", "maximized", "fullscreen"

    // State
    int visible;             // 0/1
    int focused;             // Read-only: 0/1
    int resizable;           // 0/1
    int always_on_top;       // 0/1

    // Modal
    int modal;               // Read-only: 0/1
    int parent_window;       // Read-only: 0 or parent_id
};
```

### Property Details

#### `title` (String)

- **Format**: UTF-8 text, max 256 characters
- **Default**: "" (empty)
- **Behavior**: Displayed in title bar (if decorations enabled)
- **Example**: `"My Application - Document 1"`

#### `rect` (Rectangle)

- **Format**: `"x y width height"`
- **Coordinates**: Screen-relative (not parent-relative)
- **Constraints**:
  - `width >= 100` (minimum window width)
  - `height >= 100` (minimum window height)
- **Example**: `"100 100 800 600"`

#### `visible` (Boolean)

- **Values**: 0 (hidden), 1 (shown)
- **Default**: 0 (hidden after creation)
- **Behavior**:
  - 0 → 1: Window appears
  - 1 → 0: Window disappears (still exists)

#### `state` (Enum)

- **Valid Values**:
  - `"normal"`: Regular window
  - `"minimized"`: Hidden, shown in taskbar/dock
  - `"maximized"`: Fills screen (minus decorations)
  - `"fullscreen"`: Fills entire screen (no decorations)

- **Transitions**:
  ```
  normal → minimized → normal
  normal → maximized → normal
  normal → fullscreen → normal
  ```

#### `resizable` (Boolean)

- **Values**: 0 (fixed size), 1 (user-resizable)
- **Default**: 1
- **Effect**:
  - 0: Resize handles hidden, user can't resize
  - 1: Resize handles shown, user can drag edges

#### `decorations` (Boolean)

- **Values**: 0 (frameless), 1 (with title bar)
- **Default**: 1
- **Effect**:
  - 0: No title bar, no close button, no resize handles
  - 1: Title bar, close button, resize handles

#### `always_on_top` (Boolean)

- **Values**: 0 (normal), 1 (always above)
- **Default**: 0
- **Effect**: Window stays above other windows (except modals)

#### `modal` (Boolean, Read-Only)

- **Values**: 0 (normal), 1 (modal)
- **Set by**: `create_window` command
- **Effect**: Blocks parent window until closed

#### `parent_window` (Integer, Read-Only)

- **Values**: 0 (none) or parent window ID
- **Set by**: `create_window` command
- **Effect**: Modal parent relationship

---

## Z-Order and Focus

### Window Stacking

Windows are rendered in Z-order (back-to-front):

```
[Bottom] Window 3 → Window 5 → Window 1 → Window 2 [Top]
```

**Raising Window**:
```bash
echo "focus_window 1" > /ctl
# Window 1 moves to top of Z-order
```

### Focus Management

**Focus Rules**:
1. Only one window has focus at a time
2. Clicking window gives it focus
3. Modal windows always grab focus
4. Minimized windows can't have focus

**Focus Events**:
```
# Global events pipe
window_focus_gained 1
window_focus_lost 2

# Window event pipe
focus_gained
focus_lost
```

**Checking Focus**:
```bash
cat /windows/1/focused
# Output: 1 (has focus) or 0 (doesn't have focus)
```

### Modal Window Behavior

**Modal Characteristics**:
- Blocks parent window interaction
- Always stays above parent
- Can't be minimized
- Must be closed before parent regains focus

**Creating Modal**:
```bash
echo "create_window 2 modal=1 parent=1" > /ctl
```

**Modal Rules**:
1. Parent window disabled while modal open
2. Modal can't lose focus to other windows
3. Closing modal re-enables parent

### Always-On-Top Behavior

**Always-On-Top Characteristics**:
- Stays above normal windows
- Below modal windows
- Can lose focus (unlike modals)

**Use Cases**:
- Tool palettes
- Status windows
- Floating controls

---

## Cross-Window Communication

### Drag and Drop

**Planned Feature** (not in initial implementation):

**Flow**:
1. User drags from widget in Window A
2. Event: `drag_start widget=5 x=10 y=20 data="..."`
3. User drops on widget in Window B
4. Event: `drop widget=10 x=100 y=50 data="..."`

**Data Transfer**:
- Data embedded in event
- Types: text, file paths, custom

### Clipboard Sharing

**Implementation**: Global clipboard (not per-window)

**Read Clipboard**:
```bash
cat /clipboard/content
```

**Write Clipboard**:
```bash
echo "copied text" > /clipboard/content
```

**Clipboard Events**:
```
clipboard_changed
```

### Inter-Window Widget References

**Planned Feature** (future):

Widgets in one window referencing widgets in another:

```
/windows/1/widgets/5/
├── target_window → 2
└── target_widget → 10
```

**Use Case**:
- Dashboard windows updating main window
- Preview windows reflecting changes

---

## Platform Considerations

### 9front Implementation

**Architecture**:
- Multiple `libdraw` Image structures
- One Image per window
- Shared `screen` for composition

**Window Creation**:
```c
Image* window = allocwindow(
    screen,
    Rect(x, y, x+w, y+h),
    REFRESHnone,
    Chalk
);
```

**Multi-Window Support**:
- Each window has separate Image
- Composited onto shared screen
- Window manager handles Z-order

**Rendering**:
```c
// Draw window frame
border(window, Rect(0, 0, w, h), 2, Borderwidth, display->black);

// Draw widgets
draw_widget_tree(window, widgets);
```

**Event Handling**:
- `Mouse` events from `/dev/mouse`
- Keyboard events from `/dev/cons`
- Dispatch to focused window

### MirageOS Implementation

**Architecture**:
- Multiple framebuffer contexts OR
- Single framebuffer with virtual screens

**Option 1: Multiple Framebuffers**:
```ocaml
let fb1 = Framebuffer.create (width, height) in
let fb2 = Framebuffer.create (width, height) in
(* Compose fb1 and fb2 onto physical framebuffer *)
```

**Option 2: Virtual Screens**:
```ocaml
let virtual_screens = [
  { id = 1; rect = (0, 0, 800, 600); };
  { id = 2; rect = (100, 100, 400, 300); };
];;
```

**Windowing**:
- Software composition (VESA/VGA)
- Window manager in unikernel
- Z-order maintained by server

**Constraints**:
- Limited by framebuffer memory
- May need to tile windows (no overlap)

### Web Platform (Future)

**Implementation**:
- Each window = browser window or iframe
- 9P over WebSockets
- Canvas rendering

**Multi-Window**:
- `window.open()` for new windows
- PostMessage for events
- IndexedDB for shared state

---

## Window Creation Examples

### Main Application Window

```bash
# Create main window
echo "create_window" > /ctl
# Assume window ID 1

# Configure
echo "My App v1.0" > /windows/1/title
echo "100 100 800 600" > /windows/1/rect
echo "1" > /windows/1/resizable
echo "1" > /windows/1/visible

# Create menu bar
echo "create_widget 1 menubar 0" > /ctl
# Assume widget ID 1

# Create content area
echo "create_widget 1 container 0 rect='0 30 800 600'" > /ctl
# Assume widget ID 2
```

### Preferences Dialog

```bash
# Create dialog
echo "create_window 2 modal=1 parent=1" > /ctl

# Configure
echo "Preferences" > /windows/2/title
echo "center center 400 300" > /windows/2/rect
echo "0" > /windows/2/resizable
echo "1" > /windows/2/visible

# Create form widgets
echo "create_widget 2 checkbox 0 label='Enable notifications'" > /ctl
echo "create_widget 2 checkbox 0 label='Dark mode'" > /ctl
echo "create_widget 2 button 0 text='Save' rect='150 250 100 30'" > /ctl
```

### Floating Palette

```bash
# Create palette
echo "create_window" > /ctl
# Assume window ID 3

# Configure
echo "Tools" > /windows/3/title
echo "850 100 200 400" > /windows/3/rect
echo "1" > /windows/3/always_on_top
echo "0" > /windows/3/decorations
echo "1" > /windows/3/visible

# Create tool buttons
for tool in pencil brush eraser; do
    echo "create_widget 3 button 0 icon=$tool" > /ctl
done
```

---

## See Also

- [Filesystem Namespace](02-filesystem-namespace.md) - Complete directory layout
- [9P Specification](01-9p-specification.md) - Wire protocol
- [Permissions](03-permissions.md) - Access control
