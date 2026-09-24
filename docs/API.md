# Kryon API

Kryon is a collection of ordinary Ziran modules under `src/ui/`. An
application imports only the modules it needs. The modules listed in
[`modules.txt`](../src/ui/modules.txt) currently pass Kryon's C, C++, and Go
build; other `.zi` files are moved source awaiting conversion.

## Use a checked module

Pass the Kryon source directory as an ordinary module path:

```sh
../ziran/build/bin/ziran check --root app \
    --module-path ../kryon/src/ui app/main.zi
```

For example, a non-graphical program can import layout policy without
initializing a window:

```zi
#module "main"
#import "scroll"

Answer :: () -> i32 #export {
    return ScrollClamp(120, 80)
}
```

The same module can be built into C, C++, or Go, serialized as `.zir`, or
linked into a `.zib`. Ziran owns those commands and formats. Kryon has no
compiler or bundle loader. A `.zib` links Kryon code only when imported by the
program; effects that need a platform host must be supplied by declared host
interfaces.

## Current surfaces

The checked modules cover geometry, layout, styling, themes, accessibility,
focus, input, scroll, canvas transforms, popup policy, and several widget
measurement and paint decisions, including `Text(TextProps)`.
`ThemeDecisionFor(ThemePreference)` chooses the effective light or dark theme
from an observed system preference. `OrientationDecisionFor` returns a resize
or platform mode request from the current dimensions and host capabilities.
Both are checked values; applications apply the returned request through
their platform host.
`Swipe(SwipeSpec, SwipeGesture, SwipeFrame)` updates a gesture from raw pointer,
time, scale, and input ownership observations. The caller stores the returned
`SwipeResult.gesture` and applies its pointer claim, release, input capture,
and release consumption effects. Direction and progress decisions live in the
checked library; a host does not maintain a separate gesture state.
The checked `Button(ButtonProps)` path now submits a retained interactive node,
resolves KSS class and state rules, measures a label and optional `ImageProps`
asset or texture, and queues shape, clipped image, and clipped text paint.
The image uses the same fit rules as `Image(ImageProps)`; `icon_placement`
selects its side of the label. It consumes an activation routed from the previous
committed tree and returns `1` for that frame. The host supplies raw pointer
samples, glyph and raster callbacks, and asset dimensions when an image path is
used; it does not implement Button state policy.
This path currently covers text and image Buttons. Glyph and raw texture icons,
menu, split,
loading animation, material layers, keyboard focus, and immediate input still
need migration. `ButtonProps` has a portable string label and no C pointers;
menu items and mutable menu state need a separate value-based surface.

The checked `Checkbox(CheckboxProps)` accepts a portable `checked` value and
returns `{checked, changed}` after a retained pointer activation. It records
the resulting selected state on its tree node, resolves KSS box and label
roles, and queues the box, check mark, and clipped label. Hosts supply raw
pointer samples, glyph metrics, and raster callbacks; callers keep the value
returned by Checkbox for the next frame. The old pointer and flag fields are
not part of this checked API. Keyboard focus, wiring the tree's explicit
child clip into this widget, and native widget integration remain open.

The checked `Toggle(ToggleProps)` uses the same caller-owned value pattern and
returns `{value, changed}`. A plain switch and an Off/On label control both
resolve KSS roles in Ziran, submit retained interactive nodes, and queue
rounded shapes and clipped text. The tree records the proposed selected
state. Hosts only measure glyphs, rasterize, and provide raw pointer samples.
The old pointer field is outside the checked API. Keyboard focus, animated
transitions, material layers, and native widget integration remain open.

The checked `Radio(RadioProps)` returns the activated option id; the caller
chooses the checked option on the next frame. It resolves KSS Ring, Mark, and
Label roles, submits a retained interactive node with selected state, and
queues the ring, fill, hover layer, and clipped label. The host supplies raw
pointer samples, glyph metrics, and raster effects. Selection animation,
ripple, keyboard focus, and native widget integration remain open.

The retained input router now admits Slider nodes. `TreeTakeDragAt(index)`
returns the latest held pointer coordinate, a one-time start marker, and a
one-time release coordinate for the same stable node identity, including when
the pointer leaves the hit box. It also returns the pointer's grab offset
within the pressed node, which stays stable as that node moves. A disabled
node clears its pending drag.
The checked `Slider(SliderProps)` returns `{value, changed}` for a continuous
value, and `DiscreteSlider(DiscreteSliderProps)` returns the same shape with an
integer value. Both use caller owned values, horizontal or vertical layouts,
an optional label, KSS Track, Fill, Label, and Thumb roles, and retained drag
input. Applications can compose multiple controls using their own arrays and
`SliderCellBoundsFor()` without a fixed limit in the widget API. Angle editing,
step buttons, numeric text editing, limit labels, keyboard control, richer
materials, and native widget integration remain open.

The checked `Spinbox(SpinboxProps)` returns a caller owned integer value and
change flag. It submits decrement and increment Button children, applies step,
clamp, and wrap policy in Ziran, and renders the current integer as clipped
glyphs, including negative values. `value_text` overrides the numeric display
when an application supplies its own formatted text. The parent and value
surfaces resolve their KSS styles independently. Keyboard editing, locale
number formatting, and native host integration remain open.

The checked `Selectable(SelectableProps)` returns `{selected, changed}` from a
caller owned boolean. It uses retained pointer activation, KSS styling,
selected tree semantics, and clipped label paint. Keyboard activation and
native host integration remain open.

The checked `Fieldset(FieldsetProps)` submits a noninteractive labeled frame.
KSS controls its frame and title colors, while Kryon measures the title and
queues clipped text and shape paint. Child layout composition and native host
integration remain open.

The checked route helpers operate on caller owned `[]i32` slices and an
explicit occupied count. `RouteSanitizeSet` filters with an allowed route set;
`RouteSanitizeMask` accepts one decision per input slot for arbitrary app
predicates. Both compact unique routes and clear the unused tail. Move, search,
first-unused, and stack push/pop/reset helpers return portable values. The old
pointer records and C callback route ABI have been removed.

The checked `locale_policy.zi` module accepts caller owned slices of
`LocaleLanguage` and `LocaleEntry` records with portable strings. It selects
preferred codes from a language catalog, including normalized region tags and
base language fallback, and resolves active/base translation entries. The
caller owns catalog loading and adds English when `LocaleHasEnglish` is false.
`LocaleParseEntries` and `LocaleParseLanguages` parse UTF-8 catalog text into
caller owned `[]u8` storage and span tables. `LocaleParseResult.complete` is
false when either table or byte storage is too small. `LocaleSpanEquals`
compares a stored span with a string without a C string allocation.

[`modules.txt`](../src/ui/modules.txt) is the
precise build inventory. [`ziran_*_test.sh`](../tests/ziran_moved_modules_test.sh)
files show source, saved-IR, and bundle use of those modules.

`build/ziran/libkryon.a` is the current C archive; generated headers are in
`build/ziran/c/`. Portable host bindings are declared in
`include/kryon_portable_host.h` and built into `build/ziran/libkryon_host.a`.
`FramePacingBinding()` uses the platform's `SetTargetFPS`;
`RasterLineBinding()` accepts a line renderer for Bevel and Separator lines.
`RasterRoundedRectangleBinding()`,
`RasterRoundedRectangleOutlineBinding()`, and `RasterTextBinding()` accept
shape and UTF-8 text renderers for the checked Progress paint path. Shape and
text effects live in separate modules, so a line-only Go host needs only the
line interface.
`MeasureGlyphWidthBinding()` and `MeasureGlyphLineHeightBinding()` accept
font metric callbacks for the checked Progress composition path. Both callbacks
receive the requested typeface as borrowed UTF-8 bytes.
`TextSliceBinding()` supplies borrowed byte ranges for checked `Text` line
composition; the host does not choose wrap points or positions.
`RasterTextClippedBinding()` supplies a glyph raster callback that must honor
the clip rectangle selected by `Text`, Button, and Checkbox. Some other widget
labels still use `RasterTextBinding()` without a clip.
The caller passes required bindings to Ziran's `BundleRun`. Build and test
with `make` and `make test` from the Kryon repository. No display is started.

For a headless RGBA8 target, `include/image_canvas.h` provides `ImageAsset`,
`ImageCanvas`, and `ImageCanvasRasterizer()`. The caller owns the straight-alpha
pixel buffers and asset table, then binds the resulting `ImageRasterizer` with
`ImageWidthBinding()`, `ImageHeightBinding()`, and `RasterImageBinding()`.
Assets can be found by path or texture ID. The software host applies source
crop, nearest-neighbor scaling, clip, rounded corners, rotation, and tint.
Link it with `libkryon_host.a` and `-lm`; the
[`ziran_image_canvas_test.sh`](../tests/ziran_image_canvas_test.sh) example runs
the same widget from source and saved `.zir` bundles into an in-memory canvas.

## Migration boundary

This library currently covers selected widget policy and line rendering.
Progress layout, fill and border visibility, radius, and label color decisions
are checked Ziran functions. `PaintProgress()` emits its track, fill, outline,
and label through declared raster effects. `PrepareProgress()` selects the font
and uses host glyph measurements; `PaintProgressProps()` combines that with
the raster path. `StyleRules` is an owned 320-rule value table; the checked
`ProgressFacesFor()` resolves track, fill, and label roles by class and cascade
priority. `InstallStyleRules()` installs the owned table, and the checked
one-argument `Progress(props)` paints from that table with Ziran-defined
default faces. Lower-level `PaintProgressFromRules()` still accepts explicit
defaults and rules. The pointer-backed style loader remains unfinished.
`BeginTree(screen_key, bounds)` begins a portable retained submission, and
`EndTree()` commits and paints it. During an open submission,
`Progress(props)` registers a Progress node and queues its generic paint
commands; raster effects are emitted only after the tree commits. Style
resolution and glyph measurement happen during submission. Outside a tree,
`Progress(props)` paints immediately.
`ButtonProps.key`, `ProgressProps.key`, and `SeparatorProps.key` provide stable identity; zero
uses the submission position. `TreeCount()` and `TreeNodeAt(index)` inspect
committed nodes.
Identity survives repeated `BundleInstanceRun()` calls, including bounds
changes. Submission is limited to 1024 nodes; `EndTree()` returns false,
preserves the previous tree, and emits no paint effects if that limit is
exceeded. The generic queue holds 4096 paint commands and likewise rejects an
overfull frame before commit. Progress, Separator, Text, and Image have portable
retained paint paths. `TreeSetInteractive()` marks an eligible submitted node;
after commit, `TreePointerUpdate(PointerFrame)` hit tests committed nodes in
reverse paint order, blocks click-through at disabled controls, and owns press
and release activation. `TreeHoveredAt()`,
`TreePressedAt()`, and `TreeTakeActivationAt()` expose that state. The host
supplies raw pointer samples. Button uses this path for retained pointer
activation; retained layout, keyboard focus, wiring child viewport clips into
widgets, and broader interaction routing remain unfinished.
`EndTree()` links the generic line, rounded shape, and text raster effects;
the image raster effect is also linked for generic paint commands. Glyph
metric and asset dimension requirements follow the widgets the application
imports.
`Separator(props)` resolves Line and Label KSS roles in checked Ziran. It
positions an unlabeled vertical or horizontal line, or measures and paints a
label followed by a line. It paints immediately outside a tree and submits a
node for deferred paint inside one.
`BeginStyleRules()` and `ParseStyleRules()` collect checked KSS parser output
into that table. Parsing stops at `NeedImport` so the caller can use
`ProvideStyleRulesImport()` or `FailStyleRulesImport()` before continuing.
`StyleRulesParse.overflow` reports when a 321st rule would exceed the table.
Portable hosts that import the KSS parser bind `KssStringSliceBinding()` for
source byte ranges. A portable bundle's installed rules last for one
`BundleRun`; another run begins with an empty rule table. A
`BundleInstance` keeps those rules between runs, allowing a frame entry to
install a style sheet once and paint subsequent frames from it.
`Image(ImageProps)` is now a checked Ziran widget. It accepts an asset path or
a supplied texture handle, resolves class styles, chooses source and fit,
queues a clipped and optionally rounded image draw, and retains semantic alt
text on its node. The platform supplies asset dimensions and executes the raw
image raster command through declared host effects. A missing asset paints its
alt text or the fallback label. The headless software host covers RGBA output;
broader image materials, accessibility event publication, and concrete
desktop/browser image hosts still need migration.
`Text(TextProps)` is a checked Ziran widget. It resolves KSS text rules,
measures glyphs, wraps at word boundaries, positions lines with horizontal and
vertical alignment, clips glyphs and strikethrough to its bounds, and submits
semantic text to the retained tree. The host supplies byte slices, glyph
metrics, and raw clipped text rasterization. Letter spacing, parent foreground
inheritance, and selectable text interaction still need migration.
Text fields in `TextProps`, `LinkProps`, `ToastProps`, `SeparatorProps`, `RadioProps`,
`FieldsetProps`, `ProgressProps`, and `RouterRoute` now use Ziran strings. The
checked `Router(RouterProps, RouterState, []RouterRoute, hash, base_path,
route_version)` stores navigation decisions in a caller owned state value. It
accepts a caller owned route slice, reads the host's hash and version as plain
values, and returns the new state and an optional URL effect. The host applies
that effect using `RouterFormatUrl(result, []u8)` and `result.push_url`, then
passes the new route version through `RouterAcknowledgeVersion` or the next
`Router` call. `RouterNavigate` queues an app navigation request; `RouterSetRoute`
changes the route immediately without submitting a tree node. An active
`Router` call submits an inert retained node when the tree is building. Other
props still contain pointer-based state or collections. Remaining widget
composition, broader platform rendering and host linking, and downstream
application integration are unfinished. Modules outside `modules.txt` may
still contain syntax or imports from the retired implementation and are not
supported API yet. See [architecture](ARCHITECTURE.md) and
[boundaries](BOUNDARIES.md) for the intended ownership and completion gates.

The checked retained tree supports nested composition. `TreeStart` opens the
root scope. A container calls `TreeSubmitCurrent`, then `TreePushScope(node)`
before composing children, and `TreePopScope()` afterward. Standard checked
widgets submit to the current scope; controls that create their own children
can still call `TreeSubmit` with an explicit parent index. `TreeFinish` rejects
an unclosed scope and preserves the previous committed tree. `TreeCancel`
discards the current build and its scope stack.

`TreeSetChildClip(node, bounds)` sets a viewport before a container submits
children. Submitted descendants inherit the intersection of all ancestor
viewports, and `TreeHitAt` excludes points outside that intersection. The
container's own hit area remains available for controls such as scrollbars.
Generic paint commands do not yet inherit this clip; widgets must still pass
their own paint clip until that path is wired through the tree.

`Page(PageProps)` and `Section(SectionProps)` are checked Ziran containers.
They open a child scope, return its content bounds and KSS gap/padding values,
and close with `End()`. Children with zero x and y are placed in a vertical
sequence using the page or section padding and gap; explicitly positioned
children retain their bounds. `PageResult` also returns title, description,
canonical URL, and optional theme color for the platform to apply. The library does not set
browser or window metadata itself. `Heading(HeadingProps)`
and `ParagraphText(ParagraphTextProps)` compose the checked `Text` widget with
their own KSS style kinds and semantic roles. The retained heading level is
clamped to 1–6; ParagraphText uses its parent scope width when none is given.
`Flow(FlowProps)` opens a row scope and returns content bounds; the caller
places children with `FlowChildBounds` before submitting them, then calls
`End()`. `Link(LinkProps)` paints checked KSS text and consumes retained
pointer activation or an explicit focus action. It returns a URL effect for
the platform to apply when `LinkResult.open_url` is true.
`Column`, `Row`, `Stack`, `Group`, `Screen`, and `Grid` are checked Ziran
containers. Each opens a retained child scope that closes with `End()`.
Children with zero x and y are placed by Column, Row, Stack, or Grid during
tree submission. Column and Row advance by the measured size and gap; Grid
uses its configured cell widths and row heights. Queued paint follows the
retained position. Explicitly positioned children retain their bounds. The
`ColumnChildBounds`, `RowChildBounds`, `StackChildBoundsFor`, and `GridStep`
helpers remain available when the caller needs to calculate bounds itself.
`Screen` fills the current tree viewport when its size is unspecified.

`TreeView(TreeViewProps, []TreeItem)` takes a borrowed item slice and returns
the selected item id, scroll offset, and whether selection changed. The caller
stores those returned values between frames and supplies wheel or other scroll
movement as `scroll_delta`. When `focused` is true, `navigation` accepts Home,
End, Up, or Down and reveals the selected row. Each visible row has a retained
selectable node; partly visible rows are clipped for both paint and pointer
input. Unique positive item ids preserve row identity when items move. The
checked surface paints the
panel, rows, and scrollbar; retained pointer capture controls its draggable
thumb. A platform host supplies the raw key and wheel observations.

`PanedView(PanedViewProps)` takes a split value and returns a
`PanedViewResult` with the clamped split, change flag, and the first pane,
second pane, and handle rectangles. The caller stores the returned split and
places its pane content in the returned rectangles. The checked handle uses
retained pointer capture, including release outside its bounds, and paints
through the generic raster capability. `vertical` places the panes side by
side; the other orientation stacks them. Set `has_split` when supplying a
saved split; otherwise the widget chooses its default. `PaneDropZone` returns
the drop region for a point using the same styled edge metric.

`Toast(ToastProps)` takes the previous `ToastState`, the current time, and an
optional new message. A nonempty message starts or replaces the toast;
`clear` removes it. The caller stores `ToastResult.state` and keeps its message
bytes alive until the state expires or is cleared. The widget resolves KSS
surface and label styles, measures and truncates text at UTF-8 boundaries,
places the toast in the supplied viewport or root tree, and queues paint.
Font measurement and raster effects remain host capabilities.

`Collapsible(CollapsibleProps, []CollapsibleHeader)` returns the open and hidden
values, a change flag, the header and content rectangles, and any requested
focus target. The caller stores `open` and `hidden`; `has_open` enables toggling
and `closable` adds a retained close control. `close_label` supplies its
localized accessibility text. For tree headers, pass the ordered id and depth
slice so Right and Left can target a child or parent; Up and Down target the
adjacent header. The platform supplies focus and key observations, while
Kryon chooses the navigation target and handles retained pointer activation.

`SegmentedControl(SegmentedControlProps, []SegmentOption)` lays out a borrowed
option slice and returns the selected and clicked indexes, change flag, and
measured height. The caller stores `selected_index` and sets `has_selection`
when it has a saved selection. Option keys keep button identity when options
move. Each segment uses the checked Button widget with the `Segment` KSS style
kind; disabled options do not accept pointer activation. Use
`SegmentedControlHeight` when the container needs the height before submission.
