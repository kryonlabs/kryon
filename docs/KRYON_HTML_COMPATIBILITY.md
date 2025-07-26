# Kryon HTML Compatibility Reference

This document outlines how Kryon elements and properties map to HTML elements to provide comprehensive HTML compatibility while maintaining Kryon's simplified and efficient approach.

## Compatibility Strategy

Kryon achieves HTML compatibility through three main approaches:

1. **Direct Mapping**: Core HTML elements map directly to Kryon elements
2. **Aliasing**: Multiple HTML element names resolve to the same Kryon hex value
3. **Property-Based Semantics**: Semantic differences handled through properties rather than separate elements

## Element Compatibility Matrix

### 1. Document Structure (HTML Root & Metadata)

| HTML Element | Kryon Element | Hex | Compatibility Method | Notes |
|--------------|---------------|-----|---------------------|--------|
| `<html>` | `App` | `0x00` | Direct mapping | Root container |
| `<head>` | *Handled by compiler* | - | Compile-time | Metadata processed during compilation |
| `<body>` | `Container` | `0x01` | Direct mapping | Main content container |
| `<title>` | *Window property* | - | Property-based | `window-title` property |
| `<meta>` | *Compile-time* | - | Compiler directive | Processed during build |
| `<link>` | `Link` | `0x12` | Direct mapping | External resource links |
| `<style>` | *Kryon styling* | - | Native system | Uses Kryon's style system |
| `<base>` | *Global property* | - | Configuration | Base URL in app config |

### 2. Content Sectioning

| HTML Element | Kryon Element | Hex | Compatibility Method | Properties |
|--------------|---------------|-----|---------------------|------------|
| `<section>` | `Container` | `0x01` | Alias + property | `semantic-role: "section"` |
| `<article>` | `Container` | `0x01` | Alias + property | `semantic-role: "article"` |
| `<aside>` | `Container` | `0x01` | Alias + property | `semantic-role: "aside"` |
| `<header>` | `Container` | `0x01` | Alias + property | `semantic-role: "header"` |
| `<footer>` | `Container` | `0x01` | Alias + property | `semantic-role: "footer"` |
| `<nav>` | `Container` | `0x01` | Alias + property | `semantic-role: "nav"` |
| `<main>` | `Container` | `0x01` | Alias + property | `semantic-role: "main"` |
| `<h1>-<h6>` | `Text` | `0x02` | Property-based | `heading-level: 1-6` |
| `<hgroup>` | `Container` | `0x01` | Alias + property | `semantic-role: "hgroup"` |
| `<address>` | `Text` | `0x02` | Property-based | `semantic-role: "address"` |
| `<search>` | `Container` | `0x01` | Alias + property | `semantic-role: "search"` |

### 3. Text Content Elements

| HTML Element | Kryon Element | Hex | Compatibility Method | Properties |
|--------------|---------------|-----|---------------------|------------|
| `<p>` | `Text` | `0x02` | Direct mapping | Default text element |
| `<div>` | `Container` | `0x01` | Direct mapping | Generic container |
| `<span>` | `Text` | `0x02` | Alias | Inline text container |
| `<blockquote>` | `Text` | `0x02` | Property-based | `semantic-role: "blockquote"` |
| `<pre>` | `Text` | `0x02` | Property-based | `white-space: "pre"` |
| `<code>` | `Text` | `0x02` | Property-based | `font-family: "monospace"`, `semantic-role: "code"` |
| `<ul>` | `List` | `0x41` | Direct mapping | `list-type: "unordered"` |
| `<ol>` | `List` | `0x41` | Direct mapping | `list-type: "ordered"` |
| `<li>` | `ListItem` | `0x42` | Direct mapping | List item element |
| `<dl>` | `List` | `0x41` | Alias | `list-type: "description"` |
| `<dt>` | `ListItem` | `0x42` | Property-based | `list-item-role: "term"` |
| `<dd>` | `ListItem` | `0x42` | Property-based | `list-item-role: "definition"` |
| `<figure>` | `Container` | `0x01` | Property-based | `semantic-role: "figure"` |
| `<figcaption>` | `Text` | `0x02` | Property-based | `semantic-role: "figcaption"` |
| `<hr>` | `Container` | `0x01` | Property-based | `semantic-role: "separator"` |
| `<menu>` | `List` | `0x41` | Alias | Same as `<ul>` |

### 4. Inline Text Semantics

| HTML Element | Kryon Element | Hex | Compatibility Method | Properties |
|--------------|---------------|-----|---------------------|------------|
| `<a>` | `Link` | `0x12` | Direct mapping | Hyperlink element |
| `<strong>` | `Text` | `0x02` | Property-based | `font-weight: 700` |
| `<em>` | `Text` | `0x02` | Property-based | `font-style: "italic"` |
| `<b>` | `Text` | `0x02` | Property-based | `font-weight: 700` |
| `<i>` | `Text` | `0x02` | Property-based | `font-style: "italic"` |
| `<u>` | `Text` | `0x02` | Property-based | `text-decoration: "underline"` |
| `<s>` | `Text` | `0x02` | Property-based | `text-decoration: "line-through"` |
| `<mark>` | `Text` | `0x02` | Property-based | `background-color: "yellow"` |
| `<small>` | `Text` | `0x02` | Property-based | `font-size: "smaller"` |
| `<sub>` | `Text` | `0x02` | Property-based | `vertical-align: "sub"` |
| `<sup>` | `Text` | `0x02` | Property-based | `vertical-align: "super"` |
| `<q>` | `Text` | `0x02` | Property-based | `semantic-role: "quote"` |
| `<cite>` | `Text` | `0x02` | Property-based | `semantic-role: "cite"` |
| `<abbr>` | `Text` | `0x02` | Property-based | `semantic-role: "abbreviation"` |
| `<dfn>` | `Text` | `0x02` | Property-based | `semantic-role: "definition"` |
| `<time>` | `Text` | `0x02` | Property-based | `semantic-role: "time"` |
| `<data>` | `Text` | `0x02` | Property-based | `semantic-role: "data"` |
| `<kbd>` | `Text` | `0x02` | Property-based | `font-family: "monospace"`, `semantic-role: "keyboard"` |
| `<samp>` | `Text` | `0x02` | Property-based | `font-family: "monospace"`, `semantic-role: "sample"` |
| `<var>` | `Text` | `0x02` | Property-based | `font-style: "italic"`, `semantic-role: "variable"` |
| `<br>` | `Text` | `0x02` | Special | Line break character in text content |
| `<wbr>` | `Text` | `0x02` | Special | Word break opportunity in text |

### 5. Media Elements

| HTML Element | Kryon Element | Hex | Compatibility Method | Properties |
|--------------|---------------|-----|---------------------|------------|
| `<img>` | `Image` | `0x20` | Direct mapping | Image display |
| `<audio>` | `Video` | `0x21` | Alias | `media-type: "audio"` |
| `<video>` | `Video` | `0x21` | Direct mapping | Video playback |
| `<svg>` | `Svg` | `0x23` | Direct mapping | SVG graphics |
| `<canvas>` | `Canvas` | `0x22` | Direct mapping | 2D/3D drawing |
| `<picture>` | `Image` | `0x20` | Enhanced | Responsive image with sources |
| `<source>` | *Property* | - | Media source | Sources array in media elements |
| `<track>` | *Property* | - | Media track | Tracks array in media elements |
| `<area>` | *Interactive zone* | - | Canvas/SVG | Clickable areas in images |
| `<map>` | *Image property* | - | Image map | Image map definition |

### 6. Embedded Content

| HTML Element | Kryon Element | Hex | Compatibility Method | Properties |
|--------------|---------------|-----|---------------------|------------|
| `<iframe>` | `EmbedView` | `0x03` | Direct mapping | `embed-type: "webview"` |
| `<embed>` | `EmbedView` | `0x03` | Alias | `embed-type: "plugin"` |
| `<object>` | `EmbedView` | `0x03` | Alias | `embed-type: "object"` |
| `<fencedframe>` | `EmbedView` | `0x03` | Alias | `embed-type: "fenced"` |

### 7. Form Elements

| HTML Element | Kryon Element | Hex | Compatibility Method | Properties |
|--------------|---------------|-----|---------------------|------------|
| `<form>` | `Form` | `0x40` | Direct mapping | Form container |
| `<input>` | `Input` | `0x11` | Direct mapping | Various input types |
| `<button>` | `Button` | `0x10` | Direct mapping | Interactive button |
| `<select>` | `Select` | `0x30` | Direct mapping | Dropdown selection |
| `<option>` | *Property* | - | Select option | Options array in Select |
| `<optgroup>` | *Property* | - | Option group | Grouped options in Select |
| `<textarea>` | `Input` | `0x11` | Property-based | `input-type: "textarea"` |
| `<label>` | `Text` | `0x02` | Property-based | `semantic-role: "label"` |
| `<fieldset>` | `Container` | `0x01` | Property-based | `semantic-role: "fieldset"` |
| `<legend>` | `Text` | `0x02` | Property-based | `semantic-role: "legend"` |
| `<datalist>` | *Property* | - | Input datalist | Suggestions array in Input |
| `<output>` | `Text` | `0x02` | Property-based | `semantic-role: "output"` |
| `<progress>` | `ProgressBar` | `0x32` | Direct mapping | Progress indicator |
| `<meter>` | `ProgressBar` | `0x32` | Alias | `meter-mode: true` |

### 8. Table Elements

| HTML Element | Kryon Element | Hex | Compatibility Method | Properties |
|--------------|---------------|-----|---------------------|------------|
| `<table>` | `Table` | `0x50` | Direct mapping | Table container |
| `<tr>` | `TableRow` | `0x51` | Direct mapping | Table row |
| `<td>` | `TableCell` | `0x52` | Direct mapping | Table data cell |
| `<th>` | `TableHeader` | `0x53` | Direct mapping | Table header cell |
| `<thead>` | `Container` | `0x01` | Property-based | `table-section: "head"` |
| `<tbody>` | `Container` | `0x01` | Property-based | `table-section: "body"` |
| `<tfoot>` | `Container` | `0x01` | Property-based | `table-section: "foot"` |
| `<caption>` | `Text` | `0x02` | Property-based | `semantic-role: "table-caption"` |
| `<colgroup>` | *Property* | - | Table property | Column group definition |
| `<col>` | *Property* | - | Table property | Column definition |

### 9. Interactive Elements

| HTML Element | Kryon Element | Hex | Compatibility Method | Properties |
|--------------|---------------|-----|---------------------|------------|
| `<details>` | `Container` | `0x01` | Property-based | `interactive-type: "details"` |
| `<summary>` | `Text` | `0x02` | Property-based | `semantic-role: "summary"` |
| `<dialog>` | `Container` | `0x01` | Property-based | `interactive-type: "dialog"` |

### 10. Scripting Elements

| HTML Element | Kryon Element | Hex | Compatibility Method | Properties |
|--------------|---------------|-----|---------------------|------------|
| `<script>` | *Kryon scripts* | - | Native system | Uses Kryon's multi-VM scripting |
| `<noscript>` | *Conditional* | - | Conditional rendering | Kryon's conditional system |

### 11. Web Components

| HTML Element | Kryon Element | Hex | Compatibility Method | Properties |
|--------------|---------------|-----|---------------------|------------|
| `<template>` | *Component* | - | Kryon components | Define/Render component system |
| `<slot>` | *Component slot* | - | Component property | Component property system |

## New Properties for HTML Compatibility

To support HTML semantics, we need these additional properties:

### Semantic Properties (0xC0-0xCF)

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

## Implementation Strategy

### Phase 1: Core Compatibility (90% HTML support)
- Implement all direct mappings
- Add semantic-role property system  
- Support all common HTML elements through aliasing
- Implement essential new properties

### Phase 2: Advanced Features (95% HTML support)
- Complex table features (colgroup, col)
- Advanced form features (datalist, fieldset)
- Media element enhancements (picture, source, track)
- Web component compatibility

### Phase 3: Complete Compatibility (100% HTML support)
- All remaining edge cases
- Full accessibility support
- Legacy element compatibility (with deprecation warnings)
- Advanced interactive elements

## Alias Registration System

Elements can be registered with multiple names pointing to the same hex value:

```rust
// Element alias system
pub enum ElementAlias {
    Primary(&'static str),    // Primary name (e.g., "Container")
    Alias(&'static str),      // HTML alias (e.g., "div", "section", "article")
}

pub const ELEMENT_ALIASES: &[(ElementAlias, u8)] = &[
    (ElementAlias::Primary("Container"), 0x01),
    (ElementAlias::Alias("div"), 0x01),
    (ElementAlias::Alias("section"), 0x01),
    (ElementAlias::Alias("article"), 0x01),
    (ElementAlias::Alias("aside"), 0x01),
    (ElementAlias::Alias("header"), 0x01),
    (ElementAlias::Alias("footer"), 0x01),
    (ElementAlias::Alias("nav"), 0x01),
    (ElementAlias::Alias("main"), 0x01),
    // ... more aliases
];
```

## HTML-to-Kryon Compilation Examples

### Input HTML:
```html
<article>
    <header>
        <h1>Article Title</h1>
    </header>
    <p>Article content with <strong>bold text</strong> and <em>italic text</em>.</p>
    <footer>
        <small>Copyright info</small>
    </footer>
</article>
```

### Compiled Kryon:
```kry
Container {
    semantic-role: "article"
    
    Container {
        semantic-role: "header"
        
        Text {
            text: "Article Title"
            heading-level: 1
            font-size: 24
            font-weight: 700
        }
    }
    
    Text {
        text: "Article content with bold text and italic text."
        // Rich text formatting handled internally
    }
    
    Container {
        semantic-role: "footer"
        
        Text {
            text: "Copyright info"
            font-size: 12
        }
    }
}
```

This approach provides near-complete HTML compatibility while maintaining Kryon's efficiency and simplicity.