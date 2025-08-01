# KRB to HTML Mapping Reference

## Element Mapping

| KRB Element | HTML Element | Notes |
|-------------|--------------|-------|
| App | `<div class="kryon-app">` | Root container with window metadata |
| Container | `<div>` | Generic container |
| Text | `<span>` or `<p>` | Based on context |
| Button | `<button>` | Interactive button |
| Input | `<input>` | Various input types |
| Link | `<a>` | Hyperlink |
| Image | `<img>` | Image element |
| Video | `<video>` | Video player |
| Audio | `<audio>` | Audio player |
| Canvas | `<canvas>` | Drawing surface |
| Svg | `<svg>` | SVG container |
| WebView | `<iframe>` | Embedded web content |
| Form | `<form>` | Form container |
| Select | `<select>` | Dropdown list |
| TextArea | `<textarea>` | Multi-line input |
| Checkbox | `<input type="checkbox">` | Checkbox input |
| Radio | `<input type="radio">` | Radio button |
| Slider | `<input type="range">` | Range slider |
| FileInput | `<input type="file">` | File picker |
| DatePicker | `<input type="date">` | Date picker |
| ColorPicker | `<input type="color">` | Color picker |
| List | `<ul>` or `<ol>` | List container |
| ListItem | `<li>` | List item |
| Table | `<table>` | Table container |
| TableRow | `<tr>` | Table row |
| TableCell | `<td>` | Table cell |
| TableHeader | `<th>` | Table header |
| Header | `<header>` | Header section |
| Footer | `<footer>` | Footer section |
| Nav | `<nav>` | Navigation section |
| Section | `<section>` | Content section |
| Article | `<article>` | Article content |
| Aside | `<aside>` | Sidebar content |
| ScrollView | `<div style="overflow:auto">` | Scrollable container |
| Grid | `<div style="display:grid">` | Grid container |
| Stack | `<div style="position:relative">` | Stacked layout |
| Spacer | `<div style="flex:1">` | Flexible space |
| EmbedView | `<div>` or platform-specific | Platform embed |

## Property Mapping

### Visual Properties

| KRB Property | CSS Property | Value Mapping |
|--------------|--------------|---------------|
| backgroundColor | background-color | Direct |
| textColor | color | Direct |
| borderColor | border-color | Direct |
| borderWidth | border-width | `{value}px` |
| borderRadius | border-radius | `{value}px` |
| borderStyle | border-style | Direct |
| opacity | opacity | Direct (0-1) |
| visibility | visibility | `true` → "visible", `false` → "hidden" |
| cursor | cursor | Direct |
| boxShadow | box-shadow | Direct |
| filter | filter | Direct |
| backdropFilter | backdrop-filter | Direct |
| transform | transform | Direct |
| transition | transition | Direct |
| animation | animation | Direct |
| gradient | background | Linear/radial gradient |

### Text Properties

| KRB Property | CSS Property | Value Mapping |
|--------------|--------------|---------------|
| fontSize | font-size | `{value}px` |
| fontWeight | font-weight | Direct |
| fontFamily | font-family | Direct |
| fontStyle | font-style | Direct |
| textAlign | text-align | Direct |
| textDecoration | text-decoration | Direct |
| textTransform | text-transform | Direct |
| lineHeight | line-height | Direct |
| letterSpacing | letter-spacing | `{value}px` |
| wordSpacing | word-spacing | `{value}px` |
| whiteSpace | white-space | Direct |
| textOverflow | text-overflow | Direct |
| wordBreak | word-break | Direct |

### Layout Properties

| KRB Property | CSS Property | Value Mapping |
|--------------|--------------|---------------|
| width | width | Dimension mapping |
| height | height | Dimension mapping |
| minWidth | min-width | Dimension mapping |
| maxWidth | max-width | Dimension mapping |
| minHeight | min-height | Dimension mapping |
| maxHeight | max-height | Dimension mapping |
| display | display | Direct |
| position | position | Direct |
| left/x | left | Dimension mapping |
| top/y | top | Dimension mapping |
| right | right | Dimension mapping |
| bottom | bottom | Dimension mapping |
| zIndex | z-index | Direct |
| overflow | overflow | Direct |
| overflowX | overflow-x | Direct |
| overflowY | overflow-y | Direct |

### Flexbox Properties

| KRB Property | CSS Property | Value Mapping |
|--------------|--------------|---------------|
| flexDirection | flex-direction | Direct |
| flexWrap | flex-wrap | Direct |
| justifyContent | justify-content | Direct |
| alignItems | align-items | Direct |
| alignContent | align-content | Direct |
| alignSelf | align-self | Direct |
| flexGrow | flex-grow | Direct |
| flexShrink | flex-shrink | Direct |
| flexBasis | flex-basis | Dimension mapping |
| gap | gap | `{value}px` |
| rowGap | row-gap | `{value}px` |
| columnGap | column-gap | `{value}px` |

### Grid Properties

| KRB Property | CSS Property | Value Mapping |
|--------------|--------------|---------------|
| gridTemplateColumns | grid-template-columns | Direct |
| gridTemplateRows | grid-template-rows | Direct |
| gridColumn | grid-column | Direct |
| gridRow | grid-row | Direct |
| gridArea | grid-area | Direct |
| gridGap | grid-gap | `{value}px` |

### Spacing Properties

| KRB Property | CSS Property | Value Mapping |
|--------------|--------------|---------------|
| padding | padding | `{value}px` or `{t}px {r}px {b}px {l}px` |
| paddingTop | padding-top | `{value}px` |
| paddingRight | padding-right | `{value}px` |
| paddingBottom | padding-bottom | `{value}px` |
| paddingLeft | padding-left | `{value}px` |
| margin | margin | `{value}px` or `{t}px {r}px {b}px {l}px` |
| marginTop | margin-top | `{value}px` |
| marginRight | margin-right | `{value}px` |
| marginBottom | margin-bottom | `{value}px` |
| marginLeft | margin-left | `{value}px` |

### Interactive Properties

| KRB Property | HTML Attribute | CSS Property | Notes |
|--------------|----------------|--------------|-------|
| id | id | - | Direct |
| class | class | - | Direct |
| style | - | - | Apply to style attribute |
| disabled | disabled | - | Boolean attribute |
| enabled | - | - | Inverse of disabled |
| tooltip | title | - | Direct |
| pointerEvents | - | pointer-events | Direct |
| userSelect | - | user-select | Direct |

## Dimension Mapping

| KRB Dimension | CSS Value |
|---------------|-----------|
| Auto | `auto` |
| Pixels(n) | `{n}px` |
| Percent(n) | `{n}%` |
| MinContent | `min-content` |
| MaxContent | `max-content` |
| Rem(n) | `{n}rem` |
| Em(n) | `{n}em` |
| Vw(n) | `{n}vw` |
| Vh(n) | `{n}vh` |

## Event Mapping

| KRB Event | HTML Event | JavaScript Handler |
|-----------|------------|-------------------|
| onClick | onclick | `element.addEventListener('click', handler)` |
| onDoubleClick | ondblclick | `element.addEventListener('dblclick', handler)` |
| onMouseEnter | onmouseenter | `element.addEventListener('mouseenter', handler)` |
| onMouseLeave | onmouseleave | `element.addEventListener('mouseleave', handler)` |
| onMouseMove | onmousemove | `element.addEventListener('mousemove', handler)` |
| onMouseDown | onmousedown | `element.addEventListener('mousedown', handler)` |
| onMouseUp | onmouseup | `element.addEventListener('mouseup', handler)` |
| onFocus | onfocus | `element.addEventListener('focus', handler)` |
| onBlur | onblur | `element.addEventListener('blur', handler)` |
| onChange | onchange | `element.addEventListener('change', handler)` |
| onInput | oninput | `element.addEventListener('input', handler)` |
| onKeyDown | onkeydown | `element.addEventListener('keydown', handler)` |
| onKeyUp | onkeyup | `element.addEventListener('keyup', handler)` |
| onKeyPress | onkeypress | `element.addEventListener('keypress', handler)` |
| onScroll | onscroll | `element.addEventListener('scroll', handler)` |
| onLoad | onload | `element.addEventListener('load', handler)` |
| onError | onerror | `element.addEventListener('error', handler)` |

## Element-Specific Attributes

### Input Element

| KRB Property | HTML Attribute |
|--------------|----------------|
| type | type |
| value | value |
| placeholder | placeholder |
| required | required |
| readonly | readonly |
| maxLength | maxlength |
| minLength | minlength |
| pattern | pattern |
| autocomplete | autocomplete |
| spellcheck | spellcheck |

### Image Element

| KRB Property | HTML Attribute |
|--------------|----------------|
| src | src |
| alt | alt |
| loading | loading |

### Link Element

| KRB Property | HTML Attribute |
|--------------|----------------|
| href | href |
| target | target |
| download | download |
| rel | rel |

### Media Elements (Video/Audio)

| KRB Property | HTML Attribute |
|--------------|----------------|
| src | src |
| autoplay | autoplay |
| controls | controls |
| loop | loop |
| muted | muted |
| poster | poster |
| preload | preload |

### Select Element

| KRB Property | HTML Implementation |
|--------------|-------------------|
| options | `<option>` children |
| selected | selected attribute on option |
| multiple | multiple attribute |
| size | size attribute |

## Style Classes

Generated HTML includes Kryon-specific classes:

```html
<div class="kryon-app">
  <div class="kryon-container">
    <span class="kryon-text">Text</span>
    <button class="kryon-button">Button</button>
  </div>
</div>
```

## Data Attributes

KRB metadata is preserved as data attributes:

```html
<div data-kryon-id="123" 
     data-kryon-type="Container"
     data-kryon-parent="122">
```

## Script Integration

Scripts are included based on language:

```html
<!-- Lua -->
<script src="https://unpkg.com/fengari-web/dist/fengari-web.js"></script>
<script type="application/lua">
  -- Lua code
</script>

<!-- JavaScript -->
<script>
  // JavaScript code
</script>

<!-- Python (Pyodide) -->
<script src="https://cdn.jsdelivr.net/pyodide/v0.24.1/full/pyodide.js"></script>
<script type="text/python">
  # Python code
</script>
```

## Component Rendering

Components are expanded inline:

```kry
@component Card {
  @props { title: String }
  Container {
    Text { text: props.title }
  }
}

Card { title: "Hello" }
```

Becomes:

```html
<div class="kryon-container kryon-component-card">
  <span class="kryon-text">Hello</span>
</div>
```