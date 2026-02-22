# Property System

## Table of Contents

1. [Property Types](#property-types)
2. [Property Metadata](#property-metadata)
3. [Property Inheritance](#property-inheritance)
4. [Validation Rules](#validation-rules)

---

## Property Types

### Primitive Types

#### `string` (Text)

- **Format**: UTF-8 encoded text
- **Max Length**: Varies by property (typically 256-4096 characters)
- **Validation**: Valid UTF-8
- **Examples**: "Hello", "Button Label", ""

#### `int` (Integer)

- **Format**: Signed 32-bit integer
- **Range**: -2^31 to 2^31-1
- **Validation**: Must be within property-specific range
- **Examples**: 0, 42, -100

#### `real` (Floating Point)

- **Format**: 64-bit IEEE 754 floating point
- **Range**: ±1.7976931348623157×10^308
- **Precision**: ~15-17 decimal digits
- **Examples**: 3.14, -0.5, 1.0e10

#### `bool` (Boolean)

- **Format**: 0 or 1
- **Aliases**: "false"/"true", "no"/"yes" (normalized to 0/1)
- **Examples**: 0, 1

#### `color` (Color)

- **Format**: "#RRGGBB" (hexadecimal)
- **Alternative**: Named color (e.g., "red", "blue")
- **Examples**: "#FF0000" (red), "#00FF00" (green), "black"
- **Named Colors**: CSS color names (red, green, blue, black, white, gray, etc.)

### Geometric Types

#### `rect` (Rectangle)

- **Format 1**: "x0 y0 x1 y1" (corner format)
- **Format 2**: "x y width height" (size format)
- **Validation**: x1 > x0, y1 > y0, width > 0, height > 0
- **Examples**: "10 10 110 60", "0 0 800 600"

#### `point` (Point)

- **Format**: "x y"
- **Validation**: Any integer coordinates
- **Examples**: "100 200", "0 0"

#### `size` (Size)

- **Format**: "width height"
- **Validation**: width >= 0, height >= 0
- **Examples**: "100 50", "800 600"

### Enumerated Types

#### `enum` (Enumeration)

- **Format**: One of predefined string values
- **Case-Sensitive**: Depends on enum (usually lowercase)
- **Examples**: "normal", "minimized", "maximized", "fullscreen"

### Collection Types

#### `array` (Array)

- **Format**: Comma-separated values
- **Examples**: "1,2,3,4", "item1,item2,item3"
- **Validation**: All elements valid for element type

#### `font` (Font Specification)

- **Format**: "family size weight style"
- **Components**:
  - `family`: Font name (e.g., "Arial", "Helvetica")
  - `size`: Point size (integer)
  - `weight`: "normal", "bold" (optional)
  - `style`: "normal", "italic" (optional)
- **Examples**: "Arial 12", "Helvetica 14 bold", "Times 10 italic"

---

## Property Metadata

For each property, the following metadata is defined:

### Metadata Schema

```c
struct PropertyMetadata {
    char* name;              // Property name
    PropertyType type;       // Data type
    AccessMode access;       // Read, Write, ReadOnly
    DefaultValue default;    // Default value
    ValidationFn validate;   // Validation function
    UpdateTrigger trigger;   // When to update
    Persistence persist;     // Persistence mode
};
```

### Access Modes

| Mode       | Description                    |
|------------|--------------------------------|
| ReadOnly   | Property cannot be changed     |
| WriteOnly  | Property can only be written   |
| ReadWrite  | Property can be read/written   |

### Update Triggers

| Trigger     | Description                              |
|-------------|------------------------------------------|
| Immediate   | Property applied immediately             |
| OnChange    | Applied when value changes               |
| OnCommit    | Applied on explicit commit (future)      |
| OnRender    | Applied during next render frame         |

### Persistence Modes

| Mode        | Description                              |
|-------------|------------------------------------------|
| None        | Not persisted (reset on restart)         |
| Session     | Persisted for current session            |
| Permanent   | Persisted across sessions (future)       |

---

## Property Inheritance

### Cascading Properties

Some properties cascade from parent to children:

**Cascade Properties**:
- `font` (if child doesn't specify)
- `color` (text color)
- `background_color`
- `opacity`

**Inheritance Algorithm**:
```
effective_font = widget.font OR parent_font OR default_font
effective_color = widget.color OR parent_color OR default_color
```

### Override Behavior

**Child Overrides Parent**:
```
Container (font = "Arial 12")
└── Label (font = "Courier 10")
    → Effective font: "Courier 10" (child overrides)
```

**Child Inherits Parent**:
```
Container (font = "Arial 12")
└── Label (font = "")
    → Effective font: "Arial 12" (inherits)
```

### Inheritance Chain Resolution

**Order** (highest priority first):
1. Widget's own property value
2. Parent's property value
3. Grandparent's property value
4. ... (continue up tree)
5. Theme default
6. Platform default

---

## Validation Rules

### String Validation

**Length Limits**:
```c
if (strlen(value) > max_length) {
    return error(EINVAL, "String too long");
}
```

**Pattern Matching**:
```c
// Email validation (simplified)
if (!matches_regex(value, "^[^@]+@[^@]+$")) {
    return error(EINVAL, "Invalid email format");
}
```

### Integer Validation

**Range Check**:
```c
if (value < min_value || value > max_value) {
    return error(EOUTOFRANGE, "Value out of range");
}
```

**Example**:
```
Property: min_value (type: int)
Range: 0 to 100
Value: 150
→ Error: "Value out of range: 150 (valid: 0-100)"
```

### Color Validation

**Hex Format**:
```c
if (!matches_regex(value, "^#[0-9A-Fa-f]{6}$")) {
    return error(EBADCOLOR, "Invalid color format");
}
```

**Named Color**:
```c
if (!is_named_color(value)) {
    return error(EBADCOLOR, "Unknown color name");
}
```

### Rectangle Validation

**Format 1** (corner):
```c
if (sscanf(value, "%d %d %d %d", &x0, &y0, &x1, &y1) != 4) {
    return error(EBADRECT, "Invalid rectangle format");
}
if (x1 <= x0 || y1 <= y0) {
    return error(EBADRECT, "Invalid rectangle dimensions");
}
```

**Format 2** (size):
```c
if (sscanf(value, "%d %d %d %d", &x, &y, &w, &h) != 4) {
    return error(EBADRECT, "Invalid rectangle format");
}
if (w <= 0 || h <= 0) {
    return error(EBADRECT, "Invalid rectangle dimensions");
}
```

### Enum Validation

```c
bool valid = false;
for (int i = 0; i < num_valid_values; i++) {
    if (strcmp(value, valid_values[i]) == 0) {
        valid = true;
        break;
    }
}
if (!valid) {
    return error(EINVAL, "Invalid enum value");
}
```

---

## Common Properties

### Text Properties

| Property    | Type   | Default  | Description                     |
|-------------|--------|----------|---------------------------------|
| text        | string | ""       | Text content                    |
| placeholder | string | ""       | Placeholder text (shown when empty) |
| max_length  | int    | 0 (unlimited) | Maximum character count   |

### Appearance Properties

| Property          | Type  | Default    | Description                     |
|-------------------|-------|------------|---------------------------------|
| color             | color | "#000000"  | Text/foreground color           |
| background_color  | color | "#FFFFFF"  | Background color                |
| font              | font  | "Arial 12" | Font specification              |
| opacity           | real  | 1.0        | Opacity (0.0 = transparent, 1.0 = opaque) |

### Layout Properties

| Property    | Type   | Default  | Description                     |
|-------------|--------|----------|---------------------------------|
| rect        | rect   | "0 0 100 100" | Position and size          |
| padding     | int    | 0        | Internal padding (pixels)       |
| margin      | int    | 0        | External margin (pixels)        |
| align       | enum   | "start"  | Alignment: start, center, end    |

### State Properties

| Property    | Type   | Default  | Description                     |
|-------------|--------|----------|---------------------------------|
| visible     | bool   | 1        | Visibility                      |
| enabled     | bool   | 1        | Enabled state                   |
| focused     | bool   | 0        | Has keyboard focus (read-only)  |

---

## Property Change Events

When a property changes, events are generated:

### `property_changed` Event

```
property_changed property=<name> old=<value> new=<value>
```

**Example**:
```bash
cat /windows/1/widgets/5/event
# Output: property_changed property=text old=Hello new=World
```

### Affected Properties

Some properties trigger additional events:

| Property   | Additional Events           |
|------------|----------------------------|
| visible    | `visible`, `hidden`         |
| enabled    | `enabled`, `disabled`       |
| rect       | `bounds_changed`, `resized` |
| text/value | `changed`                   |

---

## See Also

- [Base Widget Interface](00-base-widget.md) - Mandatory properties
- [Event System](02-event-system.md) - Property change events
- [Widget Catalog](04-widget-catalog.md) - Widget-specific properties
