# UI API Documentation

Kryon is a lightweight C UI component library for embedded applications and runtime environments. It provides core UI primitives and icon asset management without external dependencies.

## Table of Contents

- [Initialization](#initialization)
- [Canonical App API](#canonical-app-api)
- [Core Modules](#core-modules)
  - [Color](#color)
  - [Scaling](#scaling)
  - [DPI](#dpi)
  - [Layout](#layout)
  - [Clipping](#clipping)
  - [Text](#text)
  - [Text Layout](#text-layout)
  - [Icons](#icons)
  - [Theme](#theme)
  - [Locale](#locale)
  - [Sync](#sync)
  - [Transitions](#transitions)
  - [Runtime Assets](#runtime-assets)
  - [Style Sheets](#style-sheets)
  - [Desktop App Integration](#desktop-app-integration)
  - [File Dialogs](#file-dialogs)
  - [Web Utilities](#web-utilities)
- [UI Components](#ui-components)
  - [Buttons](#buttons)
  - [Images](#images)
  - [Text Input](#text-input)
  - [Navigation](#navigation)
  - [Modals](#modals)
  - [Scrolling](#scrolling)
  - [Controls](#controls)
  - [Layout Components](#layout-components)
- [Input Handling](#input-handling)
- [Focus System](#focus-system)
- [Preview And Diagnostics](#preview-and-diagnostics)

---

## Preview And Diagnostics

Build the native tools with `make tools`. In an app with a `kryon-host` Make
target producing `build/kryon/app_host.so`, run:

```sh
kryon --project /path/to/app preview --source src/main.kry
# Equivalent direct command:
kryon-preview watch --project /path/to/app --source src/main.kry
```

The visible preview rebuilds after source changes while the previous host keeps
drawing. Build, load, missing-callback, and initialization failures retain that
host and its state. A successful replacement creates fresh app state; this is
not state migration. `--width`, `--height`, `--style`, and `--theme` select the
preview presentation. F12 toggles the existing inspector; the status strip
shows the selected widget's source, bounds, and current focus ID. Compiler
errors take precedence. `--frames N --output path.png` supports bounded capture
runs. Closing the window or sending SIGINT/SIGTERM cleans up the build and
session files.

The watcher polls every 250 ms and debounces changes for 150 ms. It watches
source, style, Makefile, and common asset extensions inside the project; hidden,
build, vendor, dist, and node_modules directories and symlinks are skipped.
External imports require an in-project change to trigger rebuilding. `MAKE`
may name a make executable, and `KRYON_DIR` is passed as a literal make variable.

`k2c`, `k2cpp`, `k2go`, and `k2kir` accept `--diagnostics=json` or
`--diagnostics=text`. `KRYON_DIAGNOSTICS=json` selects JSON for shared frontend
errors; an explicit option overrides the environment. Text remains the default.
Each JSON diagnostic is one stderr line containing `severity`, `code`,
`message`, `path`, `line`, `column`, `end_line`, and `end_column`. Positions are
one-based; unavailable positions are zero. When only a start position is
known, the end repeats it. Parser errors often identify a line rather than an
exact token range. Backend/tool invocation errors can still be plain text.
Preview requests this format and displays the first structured error while
streaming the full build output to stderr.

## Initialization

### Native Go frame clock

`AppConfig.FrameClock` optionally supplies a `func() time.Time` to `New` or
`Open` for deterministic animation playback and capture. It is called once per
`BeginFrame`; the first frame has zero elapsed time, and subsequent frame
deltas drive the shared `.kry` motion policy. Supply nondecreasing timestamps.
Leaving it nil uses `time.Now`. This callback is per runtime and does not
replace wall-clock timing of input events or platform services.

### `InitInterface`

Initialize the UI system with viewport dimensions and DPI scale.

```c
void InitInterface(int width, int height, float dpi);
```

**Parameters:**
- `width` - Viewport width in pixels
- `height` - Viewport height in pixels
- `dpi` - DPI scale factor (1.0 = 96 DPI)

### `SetCurrentTheme`

Set the active theme. UI controls automatically use the active theme colors.

```c
void SetCurrentTheme(int theme_id, int current_dark_mode);
```

### `SetFrameCamera`

Update the UI camera and reset per-frame state.

```c
void SetFrameCamera(Camera2D camera);
```

`SetFrameCamera` sanitizes invalid cameras before storing them. A zero-initialized
`Camera2D` is treated as an untransformed UI camera with `zoom = 1.0f`, so controls
continue to receive pointer input. It also closes the previous focus pass and
starts a new one, so registered focus controls are reset every frame. If an
application does not need a transformed UI camera, prefer `BeginInterfaceFrame`.

### `GetDefaultCamera`

Return the canonical untransformed UI camera.

```c
Camera2D GetDefaultCamera(void);
```

### `BeginInterfaceFrame`

Convenience frame entry point for normal screen-space UI. It updates the viewport/DPI
state, updates the layout view size, and begins a frame with
`GetDefaultCamera()`.

```c
void BeginInterfaceFrame(int width, int height, float dpi);
```

### `EndInterfaceFrame`

Finish the current UI frame after all widgets have been drawn and before the
backend drawing pass ends. This draws deferred overlays, including dropdowns and
text-input context menus, and finalizes focus and inspection state.

```c
void EndInterfaceFrame(void);
```

---

## Canonical App API

### Disabled content blocks in `.kry`

Use a lexical block to disable ordinary child controls without manually pairing
runtime calls:

```kry
Disabled saving: {
    when = busy
    Button {
        label = "Save"
        bounds = {20, 20, 100, 32}
    }
}
```

The name is optional. `when` is a boolean expression, evaluated once on entry;
omitting it means `true`. Nested blocks inherit an outer disabled state even
when their own condition is false. Only `when` is accepted as a scope property.
The shared compiler cleanup pass restores the previous state at the closing
brace and on `return`, `break`, or `continue` leaving the block. This is authoring
syntax over Kryon's disabled-scope runtime operations, not a
new public widget or props type. As with explicit `defer`, functions containing
this scope currently reject raw C/preprocessor regions, `goto`/labels, and
`guard` exits; use structured control flow and explicit `if`/`return` instead.

### Scrolling blocks in native `.kry` code

```kry
Scroll content: {
    bounds = {20, 20, 240, 160}
    content_height = 600
    scroll_offset = &offset
    Button {
        bounds = {content.x, content.y, 200, 32}
        label = "Scrollable action"
    }
}
```

`bounds` is required and accepts a rectangle expression or `{x,y,w,h}`.
`content_height` defaults to zero; `scroll_offset` defaults to `nil` and, when
provided, points to caller-owned integer state. A block name optionally binds
the content rectangle for the scope, visible only inside that block.
Child widgets use this rectangle for scrolled positioning; the scope does not
silently transform explicit coordinates. Nested blocks intersect clips.

The compiler lowers `Scroll` to the native host scroll scope through lexical
cleanup, including return, break and continue. The same structured-control-flow
restrictions as `Disabled` apply. Generated C and Go parity tests exercise
nested input/clipping, wheel scrolling, scrollbar dragging and parent
restoration; C++ syntax tests cover the shared lowering. The old JavaScript
parity path is paused.

### Popup blocks in native `.kry` code

Use a `Popup` block for arbitrary caller-owned popup contents:

```kry
Popup tools: {
    bounds = {170, 50, 240, 140}
    id = 4201
    open = &tools_open

    Column content: {
        Text { text = "Tools" }
        Button { label = "Apply" }
    }
}
```

`Popup` conditionally submits its children only while open. The compiler lowers
the block through lexical cleanup, including `return`, `break`, and `continue`,
so application source cannot forget the host closing operation. For explicit
dismissal, update the caller-owned `open` value passed to the block. Its
properties are the fields of `PopupProps`.

### Runtime surface

Kryon owns the public app-facing API. The default backend still implements much
of the surface through raylib, but application and generated code should include
Kryon headers and call Kryon-owned names directly. That keeps apps portable to
future backends.

Use canonical widgets and retained primitives for normal UI:

```kry
#style <material> as material

AppBackground()
Button {
    label = "Save"
    bounds = {20, 20, 120, 34}
}
```

Generated `Draw*`, `BeginDrawing`, and raylib-shaped input/asset names are
backend and test support, not app-facing widget API. New Kryon UI code should
use the canonical widget names and props shown here.

### Style sheet classes

Web KSS declaration values are interpreted by the shared `.kry` parser. Token
references resolve at the rule's source position, including keyframes; later
overlays affect later rules. `WebStyleRule.tokenOrigins` records the actual
per-property token name and origin. Numeric CSS values accept signed decimals,
exponents, and an optional `px` suffix. CSS function and quoted values remain
opaque; delimiters inside them do not end a declaration. Unknown property names
are rejected even when their values are numeric. Source spans use UTF-8 bytes.

Style winners compare layer, selector specificity, and source order in that
sequence; exact ties choose the later rule visited. These decisions and selector
weights are shared `.kry` policy for native resolution, web resolution, and web
inspector traces. High specificity cannot outrank a higher layer, and a large
source position cannot outrank higher specificity. Web rule/trace `score` is a
diagnostic summary, not a priority override. Prebuilt web rules may omit it.

Web attribute operators and `nth-child`/`nth-last-child`/`nth-of-type`/
`nth-last-of-type` formulas use shared KSS predicates. Attribute matching is
case-sensitive and handles UTF-8 text; a missing value never matches. Empty `~=`, `^=`, `$=`, and `*=` operands
never match, and unknown operators are rejected. Position formulas accept
integers, `odd`, `even`, and `an+b`; hexadecimal, exponential, and fractional
number spellings do not match. Coefficient and offset magnitudes are limited to
2147483647, with 64-bit intermediate arithmetic. Missing sibling positions do
not match. Hosts provide attributes and sibling identity/order.

Selector state aliases, form-control classification, validity, placeholder, and
required/read-only decisions also use generated KSS policy. State names use
ASCII case folding and the shared 64-byte name limit; unknown names read their
named state flag. Basic structural pseudos (`root`, `scope`, first/last/only
child or type, `empty`, `focus-within`, and `target`) evaluate shared facts.
Hosts collect relationship, content, focus, and route facts; functional selector
dispatch and traversal remain separate migration work.

Declarative selectors now use the shared streaming KSS lexer for lists,
combinators, kinds, IDs, classes, attributes, state aliases, and functional
pseudo arguments. It preserves quoted delimiters and nested argument spans,
handles comments without stripping quoted URLs, and bounds delimiter nesting
at 64 levels. Malformed delimiters, trailing/empty list items, empty selector-list
pseudo arguments, and unsupported attribute flags report selector errors.
Attribute values containing spaces must be quoted. CSS identifier escapes and
quoted escape decoding are not implemented by this lexer; quoted escapes remain
raw. Parsing does not establish full native/web selector conformance.




KSS tokens are grouped in one `tokens { ... }` block. Accepted groups are
`color`, `length`, `number`, `duration`, and `material`; duration values are
stored as milliseconds, so `80ms` is `80` and `0.14s` is `140`.

```kss
tokens {
  color { accent: #2f6bff; }
  length { space.3: 12; }
  duration { fast: 80ms; normal: 0.14s; }
  material { default: Flat; }
}
```

KSS class selectors use CSS-style suffixes or an explicit selector attribute:

```kss
* { gap: 12; }
Button.primary { padding-y: 12; }
Button[class=primary]:pressed { focus: #2f6bff; }
```

Native code uses `StyleClassId` to assign the same stable class id to
`StyleFacts.class_name` when resolving a stylesheet. Widgets expose the same
field on their props, so a button can opt into `Button.primary` without a
legacy theme hook:

```c
int32_t primary = StyleClassId("primary");
Button((ButtonProps){.label = "Save", .class_name = primary});
```

Use canonical widget names when declaring controls from C:

```c
Button(button);
TextField(field);
Slider((SliderProps){.bounds = {20, 64, 180, 56}, .id = 12,
    .label = "Volume", .kind = NumericInt, .int_values = &volume,
    .value_count = 1, .min = 0, .max = 100, .format = "%"});
Overlays();
```

Normal application UI is declared in `#ui` functions. The root is an explicit
`Screen` node and containers own lexical blocks. The compiler lowers named
blocks to retained runtime nodes with stable path keys; reconciliation, layout,
input routing, updates, painting, and overlays remain runtime-owned:

```kry
Settings :: (viewport: Rectangle) #ui {
    Screen root: {
        bounds = viewport

        Column body: {
            bounds = {20, 20, 360, 420}
            gap = 12
            padding = 16

            Text((TextProps){
                .text = "Account"
            })
            TextField(account_field)
            Button(save_button)
        }
    }
}
```

Custom widgets can also use named blocks. Declare a `#ui`
function with a props record and optional typed slots, then supply its fields
and slot values as block properties:

```kry
CounterProps :: struct {
    value: i32
}
Counter :: (props: CounterProps) #ui {
    // Compose the widget using props.value here.
}
Counters :: () #ui {
    Counter first: {
        value = 3
    }
}
```

For C, C++, and Go, this resolves to the same declaration as an ordinary
`Counter(props)` call. Omitted fields are zero-initialized, props are
passed by value, and unknown, duplicate, or incorrectly typed properties are
errors. Declarations can appear later in the file or in an explicitly imported
module; imported declarations must be public and their props record must be
directly visible without a conflicting local type. Custom blocks currently
accept prop fields and slot values, including inline captured bodies.
Their block names do not yet allocate persistent widget identity.
Interactive compositions must therefore receive distinct stable control IDs
from their caller; reusing a declaration does not automatically scope IDs in
its body.

A generated declaration can accept typed child-content parameters declared with
`Content :: (bounds: Rectangle) #slot`. The native caller supplies a synchronous
callback and context in C/C++, or a Go function in native Go. The declaration
invokes or forwards it with signature checking and value-copy arguments.
A matching `.kry` function can also supply the content: pass `DrawChild` as a slot
argument, initialize `child: Content = DrawChild`, or assign it to an existing
slot binding. Local, private, and imported functions keep their module state and
runtime receiver. The parameter types and void return must match the slot exactly.
A block supplies a slot by its parameter name, for example `content = DrawChild`
for `Card :: (props: CardProps, content: Content) #ui`. Every slot must be supplied.
Slot names must differ from props fields and other slot names. Prop fields and
slot values evaluate once in source order, including conditional slot selection;
omitted props retain their zero values. This works for custom declarations named
`Button` or `Text` as well as application-defined names.

An inline slot body can read and update enclosing local values:

```kry
count: i32 = 0
child: Content = (bounds: Rectangle) #slot {
    count += 1
}
```

The same syntax supplies a block property: `content = (bounds: Rectangle) #slot { ... }`.
Parameters must match the declared slot signature. Nested bodies capture by
reference, including retained instance bindings and other slots. These calls
are synchronous: a host must not retain the callback. A slot can be reassigned
only within the block that declares its binding; assigning to an enclosing or
captured slot is rejected to prevent borrowed locals from escaping their scope.

A portable body can bind a typed retained record to an explicit integer key:

```kry
CounterState :: struct {
    count: i32
}
CountProps :: struct {
    key: u64
    amount: i32
}
Count :: (props: CountProps) -> i32 #ui {
    retained: CounterState #instance(props.key)
    retained.count += props.amount
    return retained.count
}
```

`#instance(key)` evaluates the key once and borrows a zero-initialized record
from the current render host. The same record type and 64-bit key share state
across calls; different types or hosts remain independent. Assigning the binding
updates retained state, while assigning it to an ordinary local copies its
value. After more than twelve unused frames in its own host, the record expires.
Bindings require a declared record, an integer key, and a fully checked portable
body. Hierarchical identity and implicit keys are still pending; callers must
supply distinct stable keys for independent instances of the same state type.

Migrate callers to the current Kryon API directly so the backend boundary stays
simple.

Cartridges (`.krb`) are portable Kryon render artifacts produced from KIR. The
runtime loads packed node data, state schema, source metadata, portable logic,
capabilities, and explicit host imports. Load the image, bind host functions by
import name when the cartridge declares them, and draw through `KryBackend`:

```c
KrbImage img;
KrbLoadFile(&img, "02_buttons.krb");
KrbBind(&img, "primary_button", on_primary, app);
KrbDraw(&img, 0, 0, width, height);
KrbFree(&img);
```

`k2b` is the cartridge compiler. It accepts `.kry` or `.kir`; `.kry` input is
lowered through KIR before the KRB sections are written. `KryBackendDraw`
implements the rendering table with the public Kryon draw/input API;
`KryBackendNull` is the headless stand-in. Mount live C fields so a cartridge
can read them as files:

```c
KrbField fields[] = {
    { "score", offsetof(App, score), KRB_I32, 4 },
    { NULL }
};
KrbMount(&img, "/app", app, fields);
KrbReadI32(&img, "/app/score", &n);
```

`OP_CALL_HOST` and `OP_SET_I32` run from `KrbExec`. The portable path is
designed around KIR-owned logic plus a capability/import table; native C apps
should continue to use the C backend when direct library integration is the
right target. See `docs/KRB_FORMAT.md`.

---

## Core Modules

### Page

Kryon page helpers layer browser-facing semantics over the normal UI runtime.
On the DOM backend they set document metadata, route state, and semantic DOM
annotations; on other backends they render through the same Kryon UI widgets
and ignore web-only metadata.

```c
SetPageTitle("Kryon");
SetPageDescription("Native-feeling apps across desktop and web.");
ReplaceRoute("/docs#getting-started");
int route_version = GetRouteVersion();

Page((PageProps){
    .bounds = {0, 0, GetViewWidth(), GetViewHeight()},
    .title = "Kryon",
    .description = "Native-feeling apps across desktop and web.",
    .key = Key("home")
});
Heading((HeadingProps){
    .bounds = {40, 40, 520, 40},
    .text = "Kryon",
    .level = 1,
    .key = Key("hero-heading")
});
Link((LinkProps){.bounds = {40, 96, 160, 28}, .text = "Docs", .link = "/docs",
    .focus_id = 101});
End();
```

Use `Page`, `Section`, `Heading`, `ParagraphText`, `Link`, `Image`, `Flow`,
and `Grid` for Kryon-authored website surfaces. Set `ImageProps.alt_text` when
the image is semantic content. The Go runtime mirrors these helpers and records
semantic `FrameOp` metadata, so `.kry` files lowered through `k2go` can use the
same page API.
`GetRouteVersion()` increments when the browser route changes through
`PushRoute`, `ReplaceRoute`, `popstate`, or `hashchange`; non-DOM backends
return `0`.

### Color

#### `LightenColor`

Lighten a color by increasing HSL lightness.

```c
Color LightenColor(Color c, int amount);
```

**Parameters:**
- `c` - Source color
- `amount` - Lightness amount (0-255)

**Returns:** Lightened color

#### `DarkenColor`

Darken a color by decreasing HSL lightness.

```c
Color DarkenColor(Color c, int amount);
```

---

### Scaling

#### `SetScale`

Set the DPI scale factor (call once at startup).

```c
void SetScale(float scale);
```

#### `GetScale`

Get the current DPI scale factor.

```c
float GetScale(void);
```

#### `Scale`

Scale a pixel value by the DPI factor.

```c
int Scale(int px);
```

#### `ClampPx`

Scale and clamp a pixel value between min and max.

```c
int ClampPx(int px, int min_px, int max_px);
```

---

### DPI

#### `dpi_state`

Global DPI state structure.

```c
typedef struct DPIState {
    int view_width;
    int view_height;
    float ui_scale;
    float ui_scale_clamped;
    float camera_zoom;
    int base_width;
    int base_height;
    int needs_update;
} DPIState;
```

#### `InitDPI`

Initialize DPI system.

```c
void InitDPI(void);
```

#### `UpdateDPI`

Update DPI state for new viewport size.

```c
void UpdateDPI(int view_width, int view_height);
```

---

### Layout

#### Flex Alignment

`ui_layout.h` provides a measured, single-line flex layout for immediate
widgets. It resolves geometry before input handling, with no previous-frame
dependency. The same `.kry` policy generates the C and native Go implementations.

```c
FlexCursor BeginFlexCursor(FlexProps props, int32_t count, float total_item_extent);
FlexCursor FlexStep(FlexCursor cursor, float width, float height);
```

`FlexProps` accepts `bounds`, `gap`, `padding`, and:

- `direction`: `FlexRow` (default) or `FlexColumn`.
- `justify_content`: `JustifyStart` (default), `JustifyCenter`, `JustifyEnd`,
  `JustifySpaceBetween`, `JustifySpaceAround`, or `JustifySpaceEvenly`.
- `align_items`: `AlignStart` (default), `AlignCenter`, `AlignEnd`, or
  `AlignStretch`. Stretch fills an unspecified (zero) cross-axis size; explicit
  sizes are preserved.

Pass the sum of item widths for a row, or heights for a column, as
`total_item_extent`, excluding gaps and padding. Then call `FlexStep` once per
item, passing its measured size, and use `cursor.item` as the widget bounds.
For unequal items, measure all sizes before beginning the cursor.

```c
float size = 52;
FlexCursor actions = BeginFlexCursor((FlexProps){
    .bounds = {20, 80, 600, 60},
    .justify_content = JustifySpaceAround,
    .align_items = AlignCenter
}, 3, size * 3);
actions = FlexStep(actions, size, size);
Button((ButtonProps){.bounds = actions.item, .icon_only = true,
                     .icon_type = ICON_CALENDAR});
```

`gap` is a minimum gap, with distributed free space added to it. Space-around
has half as much distributed space at the edges as between items; space-evenly
has equal distributed edge and interior space. One item centers for around and
evenly, but stays at the start for between. Empty or exhausted cursors return
an empty item. Negative sizes, gaps, and padding clamp to zero. Overflow uses
safe start alignment and never introduces negative gaps or shrinks controls.
Sizes use caller coordinates; scale them before measuring when needed.

This primitive does not wrap, grow/shrink items, or align text baselines. It is
separate from retained `Row`/`Column` scopes and does not change their defaults.

#### `SetViewSize`

Set the view dimensions.

```c
void SetViewSize(int width, int height);
```

#### `GetViewWidth` / `GetViewHeight`

Get current view dimensions.

```c
int GetViewWidth(void);
int GetViewHeight(void);
```

#### `GetCenteredColumn`

Calculate centered column dimensions.

```c
void GetCenteredColumn(int max_w, int side_pad, int *x, int *w);
```

**Parameters:**
- `max_w` - Maximum width
- `side_pad` - Side padding
- `x` - Output: x position (can be NULL)
- `w` - Output: width (can be NULL)

#### `GetPageSidePadding`

Calculate page side padding based on current view width.

```c
int GetPageSidePadding(void);
```

---

### Clipping

#### `GetClipIntersection`

Calculate intersection of two rectangles.

```c
Rectangle GetClipIntersection(Rectangle a, Rectangle b);
```

#### `BeginClip`

Begin a clipping region.

```c
void BeginClip(int x, int y, int w, int h);
```

#### `EndClip`

End the current clipping region.

```c
void EndClip(void);
```

#### `ResetClip`

Reset all clipping.

```c
void ResetClip(void);
```

---

### Text

Text font/color inheritance, disabled opacity, nonnegative letter spacing,
automatic extents, wrap-mode selection, and alignment offsets are defined in
`runtime/text.kry` and consumed by both native hosts. Inherited fonts are
resolved before measuring a Text child. Typeface lookup, shaping, glyph
measurement, and drawing remain backend services.

#### Font Management

```c
Font GetTextFont(void);
int RegisterTextFont(const char *name, Font font);
int RegisterSmallTextFont(const char *name, Font font);
int RegisterTextFontSourceForText(const char *name, const char *file_type, const unsigned char *font_data, unsigned int font_size, const char *text);
int RegisterTextFontFileSourceForText(const char *name, const char *path, const char *text);
int UseTextFont(const char *name);
int TextFontHasGlyph(Font font, int codepoint);
```

#### Font Loading

```c
Font LoadTextFontFromMemory(const char *file_type, const unsigned char *font_data, unsigned int font_size, int base_size);
Font LoadTextFontAsset(const char *path, int base_size);
void UnloadTextFont(Font *font);
void ClearTextFonts(void);
void TextFontMemoryReport(const char *tag);
```

`TextFontMemoryReport` prints per-font rasterization stats (codepoint counts,
rasterized sizes, glyph counts) to stderr. It is a no-op unless
`KRYON_MEM_DEBUG` is set in the environment.

Source fonts registered through `RegisterTextFontSource` rasterize their
declared codepoints at each requested physical size and retain bounded size
tiers. Their atlas coverage is immutable after registration: drawing or
typing text never reallocates a font texture. Supply every codepoint the
source is expected to render; omitting the list selects Kryon's standard UI
coverage. `RegisterFixedTextFontSource` is an equivalent explicit name.
Use `RegisterTextFontSourceForText` or `RegisterTextFontFileSourceForText` when
the font should include Kryon's standard UI coverage plus the unique
codepoints found in a UTF-8 corpus, such as localized strings.

#### Text Widgets

```c
typedef enum { TextWrapAuto, TextWrapNone } TextWrap;
typedef enum { TextAlignStart, TextAlignCenter, TextAlignEnd } TextAlign;

typedef struct {
    Rectangle bounds;
    const char *text;
    int class_name;
    TextWrap wrap;
    TextAlign align;
    TextAlign vertical_align;
    int disabled;
    int selectable;
} TextProps;

void Text(TextProps text);
```

`Text` is the single read-only text widget. A positive bounds width enables
word wrapping by default, and a positive height clips the result. Set
`wrap = TextWrapNone` for one line; `align` and `vertical_align` control its
placement inside the bounds.
Zero width measures the line intrinsically. Transparent color selects the
current UI default. Color and disabled presentation are properties, not
separate widget entry points.

`style` uses the shared `StyleForeground`, `StyleFontSize`, and `StyleOpacity`
fields. Present style fields override `color` and `font`; other style fields
do not paint a text surface. `StyleForeground` preserves every alpha value,
including transparent black, instead of selecting an inherited color.
Opacity multiplies the resolved foreground alpha after disabled presentation;
an explicit zero opacity hides the text without removing its layout space.
When foreground is absent, text continues to inherit its containing button's
animated foreground, with the text node's opacity applied afterward.
An inherited disabled foreground is already resolved and is not faded again.
Explicit child colors receive disabled presentation when their button or scope
is disabled.

KSS `typeface` selects a registered font by name for text measurement,
wrapping, and painting in C and native Go. Unknown names keep the current face.
The bundled Noto font setup also registers `"semibold"`; use a text class with
`typeface: semibold` for real semibold outlines, or register your own named
face. KSS `font-size` controls the size.

KSS `letter-spacing` adds a non-negative number of logical pixels between
Unicode codepoints (not UTF-8 bytes), with no trailing gap. Zero preserves the
font's normal spacing; negative values are treated as zero. C and native Go
include the spacing in intrinsic width, wrapping, alignment, and painting. C
text selection uses the same spaced positions. This is codepoint tracking, not
grapheme-cluster shaping; use zero for scripts that require joined shaping.

Native `.kry` can use the same properties without a compound literal:

```kry
Text message: {
    bounds = {24, 24, 240, 64}
    text = "This wraps and clips inside its bounds."
    class_name = Class("message")
}
```

### Text Layout

Layout text with embedded icons and line breaks.

Wrapped read-only text collapses ASCII spaces, tabs, vertical tabs, and form
feeds between words. LF, CRLF, and CR produce explicit line breaks; consecutive
and trailing breaks preserve empty lines. Non-breaking spaces remain within
their word, and UTF-8 words are never split into individual bytes. An oversized
word occupies its current line without inserting a leading blank line. Text
bounds still control clipping and alignment, including overflowing words.

C and Go use the same `.kry` tokenization and line decisions, with each host
providing font measurements. Go `Paragraph` advances the caller's Y position by
the complete laid-out height; `ParagraphText` preserves its style gap between
lines, with no trailing gap. Native `ParseTextLayout` recognizes `%i` only when
an icon texture or icon type is supplied; otherwise it remains literal text.
Native empty/whitespace-only strings contain one empty logical line after
reflow; a null input remains an absent layout. Inline icon painting in Go and
editable TextArea wrapping are outside this shared read-only layout contract.

#### `TextLayout`

```c
typedef struct TextLayout {
    TextElement *elements;
    int element_count;
    int *line_breaks;
    int line_count;
    int *line_widths;
    int total_height;
    int line_height;
    int last_reflow_width;
} TextLayout;
```

#### `ParseTextLayout`

Parse text input into a layout.

```c
TextLayout ParseTextLayout(const char *input, Texture2D icon, IconType icon_type, int icon_size);
```

#### `ReflowTextLayout`

Reflow layout for a given width.

```c
void ReflowTextLayout(TextLayout *layout, int max_width, int font_size, int line_height);
```

#### `GetTextLayoutHeight` / `FreeTextLayout`

```c
int GetTextLayoutHeight(TextLayout *layout);
void FreeTextLayout(TextLayout *layout);
```

---

### Icons

#### `GetIconAsset`

Get icon asset by type or name.

```c
const IconAsset *GetIconAsset(IconType type);
const IconAsset *GetIconAssetByName(const char *name);
```

Use the canonical `Icon(...)` widget to draw icons in UI code.

Kryon's checked-in `icons/` directory is the finished spritesheet package.
Each manifest icon has a permanent numeric `id`; append new IDs after the
highest existing ID and never renumber existing icons. The embedding script
validates IDs and preserves them in `IconType`, including name lookup order.
The profile catalog includes cat, sea turtle, and crescent moon artwork with
separate stable sync IDs. Applications choose which catalog entries to offer.
Regular UI entries—including workbook controls—use the rounded MingCute Core
family and are packed into the monochrome `icons/ui.png`. The manifest
records each upstream SVG mapping and revision. There are no prebuilt flat-color
variants; runtime drawing can tint the clean alpha artwork on demand.
Product and project marks retain their brand colors, stay out of the generic UI
atlas, and are packed into the separate `icons/logos.png` sheet.
Entries use names like `platforms_freebsd.png`, `proj_kryon.png`, and matching
`ICON_PLATFORMS_*` / `ICON_PROJ_*` enum values. Downstream
websites can sync shared assets from a vendored Kryon copy with
`vendor/kryon/scripts/sync-icons.sh`. Embedded C assets are refreshed with
`make icons-embed`.

Profile-image, platform, payment, and language artwork is packed into the
separate full-color `icons/pfp.png`, `icons/platforms.png`,
`icons/payments.png`, `icons/language.png`, and `icons/tiles.png` sheets. They remain in the same
indexed icon catalog with their existing `ICON_*` values. Use
`GetProfileImageIconCount`,
`GetProfileImageIconType`, and `GetProfileImageIconName` to enumerate
the standard profile-image options.

Kryon also exposes stable `SYNC_PROFILE_ICON_*` IDs and mapping helpers:

```c
IconType GetProfileImageIconTypeForSyncID(int sync_id);
int GetSyncIDForProfileImageIconType(IconType type);
```

Use those IDs for server storage or sync payloads instead of generated
`IconType` ordinals.

---

### Theme

Theme management for colors and appearance.

#### `ResetTheme`

```c
void ResetTheme(void);
```

#### `GetThemeColor`

```c
Color GetThemeColor(const char *scope, const char *key);
```

#### Dark Mode

```c
void SetCurrentTheme(int theme_id, int dark_mode);
```

#### Theme Colors

Prefer KSS style packs, `AppBackground()`, and widget defaults for app chrome.
The direct theme color getters remain for compatibility and low-level drawing
code that must choose explicit primitive colors.

```c
Color GetCurrentThemeColor(const char *key);
Color GetThemeText(void);
Color GetThemeBackground(void);
Color GetThemeSurface(void);
Color GetThemeCircle(void);
Color GetThemeButton(void);
Color GetThemeButtonHover(void);
Color GetThemeIcon(void);
Color GetThemeLink(void);
```

#### Theme Metrics

The shipped KSS style packs are Material (the default), Classic, and Lightfield.
Style selection belongs in the Appearance dropdown. Color variations use theme
overlays within the selected pack.

Visual style selection belongs to KSS style packs. Theme metrics are the shared
layout defaults used by the active pack and by low-level drawing code.

```c
ThemeMetrics GetThemeMetrics(void);
ThemeMetrics GetDefaultThemeMetrics(void);
ThemeScheme GetThemeScheme(void);
```

---

### Locale

Localization support.

#### `InitLocale` / `SetLocale`

```c
void InitLocale(void);
int SetLocale(const char *code);
```

#### `GetLocaleText` / `FormatLocaleText`

```c
const char *GetLocaleText(const char *key);
void FormatLocaleText(char *dst, size_t dst_size, const char *key, ...);
```

#### Locale Information

```c
int GetLocaleCount(void);
const char *GetLocaleCode(int index);
const char *GetLocaleLabel(int index);
int GetLocaleIndex(const char *code);
const char *GetCurrentLocaleCode(void);
int GetCurrentLocaleIndex(void);
```

---

### Sync

Sync Sync is Kryon's common sync protocol layer. Kryon owns URL handling, token auth,
challenge/login, bearer requests, sync posting, account deletion, and small JSON
helpers, and default platform transport. Applications still own their local data
model and provide callbacks to build sync payloads, apply sync responses, and
store auth tokens.

#### `SyncResult`

```c
typedef enum SyncResult {
    SYNC_OK = 0,
    SYNC_INVALID_URL,
    SYNC_NO_ACCOUNT,
    SYNC_PAYLOAD_FAILED,
    SYNC_CHALLENGE_FAILED,
    SYNC_SIGN_FAILED,
    SYNC_REQUEST_FAILED,
    SYNC_AUTH_FAILED
} SyncResult;
```

#### `SyncConfig`

```c
typedef struct SyncConfig {
    const char *base_url;
    const SyncAccount *account;
    const char *client_id;
    SyncHttpRequestFn http_request;
    SyncGetTextFn get_text;
    SyncSetTextFn set_text;
    SyncBuildPayloadFn build_payload;
    SyncFreePayloadFn free_payload;
    SyncApplyResponseFn apply_response;
    SyncVoidFn purge_synced_deleted;
    SyncLogFn log_http_failure;
    void *user;
} SyncConfig;
```

`http_request` can be app-provided, or set to `DefaultSyncHttpRequest` for
Kryon's built-in libcurl/JNI/fetch transport.
`get_text` and `set_text` store `sync_auth_token` and
`sync_auth_token_expires_at`.

#### URL Helpers

```c
int IsSyncURLValid(const char *url);
int NormalizeSyncURL(const char *input, char *out, size_t out_size);
int JoinSyncURL(char *out, size_t out_size,
                             const char *base_url, const char *path);
int JoinSyncWebSocketURL(char *out, size_t out_size,
                                const char *base_url, const char *path);
```

Remote sync URLs must be HTTPS. HTTP is accepted only for loopback hosts such as
`localhost`, `127.0.0.1`, and Android emulator host `10.0.2.2`.

#### Buffer And JSON Helpers

```c
int AppendSyncBuffer(SyncBuffer *buffer,
                                  const void *data, size_t bytes);
int AppendSyncBufferJSONString(SyncBuffer *buffer,
                                              const char *text);
void FreeSyncBuffer(SyncBuffer *buffer);
int FindSyncJSONString(const char *json, const char *key,
                                     char *out, size_t out_size);
long long FindSyncJSONInt64(const char *json, const char *key,
                                          long long fallback);
```

These are intentionally small helpers for Sync protocol payload construction and
simple response fields. Applications that already have a full JSON parser should
keep using it for domain data.

#### Auth And Sync

```c
void ClearSyncAuthToken(const SyncConfig *cfg);
SyncResult LoginSync(const SyncConfig *cfg);
SyncResult RunSync(const SyncConfig *cfg);
SyncResult RequestSyncBearer(const SyncConfig *cfg,
                                                   const char *method,
                                                   const char *path,
                                                   const char *body,
                                                   char *out,
                                                   size_t out_size);
SyncResult DeleteSyncAccount(const SyncConfig *cfg);
const char *GetSyncResultName(SyncResult result);
```

`RunSync` loads or refreshes an auth token, asks the app callback for
a local-first payload, posts it to `/api/v1/sync`, applies the response through
the callback, and purges synced tombstones on success. `RequestSyncBearer`
is for app-specific Sync endpoints that use the same account token.

#### Default Transport And Events

```c
int DefaultSyncHttpRequest(const char *method, const char *url,
                            const char *body,
                            const char *const *headers,
                            int header_count,
                            SyncBuffer *response,
                            long *status, void *user);
SyncResult WaitForRemoteSyncEvent(const SyncConfig *cfg,
                                     const char *path);
#if defined(__EMSCRIPTEN__)
int StartWebSync(const SyncConfig *cfg);
int PollWebSync(SyncResult *result, int *changed);
int StartWebRemoteEvents(const SyncConfig *cfg, const char *path);
int PollWebRemoteEvents(void);
#endif
```

`DefaultSyncHttpRequest` provides the common platform HTTP transport. Native
builds use libcurl, Android builds call `syncHttpRequest`/`syncWebSocketWait` on
the activity through JNI, and web builds use JavaScript `fetch`.

`WaitForRemoteSyncEvent` waits for one Sync WebSocket sync-change event using the
stored bearer token. Web builds use the nonblocking `StartWebRemoteEvents`
and `PollWebRemoteEvents` pair instead.

`StartWebSync` and `PollWebSync` run the same login/token/sync flow as
`RunSync` without blocking the browser frame loop.

---

### Transitions

Transition effects for screen changes.

#### `TransitionState`

```c
typedef struct TransitionState {
    int active;
    int phase;
    float elapsed_seconds;
    float duration_seconds;
} TransitionState;
```

#### `ResetTransition` / `BeginTransition`

```c
void ResetTransition(TransitionState *transition);
void BeginTransition(TransitionState *transition, float duration_seconds);
```

#### `ReverseTransitionToOut`

```c
void ReverseTransitionToOut(TransitionState *transition);
```

#### `GetTransitionAlpha` / `StepTransition`

```c
float GetTransitionAlpha(const TransitionState *transition);
int StepTransition(TransitionState *transition, float delta_seconds);
```

---

### Runtime Assets

Download and cache runtime assets.

#### `InitRuntimeAssets`

Initialize runtime asset system.

```c
int InitRuntimeAssets(const char *app_id);
```

#### `GetRuntimeAssetCacheRoot`

Get cache root directory.

```c
int GetRuntimeAssetCacheRoot(const char *app_id, char *out, size_t out_size);
```

#### `DownloadRuntimeAsset`

Download an asset.

```c
int DownloadRuntimeAsset(RuntimeAssetDownload *download, const char *url, const char *path);
```

Poll the download to copy the worker-owned status snapshot into
`RuntimeAssetDownload`. Release it with `FreeRuntimeAssetDownload`; native
builds wait for an active worker before freeing its state.

```c
RuntimeAssetStatus PollRuntimeAssetDownload(RuntimeAssetDownload *download);
void FreeRuntimeAssetDownload(RuntimeAssetDownload *download);
```

#### `SetRuntimeAssetDownloadBackend`

Set custom download backend.

```c
void SetRuntimeAssetDownloadBackend(RuntimeAssetDownloadBackend backend);
```

---

### Desktop App Integration

Desktop helpers centralize app identity, XDG paths, single-instance locks, and
file/URL open events for native desktop apps. Public names follow the normal
Kryon app-facing style, without a `Kry` prefix.

```c
typedef struct DesktopAppInfo {
    const char *app_id;
    const char *name;
    const char *display_name;
    const char *summary;
    const char *icon_name;
    const char *wm_class;
    int single_instance;
} DesktopAppInfo;

void InitDesktopApp(const DesktopAppInfo *info);
const DesktopAppInfo *GetDesktopAppInfo(void);
const char *GetDesktopAppID(void);
const char *GetDesktopDisplayName(void);
int GetDesktopConfigDir(char *out, int cap);
int GetDesktopDataDir(char *out, int cap);
int GetDesktopCacheDir(char *out, int cap);
int AcquireDesktopSingleInstance(const char *app_id, char *lock_path, int cap);
void ReleaseDesktopSingleInstance(void);
int QueueDesktopOpenPath(const char *path_or_url);
DesktopOpenEventKind PollDesktopOpenEvent(char *out, int cap);
```

`InitDesktopApp` also registers the desktop-entry id with the notification
backend so notification icons resolve through the installed desktop metadata.

---

### File Dialogs

Open native desktop file dialogs through the best available Linux backend. The
default backend order is XDG Desktop Portal, GTK, `zenity`, `kdialog`, then
`yad`. `KRYON_FILE_DIALOG_BACKEND` can force `portal`, `gtk`, `zenity`,
`kdialog`, `yad`, `auto`, or `none` for debugging and packaging checks. Explicit
forced backends fail closed when the requested backend is not available.

#### `GetFileDialogBackendName`

Return a stable backend name for logs, diagnostics, and examples.

```c
const char *GetFileDialogBackendName(void);
```

#### `LoadFileDialog` / `SaveFileDialog` / `SelectFileDialogFolder`

Open file, save file, or folder selection dialogs. They return `1` when the user
selects a path and `0` when the user cancels or no backend is available.

```c
int LoadFileDialog(FileDialog *dlg, const char *title);
int LoadFilteredFileDialog(FileDialog *dlg, const char *title, const char *filter);
int SaveFileDialog(FileDialog *dlg, const char *title, const char *default_filename);
int SelectFileDialogFolder(FileDialog *dlg, const char *title);
```

---

### Web Utilities

Web platform specific utilities.

#### `GetWebViewportSize`

Get browser viewport size.

```c
void GetWebViewportSize(int fallback_width, int fallback_height, int *width, int *height);
```

#### `GetWebWindowFlags` / `SyncWebWindowSize`

```c
unsigned int GetWebWindowFlags(void);
int SyncWebWindowSize(void);
```

---

## UI Components

### Images

```c
typedef enum ImageFit {
    ImageFitStretch,
    ImageFitContain,
    ImageFitCover
} ImageFit;

typedef struct ImageProps {
    const char *asset_path;
    const char *alt_text;
    Rectangle bounds;
    Rectangle source;
    Vector2 origin;
    float rotation;
    ImageFit fit;
    int32_t class_name;
} ImageProps;

Image((ImageProps){...})
```

Images are image-backed UI widget nodes. `asset_path` is resolved first as a
runtime file path and then as an embedded asset path. `Image` uses the full
image with contain fitting and exposes source rect, origin, rotation, tint, fit
mode, and optional material-style image treatment through `style`. The
`Sprite2D` scene node shares the same texture cache for world-space game
sprites. The public `.kry` and Go widget is `Image`; host drawing support is
internal because raylib already owns `Image` as a decoded-image-in-memory struct
type. Future web-native work should keep the same public image surface.

### Buttons

See [Widget styling](WIDGET_STYLING.md) for the shared visual contract and
dropdown role mapping.

#### `Button`

```c
typedef struct {
    Rectangle bounds;
    const char *label;
    int id;
    int class_name;
    ButtonTone tone;
    ButtonEmphasis emphasis;
    ControlSize size;
    int disabled;
    int loading;
    int selected;
    int full_width;
    int pill;
    int circle;
    ButtonState state;
} ButtonProps;
```

#### `Button`

Draw and handle a button.

```c
int Button(ButtonProps button);
```

Native Go uses the same props-only contract: `Button(ButtonProps) bool`.
The Rectangle, Vector2, Color, and Texture2D field contracts come from
`runtime/drawing_props.kry`. These records support typed field access and value
copies in shared `.kry` functions. Their C/C++ definitions remain supplied by
the graphics host; Go definitions are generated from the contract.
Button measurement reads the shared props directly. Explicit physical bounds
are preserved at fractional scale; circle, square, and icon-only shapes use
the resolved height for their width. Shared `.kry` bodies can copy and compare
the borrowed `label` and style `typeface` fields. C callers retain ownership of
those null-terminated strings; null labels behave as empty labels.
Go `FrameOpButton` stores the shared frame in `op.Button`. Inspect its
`Props`, `Appearance`, `Material`, and `Font` rather than flattened paint fields.
The operation's `Bounds` and `SurfaceBounds` still specify final placement;
painting applies them to a copy of the frame, preserving the recorded value.
The shared `AdvanceFrame` pass resolves retained motion, appearance, and the
complete ButtonFrame from a stable key, input sample, palette, metrics, style
overrides, animation timing, surface bounds, scale, and fallback font. Hosts
sample input separately so retained painting does not consume activation again.
The shared `PaintButton` pass accepts a resolved frame, physical label width,
elapsed time, disclosure state, and synchronous `SurfacePainter`/`Painter`
callbacks. It emits the surface layers, then the mark and label; callbacks must
finish during the call and must not retain borrowed text or callback contexts.
`MeasureTextWidth(text, font_size, typeface)` measures physical text width using
the requested typeface. C restores the previous typeface after the call; native
Go exposes the same service on Runtime and as a package function. Button's
shared declaration invokes this service when measuring its label.
`ReadActivation(bounds, id, enabled)` returns an `Activation` sample containing
activated, pressed, hovered, and focused flags. Native Go binds this service
to the owning Runtime. Button's shared declaration applies its disabled,
loading, and explicit-state rules to the sample. Retained painting resolves
the stored sample without calling this input service again.
Button content uses the shared `.kry` drawing path. Loading replaces both label
and icon; disclosure takes precedence over a texture or built-in icon. Content
offsets and icon sizing use logical pixels, scaled into physical drawing bounds.
Material layers also use shared `.kry` assembly, preserving gradient endpoints,
joined-surface bounds, and content displacement across native renderers.
For example, `Button(ButtonProps{Label: "Save"})` uses the shared `.kry`
measurement and KSS-resolved font defaults. Visual styling belongs in style
packs selected by `class_name`, state, tone, emphasis, and size; there is no
per-button font/style escape hatch, string overload, fixed-size shorthand, or
label-derived identity. Supply `ID` when stable explicit identity is needed.

**Returns:** 1 if clicked, 0 otherwise. In retained `BeginTree`/`EndTree`
declarations, reconciliation is atomic: a synchronous activation observed while
the tree is being declared cannot replace the previous complete tree with a
partial declaration. Retained hosts should consume `EVENT_CLICK` from
`NextEvent` after `EndTree` when wiring purely declarative state updates.

Button blocks support caller-owned child content. Logical style padding is
scaled into the content area. Unpositioned children fill missing dimensions and
are centered; explicitly positioned children retain their placement. Oversized
padding leaves an empty area, and negative padding acts as zero.

#### `Link`

```c
typedef struct {
    Rectangle bounds;
    int class_name;
    const char *text;
    const char *link;
    int focus_id;
    int disabled;
} LinkProps;
```

#### `Link`

Draw and handle a text link using the KSS-resolved `Link` foreground and font.

```c
int Link(LinkProps link);
```

---

### Text Input

#### `TextInput`

```c
typedef struct {
    Rectangle bounds;
    const char *text;
    int cursor_position;
    int focused;
    int cursor_visible;
    int focus_id;
    int class_name;
} TextInputProps;
```

Web `TextField`/`TextArea` bindings update their `.kry` state through the same
editing decisions as queued input. Cursor positions use UTF-8 byte offsets;
the DOM adapter translates browser UTF-16 selections. Preedit remains provisional
until composition commits, and blur cancels it. Redraw preserves the active DOM
editor. Declared fixed char state-buffer sizes in `sizeof(buffer)` are emitted
as numeric web capacities.

#### `TextField`

```c
typedef struct {
    Rectangle bounds;
    char *text;
    size_t text_size;
    int *cursor_position;
    int *focused;
    int max_codepoints;
    int focus_id;
    int *commit_pressed;
    int secure;
    int read_only;
    int class_name;
} TextFieldProps;
```

#### `TextField`

```c
int TextField(TextFieldProps field);
```

---

### Navigation

#### Navigation Barigation

```c
typedef struct {
    int route;
    const char *label;
    Texture2D icon;
    int icon_type;
    int active;
    int disabled;
} NavigationBarItem;

typedef struct {
    int view_width;
    int view_height;
    int class_name;
    int count;
    const NavigationBarItem *items;
    int height;
} NavigationBarProps;

NavigationBarResult NavigationBar(NavigationBarProps nav);
```

#### Toolbar

```c
typedef struct {
    int id;
    int x;
    int y;
    int width;
    int height;
    int draw_menu;
    const char **options;
    int option_count;
    int *selected_index;
    // ... more fields
} ToolbarProps;

ToolbarResult Toolbar(ToolbarProps toolbar);
```

#### Tab Bar

```c
typedef struct {
    const char *label;
    Texture2D icon;
    int disabled;
    int closeable;
} Tab;

typedef struct {
    Rectangle bounds;
    const Tab *tabs;
    int count;
    int selected_index;
    int min_tab_width;
    int max_tab_width;
    int *scroll_offset;
    int focus_selected;
} TabBarProps;

int TabBar(TabBarProps bar);
```

Use `TabBar` as the single canonical tab header. Store a non-negative returned
index into caller-owned selection state, then draw the selected tab's content
with ordinary conditionals. There is no separate Begin/End tab scope surface.

#### Dropdown

`Dropdown` is the props-based implementation and the only public option
selection widget. Use `DropdownProps.options` for plain labels and
`DropdownProps.items` for rich items. Opening or reselecting the current option
returns no change. Native C and Go share popup placement, disabled-row
navigation, dismissal, and row-based scrolling policy.

```c
int Dropdown(DropdownProps dropdown);
void Overlays(void);
```

In native C and Go, a focused, enabled `Dropdown` with a positive ID
opens with Enter, keypad Enter, Space, or Down. The opening key does not move
the highlight or commit a selection. Focused controls display a focus indicator;
disabled controls neither open from the keyboard nor display that indicator.
The dropdown trigger uses the neutral soft Button style unchanged, including
its state colors, material, and motion. Popup panels adapt that style with the
shared glass material. Selected dark rows use accent filled Button styling;
light rows use accent soft styling on a flat surface. Hover remains distinct
from selection. Text, icons, padding, and gaps use resolved Button metrics.
See [Widget styling](WIDGET_STYLING.md) for the complete role mapping.
Labels are clipped before the chevron and selection indicator. Theme color
changes apply to both dropdowns and buttons without a separate dropdown palette.
`DropdownProps.items` accepts `DropdownOption` records with `label`,
`icon_type`, `disabled`, and `separator_before` (with `option_count` in C);
Go uses `Items`. When supplied, these replace the plain string options.
Disabled rows cannot be clicked or
committed and keyboard navigation skips them; separators precede their row.
An open `Dropdown` supports Up/Down to move the
highlight, Home/End to jump to the first/last option, and Enter to commit and
close. Escape closes without committing the highlight. Navigation clamps to
the current option list. Both native runtimes constrain the popup vertically,
flip it above the control when needed, and scroll the highlighted row into view.
Popup width is capped to the window width and its horizontal position is shifted
inside the window; the owner button keeps its declared bounds. Painting and
input capture use the same shifted rectangle.
Go uses the shared scroll container for wheel input, scrollbar dragging and
row clipping, painting only visible rows. These behaviors do not yet provide general keyboard-focus
isolation for arbitrary popup children or complete ImGui navigation semantics.

For caller-defined contents, use the canonical `Popup` block:

```kry
Popup tools: {
    bounds = {170, 50, 240, 140}
    id = 4201
    open = &tools_open

    Column content: {
        Text { text = "Tools" }
        Button { label = "Apply" }
    }
}
```

`Popup` submits children only while the caller-owned `open` value is true. Its
children use the same overlay painting, clipping, nested layout and input
capture as composed popups. Escape, a pointer release outside the popup,
disabling it, or omitting its owner on a later frame closes it. Outside releases
are consumed so the background widget underneath is not activated. Explicit app
dismissal uses the same caller-owned state: set the `open` value to false.
Tooltip, modal, and context behavior are selected by popup flags.

With `PopupTooltip`, `open` is optional and visibility is derived from pointer
hover over `trigger`. Clipped, disabled or input-captured triggers do not show
the tooltip. The tooltip uses the same arbitrary-child paint and layout
scope, but does not enter popup input capture: controls beneath it continue to
receive input. Tooltip bounds and child positions are explicit, keeping sizing
and placement in the retained layout rather than creating a second text-only
renderer. Tooltips without caller-owned `open` state are hover-derived only.

With `PopupModal`, the same scope accepts arbitrary native children while
drawing a full-view dimming backdrop and owning pointer and keyboard input over
the background. Pointer releases outside the panel are blocked without closing
it; Escape, clearing the caller-owned `open` value, disabling it, or omitting
its owner closes it. Modal and tooltip flags are mutually exclusive.

Nested popup ownership applies to ongoing scrollbar drags as well as clicks.
Closing the owning popup cancels its drag; removing a scrollbar by shrinking its
content also releases that drag. Tab and Shift+Tab wrap within eligible controls
of the active popup branch.

With `PopupContext`, a right-button release inside `trigger` sets the
caller-owned `open` value and enters the same arbitrary-child popup scope.
The caller supplies the panel position in `bounds`; this keeps placement stable
after the pointer moves. Outside left-button dismissal, Escape, explicit close,
disabled state, and missing-owner cleanup use the ordinary popup lifecycle.
Context, modal, and tooltip presentation flags are mutually exclusive.

#### Segmented Control

Responsive choice control for mutually exclusive compact options. It measures
and draws through the same input, focus, text, and button primitives as other
Kryon controls, so it works across backends and can wrap onto multiple rows on
narrow screens.

```c
typedef struct {
    const char *label;
    int disabled;
} SegmentOption;

typedef struct {
    Rectangle bounds;
    int id;
    const SegmentOption *options;
    int option_count;
    int *selected_index;
    int wrap;
} SegmentedControlProps;

int GetSegmentedControlHeight(SegmentedControlProps control);
SegmentedControlResult SegmentedControl(SegmentedControlProps control);
```

### Modals

#### `Modal`

Adaptive action modal for a title, message, optional close icon, and action
buttons.

```c
typedef struct {
    const char *label;
    ButtonTone tone;
    ButtonEmphasis emphasis;
    int disabled;
} ModalAction;

typedef struct {
    const char *title;
    const char *message;
    const ModalAction *actions;
    int action_count;
    Texture2D close_icon;
    int max_width;
} ModalProps;

int Modal(ModalProps modal);
```

**Returns:** `-1` when the close icon is clicked, `0` for no action, or the
1-based action index.

The modal width is capped to the viewport and `max_width`, body text reflows to
the content width, and action buttons measure their labels. Button text is fitted
inside the button, and the action row wraps to multiple rows when labels do not
fit. Backdrop clicks are blocked automatically for the current frame and the next
frame.

### Scrolling

Public scrolling is the canonical `.kry` `Scroll` block. Native scroll
container, page, and scaffold helpers remain internal host support for
generated/runtime code and are not public widget names.

#### Node Measurement

```c
int GetNodeId(const TreeNode *node);
int GetNodeKind(const TreeNode *node);
const char *GetNodeKindName(int kind);
Rectangle GetNodeBounds(const TreeNode *node);
int GetNodeParent(const TreeNode *node);
int GetNodeFirstChild(const TreeNode *node);
int GetNodeNextSibling(const TreeNode *node);
int GetNodeHeightById(int id);
```

`GetNodeKindName` returns clean inspection names such as `"Button"`,
`"TextField"`, and `"Image"`; retained numeric kind values and retained node
payloads are internal.

---

### Controls

#### Sliders

```c
int Slider(SliderProps slider);
```

#### Toggle Switch

```c
int Toggle(ToggleProps toggle);
```

#### Checkbox

```c
int Checkbox(CheckboxProps checkbox);
```

---

### Layout Components

## App Framework Helpers

### Route Stack And Shell Layout

```c
typedef struct KryRouteStack {
    int *routes;
    int count;
    int capacity;
    int root_route;
} KryRouteStack;

void KryRouteStackInit(KryRouteStack *stack, int *routes, int capacity,
                       int root_route);
int KryRouteStackCurrent(const KryRouteStack *stack);
int KryRouteStackPush(KryRouteStack *stack, int route);
int KryRouteStackPop(KryRouteStack *stack);
void KryRouteStackReset(KryRouteStack *stack, int root_route);

KryAppShellLayout KryAppShellMeasure(KryAppShellLayoutSpec spec);
```

These helpers cover app-neutral navigation state: a bounded route history and
a safe-area-aware shell measurement for navigation barigation plus optional
wide-screen sidebars.

### Capabilities And Settings

```c
int KryCapabilitiesHas(int capabilities, KryCapability capability);
const char *KryCapabilityName(KryCapability capability);
Rectangle KrySafeContentRect(KryViewportSpec spec);

int KryClampInt(int value, int min_value, int max_value);
int KryNormalizeIntSetting(KryIntSetting setting);
int KryNormalizeBoolSetting(KryBoolSetting setting);
```

Capabilities give `.kry` apps a shared vocabulary for platform features such
as file picking, secure storage, biometrics, notifications, wakelock, and
clipboard support. Setting helpers normalize common persisted state values
before an app applies or saves them.

---

## Input Handling

The Linux X11 libdraw host supplies physical key presses/releases, modifier
shortcuts and focus state while retaining plan9port's composed-text stream.
`GetWindowHandle()` returns the X11 window ID and `IsWindowFocused()` reports its
focus. Native Plan 9 retains rune input; see [libdraw input](libdraw-input.md)
for host-specific behavior and verification.

### Unicode editing

Native C and Go editors keep byte-offset cursors and selections, but normalize
committed-text positions to Unicode 17 extended grapheme boundaries. Left/right,
Backspace/Delete, selection endpoints, and click placement treat combining
sequences, joined emoji, flags, and Indic conjuncts as whole characters. CRLF is
one cursor step. C and Go TextArea soft wrapping does not split a grapheme, and long
visible lines are no longer truncated at 1023 bytes.

Buffer capacities remain bytes and `max_codepoints` remains a scalar-value
limit, not a grapheme limit. IME preedit offsets retain their codepoint contract;
platforms can position the composing caret inside a not-yet-committed cluster.
This does not add bidirectional cursor ordering, Unicode word segmentation,
font shaping, or native Go visual-row wrapping.

### Text composition

TextArea visual rows preserve source whitespace and byte offsets in both native
runtimes. CRLF is one logical break; empty and trailing lines remain addressable.
A caret at a soft-wrap boundary belongs to the next row. Pointer placement uses
the clicked row, its font measurements and the current vertical scroll offset.

Platform adapters submit UTF-8 IME preedit and commit events through the shared
input front-end. C retained `TextField` and `TextArea` consume these events;
commits use the editor's insertion rules and cannot mutate a read-only buffer.
C retained `TextField` displays preedit separately from committed text; C
TextArea preedit rendering and immediate C composition remain incomplete.

```c
SubmitTextComposition(KRY_TEXT_COMPOSITION_UPDATE, "nihon", 5, 0);
SubmitTextComposition(KRY_TEXT_COMPOSITION_COMMIT, "日本", 2, 0);

KryTextCompositionEvent event;
while (PollTextComposition(&event)) {
    /* Custom editors may consume the same backend-neutral event stream. */
}
```

`ClearTextComposition` discards pending composition events, including commits;
it does not cancel preedit already stored in an editor. Submit a `CANCEL` event
to cancel that preedit. Android `InputConnection` and the DOM backend feed this
C API directly.

Native Go exposes the same phase names and `SubmitTextComposition`,
`PollTextComposition`, and `ClearTextComposition`, both as package functions
and Runtime methods. Its queue belongs to the runtime. Go TextField/TextArea
display preedit without changing the caller buffer, apply commits with UTF-8
cursor/length handling, and discard preedit on focus loss, removal, disabling
or popup capture. Unconsumed Go events expire at frame end. Both queues accept
up to 16 events with at most 255 text bytes per event; submission returns 1 on
success and 0 for an invalid phase or full queue. On Linux/X11, native Go receives IBus preedit and commits through the OS
input context. Candidate geometry follows the wrapped/scrolled caret. Changing
fields resets the context; disabled, read-only and secure editors do not activate
IBus. Other native window platforms and detailed preedit cursor/selection
rendering remain incomplete.

Native Go `TextProps.Selectable` supports dragging a byte range across wrapped
lines, Ctrl+A, and Ctrl+C. Endpoints follow grapheme boundaries; copying retains
original source whitespace. Clicking elsewhere or focusing an editor releases
selection ownership. Popup capture prevents background copying.
See [native input verification](NATIVE_INPUT.md).

Native Go `TextFieldProps.ReadOnly` and `TextAreaProps.ReadOnly` mirror C's
`read_only` property. Read-only editors remain focusable and allow selection,
navigation and copying, but reject typing, cut/paste mutations, deletion and
IME commits. Switching a Go editor to read-only cancels its preedit; its frame
operation carries `ReadOnly` so rendering suppresses the insertion caret without
removing focus styling. Re-enabling editing does not replay rejected input.

### Retained accessibility

`GetAccessibilitySnapshot` projects the committed retained UI tree into
backend-neutral roles, labels, bounds, focus, disabled, and checked state.
Nodes also include the app's `focus_id`, committed editor `value`, `read_only`,
`secure`, and `multiline` metadata, plus `selection_anchor` and `selection_cursor`
as UTF-8 byte offsets in committed text (Go: `SelectionAnchor`, `SelectionCursor`).
Secure fields expose an empty value, zero selection offsets, and no
password-derived label. TextArea placeholders provide a fallback label. Button
descendant text supplies the button name without a duplicate text announcement;
checkbox flag masks and disabled scopes are reflected in the snapshot.
`SetAccessibilitySink` installs a host callback invoked after every retained
frame, including frames without controls (C retains its implicit screen node).
The DOM backend
additionally publishes the existing roles, labels, and states as ARIA nodes.

```c
AccessibilityNode nodes[64];
int count = GetAccessibilitySnapshot(nodes, 64);
```

The C function returns the total node count even with a smaller output buffer;
pass `NULL, 0` to query capacity. Strings are borrowed from the current tree or
editor buffer and must be consumed before either changes.

Native Go provides `GetAccessibilitySnapshot() []AccessibilityNode` and
`SetAccessibilitySink(AccessibilitySink)` as package, Runtime, and Host methods.
Call the snapshot API after `EndFrame` on the UI thread. Returned slices belong
to the caller; callbacks run synchronously at frame end, including empty frames.
Setting a nil sink removes the callback. Go snapshots cover ordinary/composed
buttons, text editors, checkboxes, toggles, text, images, groups, and tables.
Editor values exclude uncommitted IME preedit.

Each node also carries `generation` and an `actions` bitmask. Native C and Go
provide `QueueAccessibilityAction(focus_id, generation, action)` (Go uses
`int32`, `uint64`, and `AccessibilityAction` and returns `bool`; C returns an
acceptance `int`). Go exposes package, Runtime, and Host methods. Supported
payload-free actions are `AccessibilityActionFocus` and `AccessibilityActionActivate`.
Buttons, clickable cards, checkboxes, and toggles support both; text editors
support focus, including secure/read-only editors. Editors also advertise
`AccessibilityActionSetValue` and `AccessibilityActionSetSelection`, delivered
through their dedicated payload APIs below. Read-only editors omit SetValue.
Unsupported controls,
disabled/loading controls, and controls behind a capturing popup expose no
actions. Applications must give actionable controls unique, stable positive
focus IDs.

Call on the UI thread between frames, including from the accessibility sink,
with the generation from the current completed snapshot. Requests with stale
generations, unknown/ambiguous IDs, or unsupported actions are rejected. At most
32 distinct requests may be pending; repeated requests for the same target and
action coalesce, replacing the older request and moving it to the queue's end.
Accepted requests are delivered once during the next frame: focus/activation
at widget declaration, text edits at editor input handling. Targets are visited
in declaration order; text requests for a target retain queue order.
The host must schedule that frame. Acceptance does not promise
delivery: current disabled/loading state, widget kind, and popup capture are
checked again. Missing or newly ineligible targets are discarded at frame end,
never replayed when they reappear. Activation preserves widget return values,
checkbox flag-mask updates, and normal application handlers; it does not inject
pointer coordinates or an Enter key.

`QueueAccessibilityValue(focus_id, generation, value)` requests an atomic editor
replacement. C accepts a NUL-terminated `const char *`; Go accepts a `string`.
Both own a copy, reject malformed UTF-8 and values over 65,536 bytes, and reject
ASCII control characters (TextArea allows tab, LF, and CR, preserving CRLF). Embedded
NUL is not representable in C and is rejected in Go. The live buffer must fit
the entire value plus its NUL terminator, and `max_codepoints` is checked against
Unicode scalar count. A failed live limit check leaves text, selection, and
focus unchanged. A successful replacement focuses the editor and collapses its
selection at the end; identical text does not report a text change.

`QueueAccessibilitySelection(focus_id, generation, anchor, cursor)` accepts
UTF-8 byte offsets (`int` in C, `int32` in Go). Offsets clamp to the committed
text and round down to grapheme boundaries; reversed selections are preserved.
It focuses the editor and works with read-only and secure fields. Both APIs
are available as Go package, Runtime, and Host methods, return queue acceptance
with the same generation/identity contract, and recheck live read-only/disabled
state, widget kind, and popup ownership before delivery. Applying either action
cancels preedit and queued composition input. C emits the normal text,
selection, and composition events; Go TextArea returns its usual changed flag.
Owned payload buffers are cleared when superseded, discarded, or delivered.

These are flat host snapshots, not stable OS object trees. Native screen-reader
adapters and complete composite-control coverage
remain work in progress.

### Input Capture

```c
int InputCapturesClick(Vector2 point);
int ui_base_input_captures_click(Vector2 point, int include_pointer_drag);
void SetModalCapture(Rectangle bounds);
```

`SetModalCapture` defines the active modal rectangle for the current frame and the
next frame. While a modal carried from the previous frame has not registered its current
bounds yet, all pointer input is captured. After registration, clicks outside the bounds
are captured while controls inside the modal remain usable.

Built-in modal helpers (`Modal`)
register their bounds automatically.

Applications should use `Modal` for standard title/message/action dialogs
and `Modal` for modal content instead of manually drawing a backdrop
and calling `SetModalCapture`. Manual capture remains available for
specialized overlays, but the helpers keep modal bounds, backdrop, and input
capture consistent across projects.

### Input Blocking

```c
void ui_set_input_blocked(int blocked);
```

### Hover Effects

```c
int HoverEffectsEnabled(void);
void SetTransitionCuesEnabled(int enabled);
int TransitionCuesEnabled(void);
```

`SetTransitionCuesEnabled` controls the extra subtle hover and selected-state cues
used by built-in controls. Leave it disabled when an application has transitions
turned off.

## Focus System

Keyboard navigation and focus management.

### Focus Registration

```c
int RegisterFocus(int id, Rectangle bounds);
```

**Returns:** 1 if this element has focus

### Focus State

```c
int IsFocusActive(int id);
int IsFocusActivatePressed(int id);
```

### Focus Control

```c
void SetFocus(int id);
void ClearFocus(void);
void SetFocusTextInputActive(int active);
```

### Focus Indicator

```c
void Focus(Rectangle bounds);
```

---

## Memory Diagnostics

```c
int KryonMemDebugEnabled(void);
void KryonMemReport(const char *tag);
```

Both are no-ops unless `KRYON_MEM_DEBUG` is set in the environment, so apps
can call them at interesting points unconditionally. `KryonMemReport` prints
the process RSS/high-water marks (Linux) and the glibc allocator arena
breakdown to stderr. `TextFontMemoryReport` (Text section) reports per-font
rasterization stats under the same switch.

## Utility Functions

### Styled Surfaces

Buttons accept `StyleTypeface` with `typeface = "semibold"` (or another
registered typeface name) in any `ControlStyle` entry. The shared `.kry`
policy owns inheritance and state selection; native C and Go use the selected
face for both natural-width measurement and painting. Typeface changes are
discrete, like font size, rather than interpolated. An absent field inherits;
an explicitly empty name restores the host's default/current face. Unknown
names fall back to that face. Large dark buttons default to `semibold`;
other sizes and the light theme retain their existing face.

`Surface(Rectangle bounds, Style style)` paints a non-interactive rounded
surface in the C and native Go runtimes. Set `StyleBackground`, `StyleBorder`,
`StyleRadius`, `StyleBorderWidth`, and `StyleOpacity` in `style.fields` to
override those values. An explicitly flagged zero remains zero, including
transparent colors, square corners, no border, and zero opacity.

Set `StyleBackgroundEnd` and `background_end` to fill from `background` at
the top to `background_end` at the bottom. The gradient follows the rounded
clip, interpolates RGBA channels, and respects `opacity`. A transparent
endpoint is valid; absence is determined only by the field flag. Use the same
color at both ends to replace an inherited gradient with a solid fill.
Buttons accept this same field in their `ControlStyle` entries; it replaces
the face fill while retaining the material rim, lighting, and interaction
effects. Endpoints inherited from `normal` can be overridden per state.
For automatic button states, custom gradients fade with the same hover,
press, and focus tracks as the material. A state without a custom endpoint
uses its live material fill throughout the transition, including on exit;
it is not treated as a transparent endpoint. Explicit states remain immediate.

Modern buttons accept `StyleMaterial` with `material = MaterialFlat` to use an
unshaded fill, border, and a simple inward focus edge. Flat material is the
cheap default used by the shipped Material style pack: it has no Lightfield
glow, bevel, contact shadow, or elevation offset; labels, icons, loading
indicators, activation, and state-color transitions remain available.
Material selection is discrete at the resolved state, like layout metrics;
colors and custom gradient endpoints still interpolate. Set `StyleMaterial`
and `material = MaterialLightfield` only for styles that explicitly opt into
the premium Lightfield treatment. The selector and geometry live in
`runtime/surface.kry`, shared by C and Go.

Without overrides, the surface uses the active theme's surface color, medium
radius, border width, and full opacity; its border is transparent. Radius and
border width are logical pixels. The shape and inset border coverage are
defined in `runtime/surface.kry`. `Surface` defaults to `MaterialFlat`; set
`StyleMaterial` and `material = MaterialLightfield` to opt into the shared
Lightfield shading and depth. A surface has no button input state, so it does
not acquire hover, focus, or press behavior. Custom gradient endpoints work
with either material.

```c
Style panel = {0};
panel.fields = StyleRadius;
panel.radius = 12;
Surface((Rectangle){24, 74, 720, 920}, panel);
```

## Button Properties

```c
typedef enum {
    ButtonToneNeutral,
    ButtonToneAccent,
    ButtonToneDanger,
    ButtonToneSuccess,
    ButtonToneWarning
} ButtonTone;

typedef enum {
    ButtonEmphasisFilled,
    ButtonEmphasisSoft,
    ButtonEmphasisOutline,
    ButtonEmphasisGhost,
    ButtonEmphasisLink
} ButtonEmphasis;

typedef enum {
    ButtonStateAuto,
    ButtonStateNormal,
    ButtonStateHover,
    ButtonStatePressed,
    ButtonStateFocus,
    ButtonStateDisabled,
    ButtonStateLoading,
    ButtonStateSelected
} ButtonState;
```

The zero-value button is the default neutral, filled, medium button. Applications
set one `Theme` for every widget and vary buttons with semantic properties;
there are no named button-style presets.

`ButtonStateAuto` follows live interaction. An explicit state fixes the visual
preview, including hover, press, and focus motion; activation does not add a
second visual state. Enabled previews still return activation events. Disabled
and loading states remain non-interactive.

Standard-size buttons use a softer resting and hover silhouette, midway between
the theme's medium and large radii. Press, focus, disabled, and loading states
use the medium radius; small and large size variants keep that compact radius.
Resting outlines retain their large-radius treatment. Shared style transitions
interpolate state changes, and an explicit `StyleRadius` overrides these defaults.

The default material includes a restrained contact shadow, an inset rim, and
light that fades inward from the rounded edge. This inner light is clipped to
the face and leaves the center clear; hover strengthens it and press reduces
it. Ghost and link controls retain a softer treatment. Geometry and falloff
are shared through `runtime/surface.kry` rather than separate renderer effects.
`SetFancyEffectsEnabled(0)` suppresses outer halos and inward glow while
preserving the selected material, gradient fills, contact and inset shadows,
sharp edges, and interaction transitions. Apps can apply this preference when
selecting a style without flattening that style's buttons.
In dark surroundings, focus reduces face whitening and deepens the material
while retaining a bright rim. That absorption fades with the focus track;
hover and press take precedence over it.
The dark focus indicator pairs a crisp focus edge with chromatic light that
fades inward over 12 logical pixels. The soft focus light is clipped to the
rounded face, rather than spreading fog outside the button; its center stays
clear. A soft crown reflection and lower interior light give the resting face
depth without covering its label.
In light surroundings, hovering a pastel face adds light instead of mixing in
its darker semantic color. Pressing removes elevation while preserving the
material's emphasis: outlined and link buttons keep a faint tint rather than
becoming filled buttons, and neutral soft buttons retain their surface color.
Press also introduces a soft upper inset shadow, clipped inside the rounded
face and fading toward the clear center. Its strength follows the press track,
including interrupted presses and releases. Disabled and transparent faces
do not acquire this shadow.

Automatic buttons animate hover and focus over `transition_normal_ms` (140 ms
by default), and press over `transition_fast_ms` (80 ms). The shared `.kry`
policy uses cubic ease-out, retargeting from the current value when an
interaction reverses. Focus changes the resting fill, text, border, and focus-edge colors;
hover and press take precedence while active. Focus rings fade on the same
focus track. Custom focus-edge colors (including their alpha) also blend through
hover, press, focus entry, and focus exit rather than switching at the input
boundary. Setting either duration to zero makes that transition immediate.
In light surroundings, default soft, ghost, and link focus edges use 18% of
the theme focus alpha; success buttons use the success hue with that same
alpha policy. Filled and outline edges retain full focus alpha. Dark
surroundings use luminous semantic hues for danger, success, and warning,
preserving the theme focus alpha; neutral and accent keep the theme focus color.
The default light palette uses a translucent violet-blue focus color. Pale
surfaces draw one focus edge and soften the resting border beneath it, avoiding
stacked strong outlines. A transparent focus leaves the resting border unchanged.
An explicit `StyleFocus` replaces
these defaults unchanged, including a transparent value.
Explicit `ButtonState` values snap to the requested state for previews.
Custom radius, border width, opacity, and content offset follow these same
tracks and state precedence. Their settled values are exact, including
explicitly flagged zeros. These paint transitions do not move or resize the
button's hit bounds; layout metrics such as padding and font size are not
interpolated.
Style-property interpolation lives in `runtime/style.kry`; gradient-state
assembly and interaction timing live in `runtime/surface.kry`. Both native
hosts call these shared operations rather than maintaining their own lists
of animated properties or gradient-presence rules. Disabled and loading
controls immediately clear their interaction tracks.

Loading replaces the label with a centered ring rotating at 240 degrees per
second (a continuous 1.5-second cycle). In light surroundings it has a
270-degree arc, icon-size diameter, and two-logical-pixel stroke. In dark
surroundings it has a 315-degree arc, icon-size-plus-two diameter, 2.5-pixel
stroke, and a 110-degree phase offset. Both use a faint track, a fading arc,
and a rounded leading highlight whose paint bounds include the complete cap.
Accent indicators use the theme accent rather than the link color. Geometry,
light falloff, and wrapping are defined in `runtime/surface.kry`; highlights
preserve the resolved foreground opacity. Loading buttons do not activate.

## Theme IDs

```c
typedef enum {
    THEME_SKY,
    THEME_OCEAN,
    THEME_FOREST,
    THEME_SUNSET,
    THEME_LAVENDER,
    THEME_CHERRY,
    THEME_DAWN,
    THEME_SAGE,
    THEME_INK,
    THEME_MONO,
    THEME_MINT,
    THEME_COBALT
} ThemeId;
```

---

## Text Sizes

```c
#define Text8 8
#define Text12 12
#define Text14 14
#define Text16 16
#define Text18 18
#define Text20 20
#define Text24 24
#define Text32 32
#define Text48 48
#define TextBaseSize 16
```

---

## Pragmatic Tk Toolkit

`ui_tk.h` adds Kryon's Tk-replacement layer. The rule is one simple way to use
each widget: prepare a plain struct, keep state in caller variables, and call the
matching immediate-mode function each frame.

```c
int selected = 0;
Dropdown((DropdownProps){
    .bounds = (Rectangle){40, 40, 220, 32},
    .id = 10,
    .options = items,
    .option_count = item_count,
    .selected_index = &selected,
});
```

Collection widgets use `scroll_offset` as a caller-owned pixel offset. In
native `.kry` code, canvas content uses a lexical `Canvas` block:

```kry
Canvas canvas: {
    bounds = {40, 50, 620, 420}
    scroll_x = &scroll_x
    scroll_y = &scroll_y
    zoom = &zoom
    CanvasGrid((Rectangle){40, 50, 620, 420}, 24, GetThemeButton())
    Circle((int)canvas.world.x, (int)canvas.world.y, 4, GetThemeSurface())
}
```

The compiler lowers the block to host canvas-scope support. Scroll and zoom are
applied to canvas drawing and hit coordinates. C and Go restore the parent camera
and clip after each nested Canvas, including compiler-generated cleanup on
return, break and continue. A Canvas without explicit scroll or zoom inherits
the active camera; an explicit camera temporarily replaces it.

Text fields and text areas use the shared `EditText` core. Ctrl/Cmd+C copies
the field buffer, Ctrl/Cmd+X cuts it, and Ctrl/Cmd+V pastes clipboard text
through the existing codepoint filter.

Feature families:

- Geometry: `Rectangle`, `Grid`, `Column`, `Row`, `Stack`, `Separator`
- Menus: `Menu` with bar, popup, or context behavior selected by `MenuProps`.
- Basic controls: `Radio`, `Progress`, `Spinbox`, `Dropdown`, `SegmentedControl`, `Fieldset`, `Image`
- Collections: `ListBox`, `TreeView`, `TableView`

Use `ListBox(ListBoxProps)` for selectable string lists. Multi-selection uses
`selected`, `selected_count`, and `anchor` on `ListBoxProps`. For arbitrary
scrolling child content, use the general scroll scope directly.
- Canvas: `Canvas`, `CanvasGrid`, `CanvasHitTest`
- Containers: `TabBar`, `PanedView`, `Collapsible`

`TableViewProps.header_height` controls header height, with a minimum/default
of 30 logical pixels. `header_angle` rotates header labels in degrees, clamped
to -89 through 89; zero keeps ordinary horizontal text. Sorting and body-row
hit testing use the configured height. Nonzero angles create slanted header
cells; labels are clipped to those cells, and sorting/resizing follow their
slanted boundaries. Go uses the corresponding
`HeaderHeight` and `HeaderAngle` fields.

A nonzero table `id`/`ID` participates in focus traversal. While focused, the
standard table body moves through visible columns with the arrow keys or
Tab/Shift+Tab,
activates the current cell with Enter or F2, clears selection with Escape, and
scrolls the selected row into view. Popup ownership and disabled state suppress
those keys.

Ctrl/Cmd+C copies the selected cell, row, or column; `copy_text`/`CopyText` can
override that value for an editable cell. Ctrl/Cmd+V reports the clipboard text
and target through `pasted_text`, `pasted_row`, and `pasted_column` (the matching
capitalized fields in Go). C exposes the pasted string from runtime-owned
clipboard storage, valid until the clipboard is changed again.

For interactive cell content in `.kry`, set `TableViewProps.custom_cells`,
draw `TableView`, then use a `TableCell` block for each custom cell:

```kry
TableCell action_cell: {
    table = table_props
    row = 0
    column = 0
    Button((ButtonProps){.bounds = action_cell, .label = "Open", .id = 40})
}
```

The block name binds the cell's full bounds. Child widgets should use those
coordinates. The scope clips drawing and input to the visible cell, respects
column order, visibility, scrolling and frozen rows, and inherits table
disabled state. The compiler closes the scope through lexical cleanup. In
custom-cell mode, body selection, activation and body keyboard handling belong
to the children; header sorting and resizing remain owned by the table. Keep
row entries for geometry even when their cell text arrays are empty.

Cell content may use `Row` or `Column` with the returned cell bounds and
zero-positioned child controls. Explicitly positioned children stay outside the
surrounding layout flow; ending the inner layout restores the outer cursor.

`CollapsibleProps.open` is a `bool*` in C and `Open *bool` in Go; `.kry`
callers should use boolean state. `Collapsible` returns whether that state
changed. Render child widgets conditionally on the open state. Set `tree` for
an unframed tree header, `depth` for 20-pixel-per-level header indentation,
`leaf` to show a non-expanding leaf marker, and `selected` for highlighting.
`disabled` prevents toggling and dims the label; `id` identifies the header.
Set optional `visible` / `Visible` state to show an ImGui-style close affordance.
When that state is false the header consumes no layout or input; closing sets it
false and returns changed without toggling `open`.
Children retain their own widget IDs and explicitly supplied bounds. Collapsing
a parent does not reset the caller's nested open state. Headers with a positive
ID participate in focus traversal: Left closes, Right opens, and Enter/Space
toggle the focused non-leaf header. In tree mode, Up/Down move through enabled
headers in the previous frame's visible order. Right on an open branch focuses
its first child; Left on a closed branch or leaf focuses its nearest ancestor.
Depth determines that hierarchy. Leaves and disabled content never expand;
disabled headers are skipped during directional traversal. Automatic child
indentation is not implemented.
- Dialogs/platform: `MessageDialog`, `ConfirmDialog`, `PromptDialog`, `ColorPicker`, accelerators, clipboard helpers
- Accessibility/debug: focus debug paint policy lives in `runtime/focus.kry` and the host renderer is internal.

Examples `09_geometry` through `18_accessibility` demonstrate these APIs.

---

## Integration Example

```c
#include "kryon.h"

int main(void) {
    // Initialize window with Raylib
    InitWindow(320, 560, "Kryon Demo");
    SetTargetFPS(60);

    // Configure UI theme
    SetCurrentTheme(THEME_SKY, 0);
    float dpi = 1.0f;  // Get from platform

    while (!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(BLACK);

        BeginInterfaceFrame(GetScreenWidth(), GetScreenHeight(), dpi);

        // Draw UI
        if (Button((ButtonProps){
                .bounds = {10, 10, 100, 36},
                .label = "Click Me",
                .id = 1,
        })) {
            // Button clicked
        }

        EndInterfaceFrame();
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
```

Button's public props contract is declared in `runtime/button_props.kry`. Runtime
generation produces `include/ui_button_props.generated.h` and
`go/kryon/button_props.go`; edit the declaration, then run `make generate-runtime`.
The C flags now use `bool`, matching their declared meaning and Go types. Rebuild
C consumers together with the library because the props record layout changed.
The label remains a borrowed C string and a native Go string.

Native props declarations are embedded in compiler binaries, so compiling an
application does not require locating the runtime source checkout. An explicit
application type declaration takes precedence over the native contract of the
same name. `make runtime-declarations-check` verifies that embedded sources and
generated public interfaces match their `.kry` sources.

`runtime/control_props.kry` owns the public control enums, `Style`, and
`ControlStyle`. `make generate-runtime` synchronizes their C and Go declarations
and the constants re-exported by the web runtime. Their public names, numeric
values, and Go constant types are preserved. The former internal `Policy*` enums
and `Field*` aliases are removed; shared declarations use the public names. Native
contract enum constants retain their Go enum type unless their source explicitly
casts the value to a scalar type, as the `uint32_t` Style flags do.

Input capture scopes use `PushInputCapture(bounds, allow_inside)` and
`PopInputCapture()`. Popping an empty stack is harmless. For an inert widget
preview, push a capture with `allow_inside = 0`, disable keyboard input with
`SetKeyboardInputEnabled(0)`, draw controls without focus IDs, then pop and
restore the previous keyboard setting. Outer captures remain in effect.

A `TitleBar` with `has_leading_action` uses a back arrow when no explicit
leading texture is supplied. Applications need no private back-icon texture.

### Style color variations

`RegisterStylePackVariant(id, source, label, colors, color_count)` registers a
named variation of a KSS source using `StyleColorToken { name, color }` values
(packed RGBA). It replaces declared color tokens before resolving rules, while
retaining selectors, states, dimensions, typefaces, and materials. The base
source and registered base pack remain unchanged. Unknown token names are
ignored so one semantic palette can color several packs. Invalid input leaves
an existing variant unchanged. Re-register an ID to update its palette, then
select it with `SetActiveStylePack`.

`SetStyleTheme(theme)` re-resolves every registered pack - built-ins, source
packs, and their declared variants - under a theme overlay ("light", "dark",
or empty), preserving the active pack; `kryon-preview --theme NAME` exposes
it for captures. Go also exposes `SetStyleTheme(theme)`: its source registry
retains sources and declared variant names, re-parses through generated KSS,
and publishes the new sheets only after all parses succeed. Failed imports
leave Go's registry, version, selection, and active theme unchanged. Directly
registered typed sheets and explicit color-substitution variants do not change.
`ClearStylePacks` resets the Go registry and its theme; selecting a theme before
registering sources applies it to subsequent registrations. Web hosts select
themes through their parse environments.

`KssEnvironmentWithNames` in `runtime/kss_parser.kry` interprets environment
names for all three generated targets. Names follow KSS's case-insensitive
matching. Unrecognized theme/contrast/density/pointer names select their
ordinary defaults; an unrecognized platform preserves the host's supplied
platform default (desktop for C/Go and web for the browser). Variant names
use the shared bounded UTF-8 name representation.

Go accepts `[]StyleColorToken` without a count. `ParseStyleVariants(source)`
reports the sheet's declared `@variant` names with labels, and
`ParseStyleSheetVariant(source, variant)` parses with one active;
`RegisterStylePackSource` registers each declaration as a selectable
`<pack>.<variant>` pack, matching the C host. Web callers pass a color-token
object as the second argument to `parseWebStyleSheet(source, colors)` and select
a variant through the environment's `variant` field; the web layer has no pack
registry, so variant selection happens at parse time. Built-in packs expose
their control colors as tokens; applications own mapping their theme palette to
those tokens.

### KSS language model

The KSS grammar exists once in `runtime/kss_parser.kry` and is lowered to the
active C and Go runtimes by the shared transpilers. The old JavaScript lowering
path is paused with JS/web. Hosts feed source
text and resolve imports; they never reimplement parsing:

- `@pack`, `@version 1`, `tokens { color|length|number|duration|material }`
  blocks, selectors (`Kind[attr=value].class:state`), and the closed property
  set behave as before, with diagnostics carrying file, line, and column.
- `@theme name { token: value; }` overlays existing tokens. The active theme
  (light, dark, or none) is part of the parse environment; overlay entries are
  validated even when their theme is inactive.
- `@env axis(value) { rules and token overlays }` guards rules and token
  overrides with closed, typed axes: `theme(light|dark)`,
  `contrast(normal|high)`, `density(compact|comfortable|touch)`,
  `pointer(mouse|touch|mixed)`, and
  `platform(desktop|android|web|plan9|terminal)`. Non-matching blocks are
  skipped; matching blocks contribute rules in document order.
- `@import <id>;` and `@import "file";` request sources by module id or path.
  Hosts resolve them (`RegisterStyleModule` feeds `@import <id>;` in C and Go;
  `registerWebStyleModule` on the web); the parser tracks deterministic
  depth-first order, import cycles, and per-file provenance.
- `@layer name;` selects a layer; `@layer a, b, c;` declares a total order for
  the remainder of the sheet. Built-in aliases (`reset`/`base`/`defaults`,
  `components`/`widgets`, `app`, `overrides`) keep their indices until a sheet
  declares its own order.
- `@variant name "Label" { token overlays and rules }` declares a selectable
  pack option in the sheet itself. Every declaration is recorded (name and
  label) even when inactive, so hosts can enumerate options; the parse
  environment names the active variant (C: `kss_parse_with_variant` or
  `KssSetVariant`; Go: `KssParser_KssSetVariant`; web: the environment's
  `variant` field). An active variant contributes rules in document order and
  applies token overlays with the variant origin. As with `@theme` and `@env`,
  blocks take effect from their position onward: declare variant overlays
  before the rules that reference their tokens, and declare variant rule
  blocks after the same-layer base rules they override. A sheet may repeat a
  variant declaration to split overlay and rule blocks this way, as the
  matched fixture does.
  `RegisterStylePackSource` registers each declared variant as a selectable
  pack under `<pack>.<variant>` carrying the declared label; activating it
  re-parses the source with that variant so base and variant rules resolve
  together. This is the intended home for Lightfield's glow treatment.
- Precedence for token values is imported tokens, then pack tokens, then the
  active `@theme` overlay, then matching `@env` overrides, then active
  `@variant` overlays and programmatic variant color substitution; scoped
  rules always resolve against the final table.

C hosts include `runtime/kss_parser.h` and drive `KssBegin`/`KssStep`
(`KssStatusRule` yields a rule, `KssStatusNeedImport` expects
`KssProvideImport` or `KssFailImport`); `kss_parse_string`,
`kss_parse_variant`, and `kss_parse_with_variant` remain as shims over that
loop, and `KssParseResult` reports the pack id, rule count, and declared
variant names with labels. Go hosts call
`ParseStyleSheet`, and `runtime/kss_parser.kry`'s generated module is the
single grammar implementation for every backend.

The web runtime is included: `parseWebStyleSheet(source, colors, environment)`
runs the generated module in declarative mode, where selectors, declaration
values, `@keyframes`, and `@media`/`@supports`/`@container` groups are lexed
as spans by the shared grammar and only interpreted as CSS by the web layer.
Theme overlays, environment blocks, imports, layers, and provenance behave
identically to the native runtimes; the hand-written web KSS parser is gone.

Functional pseudo arguments in web selector objects use the shared selector
lexer when matched or exported. CSS export preserves nested arguments such as
`:has(> Button:not(.quiet))`, including quoted parentheses in attributes.
Malformed pseudo strings in prebuilt selector objects throw an error instead
of being serialized as an escaped pseudo name. Parsed `WebStyleSelector.groups`
preserves each `:is`, `:where`, and `:not` list independently. Every group must
match; alternatives inside `:is`/`:where` use any-match, while `:not` requires
no matching alternative. CSS export preserves these group boundaries and names.
Prebuilt `matches` and `not` lists remain supported with their existing behavior.
Functional-selector specificity is still the existing KSS weighting; this group
representation does not establish CSS specificity conformance.

Parsed selectors also expose ordered `ids` and `attributes` lists. Every ID
condition must match the node's ID, name, or key, and every attribute condition
must hold, including multiple conditions with the same name. These lists are
authoritative for matching and export. The older `id`, `attrs`, and `attrOps`
fields retain the last value as a summary; prebuilt selectors without the new
lists continue to use those older fields. To edit a parsed selector's conditions,
edit the lists, rather than its summary fields.

Selector chains search complete ancestor or sibling alternatives. A nearer
candidate that satisfies one part does not prevent a farther candidate from
satisfying the whole chain. Child (`>`) and adjacent-sibling (`+`) relations
still require the immediate related node. Descendant and general-sibling (`~`)
relations may retry earlier ancestors/siblings. This behavior applies to style
resolution, traces, and Web Document selector queries.

Generated hosts can use `KssSelectorChainBegin` and `KssSelectorChainStep` with
host-managed frame stacks. Each step consumes the cursor node's parent and
previous-sibling indices (`-1` means missing), a simple-match result for an
unentered frame, and the relation byte. Pop discards the top frame; push replaces
it with `result.frame` and appends `result.next`; accept completes the search.
Hosts supply acyclic tree relationships and valid node indices. The driver adds
no fixed chain-length limit and does not expand the typed native selector API.

Relative `:has(...)` selectors use `KssRelativeSelectorPart` for their leading
relationship and `KssRelativeSelectorBegin` to anchor traversal to the subject.
Child or sibling prefixes constrain the first part of the relative chain, not
its final candidate: `:has(> .branch .leaf)` can match a leaf below a direct
branch child. Unprefixed arguments imply a descendant relationship. Ordinary
selectors reject leading combinators. Hosts enumerate frame candidates and
provide stable node identities; the generated driver checks the full anchored
chain. An anchored frame with `part == -1` checks anchor identity directly;
the host does not evaluate a simple selector for that frame. Nested `:has`
is rejected by the shared lexer, including inside `:is` or
`:not` within a `:has` argument. Quoted attribute values and comments containing
`:has(` remain literal content, not nested selectors.

`KssPseudoMatch` consumes a canonical pseudo name, its argument and functional
flag, and `KssStructuralFacts`. It evaluates basic structural and positional
pseudos, choosing forward/reverse and all-sibling/same-type positions in shared
policy. Results are zero for no match (including unknown or mismatched forms),
one for a match, and minus one to request relative `:has` traversal. Parsing
is the selector lexer's responsibility; positional argument validity is checked
by the shared formula parser.

`KssParseNth` returns `KssNthFormula { step, offset, ok }` for integers,
`odd`/`even`, and `An+B` formulas. Coefficient and offset magnitudes are bounded
by 2147483647. Zero and negative integers are valid formulas even though they
match no positive position. Unsupported `of` clauses, fractions, overflow, and
unterminated comments are invalid. `KssNthText` returns canonical CSS spelling
in a `KssName`, or empty text for invalid input. It normalizes case, whitespace,
and supported KSS comments; for example `odd// tail` becomes `2n+1`.
CSS export represents invalid positional predicates as `:not(*)`, preserving
runtime rejection even inside negation. `KssPseudoFunctionInfo` supplies shared
classification and axis/direction flags for canonical functional pseudo names.

`KssCSSPropertyName` resolves canonical KSS property names to CSS names,
including `foreground` → `color`, `radius` → `border-radius`, and `typeface`
→ `font-family`. It reuses declaration classification and returns empty text
for unknown names or fields requiring composite handling (`material`, offsets,
`icon-size`, and `background-end`). Custom property names are returned intact,
including names longer than the bounded parser-name buffer. Ordinary names
must use canonical lowercase spelling. Axis aliases return the first side;
callers still expand their second side when emitting complete rules.

`KssCSSNeedsPixels` owns the current numeric-unit policy for CSS names and the
DOM camel-case spellings used by the adapter. String values retain their own
units. Numeric opacity, line-height, font weight, and other existing unitless
fields do not receive `px`; other numeric values do, including custom values
under the existing adapter contract. This is not a general CSS value validator.
`KssCSSBorderShorthand` preserves the existing border-color alias decision:
internal whitespace in a trimmed value selects border shorthand. It recognizes
the adapter's ASCII and Unicode whitespace. `KssCSSExpandDeclaration` takes a property name, its nonempty value text,
and whether the style has offsets. It returns up to two property names, expands
paired padding/margin axes and content/icon fields, emits the existing WebKit
fallbacks for line-clamp and box-decoration-break, chooses border shorthand,
and defers offset/composed-transform output to shared effect recipes.

`KssCSSEffectFacts` contains already formatted background, background-end,
background-image, offset-x, offset-y, and transform strings; empty text means absent and `0px`
is present. `KssCSSHasOffsets` supplies the composition decision.
`KssCSSEffectAt` returns `KssCSSOutput` for slots 0–3: gradient, x offset,
y offset, composed transform. Empty and out-of-range slots have no name.
Emit each nonempty output as its name and the concatenation of `prefix`,
`first`, `separator`, `second`, and `suffix`. These are borrowed fragments,
without a fixed-size result buffer. Inputs must remain valid while emitting.
Emit effects after ordinary declarations in slot order. Ordinary rules and
keyframes share this expansion, including axis pairs, gradients, and combined
transforms. An explicit nonempty background-image (including `none`) suppresses
the generated gradient. A missing offset axis emits no reset; a custom value
can cascade into it, and the transform uses `0px` when none is available.

`KssCSSBorderDefault` returns `solid` when an authored border paint or nonzero
width requests a border, otherwise empty text. Emit that default before authored
declarations so explicit border styles and per-side styles override it. A zero
width alone does not request a default. Inline application, ordinary CSS rules,
and keyframes consume one declaration stream, including these shared decisions.
Inline application preserves the resolved object's declaration order rather
than imposing a separate property order. The host only translates CSS names
into DOM property access, writes custom properties through `setProperty`, clears
previously applied properties, and reapplies explicit mounted inline overrides.
This aligns emission of a given resolved declaration set; cross-rule alias and
shorthand cascade equivalence remains a separate conformance requirement.

### Style field presence and structural metrics

Widgets resolve product appearance from style rules with no hidden visual base.
A missing structural metric may use a documented layout fallback; explicit zero
must remain zero. Negative metrics follow the owning widget's policy. Page
layout's structural zeros and caller font requests are not default widget chrome.
For opacity, `StyleOpacityValue(fields, opacity)` resolves an absent declaration
to one while preserving an explicit zero. Renderers must not reinterpret that
explicit zero as a request for default paint. Native style conversion applies
this rule to composed surfaces too, including Material guide and modal panels.
## Native Go image assets

`Image(ImageProps)` and button image props render PNG, JPEG, and GIF assets from
filesystem paths in the native Go renderer. GIF uses its first frame. `Source`,
`Fit`, `Origin`, `Rotation`, style tint/opacity, rounded bounds, and surrounding
Scroll/Canvas clipping are preserved. Missing or invalid assets paint nothing;
the widget's KSS surface and semantic alt text still apply. The native cache
retains at most 64 decoded assets and 32 MiB of pixels and reloads changed files
when they are rendered. Individual decoded images are limited to 64 MiB.

### Unstyled editor rendering

Without a style pack, text fields and text areas keep content and interaction
without adding a white surface or colored border. Layout containers paint no
outline. Missing text/caret colors use a content fallback and a neutral selection
highlight; explicit transparent foreground/focus colors and opacity zero are
respected. Set appearance with KSS rules, including an explicit border width
when a border is wanted.
