# Kryon Style Sheet Plan

Status: recommended architecture\
Scope: detach visual styling from Kryon widgets and make style sheets the single app-facing styling system\
Rule: widgets own structure and behavior; style sheets own every visual value; no implicit product styling

Kryon should become a powerful CSS alternative, not a smaller copy of browser
CSS. The goal is a semantic UI runtime where widgets expose facts, state, and
layout contracts, while a typed style program decides how those facts look.
The runtime may ship excellent style packs, but app UI receives no decorative
styling unless a pack is explicitly attached.

This document replaces the older menu of competing proposals with one target:
a deterministic style sheet system for Kryon. KSS is the file syntax name;
the canonical runtime API uses clean domain names such as `StyleSheet`,
`StylePack`, `StyleRule`, `StyleToken`, and `ResolveStyle`.

The split is deliberate:

- `.kry` remains the language for UI structure, widget facts, state, layout,
  and reusable runtime logic;
- `.kss` becomes the language for visual styling, tokens, selectors, layers,
  themes, and style packs;
- the shared style data model and resolver are implemented in `.kry` so every
  backend consumes the same compiled rule tables;
- only the text parser, diagnostics, formatter, and import loader need
  `kss_` names internally.

That gives Kryon a clean canonical API: apps talk about styles, packs, rules,
tokens, selectors, and resolved frames. They do not talk about parser prefixes.

## 1. North star

Kryon UI should feel like this:

```kry
#style "app.kss"

App {
    Column account {
        Text title {
            text = "Account"
            role = Title
        }

        TextField email {
            value = account_email
            placeholder = "Email"
            class = "field"
        }

        Row actions {
            Button cancel {
                label = "Cancel"
                class = "quiet"
            }

            Button save {
                label = "Save"
                tone = Accent
                class = "primary"
            }
        }
    }
}
```

The `.kry` file says what the UI is. It does not say what color the button is,
how round the field is, what font size the title uses, or whether the surface
has glass, shadow, rim light, or a flat fill. Those decisions live in the
style sheet:

```text
@pack app;
@import <kryon.reset>;
@layer tokens, base, components, screens;

tokens {
    color {
        text: #16181d;
        muted: #6f7480;
        canvas: #fafafa;
        surface: #ffffff;
        accent: #2f6bff;
        danger: #bd2430;
    }

    length space {
        2: 8;
        3: 12;
        4: 16;
    }

    radius {
        sm: 4;
        md: 6;
    }

    duration {
        fast: 80ms;
        normal: 140ms;
    }

    font {
        ui: "Noto Sans";
    }
}

@theme dark {
    text: #f4f6fb;
    muted: #aeb4c2;
    canvas: #101116;
    surface: #191b22;
    accent: #7aa2ff;
}

@layer base {
    App {
        background: canvas;
        foreground: text;
        font_family: ui;
    }

    Text {
        foreground: text;
        font_size: 16;
        line_height: 1.35;
    }

    Text[role=Title] {
        font_size: 28;
        typeface: semibold;
    }

    Text[role=Caption] {
        foreground: muted;
        font_size: 13;
    }
}

@layer components {
    Button {
        foreground: text;
        background: transparent;
        border: color-mix(text, transparent, 82%);
        border_width: 1;
        radius: radius.md;
        padding_x: space.3;
        padding_y: space.2;
        gap: space.2;
        material: flat;
        transition: background normal ease-out, opacity fast ease-out;
    }

    Button:hover {
        background: color-mix(text, transparent, 94%);
    }

    Button:pressed {
        opacity: 0.86;
        pressed_offset: 1;
    }

    Button[tone=Accent],
    Button.primary {
        background: accent;
        foreground: white;
        border: transparent;
    }

    Button.quiet {
        background: transparent;
        border: transparent;
        foreground: muted;
    }

    TextField.field {
        background: surface;
        foreground: text;
        border: color-mix(text, transparent, 84%);
        focus: accent;
        radius: radius.md;
        padding_x: space.3;
        padding_y: space.2;
    }
}
```

That is the shape: structure in `.kry`, styling in `.kss`, one typed resolver
between them, and no visual fallback hidden inside widgets.

## 2. Goals

- Remove every raw visual value from app-facing widget props.
- Remove visual defaults from widget implementations.
- Make style sheets the only app-facing visual styling system.
- Convert the current vanilla/default Kryon styling, including the glow and
  Lightfield treatments, into ordinary shipped style packs.
- Let apps import several style packs and switch between them at runtime with
  a standard style picker/dropdown.
- Keep `.kry` structural and readable.
- Preserve immediate-mode ergonomics: widgets remain simple calls with stable
  names, classes, semantic attributes, and state.
- Compile `.kss` files into style-sheet data for release builds. Do not parse
  text in shipped hot paths.
- Hot-reload `.kss` files in `kryon-preview` and development tools.
- Resolve styles identically in C, C++, Go, JS, KRB, DOM, canvas, libdraw,
  raylib-style backends, and termi.
- Make all style values inspectable: matched rules, winning declaration,
  token origin, resolved value, state slice, and backend degradation.
- Support zero style mode for testing, debugging, and true separation.
- Keep the language small enough that every rule is deterministic and every
  property is tested.

## 3. Non-goals

- Do not implement browser CSS.
- Do not make layout a stylesheet language in this phase.
- Do not add arbitrary units, arbitrary selectors, `!important`, browser-like
  inheritance, pseudo-elements, sibling selectors, or backend-specific
  properties.
- Do not style 2D scene content such as `Sprite` and `Light2D`; their colors
  are content.
- Do not remove raw colors from low-level drawing primitives used by backends
  and renderer tests.
- Do not keep a legacy theme compatibility layer. Existing theme values are
  migrated into style packs and overlays, then the old theme file/import/export
  surface is removed.

## 4. Core split

Kryon has three layers. Values only move downward.

| Layer | Owns | Must not own |
|---|---|---|
| Structure | widget kind, name, classes, content, behavior, input, state, slots, layout participation | colors, radius, border, padding, typography, material, shadows |
| Style | selectors, tokens, variants, state appearance, metrics, typography, material, density, platform adaptation | input handling, widget state, layout tree mutation |
| Renderer | rasterization of resolved drawing commands | theme lookup, selector matching, widget policy |

The API contract is: a widget can say "I am a `Button`, named `save`, class
`primary`, tone `Accent`, currently hovered." It cannot decide the fill,
radius, font, shadow, or transition for that state.

## 5. Default attachment and zero default styling

Kryon should have no implicit product style for app UI.

New Kryon app templates should start with an explicit style attachment:

```kry
#style <kryon.material> as material
```

That keeps source-level style attachment explicit for generated app templates.
At runtime, `InitInterface()` also calls `EnsureBuiltInStylePacks()`: if the
registry is empty, Kryon loads the shipped catalog and selects
`<kryon.material>`. If a developer clears style packs or runs with an explicit
unstyled mode after startup, the app is unstyled.

No attached style pack means:

- widgets still exist;
- layout and measurement still run;
- input and focus still work;
- accessibility metadata still exists;
- test overlays can reveal hit regions and focus;
- text can use a minimal system ink fallback only so diagnostics are readable;
- controls do not acquire product chrome, rounded boxes, shadows, material,
  theme colors, or button-specific decoration.

Kryon can ship optional packs:

| Pack | Purpose |
|---|---|
| `<kryon.reset>` | minimum readable/debug affordances and normalized inherited tokens |
| `<kryon.material>` | default attached app pack: clean Material-like controls, restrained surfaces, flat/cheap paint |
| `<kryon.tk>` | toolkit-native pack for dense desktop utilities and easy picker previews |
| `<kryon.vanilla>` | the current default Kryon styling expressed as a style pack |
| `<kryon.glow>` | the current modern glow treatment expressed as a style pack |
| `<kryon.classic>` | preserved original Kryon look as an explicit pack |
| `<kryon.lightfield>` | premium Lightfield/Button/Dropdown visual language, opt-in because it is more performance intensive |
| `<kryon.high-contrast>` | accessibility-oriented overlay or full pack |
| `<kryon.terminal>` | termi-focused mapping for cell backends |

Project templates and app scaffolds still attach `<kryon.material>` explicitly
so the source describes the app's intended baseline. Runtime startup mirrors
that choice for hand-written hosts: `EnsureBuiltInStylePacks()` loads the
embedded `.kss` pack sources only when needed and preserves any active app
selection. If nothing is active, Material becomes active.

For host code that wants to force the shipped catalog back to its baseline,
`RegisterBuiltInStylePacks()` loads Material, TK, Vanilla, Glow, and Lightfield
as ordinary `StylePack` values and selects Material. Apps can immediately
switch to TK, Vanilla, Glow, Lightfield, or a product pack through
`StylePicker`; the picker lazily ensures the built-in catalog when the registry
is empty. Lightfield must never be the automatic default: it is beautiful, but
its translucent layered treatment is a premium opt-in rendering path, not the
baseline cost every app should pay.

App screens should use `AppBackground()` for their canvas instead of
`Background(GetThemeBackground())`. `AppBackground()` resolves the active
style pack's `App` rule, which keeps page chrome in KSS while preserving
`Background(Color)` for intentionally explicit primitive drawing.

The first shipped style-pack conversion should be a clean Material pack:
neutral dark/light surfaces, crisp borders, modest radius, clear typography,
lavender/accent highlights, and flat paint by default. The Android reference
for the default direction is a dark Material customization screen: charcoal
canvas, slightly raised panels, 6-10 px radii, thin dividers, clear section
labels, lavender active tabs/sliders/toggles, and no glow, blur, or Lightfield
volume. It should feel native, calm, and cheap to draw. This is the pack new
Kryon apps attach on startup.

TK should be equally real, not a placeholder: compact spacing, square-ish
controls, light desktop colors, hard borders, and dense utility ergonomics so
an app can preview and choose it immediately.

Vanilla and Glow preserve Kryon's current visual personality as explicit
stylesheets. Lightfield remains the premium pack for translucent depth and
many-layer controls, opt-in because it costs more to render. None of these
looks may live as hidden widget defaults.

Current implementation note: Card, Dropdown, Text, Slider, Toggle, Checkbox,
Radio, Progress, Separator, NavigationBar, Selectable, Fieldset, Plot, Link,
TabBar, and Button now enter rendering through semantic style facts and
attached KSS packs. Plot uses `Plot` for chart chrome/text and `PlotMark` for
data marks. Navigation uses `NavigationBar` and `NavigationBarItem`; tabs use
`TabBar`, `Tab`, and `TabClose`; segmented controls use `SegmentedControl` for
the host surface and `Segment` for each choice. Button no longer owns a hidden
palette/theme appearance path in `.kry`; its paint is resolved from KSS frames
plus explicit per-call style overrides while those overrides are being retired.

## 6. Existing leaks to remove

| Leak | Current location | Target |
|---|---|---|
| Inline override table | `ButtonProps.style: ControlStyle` | remove; use class/name/semantic selectors |
| Raw input styling | `TextInputStyle` in `include/ui_controls.h` and text-input props | remove; style rules cover input families |
| Raw text colors | `TextProps.color`, retained primitive colors | remove from app-facing text; style rules handle foreground and selection |
| Row, link, page colors | internal row helpers, `ui_page.h` | convert to semantic role/tone plus style rules |
| Alpha-zero defaults | widget paint fallbacks such as `background.a != 0` | remove; presence bits live in style declarations only |
| Paint-time theme reads | `GetTheme*` calls in `src/ui/*.c` paint paths | renderer consumes resolved frames only |
| Inline literals in app code | examples and maintained `.kry` apps | move into `.kss` or scoped `Style` |
| Widget-family palettes | dropdown/input/row local color choices | replace with style declarations over shared properties |

## 7. Style sheet file model

A `.kss` file is a style pack or style module.

```text
@pack product;
@version 1;
@import <kryon.reset>;
@import "tokens.kss";
@import "controls.kss";
@layer reset, tokens, base, components, screens, overrides;
```

Rules:

- `@pack` names the pack for diagnostics and tooling.
- `@version` selects the style sheet grammar/property version.
- `@import` loads built-in packs or project files.
- `@layer` declares a total layer order.
- `tokens { ... }` groups typed design values without repeating a directive
  per value.
- Imported layers merge by declared layer name.
- Unknown layers are errors unless declared by the importing pack.
- Cyclic imports are compile errors.
- Shipped builds embed the flattened token table and rule table.
- Preview parses files and rebuilds the same tables for hot reload.

### Multiple packs and runtime switching

Apps should be able to ship several looks without rebuilding UI code. The
import surface names each candidate pack, then selects one active pack through
app state or host preferences:

```kry
#style <kryon.material> as material
#style <kryon.tk> as tk
#style <kryon.vanilla> as vanilla
#style <kryon.glow> as glow
#style <kryon.lightfield> as lightfield
#style "brand.kss" as brand

App {
    SettingsPanel {
        StylePicker theme_style {
            value = active_style
            options = [material, tk, vanilla, glow, lightfield, brand]
        }
    }

    Column account {
        Text title { text = "Account"; role = Title }
        Button save { label = "Save"; tone = Accent }
    }
}
```

Equivalent C host API:

```c
typedef struct StylePackOption {
    const char *id;
    const char *label;
    const char *description;
} StylePackOption;

bool RegisterStylePack(const char *id, const StyleSheet *sheet);
bool SetActiveStylePack(const char *id);
const char *GetActiveStylePack(void);
int GetStylePackOptions(StylePackOption *options, int capacity);
bool StylePicker(StylePickerProps props);
```

`StylePicker` is visually styled by the currently active pack, exactly like a
dropdown. If no product pack is active, it uses the minimal reset/debug
affordance so users can still choose a style. Apps may expose this in Settings,
developer menus, onboarding, or a command palette.

Switching packs:

- changes the active token/rule table;
- invalidates style fingerprints;
- preserves widget state, focus, scroll, text input, and layout identity;
- recomputes measurements when typography or interior metrics change;
- repaints without recompiling or rebuilding the UI tree;
- can persist through the existing settings/storage layer.

Pack choice is independent from light/dark theme choice. A user can choose
`glow` plus dark, `vanilla` plus light, or a brand pack plus high contrast.
The active visual state is:

```text
style pack + theme overlay + environment overlay + scoped rules
```

This is important culturally for Kryon: today's default look should survive as
`<kryon.vanilla>` or `<kryon.glow>`, but as one selectable style among many,
not as an invisible assumption baked into every widget.

### Naming rule

KSS names the text file format only. Public and `.kry` runtime concepts use
plain style names:

| Concept | Public name |
|---|---|
| parsed stylesheet data | `StyleSheet` |
| selectable visual package | `StylePack` |
| selector plus declarations | `StyleRule` |
| typed design value | `StyleToken` |
| selector matcher | `StyleSelector` |
| active resolver | `ResolveStyle` |
| runtime chooser | `StylePicker` |

Use `kss_` only for internal parser/tooling code that specifically handles
the `.kss` text syntax, such as `kss_parse_file`. Do not expose `KssRuleTable`,
`KssStylePack`, or similar names in app-facing APIs.

### Implementation boundary

The style system has two layers with different naming rules.

The canonical runtime layer is written in `.kry` and exports plain names:

```kry
StyleToken
StyleSelector
StyleRule
StyleSheet
StylePack
StyleFacts
StyleCascade
ResolveStyle
StylePicker
```

This layer is what widgets, backends, tests, generated C, generated Go,
generated JS, KRB, and inspectors use. It must not be named after the file
format, because it is the durable styling model, not merely a parser product.

The parser/tooling layer may use `kss_` because it is specifically about
reading and writing `.kss` text:

```c
kss_parse_file
kss_format_document
kss_emit_diagnostics
kss_load_import_graph
```

Those names should stop at the compiler/tooling boundary. Once parsing
succeeds, the result is a `StyleSheet` or `StylePack`.

The practical architecture is:

```text
.kss text
    |
    v
internal kss_parse_* tooling
    |
    v
StyleSheet / StylePack typed data
    |
    v
.kry ResolveStyle runtime
    |
    v
backend resolved frame
```

This makes `.kss` optional as a source format. A tool, generated file, embedded
KRB cartridge, or host application can build `StyleSheet` data directly and
still use the same resolver.

## 8. KSS grammar

KSS is CSS-shaped but typed and finite.

```ebnf
sheet         = { directive | tokens_block | theme_block | env_block | rule } ;
directive     = pack_decl | version_decl | import_decl | layer_decl ;
pack_decl     = "@pack" identifier ";" ;
version_decl  = "@version" integer ";" ;
import_decl   = "@import" ( builtin | string ) ";" ;
layer_decl    = "@layer" identifier { "," identifier } ";" ;
tokens_block  = "tokens" "{" { token_group } "}" ;
token_group   = token_type [ token_namespace ] "{" { token_entry } "}" ;
token_entry   = token_leaf ":" value ";" ;
theme_block   = "@theme" identifier "{" { token_name ":" value ";" } "}" ;
env_block     = "@env" env_query "{" { rule | token_override } "}" ;
rule          = [ "@layer" identifier ] selector_list "{" { declaration } "}" ;
selector_list = selector { "," selector } ;
selector      = compound { combinator compound } [ state ] ;
combinator    = ">" | " " ;
compound      = [ kind ] { "." class | "#" name | attr_match } ;
attr_match    = "[" attr "=" attr_value "]" ;
state         = ":" state_name ;
declaration   = property ":" value ";" ;
```

Allowed selector atoms:

| Atom | Example | Meaning |
|---|---|---|
| kind | `Button` | widget family |
| class | `.primary` | caller-provided class |
| name | `#save` | caller-provided node name |
| semantic attr | `[tone=Accent]` | typed widget semantic value |
| state | `:hover` | widget-owned state slice |
| child scope | `Toolbar > Button` | direct structural scope |
| descendant scope | `Modal Button` | descendant structural scope |

Allowed semantic attributes are closed per widget family. Initial shared set:

- `role`
- `tone`
- `emphasis`
- `size`
- `state`
- `validation`
- `orientation`
- `placement`

Allowed states:

- `normal`
- `hover`
- `pressed`
- `focused`
- `selected`
- `disabled`
- `loading`
- `checked`
- `invalid`
- `expanded`
- `open`

State names are not input logic. They are style slots fed by widget-owned
facts.

## 9. Deliberate differences from CSS

KSS should feel familiar without inheriting CSS's hardest problems.

| CSS feature | KSS decision |
|---|---|
| Arbitrary attributes | no; only typed semantic attributes |
| `!important` | no; layer and source order are enough |
| Sibling selectors | no |
| Pseudo-elements | no in v1 |
| Browser layout properties | no in v1 |
| Implicit inheritance | only explicit inherited token categories |
| Runtime text parsing in shipped apps | no; compiled rule tables |
| Backend vendor prefixes | no |
| Global UA stylesheet | no implicit product style |
| Open units | no; typed values only |
| Dynamic DOM mutation semantics | no; immediate-mode facts and retained fingerprints |

The result should be less magical than CSS and easier to test.

## 10. Specificity and cascade

KSS must always resolve the same way in every backend.

Order:

1. Earlier imports load first.
2. Declared layers establish a total order.
3. Unlayered rules come after declared layers.
4. More specific selectors beat less specific selectors inside the same layer.
5. Later source order wins inside the same layer and specificity.
6. Scoped inline `Style` nodes win over file-level rules in their subtree.

Specificity:

| Selector part | Weight |
|---|---:|
| kind | 1 |
| semantic attribute | 10 |
| class | 20 |
| kind plus class/attribute | additive |
| name | 100 |
| inline scoped style | 1000 |

State rules do not add selector specificity. They write into state-specific
style slots. For example:

```text
Button.primary { background: accent; }
Button.primary:hover { background: accent.hover; }
```

Both rules match the same widgets. The first writes `normal`, the second
writes `hover`.

Interaction precedence remains Kryon's:

```text
disabled > loading > pressed > hovered > focused > selected > normal
```

This precedence chooses which state slice is active. It does not change rule
matching order.

## 11. Tokens

Tokens are the power feature. They are typed, validated, inspectable, and
portable across backends.

Canonical KSS does not use one directive per token. Repeating a directive for
every token makes theme diffs noisy and leaks parser-ish vocabulary into the
design language. The canonical form is grouped by type:

```text
tokens {
    color {
        accent: #2f6bff;
        accent.hover: color-mix(accent, white, 12%);
    }

    length control {
        pad.x: 12;
    }

    radius control {
        radius: 6;
    }

    duration transition {
        fast: 80ms;
    }

    font {
        ui: "Noto Sans";
    }
}
```

Token groups remove declaration noise. `color { accent: ... }` creates
`accent`; `length space { 3: 12; }` creates `space.3`; `duration transition
{ fast: 80ms; }` creates `transition.fast`.

Token types:

| Type | Examples |
|---|---|
| `color` | `#rrggbb`, `#rrggbbaa`, named colors, `color-mix(...)` |
| `length` | logical pixels |
| `radius` | logical pixels or radius tuple in later versions |
| `number` | opacity, line-height multiplier |
| `duration` | milliseconds |
| `easing` | `linear`, `ease-out`, named cubic curves |
| `font` | registered font family name |
| `material` | `flat`, `lightfield`, `glass` |
| `shadow` | named shadow recipe |

Rules use token names directly:

```text
Button {
    background: accent;
    padding_x: space.3;
    transition: background transition.fast ease-out;
}
```

The expected property type disambiguates tokens from enums and literals.
`background: accent` looks up a color token. `material: flat` reads as a
material enum because `material` expects a material. If a bare identifier could
refer to both a token and an enum for the same property type, KSS reports an
ambiguity and requires the explicit form `token(accent)`.

Raw visual literals are legal only in `.kss` or inline `Style` nodes, never in
ordinary widget calls.

Token validation:

- missing token is a compile error;
- cyclic token reference is a compile error;
- wrong token type for a property is a compile error;
- unresolved font family is a runtime diagnostic with fallback;
- preview reports diagnostics with file, line, column, token name, and
  expected type.

## 12. Themes and environment overlays

Themes are token overlays inside the style sheet system, not a separate
styling language and not an import/export compatibility surface.

```text
@theme light {
    text: #16181d;
    canvas: #fafafa;
    surface: #ffffff;
}

@theme dark {
    text: #f4f6fb;
    canvas: #101116;
    surface: #191b22;
}
```

Environment overlays are closed and typed:

```text
@env contrast(high) {
    Button { border_width: 2; focus: accent; }
}

@env density(compact) {
    Button { padding_x: 10; padding_y: 6; }
    TextField { padding_y: 6; }
}

@env platform(android) {
    Button { pointer_target_min: 48; }
}
```

Allowed axes:

- `theme(light|dark)`
- `contrast(normal|high)`
- `density(compact|comfortable|touch)`
- `pointer(mouse|touch|mixed)`
- `platform(desktop|android|web|plan9|terminal)`

Existing `themes/*.ini` files should be treated as migration input only. Their
useful values move into shipped style packs and `@theme` overlays. After that,
the legacy theme file format, theme import/export commands, and widget-facing
theme-style modes are deleted. Internally and publicly, the active visual state
is the resolved `StyleSheet` token and rule table.

## 13. Properties

The property vocabulary is closed and versioned. Adding a property requires
runtime, generated-output, parity, tests, and docs updates in the same change.

### Paint

| Property | Type | Notes |
|---|---|---|
| `background` | color | primary fill |
| `background_end` | color | optional gradient endpoint |
| `foreground` | color | text, icon, and content ink |
| `border` | color | border stroke |
| `focus` | color | focus track input |
| `shadow` | shadow | named/canonical shadow recipe |
| `selection` | color | text/list selection |
| `cursor` | color | text cursor, where relevant |

### Shape

| Property | Type | Notes |
|---|---|---|
| `radius` | radius | rounded shape |
| `border_width` | length | physical stroke width after scale |
| `corner_style` | enum | `round`, `bevel`, `square` |

### Interior metrics

| Property | Type | Notes |
|---|---|---|
| `padding_x` | length | horizontal interior padding |
| `padding_y` | length | vertical interior padding |
| `gap` | length | icon/text/content gap |
| `icon_size` | length | icon box |
| `content_offset` | vec2 | visual offset, not hit bounds |
| `pointer_target_min` | length | minimum input target, may affect measurement |

### Typography

| Property | Type | Notes |
|---|---|---|
| `font_family` | font | registered font family |
| `typeface` | string/enum | regular, semibold, mono, etc. |
| `font_size` | length | logical font size |
| `line_height` | number/length | multiplier or logical length |
| `letter_spacing` | length | defaults to zero |
| `text_wrap` | enum | `none`, `word`, `line`, `clip` |
| `text_align` | enum | `start`, `center`, `end` |

### Material

| Property | Type | Notes |
|---|---|---|
| `material` | material | `flat`, `lightfield`, `glass` |
| `elevation` | number | material intensity |
| `rim` | number/color | material edge treatment |
| `blur` | length | glass blur where supported |
| `surface_noise` | number | subtle texture recipe |

### Motion

| Property | Type | Notes |
|---|---|---|
| `transition` | transition list | property duration easing |
| `duration` | duration | family default when transition omits duration |
| `easing` | easing | family default easing |
| `pressed_offset` | length | visual press displacement |

Properties that affect natural size, such as padding and font size, must feed
measurement through the same resolved style frame used for painting. Paint-only
properties must not move hit bounds.

## 14. Widget props after separation

Public widget props should fit this shape:

```c
typedef struct ButtonProps {
    Rectangle bounds;
    const char *name;
    const char *class_name;
    const char *label;
    IconType icon;
    ButtonTone tone;
    ButtonEmphasis emphasis;
    ControlSize size;
    ButtonState state;
    bool disabled;
    bool loading;
    bool selected;
} ButtonProps;
```

Allowed props:

- identity: `name`, `class_name`, stable keys;
- content: labels, values, placeholders, images, icons, children;
- behavior: disabled, loading, selected, validation state, callbacks;
- semantics: role, tone, emphasis, size, placement;
- layout contract: bounds, scroll offsets, content size, slots.

Forbidden props:

- raw colors;
- `Style`, `ControlStyle`, or family-specific visual structs;
- radius, padding, border width, opacity;
- font size, typeface, text color;
- material, rim, glow, blur, and shadow choices;
- alpha-zero-means-default behavior.

For `Text`, role and content are structure. Foreground, font size, wrapping,
selection paint, and typeface are style.

For `Image`, source and fit are content/layout. Tint, radius, material, and
filters are style, except in low-level drawing and scene nodes where color is
actual content.

## 15. Runtime model

The final pipeline is:

```text
.kry widget facts
  -> semantic normalization
  -> collect imported, app, and scoped rule tables
  -> resolve token overlays for environment
  -> cascade typed declarations
  -> select state slice
  -> transition resolved frame
  -> emit SurfaceDrawing / TextDrawing / ImageDrawing commands
  -> backend rasterization
```

There are no built-in visual defaults at the first step. The first visual
declarations come from an attached reset/material/product style pack. Existing
`Default<Button>Style`-style functions become semantic normalization and
fallback diagnostic helpers; they no longer manufacture product appearance by
themselves.

Compiled apps embed:

- interned widget kind names;
- interned class/name strings;
- token table;
- theme overlay table;
- environment overlay table;
- selector table;
- declaration table;
- state slices;
- source maps for inspector diagnostics.

Preview tools parse text, then produce the same tables.

## 16. Style frames

Resolution produces a style frame with presence bits:

```c
typedef struct ResolvedStyle {
    uint64_t fields;
    Color background;
    Color background_end;
    Color foreground;
    Color border;
    Color focus;
    float radius;
    float border_width;
    float opacity;
    float padding_x;
    float padding_y;
    float gap;
    float font_size;
    float icon_size;
    Vector2 content_offset;
    int material;
    int font_family;
    int typeface;
} ResolvedStyle;
```

The exact generated shape can differ, but the rules are fixed:

- absent means absent;
- transparent is a real value;
- zero is a real value when present;
- state slices merge with presence bits;
- transitions interpolate resolved values, not declarations;
- renderer code receives resolved values only.

## 17. Caching

Style resolution should be cheap in immediate-mode UI.

Cache key:

```text
style_fingerprint(
    style_pack_version,
    token_overlay_version,
    environment_axes,
    scope_fingerprint,
    widget_kind,
    widget_name,
    widget_class_list,
    semantic_attrs,
    state
)
```

Invalidation happens when:

- a `.kss` file changes;
- a theme/environment overlay changes;
- a scoped `Style` node changes;
- a widget's name/class/semantic attributes change;
- a widget enters a different style scope;
- a relevant state changes.

State transitions can reuse previous and target resolved frames. The resolver
does not need to rematch selectors every frame if the fingerprint is stable.

## 18. Inline scoped style

External `.kss` files are the primary authoring surface. Inline `Style` nodes exist for
screen-local overrides and lower to the same rule table:

```kry
Column danger_zone {
    Style {
        Button { tone: Danger; }
        Button.confirm { background: danger; foreground: white; }
        Text[role=Caption] { foreground: muted; }
    }

    Text warning {
        text = "This cannot be undone"
        role = Caption
    }

    Button confirm {
        label = "Delete"
        class = "confirm"
        tone = Danger
    }
}
```

Inline style wins over imported/app-level style sheets because it is scoped
closest to the structure. It is not the main authoring surface; it is for local
screens, examples, experiments, and one-off product areas.

## 19. Inspector

The inspector is part of the feature, not a luxury.

For any widget, it should show:

- widget facts: kind, name, classes, semantic attrs, current state;
- matching rules in cascade order;
- winning declaration for each property;
- token origin and resolved token value;
- active theme/environment overlay;
- state slice selected by interaction precedence;
- transition progress and target frame;
- backend degradation, such as termi ignoring radius/material;
- source file and line for every declaration.

This makes Kryon style sheets more debuggable than CSS in a browser because
Kryon owns the entire stack.

## 20. Backend behavior

All backends consume the same resolved style frame.

| Backend | Behavior |
|---|---|
| raylib/canvas/libdraw | rasterize full resolved drawing commands |
| DOM | maps resolved frames to DOM/CSS implementation details without exposing CSS as the source of truth |
| KRB | serializes rule tables or resolved frames according to cartridge needs |
| termi | maps foreground/background/focus/selection to cells and ignores unsupported geometry/material |
| null/test | records resolved frames for parity and assertions |

Unsupported visual properties degrade; they do not fork style resolution.

## 21. Migration roadmap

### M0 - Freeze new leaks

- Add a scanner that fails on new raw visual fields in public widget props.
- Add a scanner that fails on new paint-time theme getter usage in widget
  paint paths.
- Add a literal scanner for maintained `.kry` app UI.
- Keep a non-failing inventory for existing leaks until each family migrates.

### M1 - Style sheet compiler and rule table

- Implement the `.kss` parser with stable diagnostics.
- Add typed token tables, rule tables, layer ordering, selector specificity,
  and state slices.
- Lower `.kss` imports into generated C, C++, Go, JS, and KRB data.
- Make `kryon-preview` watch and hot-reload `.kss` files.
- Add source maps for inspector output.

### M2 - Ship explicit base packs

- Add `<kryon.reset>` for zero-opinion readability/debug affordances.
- Add `<kryon.material>` as the default template-attached app pack.
- Add `<kryon.tk>` as the dense toolkit-native picker option.
- Convert today's actual default/vanilla styling into `<kryon.vanilla>`.
- Convert today's glow treatment into `<kryon.glow>`.
- Move the current approved Lightfield/Button/Dropdown look into
  `<kryon.lightfield>` as an opt-in premium pack.
- Preserve the original beveled look as `<kryon.classic>`.
- Make examples attach a pack explicitly.
- Add `StylePicker` as the standard dropdown-style control for choosing among
  registered packs.
- Add settings persistence for the active style pack.

### M3 - Move widget families to resolved styles

- Convert Button, Text, TextField, TextArea, Dropdown, Surface, Card, Slider,
  Toggle, Checkbox, Radio, Progress, Separator, rows, links, and page chrome
  to consume resolved style frames.
- Remove family-specific theme assembly from `.c` paint paths.
- Ensure measurement uses the resolved typography and interior metrics.
- Add capture boards for each family in light, dark, high contrast, and
  none-style modes.

### M4 - Delete visual props

- Remove `ButtonProps.style`.
- Remove `TextInputStyle`.
- Remove `TextProps.color` from app-facing text.
- Remove row/page color props.
- Remove alpha-zero default behavior.
- Remove legacy theme-style modes and theme import/export APIs after their
  values have been converted into style packs.
- Migrate examples, then downstream apps after the Kryon commit lands on
  `master` and each app bumps `vendor/kryon`.

### M5 - Scoped inline style and inspector

- Add inline `Style` nodes in `.kry`.
- Add inspector output for matched rules, winning declarations, token origins,
  resolved values, transitions, and backend degradation.
- Add hot-reload tests for both external `.kss` files and inline style changes.

### M6 - Zero default

- Widget implementations have zero hidden visual defaults.
- App startup ensures the shipped style catalog and selects
  `<kryon.material>` only when no pack is active.
- Project templates explicitly include `<kryon.material>` so generated source
  still shows the baseline style choice.
- Existing Kryon visual personality remains available through explicit
  `<kryon.vanilla>`, `<kryon.glow>`, `<kryon.tk>`, and `<kryon.lightfield>`
  imports.
- `KRYON_STYLE=none` becomes a required test mode for behavior/layout.
- Leak scanners flip to zero exemptions.

## 22. Testing and enforcement

| Gate | Proves |
|---|---|
| Props scanner | no public widget props carry visual values |
| Paint scanner | widget paint code does not assemble visuals from themes |
| Literal scanner | maintained `.kry` UI uses raw visuals only inside `Style` nodes |
| `.kss` parser tests | valid and invalid sheets produce stable diagnostics |
| Rule-table parity | C, C++, Go, JS, and KRB resolve declarations identically |
| Capture boards | shipped packs render approved states across themes |
| None-style tests | widgets remain interactive, measurable, and accessible without chrome |
| Hot-reload tests | `.kss` edits invalidate cached fingerprints and repaint without recompile |
| Backend degradation tests | termi, DOM, canvas, libdraw, null consume the same resolved frames |
| Inspector tests | source maps and winning declarations are reported accurately |

## 23. Acceptance criteria

The plan is complete when:

- `make test` fails on any new app-facing visual prop;
- `make test` fails on widget paint code that calls theme getters for chrome;
- every maintained example uses `#style` or scoped `Style` for visuals;
- every app-facing widget can render in `KRYON_STYLE=none`;
- legacy theme files, theme import/export, and theme-style compatibility modes
  are gone from the app-facing styling surface;
- `<kryon.material>`, `<kryon.vanilla>`, `<kryon.glow>`, `<kryon.tk>`,
  `<kryon.classic>`, and
  `<kryon.lightfield>` are ordinary style packs, not hidden runtime modes;
- apps can register multiple packs and expose a `StylePicker` dropdown to
  switch between them quickly;
- style switching preserves widget state and invalidates only style
  fingerprints and measurements affected by style;
- C, C++, Go, JS, and KRB consume the same compiled rule table;
- capture boards prove Button, Dropdown, Text, TextInput, Surface, and common
  controls match their pack definitions;
- the inspector can explain every visible style value on screen.

## 24. Final shape

The final architecture is one path:

```text
.kry structure + .kss style pack
        |
        v
typed tokens + typed rule table
        |
        v
shared style resolver
        |
        v
resolved frames
        |
        v
shared drawing commands
        |
        v
backend rasterization
```

That is the line Kryon should hold: zero visual defaults in widgets, explicit
style packs for real appearance, a small deterministic cascade, typed tokens
instead of stringly CSS values, hot reload for design work, full inspector
support, and one resolver that every backend shares.
