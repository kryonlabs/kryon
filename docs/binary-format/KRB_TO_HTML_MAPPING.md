# KRB to HTML Complete Mapping Reference

This document provides a comprehensive mapping between Kryon's binary KRB format and HTML elements/properties, enabling accurate HTML generation from KRB files.

## Table of Contents
- [Element Mappings](#element-mappings)
- [Property Mappings](#property-mappings)
- [Special Handling Cases](#special-handling-cases)
- [HTML Generation Examples](#html-generation-examples)
- [CSS Property Generation](#css-property-generation)
- [JavaScript Event Mapping](#javascript-event-mapping)

## Element Mappings

### Core Elements (0x00-0x0F)

| KRB Hex | Element Name | HTML Output | CSS Classes | Special Handling |
|---------|--------------|-------------|-------------|------------------|
| `0x00` | App | `<div class="kryon-app">` | `.kryon-app` | Root container, viewport setup |
| `0x01` | Container | `<div class="kryon-container">` | `.kryon-container` | Generic layout container |
| `0x02` | Text | `<span class="kryon-text">` or `<p>` | `.kryon-text` | Content-based element choice |
| `0x03` | EmbedView | `<iframe class="kryon-embed">` | `.kryon-embed` | Depends on embed-type property |

### Interactive Elements (0x10-0x1F)

| KRB Hex | Element Name | HTML Output | Attributes | Event Handlers |
|---------|--------------|-------------|------------|----------------|
| `0x10` | Button | `<button class="kryon-button">` | `type="button"` | `onclick`, `onhover` |
| `0x11` | Input | `<input class="kryon-input">` | `type` varies | `onchange`, `oninput` |
| `0x12` | Link | `<a class="kryon-link">` | `href` | `onclick` |

### Media Elements (0x20-0x2F)

| KRB Hex | Element Name | HTML Output | Required Attributes | CSS Handling |
|---------|--------------|-------------|-------------------|--------------|
| `0x20` | Image | `<img class="kryon-image">` | `src`, `alt` | Object-fit properties |
| `0x21` | Video | `<video class="kryon-video">` | `src`, `controls` | Media queries |
| `0x22` | Canvas | `<canvas class="kryon-canvas">` | `width`, `height` | WebGL/2D context |
| `0x23` | Svg | `<svg class="kryon-svg">` | `viewBox` | Inline SVG handling |

### Form Controls (0x30-0x3F)

| KRB Hex | Element Name | HTML Output | Special Properties | Validation |
|---------|--------------|-------------|-------------------|------------|
| `0x30` | Select | `<select class="kryon-select">` | Options array | Required, pattern |
| `0x31` | Slider | `<input type="range" class="kryon-slider">` | `min`, `max`, `step` | Value constraints |
| `0x32` | ProgressBar | `<progress class="kryon-progress">` | `value`, `max` | Percentage display |
| `0x33` | Checkbox | `<input type="checkbox" class="kryon-checkbox">` | `checked` | Boolean state |
| `0x34` | RadioGroup | `<fieldset class="kryon-radio-group">` | `name` grouping | Single selection |
| `0x35` | Toggle | `<input type="checkbox" class="kryon-toggle">` | Custom styling | Switch appearance |
| `0x36` | DatePicker | `<input type="date" class="kryon-date">` | Date format | Locale support |
| `0x37` | FilePicker | `<input type="file" class="kryon-file">` | `accept`, `multiple` | File handling |

### Semantic Elements (0x40-0x4F)

| KRB Hex | Element Name | HTML Output | Semantic Role | Accessibility |
|---------|--------------|-------------|---------------|---------------|
| `0x40` | Form | `<form class="kryon-form">` | Form container | Form validation |
| `0x41` | List | `<ul class="kryon-list">` or `<ol>` | List type property | List semantics |
| `0x42` | ListItem | `<li class="kryon-list-item">` | List item | Bullet/number styling |

### Table Elements (0x50-0x5F)

| KRB Hex | Element Name | HTML Output | Table Structure | CSS Grid Fallback |
|---------|--------------|-------------|----------------|-------------------|
| `0x50` | Table | `<table class="kryon-table">` | Table wrapper | `display: grid` |
| `0x51` | TableRow | `<tr class="kryon-table-row">` | Row container | Grid row |
| `0x52` | TableCell | `<td class="kryon-table-cell">` | Data cell | Grid cell |
| `0x53` | TableHeader | `<th class="kryon-table-header">` | Header cell | Grid header |

## Property Mappings

### Property Alias Resolution

Before properties are mapped to HTML/CSS, all aliases are resolved to their canonical forms:

```kry
# These all resolve to the same property (0x01)
backgroundColor: "red"      # Canonical camelCase form
bg: "red"                   # Short alias

# These all resolve to textAlignment (0x13)
textAlignment: "center"     # Canonical camelCase form  
align: "center"             # Short alias
```

### Visual Properties (0x01-0x0F)

| KRB Hex | Property Name | CSS Property | Value Processing | Example |
|---------|---------------|--------------|------------------|---------|
| `0x01` | backgroundColor | `background-color` | Color conversion | `#FF0000` → `background-color: #FF0000` |
| `0x02` | textColor | `color` | Color conversion | `white` → `color: white` |
| `0x03` | borderColor | `border-color` | Color conversion | `black` → `border-color: black` |
| `0x04` | borderWidth | `border-width` | Pixel units | `2` → `border-width: 2px` |
| `0x05` | borderRadius | `border-radius` | Pixel units | `8` → `border-radius: 8px` |
| `0x06` | opacity | `opacity` | Direct mapping | `0.8` → `opacity: 0.8` |
| `0x07` | visibility | `visibility` | Boolean to visible/hidden | `false` → `visibility: hidden` |
| `0x08` | cursor | `cursor` | Direct mapping | `pointer` → `cursor: pointer` |
| `0x09` | boxShadow | `box-shadow` | Shadow string parsing | Complex shadow syntax |
| `0x0A` | imageSource | `src` attribute | URL processing | Path resolution |

### Text Properties (0x10-0x1F)

| KRB Hex | Property Name | CSS Property | Value Processing | Special Cases |
|---------|---------------|--------------|------------------|---------------|
| `0x10` | textContent | Text content | String handling | Rich text parsing |
| `0x11` | fontSize | `font-size` | Pixel units | `16` → `font-size: 16px` |
| `0x12` | fontWeight | `font-weight` | Weight mapping | `700` → `font-weight: 700` |
| `0x13` | textAlignment | `text-align` | Direct mapping | `center` → `text-align: center` |
| `0x14` | fontFamily | `font-family` | Font stack | Fallback fonts |
| `0x15` | whiteSpace | `white-space` | Direct mapping | Pre/nowrap handling |
| `0x16` | listStyleType | `list-style-type` | List markers | Bullet/number styles |

### Layout Properties (0x20-0x2F)

| KRB Hex | Property Name | CSS Property | Value Processing | Responsive Handling |
|---------|---------------|--------------|------------------|---------------------|
| `0x20` | width | `width` | Dimension parsing | `100px`, `50%`, `auto` |
| `0x21` | height | `height` | Dimension parsing | `200px`, `auto` |
| `0x22` | layout-flags | Multiple CSS props | Flag decoding | Flexbox/grid setup |
| `0x23` | gap | `gap` | Pixel units | Flexbox/grid gaps |
| `0x24` | z-index | `z-index` | Integer value | Stacking context |
| `0x25` | display | `display` | Direct mapping | Block/flex/grid |
| `0x26` | position | `position` | Direct mapping | Static/relative/absolute |
| `0x27` | transform | `transform` | Transform matrix | Matrix calculations |
| `0x28` | style-id | CSS class | Class generation | Style references |

### Size Constraints (0x30-0x3F)

| KRB Hex | Property Name | CSS Property | Constraint Logic |
|---------|---------------|--------------|------------------|
| `0x30` | min-width | `min-width` | Minimum bounds |
| `0x31` | min-height | `min-height` | Minimum bounds |
| `0x32` | max-width | `max-width` | Maximum bounds |
| `0x33` | max-height | `max-height` | Maximum bounds |

### Flexbox Properties (0x40-0x4F)

| KRB Hex | Property Name | CSS Property | Flexbox Context |
|---------|---------------|--------------|-----------------|
| `0x40` | flex-direction | `flex-direction` | Main axis direction |
| `0x41` | flex-wrap | `flex-wrap` | Wrap behavior |
| `0x42` | flex-grow | `flex-grow` | Growth factor |
| `0x43` | flex-shrink | `flex-shrink` | Shrink factor |
| `0x44` | flex-basis | `flex-basis` | Initial size |
| `0x45` | align-items | `align-items` | Cross-axis alignment |
| `0x46` | align-content | `align-content` | Multi-line alignment |
| `0x47` | align-self | `align-self` | Self alignment |
| `0x48` | justify-content | `justify-content` | Main axis distribution |
| `0x49` | justify-items | `justify-items` | Grid item justification |
| `0x4A` | justify-self | `justify-self` | Self justification |

### Position Properties (0x50-0x5F)

| KRB Hex | Property Name | CSS Property | Position Context |
|---------|---------------|--------------|------------------|
| `0x50` | left | `left` | Positioned elements |
| `0x51` | top | `top` | Positioned elements |
| `0x52` | right | `right` | Positioned elements |
| `0x53` | bottom | `bottom` | Positioned elements |

### Spacing Properties (0x70-0x7F)

| KRB Hex | Property Name | CSS Property | Box Model |
|---------|---------------|--------------|-----------|
| `0x70` | padding | `padding` | All sides |
| `0x71` | padding-top | `padding-top` | Top side |
| `0x72` | padding-right | `padding-right` | Right side |
| `0x73` | padding-bottom | `padding-bottom` | Bottom side |
| `0x74` | padding-left | `padding-left` | Left side |
| `0x75` | margin | `margin` | All sides |
| `0x76` | margin-top | `margin-top` | Top side |
| `0x77` | margin-right | `margin-right` | Right side |
| `0x78` | margin-bottom | `margin-bottom` | Bottom side |
| `0x79` | margin-left | `margin-left` | Left side |

## Special Handling Cases

### 1. Layout Flags Decoding (0x22)

The layout-flags property uses binary encoding that must be decoded to CSS:

```
Flag Value → CSS Properties
0x00 (00000000) → display: flex; flex-direction: row;
0x01 (00000001) → display: flex; flex-direction: column;
0x02 (00000010) → position: absolute;
0x04 (00000100) → align-items: center; justify-content: center;
0x20 (00100000) → flex-grow: 1;
```

### 2. Transform Matrix (0x27)

Transform properties are stored as a matrix and must be decomposed:

```rust
Transform {
    scale: 1.2,
    rotate: 45deg,
    translateX: 10
} 
→ 
transform: scale(1.2) rotate(45deg) translateX(10px);
```

### 3. Rich Text Content (0x10)

Text content may contain inline formatting that needs HTML conversion:

```
KRB: "Hello **bold** and *italic* text"
HTML: Hello <strong>bold</strong> and <em>italic</em> text
```

### 4. EmbedView Type Mapping (0x03)

EmbedView elements map to different HTML based on embed-type property:

| embed-type | HTML Output | Additional Setup |
|------------|-------------|------------------|
| `webview` | `<iframe>` | Sandboxing attributes |
| `native` | `<div>` + JS | Custom renderer |
| `wasm` | `<canvas>` + WASM | WebAssembly loading |
| `terminal` | `<div>` + terminal.js | Terminal emulation |

## HTML Generation Examples

### Simple Container with Text

**KRB Input:**
```
Container (0x01) {
    width (0x20): 300px
    height (0x21): 200px
    background-color (0x01): #f0f0f0
    padding (0x70): 20px
    
    Text (0x02) {
        text-content (0x10): "Hello World"
        font-size (0x11): 18
        color (0x02): #333333
    }
}
```

**HTML Output:**
```html
<div class="kryon-container" style="width: 300px; height: 200px; background-color: #f0f0f0; padding: 20px;">
    <span class="kryon-text" style="font-size: 18px; color: #333333;">Hello World</span>
</div>
```

### Flexbox Layout

**KRB Input:**
```
Container (0x01) {
    layout-flags (0x22): 0x01  // Column layout
    gap (0x23): 10
    
    Button (0x10) {
        text-content (0x10): "Button 1"
        flex-grow (0x42): 1
    }
    
    Button (0x10) {
        text-content (0x10): "Button 2"
        flex-grow (0x42): 1
    }
}
```

**HTML Output:**
```html
<div class="kryon-container" style="display: flex; flex-direction: column; gap: 10px;">
    <button class="kryon-button" style="flex-grow: 1;">Button 1</button>
    <button class="kryon-button" style="flex-grow: 1;">Button 2</button>
</div>
```

### Form with Input Validation

**KRB Input:**
```
Form (0x40) {
    Input (0x11) {
        input-type (0xC8): email
        text-content (0x10): ""
        required: true
    }
    
    Button (0x10) {
        text-content (0x10): "Submit"
        type: submit
    }
}
```

**HTML Output:**
```html
<form class="kryon-form">
    <input type="email" class="kryon-input" required>
    <button type="submit" class="kryon-button">Submit</button>
</form>
```

## CSS Property Generation

### Color Value Processing

```rust
fn process_color_value(color_value: &PropertyValue) -> String {
    match color_value {
        PropertyValue::String(s) => {
            if s.starts_with("#") => s.clone(),
            if s == "red" => "#FF0000".to_string(),
            // ... named color mapping
        }
        PropertyValue::Array([r, g, b, a]) => {
            format!("rgba({}, {}, {}, {})", r, g, b, a)
        }
    }
}
```

### Dimension Value Processing

```rust
fn process_dimension_value(dim_value: &PropertyValue) -> String {
    match dim_value {
        PropertyValue::Float(f) => format!("{}px", f),
        PropertyValue::String(s) if s.ends_with("%") => s.clone(),
        PropertyValue::String(s) if s == "auto" => "auto".to_string(),
        PropertyValue::String(s) if s.ends_with("px") => s.clone(),
        PropertyValue::String(s) if s.ends_with("em") => s.clone(),
        PropertyValue::String(s) if s.ends_with("rem") => s.clone(),
        _ => format!("{}px", dim_value.as_float().unwrap_or(0.0))
    }
}
```

## JavaScript Event Mapping

### Event Handler Generation

KRB script references are converted to JavaScript event handlers:

```rust
fn generate_event_handlers(element: &KrbElement) -> String {
    let mut handlers = Vec::new();
    
    for script in &element.scripts {
        match script.trigger {
            EventTrigger::Click => {
                handlers.push(format!("onclick=\"{}\"", script.js_code));
            }
            EventTrigger::Hover => {
                handlers.push(format!("onmouseenter=\"{}\"", script.js_code));
            }
            EventTrigger::Change => {
                handlers.push(format!("onchange=\"{}\"", script.js_code));
            }
        }
    }
    
    handlers.join(" ")
}
```

### Script State Management

```javascript
// Generated JavaScript for state management
class KryonState {
    constructor() {
        this.state = {};
        this.elements = new Map();
    }
    
    setState(key, value) {
        this.state[key] = value;
        this.updateElements(key);
    }
    
    updateElements(stateKey) {
        // Update all elements that depend on this state
        this.elements.forEach((element, id) => {
            if (element.dependencies.includes(stateKey)) {
                this.rerenderElement(id);
            }
        });
    }
}
```

## Implementation Notes

1. **CSS Class Generation**: All Kryon elements get `kryon-{element-name}` classes for styling
2. **Responsive Design**: Percentage and auto values are preserved for responsive behavior  
3. **Accessibility**: Semantic properties are converted to appropriate ARIA attributes
4. **Performance**: Inline styles are used for dynamic properties, CSS classes for static ones
5. **Compatibility**: HTML5 semantic elements are used where appropriate
6. **Validation**: Form inputs include HTML5 validation attributes
7. **State Management**: JavaScript state system handles dynamic updates
8. **Event Handling**: Script events are converted to standard DOM events

This mapping ensures that KRB files can be accurately converted to standards-compliant HTML/CSS/JavaScript while preserving all Kryon functionality and styling.