# HTML & CSS to Kryon / Ziran Mapping

Use this guide to port a dashboard into native Kryon. Write the view in `.zi`,
keep the supported CSS declarations, and build the same frame into `.zx` with a
portable Kryon profile. The [responsive dashboard fixture](../examples/dashboard/previews/responsive.zi)
shows the composition and [its README](../examples/dashboard/README.md) has
build and verification commands.

---

## 1. Mental Model: DOM to Retained Frame

| Web / HTML Concept | Kryon / Ziran Equivalent | Notes |
| --- | --- | --- |
| DOM tree mutated by callbacks | Immediate declaration per frame inside `Frame(session, viewport) -> s32` | Kryon retains node identities across frames by `(parent, key, kind)`. |
| Component / element state | Caller-owned structs or file-scope variables (`bool`, `s32`, `TextField.State`, `[N]u8`) | Widgets never own application state; read `Result` structs each frame. |
| `key` / `id` attribute | `.key = cast(u64)N`, `.id = CSS.StyleId("...")`, `.class_name = CSS.StyleClass("...")` | Every sibling widget in a frame needs a unique nonzero `key`. |
| Parent/child nesting | Container returns `.opened: bool`; submit children, then call `End(session)` | Applies to `Page.Draw`, `Page.Section`, `Layout.*`, `Scroll.Draw`, `Modal.Draw`, `Collapsible.Draw`. |
| `window.innerWidth` / `@media` | Pass `viewport.width` to `CSS.CssEnvironmentWithViewportWidth(env, viewport.width)` | Native width, min-width and max-width accept literal `px`, `rem` or unitless zero. Use `CssSheetSameMedia` to avoid reinstalling while the matching rules stay the same. |

---

## 2. Semantic HTML Element -> Kryon Widget Table

Agents and editor tools can load [ui-map.json](ui-map.json) for the native
imports and callable symbols. Match the most specific HTML selector first.
The `css` value gives the Kryon stylesheet selector. When `scope` is present,
draw the children and call `End(session)` only if that result field is true.
`lists` gives the ordered slice arguments after props as `[name, item type]`
pairs. Item types without a package path are exported by the widget module or
are built-in Ziran types.

| HTML Element / Pattern | Kryon Package & Call | Key Props / Notes |
| --- | --- | --- |
| `<html>`, `<body>` | `Page :: #import "kryon/page";` -> `Page.Draw(session, Page.Props.{...})` or `Layout.Screen` | Call `End(session)` if `result.opened`. Also fill root background with `Box.Draw`. |
| `<main>`, `<section>` | `Page.Section(session, Page.SectionProps.{.bounds, .label, .key})` | Sets `SemanticSection`; close with `End(session)` when `opened`. |
| `<article>`, `.card` | `Card :: #import "kryon/card";` -> `Card.Draw(session, Card.Props.{.bounds, .variant})` | Variants: `.CardFilled`, `.CardElevated`, `.CardOutlined`. CSS padding sets `result.content`. Draw children inside that rectangle. |
| `<div>` (styled box) | `Box :: #import "kryon/box";` -> `Box.Draw(session, Box.Props.{.bounds, .fill, .border, .radius})` | Decorative only; captures no input. |
| `<header>` / toolbar | `Toolbar :: #import "kryon/toolbar";` or `TitleBar :: #import "kryon/title_bar";` | Or `Layout.Row` + `Button` / `Text`. |
| `<nav>` (rail / bar) | `Navigation :: #import "kryon/navigation";` -> `Navigation.Draw(session, props, items)` | Use `.compact = true, .fit_items = true` for embedded panels/rails, or vertical `Button` list. |
| `[role="tablist"]` | `TabBar :: #import "kryon/tab_bar";` -> `TabBar.Draw(session, props, tabs)` | Supports `.TabBarTop`, `.TabBarBottom`, `.TabBarLeft`, `.TabBarRight`. |
| `<h1>`..`<h6>` | `Page.Heading(session, .{.bounds, .text, .level = 1})` or `Text.Draw` with `.semantic_kind = .SemanticHeading, .heading_level = N` | Exposes heading semantics to test driver and accessibility tree. |
| `<p>`, `<span>`, `<label>` | `Page.ParagraphText(...)` or `Text :: #import "kryon/text";` -> `Text.Draw(session, Text.Props.{...})` | Supports `.wrap = .TextWrapAuto \| .TextWrapNone \| .TextWrapEllipsis \| .TextWrapAnywhere`, `.align`, `.strong`, `.links`. |
| `<a href="...">` | `Link :: #import "kryon/link";` -> `Link.Draw(session, Link.Props.{.text, .link, .bounds})` | Caller handles navigation when `result.open_url` is true. |
| `<button>` | `Button :: #import "kryon/button";` -> `Button.Draw(session, Button.Props.{...}) != 0` | `.tone` (`Neutral`, `Accent`, `Danger`, `Success`, `Warning`), `.emphasis` (`Filled`, `Soft`, `Outline`, `Ghost`, `Link`), `.detail`, `.icon_shape`, `.selected`. |
| `<input type="text\|search\|password\|number">` | `TextField :: #import "kryon/text_field";` -> `TextField.Bind(session, *state, props)` | Binds caller-owned `TextField.State` directly to session input (`session_input = true` default) and returns `BindingResult` (`node`, `focused`, `changed`, `committed`, `escaped`). Use `TextField.Draw(session, TextField.StateProps(state, props))` + `StateApplyResult` when supplying custom `props.input`. Set `.secure = true` for password, `.numeric = true` for digits. |
| `<textarea>` | `TextArea :: #import "kryon/text_area";` -> `TextArea.Draw(...)` | Multiline editor using `TextAreaInputFor(...)`. |
| `<input type="checkbox">` | `Checkbox :: #import "kryon/checkbox";` -> `Checkbox.Draw(session, Checkbox.Props.{.checked, .label})` | Read `result.changed` and `result.checked`. |
| `<input type="checkbox" role="switch">` | `Toggle :: #import "kryon/toggle";` -> `Toggle.Draw(session, Toggle.Props.{.value, .label})` | Read `result.changed` and `result.value`. |
| `<input type="radio">` | `Radio :: #import "kryon/radio";` -> `Radio.Draw(...)` | Caller tracks selected group index. |
| `<select>` | `Dropdown :: #import "kryon/dropdown";` -> `Dropdown.Draw(session, props, options)` | Manages its own popup menu & scroll; read `result.selected_index`. |
| `<input type="range">` | `Slider :: #import "kryon/slider";` -> `Slider.Draw(...)` / `Slider.DiscreteSlider(...)` | Continuous float or discrete integer steps. |
| `<progress>` | `Progress :: #import "kryon/progress";` -> `Progress.Draw(session, .{.bounds, .min, .max, .value})` | Linear progress bar. |
| `<fieldset>` | `Fieldset :: #import "kryon/fieldset";` -> `Fieldset.Draw(...)` | Grouped form section. |
| `<ul>` / `<select multiple>` | `ListBox :: #import "kryon/list_box";` -> `ListBox.Draw(...)` / `ListBox.ListBoxMulti(...)` | Built-in scroll + keyboard Up/Down/Home/End, or use `Scroll` + `Button` rows for rich rows. |
| `<table>`, `<th>`, `<td>` | `Scroll.Draw` + `Layout.Grid` + `Text.Draw` | Give all rows the same explicit tracks. Use `.horizontal = true` and `.content_width` to preserve the columns on narrow viewports. Label headers and cells in reading order. |
| `<details>` / accordion | `Collapsible :: #import "kryon/collapsible";` -> `Collapsible.Draw(...)` | Animated reveal; draw body inside `result.content` when open. |
| `<dialog>` | `Modal :: #import "kryon/modal";` -> `Modal.Draw(session, props)` or `Dialog.Dialog(...)` | Scoped container (`if modal.opened { ... End(session) }`); `modal.dismissed` fires on Escape/scrim. |
| `<hr>` | `Separator :: #import "kryon/separator";` -> `Separator.Draw(session, .{.bounds, .vertical})` | Horizontal or vertical divider, optional centered `.label`. |
| `<img>` | `Image :: #import "kryon/image";` -> `Image.Draw(session, Image.Props.{.asset_path, .bounds})` | Supports PNG/WebP/AVIF or `texture`. |
| `<svg>` (icon) | `SvgIcon :: #import "kryon/svg_icon";` -> `SvgIcon.Draw(session, .{.shape, .bounds, .color})` | Uses Kryon `Shape` path strings (e.g. `"S1.8rr:M..."`). |
| Status pill / tag | `Badge :: #import "kryon/badge";` -> `Badge.Draw(session, .{.text, .tone, .dot, .solid})` | Tones: `Neutral`, `Info`, `Success`, `Warning`, `Danger`, `Violet`, `Teal`, `Rose`, `Accent`. |
| Spinner / skeleton | `Loading :: #import "kryon/loading_indicator";` -> `Loading.Draw(session, .{.bounds, .now})` | Pass seconds clock (`CaptureFrameInput(session).elapsed_ms / 1000.0`). |
| Empty search/list state | `EmptyState :: #import "kryon/empty_state";` -> `EmptyState.Draw(session, .{.title, .description, .action})` | Read `result.action` to recover/reset filters. |
| Error / notice banner | `Alert :: #import "kryon/alert";` -> `Alert.Draw(session, props, *state)` or `Banner.Draw(...)` | Tones: `AlertToneNeutral`, `Info`, `Success`, `Warning`, `Danger`. |

---

## 3. Native Layout Equivalents (CSS Flex / Grid -> Kryon Layout)

Kryon separates **retained layout containers** (`kryon/layout`, `kryon/scroll`, `kryon/paned_view`) from **pure geometry cursors** (`layout_behavior`, `grid`, `kryon/PageLayout`).

### A. Container Scopes (`kryon/layout`, `kryon/scroll`, `kryon/flow`)

```zi
Layout :: #import "kryon/layout";
Scroll :: #import "kryon/scroll";
TextField :: #import "kryon/text_field";
Theme :: #import "kryon/theme";

// display: flex; flex-direction: column; gap: 12px; padding: 16px;
// No height: the column grows to its children when End closes it.
gap := Layout.Gap(.ThemeMetricSpace3)
col := Layout.Draw(session, Layout.Props.{
    .key = 10,
    .gap = gap, .gap_override = true,
    .padding = 16, .padding_override = true
})
if col.opened {
    // Children with unset x/y stack; an unset width fills the column.
    unused TextField.Bind(session, *search, TextField.Props.{.key = 11, .placeholder = "Search..."})
    End(session)
}

// overflow-y: auto;
sc := Scroll.Draw(session, Scroll.Props.{
    .key = 20, .bounds = list_rect, .content_height = total_h, .scroll_offset = scroll_y
})
scroll_y = sc.scroll_offset
if sc.opened {
    // Position items at sc.content.y + offset
    End(session)
}
```

> [!TIP]
> `Layout.Props.gap` and `Layout.Props.padding` only override the theme/CSS values `0` or nonzero when `.gap_override = true` / `.padding_override = true` (useful when forcing zero gap/padding against a non-zero theme default). A `Row`, `Column`, `Grid` (without items) or `Flow` with no height from props or CSS is content-sized: it opens with zero height, places each child that has no `x`/`y`, and at `End` takes the height its children used plus block padding (within `min-height` and `max-height`); its parent then moves the next sibling down. Children at explicit coordinates stay out of that size, like absolutely positioned elements. `Layout.GridSpan(session, grid.container, -1)` makes the next placed grid child span the row. A composite widget that paints its own box (Card, TitleBar) still needs its height before it draws: give its `Layout.Stack` slot a height.

### B. Measured flex and grid containers

- **Block flow** (`div` stacks, `display: flex` rows without justification or growth, `flex-wrap: wrap` chip rows) needs no measuring: use a content-sized `Layout.Draw`, `Layout.Row`, `Layout.Flow` or item-less `Layout.Grid` and draw the children.
- **CSS Flexbox** with justification, growth or cross-axis alignment uses `Layout.Row(session, props, items[:], placements[:])` or the same arguments to `Layout.Draw` for a column. Pass a `[]Layout.FlexItem` with measured sizes and each styled child's `kind`, `id` and `class_name`. The container resolves CSS direction, wrapping, gaps, padding, justification and alignment. Children can set basis, grow, shrink and self-alignment in CSS. Draw in `placements[i].bounds` and close the layout scope once. The low-level `BeginFlexCursor` and `FlexStep` functions place premeasured children in a single line.
- **CSS Grid** uses `Layout.Grid(session, Grid.GridProps.{.key, .class_name}, items[:])`.
  Give each item a height or a `measure_height(width, key)` callback. A zero height calls the measurement with the item's allocated width after tracks and spans are resolved. The tallest item sets each row's height. Pass an output `[]Rectangle` as the fourth argument to measure and place all items in one pass, then draw children using those bounds. Two-argument calls place unpositioned single-track children using their submitted sizes.
- **CSS `grid-column: 1/-1`** maps to `Grid.GridItem.{.column_span = -1}`. Positive spans occupy that many tracks. To walk the grid manually, call `Grid.GridItemStep(cursor, item)` and read the returned `cursor.item`. Fixed-height callers can continue using `Grid.GridStep`.
- **Breakpoint layout** uses the same stylesheet and frame code at every width. Set the viewport in the CSS environment before submitting widgets. `PageLayout.PageFor` remains available for manually centered or capped page geometry.
- **Split Pane** -> `PanedView.Draw(session, PanedView.Props.{.bounds, .split, .has_split = true, .vertical = true})` -> read `panes.first`, `panes.second`, `panes.split`.

---

## Author a responsive dashboard

```css
Grid.metrics {
  grid-template-columns: repeat(auto-fit, minmax(min(165px, 100%), 1fr));
  gap: 14px;
}
Grid.charts {
  grid-template-columns: repeat(2, minmax(0, 1fr));
  gap: 14px;
}
Card.panel { background: #151f30; border: 1px solid #29384f; border-radius: 16px; padding: 20px; }
@media (max-width: 820px) {
  Grid.charts { grid-template-columns: minmax(0, 1fr); }
  Card.panel { padding: 12px; }
}
```

```zi
CSS :: #import "css/Css";
Grid :: #import "kryon/grid";
Layout :: #import "kryon/layout";

// Inside a Column or Section scope, after installing the viewport styles:
items: [3]Grid.GridItem = .[.{.height = 340, .column_span = -1}, .{.height = 240}, .{.height = 240}]
charts := Layout.Grid(session, Grid.GridProps.{.key = 30, .class_name = CSS.StyleClass("charts")}, items[:])
if charts.container.opened {
    cursor := charts.cursor
    for i: 0..2 {
        cursor = Grid.GridStep(cursor, items[i].height, items[i].column_span)
        // Draw each keyed Card/Plot in cursor.item.
    }
    unused End(session)
}
```

| Dashboard HTML/CSS | Native authoring |
| --- | --- |
| `main { max-width: 1480px; margin: 0 auto }` | Use the same declarations on a `Column`, `Row` or `Grid`: the container caps its width and centers itself in its parent's inner width. |
| `margin`, `min-width`, `max-height`, `overflow: hidden` | Same CSS on `Row`, `Column` and `Grid`. Margins move the box inside the slot its parent places, so siblings keep their gaps. `overflow: hidden` clips descendants to the box. |
| `display: grid`, equal `1fr` tracks | Select `Layout.Grid` in `.zi`. Use `1fr`, `minmax(0, 1fr)`, or `repeat(N, ...)` in CSS. `N` is 1–1024. |
| `repeat(auto-fit, minmax(min(165px, 100%), 1fr))` | Use the same declaration on `Grid`. Measured grids collapse unused tracks. A single track can shrink below the minimum. Literal minimums also work without the `min(...)` wrapper. |
| `grid-column: 1/-1` | Set `.column_span = -1` on the corresponding `GridItem`. Arbitrary line numbers, named areas, unequal tracks, dense packing and explicit row templates stay in the browser layout. |
| `@media (max-width: 820px)` | Keep the declaration and supply logical viewport width to the native CSS environment. Orientation, height, print and range-operator queries stay browser-side. |
| `gap: 12px 14px`, `padding: 8px 12px` | Use the same CSS on `Grid`. Grid interprets row and column gap independently and follows the native axis-pair padding model. Card uses axis padding for its content rectangle. |
| `font-size: clamp(...)`, `padding: clamp(...)` | Use literal sizes in width media rules, or calculate the clamp from the viewport in `.zi` and pass an explicit size or padding prop. `vw`, `em`, `calc()` and `clamp()` do not resolve as native lengths. |
| `font-weight`, `line-height`, `letter-spacing` | Keep native-supported literal properties on `Heading` and `Text`. Use a typeface with fixed-width digits or a mono face for numeric columns. CSS `font-variant-numeric` and `text-transform` do not apply to native text. |
| `background: radial-gradient(...)` | Use the base `background-color`, or a supported two-stop `linear-gradient` on a surface that draws gradients. The native CSS parser does not accept radial gradients. |
| Wrapped navigation, `a[href="#section"]` | Use native `Link`/`Button` controls in a measured `Row` with `flex-wrap: wrap` or in a grid. Handle navigation and scrolling in the frame. |
| `.table-scroll`, `white-space: nowrap`, border-collapse | Use `Scroll.Draw` with `.horizontal = true` and a shared `Grid` for row/cell slots. Use `.wrap = .TextWrapNone` and separators for row lines. Browser table layout and border collapse have no native counterpart. |
| `details > summary`, `pre`, `code` | `Collapsible.Draw` with caller-owned open state, and `Text.Draw` with a code typeface and explicit wrap or scroll policy. |
| Plotly line/step, bars, heatmaps, waterfalls and candles | Import `plot/Plot`. Use `XYChart` for named shared-axis line, point or step series, `BarChart` for signed categories, `HeatmapChart` for a row-major matrix, `WaterfallChart` for deltas and totals, and `TimeCandleChart` for OHLC. Give `ChartProps.bounds` the native panel content rectangle and pass typed data. |

## 4. Kryon CSS Support (`kryonlabs/css`)

Import `CSS :: #import "css/Css";` and install a stylesheet over `Theme.InstallDefaultTheme`:

```zi
CSS :: #import "css/Css";
Theme :: #import "kryon/theme";

sheet: CSS.CssSheet;

ApplyStyles :: (dark: bool, compact: bool, viewport_width: float32) -> bool {
    unused Theme.InstallDefaultTheme(dark)
    CSS.CssSheetReset(*sheet)
    if !CSS.CssSheetParse(*sheet, app_css, "app.css", CSS.CssNoImports) {
        return false // inspect CSS.CssSheetDiagnostic(*sheet, buf[:])
    }
    env := CSS.CssEnvironmentWithNames(
        CSS.CssDefaultEnvironment(),
        ifx dark then "dark" else "light",
        "normal",
        ifx compact then "compact" else "comfortable",
        "mouse",
        "desktop")
    return CSS.CssSheetInstall(*sheet, CSS.CssEnvironmentWithViewportWidth(env, viewport_width), null)
}
```

Bind selectors on widgets via `props.class_name = CSS.StyleClass("primary")` and `props.id = CSS.StyleId("save")`.

Parse once per stylesheet in a running app. The example reparses to show the complete setup.

### Supported Selectors

- **Widget types**: `*`, `Any`, `App`, `Button`, `Text`, `TextField`, `TextArea`, `Surface`, `Dropdown`, `Card`, `Slider`, `SliderThumb`, `Toggle`, `ToggleThumb`, `Scroll`, `ScrollThumb`, `Checkbox`, `Radio`, `Progress`, `Separator`, `NavigationBar`, `NavigationBarItem`, `Selectable`, `Fieldset`, `Link`, `TabBar`, `Tab`, `TabClose`, `SegmentedControl`, `Segment`, `Menu`, `MenuItem`, `MenuSeparator`, `ListBox`, `ListBoxItem`, `ListBoxMulti`, `ListBoxMultiItem`, `TreeView`, `TreeViewItem`, `TableView`, `Collapsible`, `Modal`, `Toast`, `Toolbar`, `TitleBar`, `Image`, `Heading`, `ParagraphText`, `Page`, `Section`, `Popup`, `Row`, `Column`, `Grid`.
- **Classes & IDs**: at most one `.class` and one `#id` per compound selector (e.g. `Button.primary#save:hover`).
- **Kryon attributes**: `[tone=Neutral|Accent|Danger|Success|Warning]`, `[emphasis=Filled|Soft|Outline|Ghost|Link]`, `[size=Small|Medium|Large]`, `[role=Label|Panel|Track|Fill|Header|Close|...]`, `[state=any|normal|hover|pressed|focus|disabled|loading|selected]`.
- **State pseudo-classes** (max 1 per selector): `:hover`, `:active`, `:focus`, `:focus-visible`, `:disabled`, `:selected`, `:checked`, `:loading`.
- **Not supported** (parse error): descendant/child/sibling combinators (` `, `>`, `+`, `~`), pseudo-elements (`::before`), `!important`, `//` comments, selector functions (`:not()`).

### Supported Properties & Values

- **Native StyleData properties**:
  - Colors: `color`, `background` (color or two-stop vertical `linear-gradient([to bottom | 180deg,] top, bottom)`), `background-color`, `border-color`, `outline-color` (focus ring).
  - Borders & shape: `border` (`width`, `solid` or `none`, `color`), `border-width`, `border-style` (`solid` | `none`), `border-radius`, `opacity` (unitless number or clamped `0%`–`100%`).
  - Spacing & sizing: `padding` (1–4 values; top/bottom and left/right must be equal), `padding-inline`, `padding-block`, `gap` (one value for both axes or two values in `row column` order), `row-gap`, `column-gap` (`Row` reads `column-gap`, `Column` reads `row-gap`, uniform-gap controls read the first `gap` value; accepts `normal` = 0 or nonnegative `px`/`rem`), `min-height`, `min-block-size` (logical alias for `min-height`), `translate` (`x [y]`).
  - Retained `Row`, `Column` and `Grid` also accept `width` and `height` as positive `px` or `rem` lengths or `auto`. Other size expressions pass through to the browser.
  - Box: `Row`, `Column` and `Grid` read `min-width`/`min-inline-size`, `max-width`/`max-inline-size` and `max-height`/`max-block-size` (`px`, `rem`, `0`, or `none` for maxima), `margin` (1–4 values), `margin-top|right|bottom|left`, `margin-inline`, `margin-block`, `margin-inline-start|end` (`px`, `rem`, `0` or `auto`; `auto` inline margins share the free width, so `margin: 0 auto` centers), and `overflow` (`visible`, or `hidden`/`clip` to clip descendants). A container placed at explicit `x`/`y` ignores margins. Negative margins, percentages and `overflow: auto|scroll` stay in the browser; use `Scroll.Draw` for native scrolling.
  - Text: `text-align` (`start`/`left`, `center`, `end`/`right`) on `Text`, `Heading`, `ParagraphText` and `Link`; an explicit `Text.Props.align` wins.
  - Use `border-color: var(--line)` and `border-width` for typed border variables.
  - Typography: `font-size`, `font-family`; native `Text`, `Heading`, and `ParagraphText` read `font-weight` (`normal` = 400, `bold` = 700, or `1`–`1000`), `font-style` (`normal` | `italic` | `oblique` without angle), and `letter-spacing` (`normal` or signed `px` / `rem`); `Text` and `Button` accept `line-height` (`normal`, nonnegative unitless factor, `%` of font size, or nonnegative `px` / `rem`).
  - Kryon extensions: `-kryon-material` (`lightfield` | `flat` | `glass`), `-kryon-icon-size`, `-kryon-font-role` (`inherit` | `control` | `body` | `heading` | `code`), `-kryon-elevation` (`none` | `rest` | `raised` | `floating`), `-kryon-motion-duration`, `-kryon-motion-multiplier`, `-kryon-motion-easing` (`inherit` | `standard` | `linear` | `instant`), `-kryon-motion-reduced` (`reduce` | `no-preference`).
- **Units & values**: hex (`#rgb`, `#rgba`, `#rrggbb`, `#rrggbbaa`), CSS named colors, `rgb()`, `rgba()`, `hsl()`, `hsla()`, `hwb(hue white% black% [/ alpha])`, hue angle units (`deg`, `rad`, `grad`, `turn`), `px`, `rem` (`16px`), `s`, `ms`, `:root` custom properties (`--token: ...;`) with `var(--token, fallback)`. `em` and `%` lengths are rejected (use `px` or `rem`).
- **At-rules**: `@import` (before rules), `@layer`, `@keyframes`, `@media` (`prefers-color-scheme`, `prefers-contrast`, `prefers-reduced-motion`, `pointer`, `width`, `min-width`, `max-width`, `-kryon-theme`, `-kryon-density`, `-kryon-pointer`, `-kryon-platform`).
- **Browser-only layout and paint** includes grid declarations outside the native track subset, `text-decoration`, `text-transform`, `white-space`, `cursor`, `box-shadow`, `z-index` and `visibility`. CSS `display` maps to an authored native flex or grid container. `wrap-reverse`, percentage or intrinsic flex bases, baseline alignment, and cross-line distribution remain browser-only.

Use `color-mix(in srgb, #2076c0 35%, transparent)` in a native color
declaration or custom property to derive a tint from two literal colors.
For native `@media (prefers-reduced-motion: reduce)` rules, set
`env.reduced_motion` from `Theme.CurrentTheme().motion.reduced_motion` and
reinstall the parsed sheet when that preference changes. DOM output uses
the browser's live preference.

### CSS and layout motion

Install motion styles with `css/motion`. Supported native styles animate
through `transition` and `@keyframes`, including duration, delays, easing,
direction, fill modes and repeat counts. For example, a native Button with
`class_name = CSS.StyleClass("save")` uses these rules.

```css
button.save {
  background-color: #365bd6;
  transition: background-color 180ms ease-out;
}
button.save:hover { background-color: #2443ac; }
```

| UI effect | Kryon API |
| --- | --- |
| Move keyed items when a row, column or grid reflows | `LayoutMotion.Children(session)` from `kryon/layout_motion` in the open layout scope |
| Grow an entering item or ease a size change | `LayoutMotion.Children` with `.enter` or `.resize` |
| Keep drawing an exiting item until it settles | `LayoutMotion.Presence` with caller-owned state |
| Animate a custom shape, number, angle or graph value | `Motion.TweenTo` or `Motion.Sample` from `kryon/motion` |
| Animate a changing line chart | `Plot.AnimatedLineChart` with reusable chart state from `plot/Plot` |

See the [retained motion API](MOTION.md#retained-style-and-layout-motion) for
supported native widget properties and the [scalar motion API](MOTION.md#timed-motion-and-keyframes)
for application-owned tracks. The DOM backend applies motion directly through
browser CSS.

---

A complete runnable example with real CSS lives in [HTML_EXAMPLES.md](HTML_EXAMPLES.md).

## 5. Focus, Keyboard & Accessibility Checklist

1. **Keyboard `Tab` / `Shift+Tab` focus traversal**: `TreeKeyboardRoute` moves focus across `Button`, `Checkbox`, `Toggle`, `Radio`, `Slider`, `Selectable`, `Link`, `Collapsible`, `Dropdown`, `TextField`, `TextArea` (and `Card` / `Text` when `.focus_id > 0`). Leave `TextField.Props.capture_tab = false` so `Tab` leaves inputs cleanly.
2. **Focus rings**: Widgets draw focus outlines when `TreeFocusVisibleAt` is true (after keyboard navigation). Customize ring color in CSS via `outline-color: var(--focus-ring);`.
3. **Text editing**: Prefer `TextField.Bind(session, *state, props)` (`session_input = true` default) which manages keyboard/clipboard/composition updates against caller-owned `TextField.State`. Use `TextField.Draw(session, TextField.StateProps(state, props))` + `TextField.StateApplyResult(*state, result)` only when feeding synthetic `props.input` (`session_input = false`).
4. **Modals & Escape**: `Modal.Draw` submits an `Overlay` with `TreeSetPopup`, blocking background input and automatically dismissing on `Escape` (`modal.dismissed == true`). Always pair `if modal.opened { ... End(session) }`.
5. **Test driver & one-shot previews**: Keep `.label` meaningful on interactive widgets so `kryon/test_driver_client` (`DriverPress`, `DriverWait`, `DriverType`, `DriverKey`) and `kryon preview` tree JSON can inspect every control.

---

## 6. Live Signature & Checked Example Discovery (`ziran api`)

Do not duplicate parameter lists by hand. From a project with pinned `kryon` dependencies (such as `examples/dashboard/`), inspect imports first to resolve the local package module alias (`"Layout"` -> `#import "kryon/layout"`, etc.), then query bounded symbol metadata, stable symbol IDs (`"id": "zi:..."`), and checked `.zi.examples` entries dynamically:

```sh
# 1. Inspect resolved imports (`name` -> `resolved_module`) from your entry file
ziran api --project --json --imports-only src/dashboard.zi

# 2. Look up a resolved module dynamically and query a symbol + checked example `entry`
mod=$(ziran api --project --json --imports-only ../../src/ui/layout/layout_api_example.zi | \
  python3 -c 'import json,sys; d=json.load(sys.stdin); print(next(i["resolved_module"] for m in d["modules"] for i in m["imports"] if i["name"]=="Layout"))')
ziran api --project --json --module "$mod" --symbol Draw ../../src/ui/layout/layout_api_example.zi

# 3. Execute the returned example `entry` directly (never hardcode package hashes)
ziran run --project --target=c --entry <entry_from_json> <sibling_example.zi>
```

Each discovered symbol exposes a stable `"id": "zi:..."` identifier you can pass back via `--symbol <id>` to select a single overload deterministically.

Curated `.zi.examples` sidecars and hostless sibling fixtures ship alongside representative public modules (`layout.zi`, `layout_behavior.zi`, `button_props.zi`, `card_behavior.zi`, `text_input.zi`, `modal_behavior.zi`). Full-widget `Draw` modules (`button.zi`, `card.zi`, `textfield.zi`) require native raster/glyph host symbols (`RasterRoundedRectangle`, `MeasureGlyphWidth`, etc.), so headless standalone API examples target the pure layout/props/behavior layers while end-to-end widget frames are exercised via `kryon preview` and `examples/dashboard/tests/dashboard_test.zi`.
