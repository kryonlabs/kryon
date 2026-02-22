# Event System

## Table of Contents

1. [Base Events](#base-events)
2. [Mouse Events](#mouse-events)
3. [Keyboard Events](#keyboard-events)
4. [Touch Events](#touch-events)
5. [Event Format](#event-format)
6. [Delivery Guarantees](#delivery-guarantees)

---

## Base Events

All widgets generate these events:

### `visible`

**Trigger**: Widget becomes visible
**Parameters**: None
**Example**: `visible`

### `hidden`

**Trigger**: Widget becomes hidden
**Parameters**: None
**Example**: `hidden`

### `enabled`

**Trigger**: Widget becomes enabled
**Parameters**: None
**Example**: `enabled`

### `disabled`

**Trigger**: Widget becomes disabled
**Parameters**: None
**Example**: `disabled`

### `focused`

**Trigger**: Widget receives keyboard focus
**Parameters**: None
**Example**: `focused`

### `blurred`

**Trigger**: Widget loses keyboard focus
**Parameters**: None
**Example**: `blurred`

### `parent_changed`

**Trigger**: Widget is reparented
**Parameters**: `old_parent_id`, `new_parent_id`
**Example**: `parent_changed old_parent_id=2 new_parent_id=5`

### `bounds_changed`

**Trigger**: Widget's `rect` property changes
**Parameters**: `old_rect`, `new_rect`
**Example**: `bounds_changed old_rect='10 10 100 50' new_rect='20 20 150 60'`

---

## Mouse Events

### `mouse_enter`

**Trigger**: Mouse cursor enters widget area
**Parameters**: `x`, `y`
**Example**: `mouse_enter x=50 y=25`

### `mouse_leave`

**Trigger**: Mouse cursor leaves widget area
**Parameters**: None
**Example**: `mouse_leave`

### `mouse_move`

**Trigger**: Mouse moves within widget area
**Parameters**: `x`, `y`
**Throttled**: Maximum 60 events per second
**Example**: `mouse_move x=75 y=30`

### `mouse_down`

**Trigger**: Mouse button pressed
**Parameters**: `x`, `y`, `button`
**Button Codes**:
- `1`: Left button
- `2`: Middle button
- `3`: Right button
- `4`: Scroll up
- `5`: Scroll down
**Example**: `mouse_down x=50 y=25 button=1`

### `mouse_up`

**Trigger**: Mouse button released
**Parameters**: `x`, `y`, `button`
**Example**: `mouse_up x=52 y=26 button=1`

### `click`

**Trigger**: Full click (down + up on same widget)
**Parameters**: `x`, `y`, `button`
**Example**: `click x=50 y=25 button=1`

### `double_click`

**Trigger**: Two clicks within 500ms
**Parameters**: `x`, `y`, `button`
**Example**: `double_click x=50 y=25 button=1`

### `triple_click`

**Trigger**: Three clicks within 500ms
**Parameters**: `x`, `y`, `button`
**Example**: `triple_click x=50 y=25 button=1`

---

## Keyboard Events

### `key_down`

**Trigger**: Key pressed
**Parameters**: `key`, `modifiers`
**Key Codes**:
- Printable: ASCII character
- Special: `Enter`, `Tab`, `Escape`, `Backspace`, `Delete`, `Insert`, `Home`, `End`, `PageUp`, `PageDown`, `Up`, `Down`, `Left`, `Right`, `F1`-`F12`
**Modifiers**: Comma-separated list of `Shift`, `Ctrl`, `Alt`, `Meta`
**Example**: `key_down key=Enter modifiers=Ctrl`

### `key_up`

**Trigger**: Key released
**Parameters**: `key`, `modifiers`
**Example**: `key_up key=a modifiers=Shift`

### `key_press`

**Trigger**: Character generated (after key composition)
**Parameters**: `char`
**Example**: `key_press char=A`

---

## Touch Events

### `touch_start`

**Trigger**: Touch begins
**Parameters**: `id`, `x`, `y`
**ID**: Touch point identifier (for multi-touch)
**Example**: `touch_start id=1 x=100 y=200`

### `touch_move`

**Trigger**: Touch moves
**Parameters**: `id`, `x`, `y`
**Example**: `touch_move id=1 x=120 y=220`

### `touch_end`

**Trigger**: Touch ends
**Parameters**: `id`
**Example**: `touch_end id=1`

### `touch_cancel`

**Trigger**: Touch cancelled (system interrupt)
**Parameters**: `id`
**Example**: `touch_cancel id=1`

---

## Event Format

### Format Specification

```
<event_name> [key1=value1] [key2=value2] ...
```

**Rules**:
- Event name first (lowercase, underscores)
- Parameters: `key=value` format
- Space-separated
- No quotes around values (unless containing spaces)
- Newline-terminated

### Value Encoding

**Strings**: No quotes (unless containing spaces)
```
text_input_changed text=Hello World
```

**Integers**: Decimal
```
value_changed value=42
```

**Booleans**: 0 or 1
```
toggled state=1
```

**Enums**: Lowercase
```
state_changed state=maximized
```

**Special Characters**: URL-encoded if needed
```
text_changed text=Hello%20World%21
```

---

## Delivery Guarantees

### Ordered Delivery

Events are delivered in order of occurrence:
1. Events queued per-widget
2. FIFO delivery to readers
3. No reordering

### No Event Dropping

**Policy**: Events never dropped
- If queue full, block writes
- Backpressure to event source
- Controller must drain pipe

**Exception**: `mouse_move` events may be dropped (throttled to 60/sec)

### Filtering

**Visibility Filter**:
- Hidden widgets don't receive events
- Events queued while hidden, delivered when shown

**Enabled Filter**:
- Disabled widgets don't receive input events (mouse, keyboard, touch)
- Still receive state events (visible, enabled, focused, etc.)

**Focus Filter**:
- Keyboard events only sent to focused widget
- Exception: Tab (focus navigation), global shortcuts

### Event Propagation

**Capturing Phase** (future):
- Events propagate from root to target
- Ancestors can intercept

**Bubbling Phase** (future):
- Events propagate from target to root
- Ancestors can handle

**Current**: No propagation (direct delivery to widget)

---

## Widget-Specific Events

### Button

| Event    | Parameters  | Description                     |
|----------|-------------|---------------------------------|
| clicked  | x, y, button | Button clicked                |

### TextInput

| Event    | Parameters  | Description                     |
|----------|-------------|---------------------------------|
| changed  | value       | Text content changed            |
| submitted| value       | Enter key pressed               |

### Checkbox

| Event    | Parameters  | Description                     |
|----------|-------------|---------------------------------|
| changed  | value       | Checked state changed (0/1)     |

### Slider

| Event    | Parameters  | Description                     |
|----------|-------------|---------------------------------|
| changed  | value       | Slider value changed            |

### Dropdown

| Event    | Parameters  | Description                     |
|----------|-------------|---------------------------------|
| changed  | value       | Selection changed               |
| opened   | -           | Dropdown opened                 |
| closed   | -           | Dropdown closed                 |

---

## See Also

- [Base Widget Interface](00-base-widget.md) - Event pipe
- [Property System](01-property-system.md) - Property change events
- [Widget Catalog](04-widget-catalog.md) - Widget-specific events
