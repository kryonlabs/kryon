# Base Widget Interface

## Table of Contents

1. [Mandatory Files](#mandatory-files)
2. [Mandatory Behaviors](#mandatory-behaviors)
3. [Lifecycle Methods](#lifecycle-methods)
4. [Coordinate System](#coordinate-system)
5. [Z-Order](#z-order)

---

## Mandatory Files

Every widget has these mandatory files:

```
/windows/<window_id>/widgets/<widget_id>/
├── type           # Read-only: Widget type string
├── rect           # Read/write: "x0 y0 x1 y1" or "x y width height"
├── visible        # Read/write: 0 or 1
├── enabled        # Read/write: 0 or 1
├── focused        # Read-only: 0 or 1
└── event          # Read-only: Event pipe
```

### File Specifications

#### `type` (Read-Only String)

- **Format**: Widget type identifier (lowercase)
- **Examples**: "button", "textinput", "label", "container"
- **Purpose**: Type identification for reflection/validation
- **Permissions**: `0444` (read-only)

**Example**:
```bash
cat /windows/1/widgets/5/type
# Output: button
```

#### `rect` (Read/Write Rectangle)

- **Format**: `"x0 y0 x1 y1"` or `"x y width height"`
- **Coordinates**: Parent-relative (not screen-relative)
- **Validation**:
  - `x1 > x0` and `y1 > y0` (must have positive area)
  - Coordinates within parent bounds (enforced by layout)
- **Permissions**: `0644` (read/write)

**Formats**:

**Corner format** (`x0 y0 x1 y1`):
```
"10 10 110 60"  # x=10, y=10, width=100, height=50
```

**Size format** (`x y width height`):
```
"10 10 100 50"  # Same as above
```

**Conversion**:
```
corner_to_size(x0, y0, x1, y1) = (x0, y0, x1-x0, y1-y0)
size_to_corner(x, y, w, h) = (x, y, x+w, y+h)
```

**Example**:
```bash
# Set position and size
echo "10 10 100 50" > /windows/1/widgets/5/rect

# Read current geometry
cat /windows/1/widgets/5/rect
# Output: 10 10 110 60  (corner format)
```

#### `visible` (Read/Write Boolean)

- **Values**: `0` (hidden) or `1` (shown)
- **Default**: `1`
- **Behavior**:
  - `0`: Widget not rendered, doesn't receive events
  - `1`: Widget rendered, receives events
  - Invisible widgets still occupy layout space
- **Child Propagation**: When parent becomes invisible, all children become invisible
- **Permissions**: `0644` (read/write)

**Example**:
```bash
echo "0" > /windows/1/widgets/5/visible  # Hide widget
echo "1" > /windows/1/widgets/5/visible  # Show widget
```

#### `enabled` (Read/Write Boolean)

- **Values**: `0` (disabled) or `1` (enabled)
- **Default**: `1`
- **Behavior**:
  - `0`: Widget shown but grayed out, doesn't receive input events
  - `1`: Widget receives input events
- **Child Propagation**: When parent disabled, all children disabled
- **Permissions**: `0644` (read/write)

**Visual Feedback**:
- Grayed out appearance
- Cursor changes to "not-allowed"
- No response to clicks/keyboard

**Example**:
```bash
echo "0" > /windows/1/widgets/5/enabled  # Disable widget
echo "1" > /windows/1/widgets/5/enabled  # Enable widget
```

#### `focused` (Read-Only Boolean)

- **Values**: `0` or `1`
- **Behavior**: Indicates widget has keyboard focus
- **Focus Rules**:
  - Only one widget per window has focus
  - Clicking widget gives it focus
  - Tab navigates between focusable widgets
- **Permissions**: `0444` (read-only)

**Focusable Widgets**:
- TextInput, TextArea, PasswordInput
- Dropdown, MultiSelect, ListBox
- Button, SubmitButton
- (Non-focusable: Label, Divider, etc.)

**Example**:
```bash
cat /windows/1/widgets/5/focused
# Output: 1 (has focus) or 0 (doesn't have focus)
```

#### `event` (Read-Only Pipe)

- **Type**: Named pipe
- **Behavior**: Blocking read until event available
- **Format**: One event per line (newline-terminated)
- **Permissions**: `0444` (read-only)

**Event Format**:
```
<event_name> [key=value] [key=value]...
```

**Example**:
```bash
# Wait for event (blocks)
cat /windows/1/widgets/5/event
# Output: clicked x=50 y=20 button=1

# Next read waits for next event
cat /windows/1/widgets/5/event
# Output: changed value=42
```

---

## Mandatory Behaviors

### Rectangle Validation

All `rect` writes are validated:

```c
// Pseudocode
function validate_rect(rect) {
    if (rect.x1 <= rect.x0) return error(EBADRECT);
    if (rect.y1 <= rect.y0) return error(EBADRECT);
    if (rect.width < 0) return error(EBADRECT);
    if (rect.height < 0) return error(EBADRECT);
    return ok;
}
```

**Error Response**:
```
# Invalid rectangle
echo "10 10 5 20" > /windows/1/widgets/5/rect
# Error: Invalid rectangle (x1 must be > x0)
```

### Coordinate System

**Origin**: Top-left corner
**X Axis**: Positive right
**Y Axis**: Positive down

```
(0,0) ──────► X
  │
  │    ┌──────────┐
  │    │ Widget   │
  │    │ (x,y)    │
  ▼    └──────────┘
  Y
```

**Parent-Relative Coordinates**:
- Widget positions relative to parent
- Root widgets relative to window (0,0 is top-left of window content area)

**Screen Coordinates**:
- Screen X = Window X + Widget X
- Screen Y = Window Y + Widget Y + TitleBarHeight

### Visibility Propagation

**Inherit Visibility**:
```
visible = self.visible AND parent.visible AND parent.parent.visible ...
```

**Example**:
```
Window (visible=1)
└── Container (visible=1)
    └── Button (visible=1)
        → Actually visible (1 AND 1 AND 1 = 1)

Window (visible=1)
└── Container (visible=0)
    └── Button (visible=1)
        → Actually hidden (1 AND 0 AND 1 = 0)
```

**Events from Hidden Widgets**:
- Hidden widgets don't receive events
- Events queued while hidden, delivered when shown
- Focus lost when hidden

### Enabled State Propagation

**Inherit Enabled**:
```
enabled = self.enabled AND parent.enabled AND parent.parent.enabled ...
```

**Example**:
```
Window (enabled=1)
└── Container (enabled=1)
    └── Button (enabled=1)
        → Actually enabled

Window (enabled=1)
└── Container (enabled=0)
    └── Button (enabled=1)
        → Actually disabled (can't click)
```

**Disabled Widget Behavior**:
- Still receives mouse events (for cursor feedback)
- Doesn't generate `clicked` or `changed` events
- Visual: Grayed out, "not-allowed" cursor

---

## Lifecycle Methods

### Creation Flow

```
1. create_widget command → /ctl
2. Server allocates widget ID
3. Server creates /windows/<wid>/widgets/<id>/ directory
4. Server creates mandatory files (type, rect, visible, enabled, event)
5. Server sets initial properties from command args
6. Widget enters "initialized" state
7. Widget added to parent's children list
8. Layout recalculated
9. Widget rendered
```

### States

```
[Uninitialized] → [Created] → [Attached] → [Visible] → [Focused]
                      │           │           │
                      ▼           ▼           ▼
                 [Destroyed]  [Detached]   [Hidden]
```

### State Transitions

**Created**:
- Widget directory exists
- Properties initialized to defaults
- Not yet in parent's children list

**Attached**:
- Widget in parent's children list
- Layout calculated
- Rendered (if visible)

**Detached**:
- Removed from parent's children list
- No longer rendered
- Can be reattached to new parent

**Destroyed**:
- Widget directory deleted
- All children recursively destroyed
- ID becomes reusable

### Destruction Flow

```
1. destroy_widget command → /ctl
2. Server removes widget from parent's children list
3. Server recursively destroys all children
4. Server deletes widget directory
5. Server deallocates widget ID
6. Layout recalculated
7. Remaining widgets rerendered
```

---

## Z-Order

### Z-Order Within Parent

Widgets drawn in order (first child = bottom, last child = top):

```
Container
├── Widget 1 (drawn first, appears bottom)
├── Widget 2 (drawn second)
└── Widget 3 (drawn last, appears top)
```

### Changing Z-Order

**Reparenting**:
```bash
# Move widget to end (top)
echo "reparent_widget 1 5 2" > /ctl
# (moves widget 5 to be last child of parent 2)
```

**Future**: `z_index` property for explicit control

### Overlapping Widgets

- Higher Z-order widgets receive events first
- Events propagate down if not handled
- Use `z_index` (future) to control layering

---

## Implementation Requirements

### For 9front (libdraw)

```c
struct Widget {
    int id;
    char* type;
    Rectangle rect;      // libdraw Rectangle
    int visible;
    int enabled;
    int focused;
    Image* image;        // Rendering target
    struct Widget** children;
    int num_children;
};
```

### For MirageOS (mirage-framebuffer)

```ocaml
type widget = {
    id: int;
    widget_type: string;
    rect: (int * int * int * int);  (* x0, y0, x1, y1 *)
    mutable visible: bool;
    mutable enabled: bool;
    mutable focused: bool;
    children: widget array;
}
```

---

## See Also

- [Property System](01-property-system.md) - Property types and validation
- [Event System](02-event-system.md) - Event delivery
- [Layout System](03-layout-system.md) - Automatic layout
- [Widget Catalog](04-widget-catalog.md) - All widget specifications
