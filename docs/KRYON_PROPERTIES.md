# Kryon Properties Reference

This document lists all Kryon UI properties with their hex values and usage, organized by functionality.

## Table of Contents
- [Visual Properties](#visual-properties)
- [Text Properties](#text-properties)
- [Layout Properties](#layout-properties)
- [Size Constraints](#size-constraints)
- [Flexbox Properties](#flexbox-properties)
- [Position Properties](#position-properties)
- [Grid Properties](#grid-properties)
- [Spacing Properties](#spacing-properties)
- [Border Properties](#border-properties)
- [Box Model Properties](#box-model-properties)
- [Window Properties](#window-properties)
- [Transform Properties](#transform-properties)
- [Special Properties](#special-properties)
- [Property Aliases](#property-aliases)

## Properties

Complete list of all properties with their hex values and usage, organized by functionality.

### Visual Properties (0x01-0x0F)

Core visual styling properties:

| Property | Hex | Type | Inheritable | Description | Example |
|----------|-----|------|-------------|-------------|---------|
| `background-color` | `0x01` | Color | No | Element background color | `background-color: "#FF0000"` |
| `color` (text-color) | `0x02` | Color | Yes | Text color | `color: "white"` |
| `border-color` | `0x03` | Color | No | Border color | `border-color: "black"` |
| `border-width` | `0x04` | Float | No | Border thickness in pixels | `border-width: 2` |
| `border-radius` | `0x05` | Float | No | Corner radius in pixels | `border-radius: 8` |
| `opacity` | `0x06` | Float | Yes | Opacity (0.0-1.0) | `opacity: 0.8` |
| `visibility` | `0x07` | Bool | Yes | Element visibility | `visibility: false` |
| `cursor` | `0x08` | String | Yes | Mouse cursor type | `cursor: "pointer"` |
| `box-shadow`/`shadow` | `0x09` | String | No | Drop shadow | `shadow: "2px 2px 4px rgba(0,0,0,0.3)"` |
| `image-source` | `0x0A` | String | No | Image path | `src: "image.png"` |

### Text Properties (0x10-0x1F)

Typography and text-related properties:

| Property | Hex | Type | Inheritable | Description | Example |
|----------|-----|------|-------------|-------------|---------|
| `text-content` | `0x10` | String | - | Text content | `text: "Hello"` |
| `font-size` | `0x11` | Float | Yes | Font size in pixels | `font-size: 16` |
| `font-weight` | `0x12` | Int | Yes | Font weight (300-900) | `font-weight: 700` |
| `text-align` | `0x13` | String | Yes | Text alignment | `text-align: "center"` |
| `font-family` | `0x14` | String | Yes | Font family name | `font-family: "Arial"` |
| `white-space` | `0x15` | String | Yes | Whitespace handling | `white-space: "nowrap"` |
| `list-style-type` | `0x16` | String | Yes | List marker style | `list-style-type: "circle"` |

### Layout Properties (0x20-0x2F)

Core layout and positioning properties:

| Property | Hex | Type | Inheritable | Description | Example |
|----------|-----|------|-------------|-------------|---------|
| `width` | `0x20` | Dimension | No | Element width | `width: 100px` or `width: 50%` |
| `height` | `0x21` | Dimension | No | Element height | `height: 200px` or `height: auto` |
| `layout-flags` | `0x22` | Flags | No | Layout type flags | (Binary encoded) |
| `gap` | `0x23` | Float | No | Gap between flex items | `gap: 10` |
| `z-index`/`z_index` | `0x24` | Int | No | Stacking order | `z-index: 10` |
| `display` | `0x25` | String | No | Display mode | `display: "flex"` |
| `position` | `0x26` | String | No | Position type | `position: "absolute"` |
| `transform` | `0x27` | Transform | No | Transform matrix | `transform: { scale: 1.2 }` |
| `style-id` | `0x28` | String | No | Style reference | `style: "button_primary"` |

### Size Constraints (0x30-0x3F)

Minimum and maximum size constraints:

| Property | Hex | Type | Description | Example |
|----------|-----|------|-------------|---------|
| `min-width` | `0x30` | Dimension | Minimum width | `min-width: 100px` |
| `min-height` | `0x31` | Dimension | Minimum height | `min-height: 50px` |
| `max-width` | `0x32` | Dimension | Maximum width | `max-width: 800px` |
| `max-height` | `0x33` | Dimension | Maximum height | `max-height: 600px` |

### Flexbox Properties (0x40-0x4F)

Flexible box layout properties:

| Property | Hex | Type | Description | Example |
|----------|-----|------|-------------|---------|
| `flex-direction` | `0x40` | String | Main axis direction | `flex-direction: "column"` |
| `flex-wrap` | `0x41` | String | Wrap behavior | `flex-wrap: "wrap"` |
| `flex-grow` | `0x42` | Float | Grow factor | `flex-grow: 1` |
| `flex-shrink` | `0x43` | Float | Shrink factor | `flex-shrink: 0` |
| `flex-basis` | `0x44` | Dimension | Initial size | `flex-basis: 200px` |
| `align-items` | `0x45` | String | Cross axis alignment | `align-items: "center"` |
| `align-content` | `0x46` | String | Multi-line alignment | `align-content: "space-between"` |
| `align-self` | `0x47` | String | Self alignment | `align-self: "flex-end"` |
| `justify-content` | `0x48` | String | Main axis alignment | `justify-content: "center"` |
| `justify-items` | `0x49` | String | Item justification | `justify-items: "start"` |
| `justify-self` | `0x4A` | String | Self justification | `justify-self: "end"` |

### Position Properties (0x50-0x5F)

Absolute and relative positioning:

| Property | Hex | Type | Description | Example |
|----------|-----|------|-------------|---------|
| `left` | `0x50` | Dimension | Left offset | `left: 10px` |
| `top` | `0x51` | Dimension | Top offset | `top: 20px` |
| `right` | `0x52` | Dimension | Right offset | `right: 10px` |
| `bottom` | `0x53` | Dimension | Bottom offset | `bottom: 20px` |

### Grid Properties (0x60-0x6F)

CSS Grid layout system:

| Property | Hex | Type | Description |
|----------|-----|------|-------------|
| `grid-template-columns` | `0x60` | String | Column template |
| `grid-template-rows` | `0x61` | String | Row template |
| `grid-template-areas` | `0x62` | String | Named areas |
| `grid-auto-columns` | `0x63` | String | Auto column size |
| `grid-auto-rows` | `0x64` | String | Auto row size |
| `grid-auto-flow` | `0x65` | String | Auto placement |
| `grid-area` | `0x66` | String | Grid area |
| `grid-column` | `0x67` | String | Column placement |
| `grid-row` | `0x68` | String | Row placement |
| `grid-column-start` | `0x69` | Int | Column start |
| `grid-column-end` | `0x6A` | Int | Column end |
| `grid-row-start` | `0x6B` | Int | Row start |
| `grid-row-end` | `0x6C` | Int | Row end |
| `grid-gap` | `0x6D` | Dimension | Grid gap |
| `grid-column-gap` | `0x6E` | Dimension | Column gap |
| `grid-row-gap` | `0x6F` | Dimension | Row gap |

### Spacing Properties (0x70-0x7F)

Padding and margin properties:

| Property | Hex | Type | Description | Example |
|----------|-----|------|-------------|---------|
| `padding` | `0x70` | Dimension | All padding | `padding: 10px` |
| `padding-top` | `0x71` | Dimension | Top padding | `padding-top: 5px` |
| `padding-right` | `0x72` | Dimension | Right padding | `padding-right: 10px` |
| `padding-bottom` | `0x73` | Dimension | Bottom padding | `padding-bottom: 5px` |
| `padding-left` | `0x74` | Dimension | Left padding | `padding-left: 10px` |
| `margin` | `0x75` | Dimension | All margins | `margin: 20px` |
| `margin-top` | `0x76` | Dimension | Top margin | `margin-top: 10px` |
| `margin-right` | `0x77` | Dimension | Right margin | `margin-right: 15px` |
| `margin-bottom` | `0x78` | Dimension | Bottom margin | `margin-bottom: 10px` |
| `margin-left` | `0x79` | Dimension | Left margin | `margin-left: 15px` |

### Border Properties (0x80-0x8F)

Individual border side properties:

| Property | Hex | Type | Description |
|----------|-----|------|-------------|
| `border-top-width` | `0x80` | Float | Top border width |
| `border-right-width` | `0x81` | Float | Right border width |
| `border-bottom-width` | `0x82` | Float | Bottom border width |
| `border-left-width` | `0x83` | Float | Left border width |
| `border-top-color` | `0x84` | Color | Top border color |
| `border-right-color` | `0x85` | Color | Right border color |
| `border-bottom-color` | `0x86` | Color | Bottom border color |
| `border-left-color` | `0x87` | Color | Left border color |
| `border-top-left-radius` | `0x88` | Float | Top-left radius |
| `border-top-right-radius` | `0x89` | Float | Top-right radius |
| `border-bottom-right-radius` | `0x8A` | Float | Bottom-right radius |
| `border-bottom-left-radius` | `0x8B` | Float | Bottom-left radius |

### Box Model Properties (0x90-0x9F)

Box model and overflow properties:

| Property | Hex | Type | Description |
|----------|-----|------|-------------|
| `box-sizing` | `0x90` | String | Box sizing model |
| `outline` | `0x91` | String | Outline style |
| `outline-color` | `0x92` | Color | Outline color |
| `outline-width` | `0x93` | Float | Outline width |
| `outline-offset` | `0x94` | Float | Outline offset |
| `overflow` | `0x95` | String | Overflow behavior |
| `overflow-x` | `0x96` | String | Horizontal overflow |
| `overflow-y` | `0x97` | String | Vertical overflow |

### Window Properties (0xA0-0xAF)

Application window configuration:

| Property | Hex | Type | Description | Example |
|----------|-----|------|-------------|---------|
| `window-width` | `0xA0` | Int | Window width | `window-width: 1024` |
| `window-height` | `0xA1` | Int | Window height | `window-height: 768` |
| `window-title` | `0xA2` | String | Window title | `window-title: "My App"` |
| `window-resizable` | `0xA3` | Bool | Allow resize | `window-resizable: true` |
| `window-fullscreen` | `0xA4` | Bool | Fullscreen mode | `window-fullscreen: false` |
| `window-vsync` | `0xA5` | Bool | VSync enabled | `window-vsync: true` |
| `window-target-fps` | `0xA6` | Int | Target FPS | `window-target-fps: 60` |
| `window-antialiasing` | `0xA7` | Int | AA samples | `window-antialiasing: 4` |
| `window-icon` | `0xA8` | String | Window icon path | `window-icon: "icon.png"` |

### Transform Properties (0xB0-0xBF)

2D and 3D transformation properties:

| Transform | Hex | Description | Example |
|-----------|-----|-------------|---------|
| `scale` | `0xB0` | Uniform scale | `scale: 1.5` |
| `scaleX` | `0xB1` | X-axis scale | `scaleX: 2.0` |
| `scaleY` | `0xB2` | Y-axis scale | `scaleY: 0.5` |
| `scaleZ` | `0xB3` | Z-axis scale | `scaleZ: 1.0` |
| `translateX` | `0xB4` | X translation | `translateX: 100` |
| `translateY` | `0xB5` | Y translation | `translateY: -50` |
| `translateZ` | `0xB6` | Z translation | `translateZ: 0` |
| `rotate` | `0xB7` | Z rotation | `rotate: 45deg` |
| `rotateX` | `0xB8` | X rotation | `rotateX: 30deg` |
| `rotateY` | `0xB9` | Y rotation | `rotateY: 60deg` |
| `rotateZ` | `0xBA` | Z rotation | `rotateZ: 90deg` |
| `skewX` | `0xBB` | X skew | `skewX: 15deg` |
| `skewY` | `0xBC` | Y skew | `skewY: 10deg` |
| `perspective` | `0xBD` | Perspective | `perspective: 1000` |
| `matrix` | `0xBE` | Transform matrix | (Not implemented) |

### Semantic Properties (0xC0-0xCF)

Properties for HTML semantic compatibility:

| Property | Hex | Type | Description | Example |
|----------|-----|------|-------------|---------|
| `semantic-role` | `0xC0` | String | HTML semantic role | `semantic-role: "article"` |
| `heading-level` | `0xC1` | Int | Heading level (1-6) | `heading-level: 2` |
| `list-type` | `0xC2` | String | List type | `list-type: "ordered"` |
| `list-item-role` | `0xC3` | String | List item role | `list-item-role: "definition"` |
| `table-section` | `0xC4` | String | Table section type | `table-section: "header"` |
| `interactive-type` | `0xC5` | String | Interactive element type | `interactive-type: "dialog"` |
| `media-type` | `0xC6` | String | Media element type | `media-type: "audio"` |
| `embed-type` | `0xC7` | String | Embed content type | `embed-type: "webview"` |
| `input-type` | `0xC8` | String | Input element type | `input-type: "email"` |

### Text Formatting Properties (0xD0-0xDF)

Enhanced text styling properties for HTML compatibility:

| Property | Hex | Type | Description | Example |
|----------|-----|------|-------------|---------|
| `font-style` | `0xD0` | String | Font style | `font-style: "italic"` |
| `text-decoration` | `0xD1` | String | Text decoration | `text-decoration: "underline"` |
| `vertical-align` | `0xD2` | String | Vertical alignment | `vertical-align: "sub"` |
| `line-height` | `0xD3` | Float | Line height | `line-height: 1.5` |
| `letter-spacing` | `0xD4` | Float | Letter spacing | `letter-spacing: 0.1em` |
| `word-spacing` | `0xD5` | Float | Word spacing | `word-spacing: 0.2em` |
| `text-indent` | `0xD6` | Dimension | Text indentation | `text-indent: 2em` |
| `text-transform` | `0xD7` | String | Text transformation | `text-transform: "uppercase"` |
| `text-shadow` | `0xD8` | String | Text shadow | `text-shadow: "1px 1px 2px black"` |
| `word-wrap` | `0xD9` | String | Word wrap behavior | `word-wrap: "break-word"` |
| `text-overflow` | `0xDA` | String | Text overflow handling | `text-overflow: "ellipsis"` |
| `writing-mode` | `0xDB` | String | Writing direction | `writing-mode: "vertical-lr"` |

### Extended Visual Properties (0xE0-0xEF)

Additional visual properties for comprehensive styling:

| Property | Hex | Type | Description | Example |
|----------|-----|------|-------------|---------|
| `background-image` | `0xE0` | String | Background image | `background-image: "url(bg.jpg)"` |
| `background-repeat` | `0xE1` | String | Background repeat | `background-repeat: "no-repeat"` |
| `background-position` | `0xE2` | String | Background position | `background-position: "center"` |
| `background-size` | `0xE3` | String | Background size | `background-size: "cover"` |
| `background-attachment` | `0xE4` | String | Background attachment | `background-attachment: "fixed"` |
| `border-style` | `0xE5` | String | Border style | `border-style: "dashed"` |
| `border-image` | `0xE6` | String | Border image | `border-image: "url(border.png)"` |
| `filter` | `0xE7` | String | CSS filters | `filter: "blur(5px)"` |
| `backdrop-filter` | `0xE8` | String | Backdrop filters | `backdrop-filter: "blur(10px)"` |
| `clip-path` | `0xE9` | String | Clipping path | `clip-path: "circle(50%)"` |
| `mask` | `0xEA` | String | CSS mask | `mask: "url(mask.svg)"` |
| `mix-blend-mode` | `0xEB` | String | Blend mode | `mix-blend-mode: "multiply"` |
| `object-fit` | `0xEC` | String | Object fit | `object-fit: "cover"` |
| `object-position` | `0xED` | String | Object position | `object-position: "top left"` |

### Special Properties (0xF0-0xFF)

Reserved properties for internal use and future extensions:

| Property | Hex | Type | Description |
|----------|-----|------|-------------|
| `spans` | `0xF0` | Int | Grid spans |
| `rich-text-content` | `0xF1` | String | Rich text with inline formatting |
| `accessibility-label` | `0xF2` | String | Accessibility label |
| `accessibility-role` | `0xF3` | String | ARIA role |
| `accessibility-description` | `0xF4` | String | Accessibility description |
| `data-attributes` | `0xF5` | Object | HTML data attributes |
| `custom-properties` | `0xF6` | Object | CSS custom properties |

## Property Aliases

Common property aliases that are automatically resolved:

### Text Element Aliases
| Alias | Resolves To | Element |
|-------|-------------|---------|
| `color` | `text_color` | Text, Button |
| `font` | `font_family` | Text, Button |
| `size` | `font_size` | Text, Button |
| `align` | `text_alignment` | Text, Button |

### Position Aliases
| Alias | Resolves To | Description |
|-------|-------------|-------------|
| `x` | `pos_x` | X position |
| `y` | `pos_y` | Y position |

### Style Aliases
| Alias | Resolves To |
|-------|-------------|
| `bg` | `background_color` |
| `z_index` | `z-index` |

## Usage Examples

### Transformed Element
```kry
Image {
    src: "logo.png"
    transform: {
        scale: 1.2
        rotate: 45deg
        translateX: 10
    }
}
```

### Styled Text
```kry
Text {
    text: "Styled Text"
    font-family: "Arial"
    font-size: 18
    font-weight: 700
    color: "#333333"
    text-align: "center"
}
```

### Responsive Container
```kry
Container {
    width: 100%
    max-width: 1200px
    padding: 20
    margin: "0 auto"
    background-color: "#f5f5f5"
    border-radius: 8
    box-shadow: "0 2px 4px rgba(0,0,0,0.1)"
}
```

## Notes

- All hex values are used in the binary KRB format for efficient storage
- Properties marked as "Inheritable" are inherited from parent elements
- Transform properties can be combined for complex transformations
- Dimension properties support pixels, percentages, and auto values
- Color properties support hex, rgb, rgba, hsl, and named colors