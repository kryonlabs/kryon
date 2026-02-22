# Layout System

## Table of Contents

1. [Layout Types](#layout-types)
2. [Layout Properties](#layout-properties)
3. [Size Negotiation](#size-negotiation)
4. [Layout Algorithms](#layout-algorithms)

---

## Layout Types

### Fixed Layout

**Description**: Absolute positioning, no automatic layout

**Properties**:
- `rect`: Explicit position and size

**Use Cases**:
- Canvas-style layouts
- Precise positioning
- Overlapping widgets

**Example**:
```
Container (layout: fixed)
├── Button (rect: "10 10 100 40")
└── Label (rect: "10 60 200 80")
```

### Column Layout

**Description**: Vertical stacking

**Properties**:
- `layout`: "column"
- `gap`: Space between children (pixels)
- `padding`: Internal padding

**Behavior**:
- Children arranged vertically
- Width = parent width (minus padding)
- Height = intrinsic or expanded

**Example**:
```
Container (layout: column, gap: 10, padding: 10)
├── Label (height: 30)
├── TextInput (height: 40)
└── Button (height: 40)
```

**Positions**:
- Label: y=10
- TextInput: y=50 (10 + 30 + 10)
- Button: y=100 (50 + 40 + 10)

### Row Layout

**Description**: Horizontal stacking

**Properties**:
- `layout`: "row"
- `gap`: Space between children (pixels)
- `padding`: Internal padding

**Behavior**:
- Children arranged horizontally
- Height = parent height (minus padding)
- Width = intrinsic or expanded

**Example**:
```
Container (layout: row, gap: 10, padding: 10)
├── Label (width: 100)
├── TextInput (width: 200)
└── Button (width: 80)
```

**Positions**:
- Label: x=10
- TextInput: x=120 (10 + 100 + 10)
- Button: x=330 (120 + 200 + 10)

### Center Layout

**Description**: Center single child

**Properties**:
- `layout`: "center"

**Behavior**:
- Single child centered in parent
- Multiple children: stacked, all centered

**Example**:
```
Container (layout: center, size: 400x300)
└── Button (size: 100x40)
    → Position: (150, 130)  # Centered
```

---

## Layout Properties

### Common Layout Properties

| Property   | Type   | Default | Description                     |
|------------|--------|---------|---------------------------------|
| layout     | enum   | "fixed" | Layout type                     |
| padding    | int    | 0       | Internal padding (pixels)       |
| gap        | int    | 0       | Space between children (pixels) |
| expand     | bool   | 0       | Expand to fill available space  |
| shrink     | bool   | 1       | Shrink to fit content           |
| align      | enum   | "start" | Alignment: start, center, end    |
| weight     | int    | 0       | Distribution priority           |

### Property Details

#### `padding`

**Description**: Space between container edge and children
**Format**: Single integer (all sides) or four integers (top, right, bottom, left)
**Examples**:
- `padding: 10` (10px all sides)
- `padding: "10 20 10 20"` (top=10, right=20, bottom=10, left=20)

#### `gap`

**Description**: Space between children
**Format**: Single integer
**Examples**:
- `gap: 10` (10px between children)

#### `expand`

**Description**: Widget grows to fill available space
**Values**: 0 (fixed size) or 1 (expand)
**Behavior**:
- In column: Expand height
- In row: Expand width
- Multiple expand: Split available space by weight

**Example**:
```
Row (width: 600)
├── Label (width: 100, expand: 0)
├── Spacer (expand: 1)  # Grows to fill
└── Button (width: 80, expand: 0)
```

#### `shrink`

**Description**: Widget shrinks to fit content
**Values**: 0 (fixed size) or 1 (shrink)
**Behavior**:
- If 1: Widget sized to content
- If 0: Widget uses explicit size

#### `align`

**Description**: Alignment within container
**Values**: "start", "center", "end", "stretch"
**Per-Axis**: `align_x`, `align_y` (future)

**Examples**:
```
Column (align: center)
└── All children centered horizontally
```

```
Row (align: center)
└── All children centered vertically
```

#### `weight`

**Description**: Priority for distributing extra space
**Values**: Non-negative integer
**Default**: 0 (no extra space)
**Behavior**: Higher weight = more space

**Example**:
```
Row (width: 600)
├── Col1 (width: 100, weight: 1)
├── Col2 (width: 100, weight: 2)  # Gets 2x extra space
└── Col3 (width: 100, weight: 1)
```

---

## Size Negotiation

### Size Types

Each widget has three sizes:

**Minimum Size** (`min_width`, `min_height`):
- Smallest usable size
- Buttons: text + padding
- Text inputs: system default

**Maximum Size** (`max_width`, `max_height`):
- Largest usable size
- 0 or -1: unlimited

**Preferred Size** (`pref_width`, `pref_height`):
- Ideal size for content
- Labels: text size
- Images: natural size

### Size Negotiation Algorithm

```
1. Parent asks child for min/max/pref sizes
2. Parent calculates available space
3. Parent allocates space based on layout + properties
4. Parent sets child's rect
5. Child recalculates its own children (recursive)
```

### Example: Column Layout

```
Container (size: 400x300, layout: column, gap: 10, padding: 10)

Children:
├── Label: min=(0,20), pref=(200,20), max=(inf,20)
├── TextInput: min=(100,40), pref=(200,40), max=(inf,40)
└── Button: min=(80,40), pref=(100,40), max=(inf,40)

Available height: 300 - 20 (padding) - 20 (gap) = 260

Allocation:
- Label: 20 (pref)
- TextInput: 40 (pref)
- Button: 40 (pref)
- Remaining: 260 - 100 = 160

If children expand:
- All expand: 160 / 3 = 53.33 extra each
- Only TextInput expand: 160 extra to TextInput
```

---

## Layout Algorithms

### Column Algorithm

```
available_width = parent_width - 2*padding
available_height = parent_height - 2*padding

y = padding
for each child:
    child_width = available_width
    child_height = child.pref_height

    if child.expand:
        child_height += share of extra space

    child.rect = (padding, y, padding + child_width, y + child_height)
    y += child_height + gap
```

### Row Algorithm

```
available_height = parent_height - 2*padding
available_width = parent_width - 2*padding

x = padding
for each child:
    child_height = available_height
    child_width = child.pref_width

    if child.expand:
        child_width += share of extra space

    child.rect = (x, padding, x + child_width, padding + child_height)
    x += child_width + gap
```

### Center Algorithm

```
for each child:
    child_width = child.pref_width
    child_height = child.pref_height

    x = (parent_width - child_width) / 2
    y = (parent_height - child_height) / 2

    child.rect = (x, y, x + child_width, y + child_height)
```

---

## Overflow Handling

### Scrollable Containers

**Property**: `overflow: "scroll"` or `"auto"`

**Behavior**:
- Content larger than container → Scrollbars appear
- Scroll positions exposed as properties

**Example**:
```
ScrollArea (size: 400x300, overflow: scroll)
└── Content (size: 400x600)
    → Vertical scrollbar appears
```

### Clipping

**Property**: `overflow: "hidden"`

**Behavior**:
- Content outside container bounds not rendered
- No scrollbars

### Visible Overflow

**Property**: `overflow: "visible"`

**Behavior**:
- Content rendered outside container bounds
- May overlap other widgets

---

## Layout State

### Dirty Flag

**Triggered By**:
- Child added/removed
- Child rect changed
- Container rect changed
- Layout property changed

**Behavior**:
- Set `dirty = 1`
- On next render, recalculate layout
- Reset `dirty = 0`

### Layout Pass

```
function layout_pass(widget):
    if widget.dirty:
        recalculate_layout(widget)
        widget.dirty = 0

    for each child:
        layout_pass(child)
```

---

## See Also

- [Base Widget Interface](00-base-widget.md) - Widget lifecycle
- [Property System](01-property-system.md) - Layout properties
- [Widget Catalog](04-widget-catalog.md) - Container widgets
