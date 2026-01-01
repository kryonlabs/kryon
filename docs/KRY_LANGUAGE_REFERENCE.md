# Kry Language Reference

## Introduction

**Kry** is a modern UI DSL (Domain Specific Language) designed as a unified alternative to HTML and CSS. Unlike HTML's separation of structure (HTML) and styling (CSS), Kry combines both into a single, declarative syntax with built-in state management and event handling.

### Key Differences from HTML/CSS

| Feature | HTML/CSS | Kry |
|---------|----------|-----|
| Structure | Separate HTML file | Component hierarchy |
| Styling | Separate CSS file | Inline properties |
| State | Requires JavaScript | Native `state` keyword |
| Layout | Flexbox/Grid via CSS | Built-in Row/Column/Container |
| Events | JavaScript event listeners | Native event handlers |
| Compilation | Browser interpretation | Compiles to native code |

---

## Kry Syntax Overview

### Basic Structure

```kry
App {
    windowTitle = "My Application"
    windowWidth = 800
    windowHeight = 600

    Container {
        width = 100%
        height = 100%
        backgroundColor = "#1a1a2e"

        Column {
            justifyContent = "center"
            alignItems = "center"
            gap = 20

            Text {
                text = "Hello, World!"
                color = "white"
                fontSize = 24
            }

            Button {
                text = "Click Me"
                onClick = { count += 1 }
            }
        }
    }
}
```

### State Management

```kry
state count: int = 0
state message: string = "Hello"
state items: array = ["item1", "item2"]

state user: object = {
    name: "John",
    age: 30
}
```

### Event Handlers

```kry
Button {
    text = "Increment"
    onClick = {
        count += 1
        if count > 10 {
            showMessage = true
        }
    }
}

Input {
    value = $textValue
    onChange = { textValue = newValue }
}
```

### Control Flow

```kry
// Conditional rendering
if showWarning {
    Text {
        text = "Warning!"
        color = "red"
    }
}

// Loops
for item in items {
    Text {
        text = $item
    }
}
```

### Custom Components

```kry
component Counter(initialValue: int = 0) {
    state value: int = initialValue

    Row {
        gap = 10

        Button {
            text = "-"
            onClick = { value -= 1 }
        }

        Text {
            text = $value
        }

        Button {
            text = "+"
            onClick = { value += 1 }
        }
    }
}

// Usage
Counter(5)
Counter(initialValue = 10)
```

---

## HTML to Kry Element Mapping

### Document Structure Elements

| HTML Element | Kry Component | Description | Implementation Status |
|--------------|---------------|-------------|----------------------|
| `<html>` | App | Root container | ✅ Full |
| `<body>` | App children | Body content | ✅ Full |
| `<head>` | *(ignored)* | Metadata, not rendered | ✅ Parsed (skipped) |
| `<title>` | *(ignored)* | Window title via `windowTitle` | ✅ Full |
| `<meta>` | *(ignored)* | Metadata | ✅ Parsed (skipped) |
| `<link>` | *(stylesheet only)* | External CSS | ⚠️ Stylesheet only |
| `<style>` | *(stylesheet only)* | Embedded CSS | ⚠️ Parsed as CSS |
| `<script>` | *(ignored)* | JavaScript | ❌ Not supported |

### Text Content Elements

| HTML Element | Kry Component | Description | Implementation Status |
|--------------|---------------|-------------|----------------------|
| `<h1>` - `<h6>` | Heading(level=1-6) | Headings levels 1-6 | ✅ Full |
| `<p>` | Paragraph | Paragraph text | ✅ Full |
| `<span>` | Span | Inline container | ✅ Full |
| `<div>` | Container | Generic block container | ✅ Full |
| `<br>` | *(newline in text)* | Line break | ✅ Full |
| `<hr>` | HorizontalRule | Horizontal rule/separator | ✅ Full |
| `<pre>` | CodeBlock | Preformatted text/code block | ✅ Full |

### Text Formatting Elements (Inline)

| HTML Element | Kry Component | Description | Implementation Status |
|--------------|---------------|-------------|----------------------|
| `<strong>` | Strong | Bold/important text | ✅ Full |
| `<b>` | Strong | Bold text | ✅ Full |
| `<em>` | Em | Emphasized/italic text | ✅ Full |
| `<i>` | Em | Italic text | ✅ Full |
| `<small>` | Small | Smaller text | ✅ Full |
| `<mark>` | Mark | Highlighted text | ✅ Full |
| `<code>` | CodeInline | Inline code | ✅ Full |
| `<sub>` | *(not implemented)* | Subscript | ❌ Not supported |
| `<sup>` | *(not implemented)* | Superscript | ❌ Not supported |
| `<del>` | *(textDecoration)* | Deleted text | ⚠️ Via textDecoration |
| `<ins>` | *(not implemented)* | Inserted text | ❌ Not supported |
| `<abbr>` | *(not implemented)* | Abbreviation | ❌ Not supported |
| `<dfn>` | *(not implemented)* | Definition term | ❌ Not supported |
| `<kbd>` | *(not implemented)* | Keyboard input | ❌ Not supported |
| `<samp>` | *(not implemented)* | Sample output | ❌ Not supported |
| `<var>` | *(not implemented)* | Variable | ❌ Not supported |
| `<time>` | *(not implemented)* | Date/time | ❌ Not supported |
| `<data>` | *(not implemented)* | Machine-readable data | ❌ Not supported |

### List Elements

| HTML Element | Kry Component | Description | Implementation Status |
|--------------|---------------|-------------|----------------------|
| `<ul>` | List(type=unordered) | Unordered list (bullets) | ✅ Full |
| `<ol>` | List(type=ordered) | Ordered list (numbered) | ✅ Full |
| `<li>` | ListItem | List item | ✅ Full |
| `<dl>` | *(not implemented)* | Description list | ❌ Not supported |
| `<dt>` | *(not implemented)* | Description term | ❌ Not supported |
| `<dd>` | *(not implemented)* | Description details | ❌ Not supported |

### Table Elements

| HTML Element | Kry Component | Description | Implementation Status |
|--------------|---------------|-------------|----------------------|
| `<table>` | Table | Table container | ✅ Full |
| `<thead>` | TableHead | Table header section | ✅ Full |
| `<tbody>` | TableBody | Table body section | ✅ Full |
| `<tfoot>` | TableFoot | Table footer section | ✅ Full |
| `<tr>` | TableRow | Table row | ✅ Full |
| `<td>` | TableCell | Table data cell | ⚠️ colspan/rowspan TODO |
| `<th>` | TableHeaderCell | Table header cell | ⚠️ colspan/rowspan TODO |
| `<caption>` | *(not implemented)* | Table caption | ❌ Not supported |
| `<col>` | *(not implemented)* | Column | ❌ Not supported |
| `<colgroup>` | *(not implemented)* | Column group | ❌ Not supported |

### Form Elements

| HTML Element | Kry Component | Description | Implementation Status |
|--------------|---------------|-------------|----------------------|
| `<form>` | Container | Form container | ⚠️ No submit handling |
| `<button>` | Button | Clickable button | ✅ Full |
| `<input type="text">` | Input | Text input field | ✅ Full |
| `<input type="password">` | Input(secure) | Password field | ⚠️ Via property |
| `<input type="email">` | Input | Email input | ⚠️ Via type attribute |
| `<input type="number">` | Input | Number input | ⚠️ Via type attribute |
| `<input type="checkbox">` | Checkbox | Checkbox | ✅ Full |
| `<input type="radio">` | *(not implemented)* | Radio button | ❌ Not supported |
| `<input type="file">` | *(not implemented)* | File upload | ❌ Not supported |
| `<input type="range">` | *(not implemented)* | Slider | ❌ Not supported |
| `<input type="date">` | *(not implemented)* | Date picker | ❌ Not supported |
| `<input type="color">` | *(not implemented)* | Color picker | ❌ Not supported |
| `<textarea>` | Input(multiline) | Multi-line text input | ⚠️ Via property |
| `<select>` | Dropdown | Select dropdown | ✅ Full |
| `<option>` | *(dropdown options)* | Dropdown option | ✅ Full |
| `<optgroup>` | *(not implemented)* | Option group | ❌ Not supported |
| `<label>` | *(text or prop)* | Form label | ⚠️ Via text/properties |
| `<fieldset>` | Container | Fieldset | ⚠️ Via Container |
| `<legend>` | *(not implemented)* | Fieldset caption | ❌ Not supported |
| `<datalist>` | *(not implemented)* | Data list | ❌ Not supported |
| `<output>` | *(not implemented)* | Output value | ❌ Not supported |
| `<progress>` | *(not implemented)* | Progress bar | ❌ Not supported |
| `<meter>` | *(not implemented)* | Measurement gauge | ❌ Not supported |

### Interactive Elements

| HTML Element | Kry Component | Description | Implementation Status |
|--------------|---------------|-------------|----------------------|
| `<details>` | *(not implemented)* | Collapsible details | ❌ Not supported |
| `<summary>` | *(not implemented)* | Details summary | ❌ Not supported |
| `<dialog>` | *(not implemented)* | Dialog box | ❌ Not supported |
| `<menu>` | *(not implemented)* | Menu | ❌ Not supported |

### Embedded Content

| HTML Element | Kry Component | Description | Implementation Status |
|--------------|---------------|-------------|----------------------|
| `<img>` | Image | Image display | ✅ Full |
| `<picture>` | *(not implemented)* | Responsive images | ❌ Not supported |
| `<source>` | *(not implemented)* | Media source | ❌ Not supported |
| `<iframe>` | *(not implemented)* | Inline frame | ❌ Not supported |
| `<embed>` | *(not implemented)* | Embedded content | ❌ Not supported |
| `<object>` | *(not implemented)* | Embedded object | ❌ Not supported |
| `<param>` | *(not implemented)* | Object parameter | ❌ Not supported |
| `<video>` | *(not implemented)* | Video player | ❌ Not supported |
| `<audio>` | *(not implemented)* | Audio player | ❌ Not supported |
| `<track>` | *(not implemented)* | Text track | ❌ Not supported |
| `<map>` | *(not implemented)* | Image map | ❌ Not supported |
| `<area>` | *(not implemented)* | Map area | ❌ Not supported |
| `<canvas>` | Canvas / NativeCanvas | Drawing canvas | ✅ Full |

### Semantic HTML5 Elements

| HTML Element | Kry Component | Description | Implementation Status |
|--------------|---------------|-------------|----------------------|
| `<main>` | Container | Main content area | ✅ Full (preserves tag) |
| `<header>` | Container | Page/section header | ✅ Full (preserves tag) |
| `<footer>` | Container | Page/section footer | ✅ Full (preserves tag) |
| `<nav>` | Container | Navigation | ✅ Full (preserves tag) |
| `<section>` | Container | Section | ✅ Full (preserves tag) |
| `<article>` | Container | Article | ✅ Full (preserves tag) |
| `<aside>` | Container | Sidebar | ✅ Full (preserves tag) |
| `<figure>` | Container | Figure | ✅ Full (preserves tag) |
| `<figcaption>` | Container | Figure caption | ✅ Full (preserves tag) |
| `<address>` | *(not implemented)* | Contact info | ❌ Not supported |
| `<blockquote>` | BlockQuote | Block quotation | ✅ Full |

### Link and Metadata Elements

| HTML Element | Kry Component | Description | Implementation Status |
|--------------|---------------|-------------|----------------------|
| `<a>` | Link | Hyperlink | ✅ Full |
| `<base>` | *(not implemented)* | Base URL | ❌ Not supported |

### Scripting and Data Elements

| HTML Element | Kry Component | Description | Implementation Status |
|--------------|---------------|-------------|----------------------|
| `<script>` | *(logic block)* | JavaScript/logic | ⚠️ Extracted as logic |
| `<noscript>` | *(ignored)* | Fallback content | ❌ Not supported |
| `<template>` | *(not implemented)* | HTML template | ❌ Not supported |
| `<slot>` | *(not implemented)* | Web component slot | ❌ Not supported |

---

## CSS to Kry Property Mapping

### Layout Properties

| CSS Property | Kry Property | Values | Implementation Status |
|--------------|--------------|--------|----------------------|
| `display` | display (via component) | flex, inline-flex, grid, inline-grid, block, inline, inline-block, none | ✅ Full |
| `flex-direction` | flexDirection | row, column | ✅ Full |
| `flex-wrap` | flexWrap | wrap, nowrap | ✅ Full |
| `justify-content` | justifyContent | flex-start, center, flex-end, space-between, space-around, space-evenly | ✅ Full |
| `align-items` | alignItems | flex-start, center, flex-end, stretch | ✅ Full |
| `align-content` | *(not implemented)* | - | ❌ Not supported |
| `align-self` | *(not implemented)* | - | ❌ Not supported |
| `flex-grow` | flexGrow | number | ✅ Full |
| `flex-shrink` | flexShrink | number | ✅ Full |
| `flex-basis` | *(not implemented)* | - | ❌ Not supported |
| `flex` | *(individual props)* | shorthand | ⚠️ Via individual props |
| `order` | *(not implemented)* | - | ❌ Not supported |
| `gap` | gap | px value | ✅ Full |
| `row-gap` | rowGap | px value | ✅ Full |
| `column-gap` | columnGap | px value | ✅ Full |

### Grid Layout

| CSS Property | Kry Property | Values | Implementation Status |
|--------------|--------------|--------|----------------------|
| `display: grid` | display mode | grid | ✅ Full |
| `grid-template-columns` | gridTemplateColumns | tracks, repeat(), minmax() | ✅ Full |
| `grid-template-rows` | gridTemplateRows | tracks, repeat(), minmax() | ✅ Full |
| `grid-template-areas` | *(not implemented)* | - | ❌ Not supported |
| `grid-area` | *(not implemented)* | - | ❌ Not supported |
| `grid-column` | *(not implemented)* | - | ❌ Not supported |
| `grid-row` | *(not implemented)* | - | ❌ Not supported |
| `grid-auto-rows` | *(not implemented)* | - | ❌ Not supported |
| `grid-auto-columns` | *(not implemented)* | - | ❌ Not supported |
| `grid-auto-flow` | *(not implemented)* | - | ❌ Not supported |
| `justify-items` | *(not implemented)* | - | ❌ Not supported |
| `justify-self` | *(not implemented)* | - | ❌ Not supported |

### Dimension Properties

| CSS Property | Kry Property | Values | Implementation Status |
|--------------|--------------|--------|----------------------|
| `width` | width | px, %, auto, rem, em, vw, vh, vmin, vmax | ✅ Full |
| `height` | height | px, %, auto, rem, em, vw, vh, vmin, vmax | ✅ Full |
| `min-width` | minWidth | px, %, auto | ✅ Full |
| `min-height` | minHeight | px, %, auto | ✅ Full |
| `max-width` | maxWidth | px, %, auto | ✅ Full |
| `max-height` | maxHeight | px, %, auto | ✅ Full |
| `aspect-ratio` | *(not implemented)* | - | ❌ Not supported |
| `box-sizing` | boxSizing | border-box, content-box | ✅ Full |

### Spacing (Box Model)

| CSS Property | Kry Property | Values | Implementation Status |
|--------------|--------------|--------|----------------------|
| `margin` | margin | shorthand (1-4 values) | ✅ Full |
| `margin-top` | marginTop | px, %, auto | ✅ Full |
| `margin-right` | marginRight | px, %, auto | ✅ Full |
| `margin-bottom` | marginBottom | px, %, auto | ✅ Full |
| `margin-left` | marginLeft | px, %, auto | ✅ Full |
| `padding` | padding | shorthand (1-4 values) | ✅ Full |
| `padding-top` | paddingTop | px, % | ✅ Full |
| `padding-right` | paddingRight | px, % | ✅ Full |
| `padding-bottom` | paddingBottom | px, % | ✅ Full |
| `padding-left` | paddingLeft | px, % | ✅ Full |

### Color and Background

| CSS Property | Kry Property | Values | Implementation Status |
|--------------|--------------|--------|----------------------|
| `color` | color | hex, rgb, rgba, named, var() | ✅ Full |
| `background-color` | backgroundColor | hex, rgb, rgba, named, var() | ✅ Full |
| `background` | background | color or gradient | ⚠️ Gradients as raw string |
| `background-image` | background | url(), gradient | ⚠️ Gradients stored raw |
| `background-gradient` | background | linear-gradient() | ✅ Parsed |
| `background-clip` | backgroundClip | text, content-box, padding-box, border-box | ✅ Full |
| `-webkit-background-clip` | backgroundClip | text (for gradient text) | ✅ Full |
| `-webkit-text-fill-color` | textFillColor | transparent (for gradient text) | ✅ Full |
| `opacity` | opacity | 0.0-1.0 | ✅ Full |

### Border Properties

| CSS Property | Kry Property | Values | Implementation Status |
|--------------|--------------|--------|----------------------|
| `border` | border | width style color shorthand | ✅ Full |
| `border-top` | borderTop | width style color | ✅ Full |
| `border-right` | borderRight | width style color | ✅ Full |
| `border-bottom` | borderBottom | width style color | ✅ Full |
| `border-left` | borderLeft | width style color | ✅ Full |
| `border-width` | borderWidth | px | ✅ Full |
| `border-style` | *(not implemented)* | solid, dashed, etc. | ⚠️ Solid only |
| `border-color` | borderColor | color | ✅ Full |
| `border-radius` | borderRadius | px | ✅ Full |
| `border-top-left-radius` | *(not implemented)* | - | ❌ Not supported |
| `border-top-right-radius` | *(not implemented)* | - | ❌ Not supported |
| `border-bottom-left-radius` | *(not implemented)* | - | ❌ Not supported |
| `border-bottom-right-radius` | *(not implemented)* | - | ❌ Not supported |
| `outline` | *(not implemented)* | - | ❌ Not supported |

### Typography Properties

| CSS Property | Kry Property | Values | Implementation Status |
|--------------|--------------|--------|----------------------|
| `font-family` | fontFamily | font name | ✅ Full |
| `font-size` | fontSize | px, rem, em | ✅ Full |
| `font-weight` | fontWeight | 100-900, bold | ✅ Full |
| `font-style` | fontStyle | normal, italic | ✅ Full |
| `font-variant` | *(not implemented)* | - | ❌ Not supported |
| `font-stretch` | *(not implemented)* | - | ❌ Not supported |
| `font-size-adjust` | *(not implemented)* | - | ❌ Not supported |
| `line-height` | lineHeight | number, px | ✅ Full |
| `letter-spacing` | letterSpacing | px, em | ✅ Full |
| `word-spacing` | wordSpacing | px, em | ✅ Full |
| `text-align` | textAlign | left, right, center, justify | ✅ Full |
| `text-decoration` | textDecoration | none, underline, line-through, overline | ✅ Full |
| `text-decoration-line` | textDecoration | - | ⚠️ Via textDecoration |
| `text-decoration-style` | *(not implemented)* | solid, wavy, etc. | ❌ Not supported |
| `text-decoration-color` | *(not implemented)* | - | ❌ Not supported |
| `text-transform` | *(not implemented)* | uppercase, etc. | ❌ Not supported |
| `text-indent` | *(not implemented)* | - | ❌ Not supported |
| `text-shadow` | *(not implemented)* | - | ❌ Not supported |
| `white-space` | *(not implemented)* | nowrap, etc. | ❌ Not supported |
| `word-break` | *(not implemented)* | - | ❌ Not supported |
| `word-wrap` | *(not implemented)* | - | ❌ Not supported |
| `overflow-wrap` | *(not implemented)* | - | ❌ Not supported |

### Positioning

| CSS Property | Kry Property | Values | Implementation Status |
|--------------|--------------|--------|----------------------|
| `position` | positionMode | static, relative, absolute, fixed | ⚠️ Limited |
| `top` | absoluteY | px, % | ⚠️ With position |
| `bottom` | *(not implemented)* | - | ❌ Not supported |
| `left` | absoluteX | px, % | ⚠️ With position |
| `right` | *(not implemented)* | - | ❌ Not supported |
| `z-index` | zIndex | number | ✅ Full |
| `float` | *(not implemented)* | - | ❌ Not supported |
| `clear` | *(not implemented)* | - | ❌ Not supported |

### Overflow and Clipping

| CSS Property | Kry Property | Values | Implementation Status |
|--------------|--------------|--------|----------------------|
| `overflow` | overflowX, overflowY | visible, hidden, scroll, auto | ⚠️ Partial |
| `overflow-x` | overflowX | visible, hidden, scroll, auto | ⚠️ Partial |
| `overflow-y` | overflowY | visible, hidden, scroll, auto | ⚠️ Partial |
| `clip` | *(not implemented)* | - | ❌ Not supported |
| `clip-path` | *(not implemented)* | - | ❌ Not supported |

### Visibility and Display

| CSS Property | Kry Property | Values | Implementation Status |
|--------------|--------------|--------|----------------------|
| `visibility` | visible | true, false | ✅ Full |
| `display` | display | none, flex, grid, etc. | ✅ Full |
| `opacity` | opacity | 0.0-1.0 | ✅ Full |

### Transitions and Animations

| CSS Property | Kry Property | Values | Implementation Status |
|--------------|--------------|--------|----------------------|
| `transition` | transitions | property duration timing-function delay | ✅ Full |
| `transition-property` | *(in transitions)* | - | ⚠️ Via transition |
| `transition-duration` | *(in transitions)* | - | ⚠️ Via transition |
| `transition-timing-function` | *(in transitions)* | - | ⚠️ Via transition |
| `transition-delay` | *(in transitions)* | - | ⚠️ Via transition |
| `animation` | animations | keyframe animation | ✅ Full |
| `@keyframes` | animations | keyframe definition | ✅ Full |
| `animation-name` | *(in animations)* | - | ⚠️ Via animation |
| `animation-duration` | *(in animations)* | - | ⚠️ Via animation |
| `animation-timing-function` | *(in animations)* | - | ⚠️ Via animation |
| `animation-delay` | *(in animations)* | - | ⚠️ Via animation |
| `animation-iteration-count` | *(in animations)* | - | ⚠️ Via animation |
| `animation-direction` | *(in animations)* | - | ⚠️ Via animation |
| `animation-fill-mode` | *(in animations)* | - | ⚠️ Via animation |

### Transforms

| CSS Property | Kry Property | Values | Implementation Status |
|--------------|--------------|--------|----------------------|
| `transform` | transform | scale(), scaleX(), scaleY(), translateX(), translateY(), rotate() | ✅ Full |
| `transform-origin` | *(not implemented)* | - | ❌ Not supported |
| `transform-style` | *(not implemented)* | - | ❌ Not supported |
| `perspective` | *(not implemented)* | - | ❌ Not supported |
| `rotate` | *(not implemented)* | - | ⚠️ Via transform |
| `scale` | *(not implemented)* | - | ⚠️ Via transform |
| `translate` | *(not implemented)* | - | ⚠️ Via transform |

### Effects and Filters

| CSS Property | Kry Property | Values | Implementation Status |
|--------------|--------------|--------|----------------------|
| `box-shadow` | boxShadow | offset-x offset-y blur spread color | ✅ Full |
| `filter` | filters | blur(), brightness(), etc. | ✅ Full |
| `backdrop-filter` | *(not implemented)* | - | ❌ Not supported |
| `mix-blend-mode` | *(not implemented)* | - | ❌ Not supported |

### CSS Variables (Custom Properties)

| CSS Property | Kry Property | Values | Implementation Status |
|--------------|--------------|--------|----------------------|
| `--name` | CSS Variables | any value | ✅ Full |
| `var(--name)` | Variable Reference | with fallback | ✅ Full |
| `@property` | *(not implemented)* | - | ❌ Not supported |

### Responsive Design

| CSS Property | Kry Property | Values | Implementation Status |
|--------------|--------------|--------|----------------------|
| `@media` | Media Queries | conditions | ⚠️ Parsed, not evaluated |
| `@container` | Container Queries | conditions | ⚠️ Parsed, not evaluated |
| `@viewport` | *(not implemented)* | - | ❌ Not supported |

### Other Properties

| CSS Property | Kry Property | Values | Implementation Status |
|--------------|--------------|--------|----------------------|
| `cursor` | *(not implemented)* | pointer, etc. | ❌ Not supported |
| `pointer-events` | *(not implemented)* | - | ❌ Not supported |
| `user-select` | *(not implemented)* | - | ❌ Not supported |
| `resize` | *(not implemented)* | - | ❌ Not supported |
| `scroll-behavior` | *(not implemented)* | - | ❌ Not supported |

---

## Kry Component Types

### Layout Components

| Component | Description | CSS Equivalent |
|-----------|-------------|-----------------|
| `Container` | Generic container | `<div>` with flex/grid |
| `Row` | Horizontal flex container | `display: flex; flex-direction: row` |
| `Column` | Vertical flex container | `display: flex; flex-direction: column` |
| `Center` | Centered content | `display: flex; justify-content: center; align-items: center` |
| `Grid` | Grid layout (via properties) | `display: grid` |

### Content Components

| Component | Description | HTML Equivalent |
|-----------|-------------|-----------------|
| `Text` | Text content | `<span>`, text node |
| `Heading` | Heading (1-6) | `<h1>`-`<h6>` |
| `Paragraph` | Paragraph | `<p>` |
| `Image` | Image | `<img>` |
| `CodeBlock` | Code block | `<pre><code>` |
| `CodeInline` | Inline code | `<code>` |
| `BlockQuote` | Blockquote | `<blockquote>` |
| `HorizontalRule` | Separator | `<hr>` |

### Form Components

| Component | Description | HTML Equivalent |
|-----------|-------------|-----------------|
| `Button` | Button | `<button>` |
| `Input` | Text input | `<input type="text">` |
| `Checkbox` | Checkbox | `<input type="checkbox">` |
| `Dropdown` | Select dropdown | `<select><option>` |
| `TextArea` | Multi-line input | `<textarea>` |

### List Components

| Component | Description | HTML Equivalent |
|-----------|-------------|-----------------|
| `List` | Ordered/unordered list | `<ul>`, `<ol>` |
| `ListItem` | List item | `<li>` |

### Table Components

| Component | Description | HTML Equivalent |
|-----------|-------------|-----------------|
| `Table` | Table | `<table>` |
| `TableHead` | Table header | `<thead>` |
| `TableBody` | Table body | `<tbody>` |
| `TableFoot` | Table footer | `<tfoot>` |
| `TableRow` | Table row | `<tr>` |
| `TableCell` | Table cell | `<td>` |
| `TableHeaderCell` | Header cell | `<th>` |

### Special Components

| Component | Description | HTML Equivalent |
|-----------|-------------|-----------------|
| `Canvas` | Drawing surface | `<canvas>` |
| `TabGroup` | Tab container | *(custom widget)* |
| `TabBar` | Tab navigation bar | *(custom widget)* |
| `Tab` | Individual tab | *(custom widget)* |
| `TabContent` | Tab content area | *(custom widget)* |
| `TabPanel` | Tab panel | *(custom widget)* |
| `Sprite` | Animated sprite | *(custom widget)* |
| `Markdown` | Markdown document | *(custom widget)* |

---

## Example Comparisons

### Card Component

**HTML/CSS:**
```html
<div class="card">
  <img src="image.jpg" alt="Card image">
  <div class="card-content">
    <h2>Card Title</h2>
    <p>Card description goes here.</p>
    <button>Click Me</button>
  </div>
</div>

<style>
.card {
  background: white;
  border-radius: 8px;
  box-shadow: 0 2px 8px rgba(0,0,0,0.1);
  padding: 16px;
}
.card img {
  width: 100%;
  border-radius: 4px;
}
.card-content h2 {
  margin: 12px 0 8px;
  font-size: 20px;
}
</style>
```

**Kry:**
```kry
Container {
    backgroundColor = "white"
    borderRadius = 8
    boxShadow = "0 2px 8px rgba(0,0,0,0.1)"
    padding = 16

    Column {
        gap = 8

        Image {
            src = "image.jpg"
            width = 100%
            borderRadius = 4
        }

        Column {
            gap = 4

            Heading(level=2) {
                text = "Card Title"
                marginTop = 12
                marginBottom = 8
                fontSize = 20
            }

            Text {
                text = "Card description goes here."
            }

            Button {
                text = "Click Me"
                onClick = { handleClick() }
            }
        }
    }
}
```

### Navigation Bar

**HTML/CSS:**
```html
<nav class="navbar">
  <div class="logo">MyApp</div>
  <ul class="nav-links">
    <li><a href="/">Home</a></li>
    <li><a href="/about">About</a></li>
    <li><a href="/contact">Contact</a></li>
  </ul>
</nav>

<style>
.navbar {
  display: flex;
  justify-content: space-between;
  align-items: center;
  padding: 16px 24px;
  background: #333;
}
.nav-links {
  display: flex;
  gap: 24px;
  list-style: none;
}
.nav-links a {
  color: white;
  text-decoration: none;
}
</style>
```

**Kry:**
```kry
nav {
    display = "flex"
    justifyContent = "space-between"
    alignItems = "center"
    padding = "16px 24px"
    backgroundColor = "#333"

    Row {
        gap = 24

        Text {
            text = "MyApp"
            color = "white"
            fontWeight = 700
        }

        Row {
            gap = 24

            Link {
                text = "Home"
                url = "/"
                color = "white"
            }

            Link {
                text = "About"
                url = "/about"
                color = "white"
            }

            Link {
                text = "Contact"
                url = "/contact"
                color = "white"
            }
        }
    }
}
```

### Form with Validation

**HTML/CSS/JS:**
```html
<form id="signup-form" onsubmit="handleSubmit(event)">
  <input type="email" id="email" placeholder="Email" required>
  <input type="password" id="password" placeholder="Password" required>
  <button type="submit">Sign Up</button>
  <p id="error" class="error"></p>
</form>

<style>
form {
  display: flex;
  flex-direction: column;
  gap: 16px;
  max-width: 400px;
}
.error {
  color: red;
  display: none;
}
</style>

<script>
let email = '';
let password = '';
let error = '';

function handleSubmit(e) {
  e.preventDefault();
  if (!email.includes('@')) {
    error = 'Invalid email';
  }
}
</script>
```

**Kry:**
```kry
state email: string = ""
state password: string = ""
state error: string = ""

Column {
    gap = 16
    maxWidth = 400

    Input {
        placeholder = "Email"
        type = "email"
        value = $email
        onChange = { email = newValue }
    }

    Input {
        placeholder = "Password"
        type = "password"
        value = $password
        onChange = { password = newValue }
    }

    Button {
        text = "Sign Up"
        onClick = {
            if !email.contains("@") {
                error = "Invalid email"
            } else {
                submitForm()
            }
        }
    }

    if error != "" {
        Text {
            text = $error
            color = "red"
        }
    }
}
```

---

## Implementation Status Legend

| Status | Description |
|--------|-------------|
| ✅ Full | Fully implemented and functional |
| ⚠️ Partial | Partially implemented with limitations |
| ❌ Not Supported | Not currently supported |

---

## Summary Statistics

### HTML Element Coverage

| Category | Total | Supported | Partial | Not Supported |
|----------|-------|-----------|---------|---------------|
| Text Content | 9 | 7 | 1 | 1 |
| Text Formatting | 17 | 6 | 1 | 10 |
| Lists | 6 | 3 | 0 | 3 |
| Tables | 10 | 7 | 2 | 1 |
| Forms | 24 | 8 | 6 | 10 |
| Semantic HTML5 | 12 | 11 | 0 | 1 |
| Embedded Content | 13 | 2 | 0 | 11 |
| **TOTAL** | **91** | **44** | **10** | **37** |

**Overall HTML Coverage: ~48% (48% full + partial support)**

### CSS Property Coverage

| Category | Total | Supported | Partial | Not Supported |
|----------|-------|-----------|---------|---------------|
| Layout/Flexbox | 14 | 10 | 2 | 2 |
| Grid | 10 | 3 | 0 | 7 |
| Dimensions | 7 | 7 | 0 | 0 |
| Spacing | 10 | 10 | 0 | 0 |
| Colors | 8 | 7 | 1 | 0 |
| Borders | 11 | 5 | 2 | 4 |
| Typography | 18 | 9 | 1 | 8 |
| Positioning | 8 | 2 | 2 | 4 |
| Overflow | 4 | 0 | 3 | 1 |
| Transitions | 5 | 1 | 4 | 0 |
| Animations | 7 | 2 | 5 | 0 |
| Transforms | 6 | 1 | 1 | 4 |
| Effects | 3 | 2 | 0 | 1 |
| Other | 4 | 0 | 1 | 3 |
| **TOTAL** | **115** | **59** | **22** | **34** |

**Overall CSS Coverage: ~51% (51% full + partial support)**

---

## Notes on Implementation

1. **Kry is not HTML**: Kry is its own language with different semantics. The mapping tables show approximate equivalents for developers familiar with HTML/CSS.

2. **State Management**: Kry has built-in reactive state management, eliminating the need for JavaScript state solutions.

3. **Event Handling**: Event handlers in Kry can contain simple universal logic that compiles to multiple target languages, or language-specific blocks.

4. **Layout System**: Kry uses explicit layout components (Row, Column) rather than CSS display modes, providing clearer intent.

5. **Round-trip Support**: The Kryon IR preserves semantic HTML tags and CSS classes for faithful round-trip conversion.

6. **Extensibility**: New components can be defined as reusable `component` blocks with parameters.

---

*This document is a reference for the Kry language as implemented in the Kryon UI framework. For implementation details, see the source code in `ir/parsers/kry/` and `ir/parsers/html/`.*
