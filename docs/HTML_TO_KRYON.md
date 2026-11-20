# HTML & CSS to Kryon / Ziran Mapping

A concise, agent-readable reference for translating HTML/CSS interfaces into native Kryon `.zi` code and Kryon CSS (`kryonlabs/css`).

---

## 1. Mental Model: DOM to Retained Frame

| Web / HTML Concept | Kryon / Ziran Equivalent | Notes |
| --- | --- | --- |
| DOM tree mutated by callbacks | Immediate declaration per frame inside `Frame(session, viewport) -> s32` | Kryon retains node identities across frames by `(parent, key, kind)`. |
| Component / element state | Caller-owned structs or file-scope variables (`bool`, `s32`, `TextField.State`, `[N]u8`) | Widgets never own application state; read `Result` structs each frame. |
| `key` / `id` attribute | `.key = cast(u64)N`, `.id = CSS.StyleId("...")`, `.class_name = CSS.StyleClass("...")` | Every sibling widget in a frame needs a unique nonzero `key`. |
| Parent/child nesting | Container returns `.opened: bool`; submit children, then call `End(session)` | Applies to `Page.Draw`, `Page.Section`, `Layout.*`, `Scroll.Draw`, `Modal.Draw`, `Collapsible.Draw`. |
| `window.innerWidth` / `@media (min-width)` | Branch in `.zi` on `viewport.width` (or `IsDesktopWidth`) | Native CSS skips `width`, `display`, `flex*`, and viewport `@media` queries; `.zi` computes `Rectangle` bounds. |

---

## 2. Semantic HTML Element -> Kryon Widget Table

| HTML Element / Pattern | Kryon Package & Call | Key Props / Notes |
| --- | --- | --- |
| `<html>`, `<body>` | `Page :: #import "kryon/page";` -> `Page.Draw(session, Page.Props.{...})` or `Layout.Screen` | Call `End(session)` if `result.opened`. Also fill root background with `Box.Draw`. |
| `<main>`, `<section>` | `Page.Section(session, Page.SectionProps.{.bounds, .label, .key})` | Sets `SemanticSection`; close with `End(session)` when `opened`. |
| `<article>`, `.card` | `Card :: #import "kryon/card";` -> `Card.Draw(session, Card.Props.{.bounds, .variant})` | Variants: `.CardFilled`, `.CardElevated`, `.CardOutlined`. Draw children inside `result.content` (does not push scope). |
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
gap := Layout.Gap(.ThemeMetricSpace3)
card_size := TextField.Measure(TextField.Props.{.placeholder = "Search..."})
items: [2]Rectangle = .{card_size, card_size}
col := Layout.Draw(session, Layout.Props.{
    .key = 10,
    .bounds = Layout.LayoutContentBounds(viewport, .FlexColumn, items[:], gap, 16),
    .gap = gap, .gap_override = true,
    .padding = 16, .padding_override = true
})
if col.opened {
    // Children with unset width/x/y (or .bounds.width = Layout.ColumnWidth(session)) auto-stack cleanly
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
> `Layout.Props.gap` and `Layout.Props.padding` only override the theme/CSS values `0` or nonzero when `.gap_override = true` / `.padding_override = true` (useful when forcing zero gap/padding against a non-zero theme default). Use `Layout.ColumnWidth(session)` inside a container to size children to the current content width, and `Layout.LayoutContentBounds(bounds, direction, child_rects, gap, padding)` to size a container around its children.

### B. Flex & Grid Geometry Cursors (Zero-Allocation `Rectangle` Math)

- **CSS Flexbox (`justify-content`, `align-items`, `gap`, `padding`)** -> `BeginFlexCursor` + `FlexStep` (in `kryon/layout_behavior` / `kryon/layout`):
  - `FlexDirection`: `.FlexRow`, `.FlexColumn`
  - `JustifyContent`: `.JustifyStart`, `.JustifyCenter`, `.JustifyEnd`, `.JustifySpaceBetween`, `.JustifySpaceAround`, `.JustifySpaceEvenly`
  - `AlignItems`: `.AlignStart`, `.AlignCenter`, `.AlignEnd`, `.AlignStretch`
- **CSS Grid (`grid-template-columns: repeat(auto-fill, minmax(220px, 1fr))`)** -> `Grid :: #import "kryon/grid";` (`MeasureGrid`, `BeginGridCursor`, `GridStep`, `GridCellBounds`):
  - Supply `.columns` (fixed) or `.min_item_width` + `.max_columns` (responsive), `.gap`, `.padding`.
- **Responsive Column Stack (`@media` max-width container)** -> `using PageLayout :: #import "kryon/PageLayout";`:
  - `page := PageFor(viewport, 520.0)`
  - `row := NextRow(*page, 40.0)`
  - `left := RowPart(*page, row, 0, 2)` / `right := RowPart(*page, row, 1, 2)`
- **Split Pane** -> `PanedView.Draw(session, PanedView.Props.{.bounds, .split, .has_split = true, .vertical = true})` -> read `panes.first`, `panes.second`, `panes.split`.

---

## 4. Kryon CSS Support (`kryonlabs/css`)

Import `CSS :: #import "css/Css";` and install a stylesheet over `Theme.InstallDefaultTheme`:

```zi
CSS :: #import "css/Css";
Theme :: #import "kryon/theme";

sheet: CSS.CssSheet;

ApplyStyles :: (dark: bool, compact: bool) -> bool {
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
    return CSS.CssSheetInstall(*sheet, env, null)
}
```

Bind selectors on widgets via `props.class_name = CSS.StyleClass("primary")` and `props.id = CSS.StyleId("save")`.

### Supported Selectors

- **Widget types**: `*`, `Any`, `App`, `Button`, `Text`, `TextField`, `TextArea`, `Surface`, `Dropdown`, `Card`, `Slider`, `SliderThumb`, `Toggle`, `ToggleThumb`, `Scroll`, `ScrollThumb`, `Checkbox`, `Radio`, `Progress`, `Separator`, `NavigationBar`, `NavigationBarItem`, `Selectable`, `Fieldset`, `Link`, `TabBar`, `Tab`, `TabClose`, `SegmentedControl`, `Segment`, `Menu`, `MenuItem`, `MenuSeparator`, `ListBox`, `ListBoxItem`, `ListBoxMulti`, `ListBoxMultiItem`, `TreeView`, `TreeViewItem`, `TableView`, `Collapsible`, `Modal`, `Toast`, `Toolbar`, `TitleBar`, `Image`, `Heading`, `ParagraphText`, `Page`, `Section`, `Popup`, `Row`, `Column`.
- **Classes & IDs**: at most one `.class` and one `#id` per compound selector (e.g. `Button.primary#save:hover`).
- **Kryon attributes**: `[tone=Neutral|Accent|Danger|Success|Warning]`, `[emphasis=Filled|Soft|Outline|Ghost|Link]`, `[size=Small|Medium|Large]`, `[role=Label|Panel|Track|Fill|Header|Close|...]`, `[state=any|normal|hover|pressed|focus|disabled|loading|selected]`.
- **State pseudo-classes** (max 1 per selector): `:hover`, `:active`, `:focus`, `:focus-visible`, `:disabled`, `:selected`, `:checked`, `:loading`.
- **Not supported** (parse error): descendant/child/sibling combinators (` `, `>`, `+`, `~`), pseudo-elements (`::before`), `!important`, `//` comments, selector functions (`:not()`).

### Supported Properties & Values

- **Native StyleData properties**:
  - Colors: `color`, `background` (color or two-stop vertical `linear-gradient([to bottom | 180deg,] top, bottom)`), `background-color`, `border-color`, `outline-color` (focus ring).
  - Borders & shape: `border` (`width`, `solid` or `none`, `color`), `border-width`, `border-style` (`solid` | `none`), `border-radius`, `opacity` (unitless number or clamped `0%`–`100%`).
  - Spacing & sizing: `padding` (1–4 values; top/bottom and left/right must be equal), `padding-inline`, `padding-block`, `gap` (one value for both axes or two values in `row column` order), `row-gap`, `column-gap` (`Row` reads `column-gap`, `Column` reads `row-gap`, uniform-gap controls read the first `gap` value; accepts `normal` = 0 or nonnegative `px`/`rem`), `min-height`, `min-block-size` (logical alias for `min-height`), `translate` (`x [y]`).
  - Typography: `font-size`, `font-family`; native `Text`, `Heading`, and `ParagraphText` read `font-weight` (`normal` = 400, `bold` = 700, or `1`–`1000`), `font-style` (`normal` | `italic` | `oblique` without angle), and `letter-spacing` (`normal` or signed `px` / `rem`); `Text` and `Button` accept `line-height` (`normal`, nonnegative unitless factor, `%` of font size, or nonnegative `px` / `rem`).
  - Kryon extensions: `-kryon-material` (`lightfield` | `flat` | `glass`), `-kryon-icon-size`, `-kryon-font-role` (`inherit` | `control` | `body` | `heading` | `code`), `-kryon-elevation` (`none` | `rest` | `raised` | `floating`), `-kryon-motion-duration`, `-kryon-motion-multiplier`, `-kryon-motion-easing` (`inherit` | `standard` | `linear` | `instant`), `-kryon-motion-reduced` (`reduce` | `no-preference`).
- **Units & values**: hex (`#rgb`, `#rgba`, `#rrggbb`, `#rrggbbaa`), CSS named colors, `rgb()`, `rgba()`, `hsl()`, `hsla()`, `hwb(hue white% black% [/ alpha])`, hue angle units (`deg`, `rad`, `grad`, `turn`), `px`, `rem` (`16px`), `s`, `ms`, `:root` custom properties (`--token: ...;`) with `var(--token, fallback)`. `em` and `%` lengths are rejected (use `px` or `rem`).
- **At-rules**: `@import` (before rules), `@layer`, `@keyframes`, `@media` (`prefers-color-scheme`, `prefers-contrast`, `prefers-reduced-motion`, `pointer`, `min-width`, `max-width`, `-kryon-theme`, `-kryon-density`, `-kryon-pointer`, `-kryon-platform`).
- **Web-only pass-through properties** (accepted by parser for `kryon/Dom`, ignored by native widgets): `width`, `height`, `min-width`, `max-width`, `max-height`, `margin*`, `display`, `flex*`, `justify-content`, `align-items`, `align-self`, `grid-*`, `text-align`, `text-decoration`, `text-transform`, `white-space`, `cursor`, `box-shadow`, `transition`, `animation`, `overflow`, `z-index`, `visibility`.

---

## 5. Runnable Example: Searchable Card with Real CSS

This complete `.zi` program translates a common HTML pattern (`<section class="card">` + `<input type="search">` + `<button class="primary">` + status text) into native Kryon + `css/Css`:

```zi
App :: #import "kryon/app";
Button :: #import "kryon/button";
Card :: #import "kryon/card";
Text :: #import "kryon/text";
TextField :: #import "kryon/text_field";
Theme :: #import "kryon/theme";
CSS :: #import "css/Css";
#import "kryon/control_props"
#import "kryon/geometry"
#import "kryon/session"
#import "kryon/tree"

APP_CSS :: #string CSS
:root {
  --card-bg: #ffffff;
  --border: #cbd5e1;
  --accent: hwb(160deg 2% 41%);
  --focus: hwb(199deg 5% 8% / 95%);
}

@media (prefers-color-scheme: dark) {
  :root {
    --card-bg: #181d25;
    --border: #334155;
    --accent: #34d399;
    --focus: #38bdf8;
  }
}

Card.panel {
  background: var(--card-bg);
  border: 1px solid var(--border);
  border-radius: 12px;
  -kryon-elevation: rest;
}

TextField.query-box {
  border: 1px solid var(--border);
  border-radius: 8px;
  padding: 0px 12px;
  outline-color: var(--focus);
}

TextField.query-box:focus {
  border-color: var(--accent);
}

Button.primary {
  background: var(--accent);
  color: #ffffff;
  border-radius: 8px;
  min-block-size: 36px;
}

Button.primary:hover {
  opacity: 92%;
}
CSS;

sheet: CSS.CssSheet;
started: bool;
dark: bool;
query_bytes: [64]u8;
query: TextField.State;
submitted: bool;

main :: () -> s32 {
    return App.Run(App.Window.{.title = "Search Card", .width = 520, .height = 240}, Draw)
}

InstallTheme :: () {
    unused Theme.InstallDefaultTheme(dark)
    CSS.CssSheetReset(*sheet)
    if CSS.CssSheetParse(*sheet, APP_CSS, "app.css", CSS.CssNoImports) {
        env := CSS.CssEnvironmentWithNames(
            CSS.CssDefaultEnvironment(), ifx dark then "dark" else "light",
            "normal", "comfortable", "mouse", "desktop")
        unused CSS.CssSheetInstall(*sheet, env, null)
    }
}

Draw :: (frame: App.Frame) {
    session := Session.{.slot = frame.slot, .epoch = frame.epoch}
    viewport := TreeRootBounds(session)
    if !started {
        query.bytes = query_bytes[:]
        query.focused = true
        InstallTheme()
        started = true
    }

    card := Card.Draw(session, Card.Props.{
        .key = 1, .class_name = CSS.StyleClass("panel"),
        .bounds = Rectangle.{20.0, 20.0, viewport.width - 40.0, viewport.height - 40.0},
        .variant = .CardOutlined
    })
    c := card.content
    Text.Draw(session, Text.Props.{
        .key = 2, .bounds = Rectangle.{c.x, c.y, c.width, 24.0},
        .text = "Filter cluster nodes", .size = 18
    })
    edited := TextField.Bind(session, *query, TextField.Props.{
        .key = 3, .id = 3, .focus_id = 3, .class_name = CSS.StyleClass("query-box"),
        .bounds = Rectangle.{c.x, c.y + 34.0, c.width - 110.0, 38.0},
        .label = "Filter", .placeholder = "Type host or region..."
    })

    if Button.Draw(session, Button.Props.{
        .key = 4, .id = 4, .class_name = CSS.StyleClass("primary"),
        .bounds = Rectangle.{c.x + c.width - 98.0, c.y + 34.0, 98.0, 38.0},
        .label = "Apply", .tone = .ButtonToneAccent, .emphasis = .ButtonEmphasisFilled
    }) != 0 || edited.committed {
        submitted = true
    }

    if Button.Draw(session, Button.Props.{
        .key = 5, .id = 5, .bounds = Rectangle.{c.x, c.y + 84.0, 120.0, 32.0},
        .label = ifx dark then "Light mode" else "Dark mode",
        .emphasis = .ButtonEmphasisOutline
    }) != 0 {
        dark = !dark
        InstallTheme()
    }

    status := ifx submitted && query.length > 0 then TextView(query_bytes[0:query.length]) else "All nodes active"
    Text.Draw(session, Text.Props.{
        .key = 6, .bounds = Rectangle.{c.x, c.y + 128.0, c.width, 22.0},
        .text = status, .size = 14
    })
}
```

For a complete multi-view example with sidebar navigation, KPI cards, scrollable results, loading/empty/error states, responsive wide/narrow breakpoints, and a keyboard-operable settings modal, see [`examples/dashboard/`](../examples/dashboard/README.md).

---

## 6. Focus, Keyboard & Accessibility Checklist

1. **Keyboard `Tab` / `Shift+Tab` focus traversal**: `TreeKeyboardRoute` moves focus across `Button`, `Checkbox`, `Toggle`, `Radio`, `Slider`, `Selectable`, `Link`, `Collapsible`, `Dropdown`, `TextField`, `TextArea` (and `Card` / `Text` when `.focus_id > 0`). Leave `TextField.Props.capture_tab = false` so `Tab` leaves inputs cleanly.
2. **Focus rings**: Widgets draw focus outlines when `TreeFocusVisibleAt` is true (after keyboard navigation). Customize ring color in CSS via `outline-color: var(--focus-ring);`.
3. **Text editing**: Prefer `TextField.Bind(session, *state, props)` (`session_input = true` default) which manages keyboard/clipboard/composition updates against caller-owned `TextField.State`. Use `TextField.Draw(session, TextField.StateProps(state, props))` + `TextField.StateApplyResult(*state, result)` only when feeding synthetic `props.input` (`session_input = false`).
4. **Modals & Escape**: `Modal.Draw` submits an `Overlay` with `TreeSetPopup`, blocking background input and automatically dismissing on `Escape` (`modal.dismissed == true`). Always pair `if modal.opened { ... End(session) }`.
5. **Test driver & one-shot previews**: Keep `.label` meaningful on interactive widgets so `kryon/test_driver_client` (`DriverPress`, `DriverWait`, `DriverType`, `DriverKey`) and `kryon preview` tree JSON can inspect every control.

---

## 7. Live Signature & Checked Example Discovery (`ziran api`)

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
