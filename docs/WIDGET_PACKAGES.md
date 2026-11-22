# Widget packages

`EmptyState.Props.disabled` and `Banner.Props.disabled` keep configured action and dismiss buttons visible with disabled styling and semantics. They reject pointer and keyboard activation, including input queued before disabling, and become usable again when enabled.

`Carousel.Props.disabled` disables swipe, wheel, keyboard navigation, arrows,
and the selected action together. It defaults to false; item-level `disabled`
continues to disable only that item's action. Disabled controls use their normal
disabled styling and become interactive again when reenabled.

`Carousel.Props.logo_max_width` optionally caps the cover logo's logical width;
`logo_top` optionally positions it below surrounding controls, relative to the
carousel bounds. Zero preserves the default title layout. `CarouselArtwork`
accepts the same optional values after its source rectangle for startup covers.

A widget that is not part of the core catalog lives in its own Git package
and depends on Kryon. [Plot](https://github.com/kryonlabs/plot) and
[DataViews](https://github.com/kryonlabs/data-views) are examples. Import
the building blocks by their package-qualified names, so a reader sees which
package each name comes from:

```zi
#import "kryon/session"
#import "kryon/tree"
#import "kryon/surface"
#import "kryon/paint_queue"
```

Tests that need a host bind Kryon's internal capabilities the same way, for
example `--bind kryon/raster_text:RasterText=my_test_host:RasterText` with
`ziran bundle --project`.

Source coloring is the separate [Syntax](https://github.com/ziranlang/syntax)
package. Its `SyntaxColorSpan` has the same fields as `TextArea.ColorSpan`.

Hosts with stable font measurements call `font_metrics.GlyphMetricsChanged()`
after initializing their font services and whenever fonts or their advances
change. This lets `TextArea` retain wrapped rows in a bounded cache of owned
content. Publish revisions on the UI's owning thread. The pixmap host enables
reuse automatically for its bundled, fixed
font outlines. Hosts that have not published a metrics revision keep measuring
rows on each call; they do not acquire an invalidation requirement implicitly.
Font size, typeface, wrap width and content changes always select a new layout.

`TextArea.Props.manual_scroll` keeps a reader's scroll position independent of
caret and selection changes. Wheel, scrollbar and touch scrolling remain
available. Editors keep the normal caret reveal unless they enable this flag.
`Scroll.Props.manual_scroll` similarly suppresses automatic focused-field
reveal for a reader that owns its viewport.

`TextField.Props.capture_tab` defaults to false. An editable field can opt in
so an unmodified Tab queued before `BeginFrame` remains available through
`KeyboardTake(session)` for application completion. Only the actual retained
focus owner can capture it, using its last committed intent; disabled,
read-only, fully clipped and unfocused fields do not capture. Shift+Tab retains normal focus traversal.
Control-, Alt- and Super-modified Tab stay queued for application input. Intent is scoped to the retained
node and session epoch, including suspended embedded-app input.
`input.focus` also transfers actual focus to a newly submitted TextField in
its first frame, so the next queued key reaches it without an idle render.

### Intrinsic layouts and bound text fields

A retained form can use `Layout.Draw` for a column and `Layout.Row` for a row.
Submit ordinary widgets inside the scope and close it with `End(session)`.
Omit the bounds of an intrinsic button in a row or a bound text field in a
column. The button sizes to its content, and the field fills the column's inner
width and uses the themed control height. `Layout.Gap(.ThemeMetricSpace2)`
resolves a theme spacing token in logical units. `Layout.LayoutContentBounds`
measures unset outer dimensions from child rectangles, direction, main-axis gap
and padding. Its optional `padding_y` sets the vertical inset independently.
Pass a calculated dimension in `Layout.Props.bounds` to fit it to
content, or use an explicit width to compose inside a viewport.

Retained layout rules match `StyleKindRow()` or `StyleKindColumn()` and the
optional `Layout.Props.id` and `class_name`. Styles support `StyleGap` as a
shorthand and `StyleRowGap`, `StyleColumnGap`, `StylePaddingX` and
`StylePaddingY` for independent spacing, rounded to the nearest logical pixel.
A single-line row uses `column_gap` between children and a column uses `row_gap`.
`Layout.Props` accepts uniform `gap` and `padding` over the style and independent
`row_gap`, `column_gap`, `padding_x` and `padding_y` over those shorthands.
Set the corresponding `_override` flag to supply explicit zero. Negative
spacing is clamped to zero.

`Layout.Grid(session, Grid.GridProps, items)` uses CSS `Grid` width, height,
padding, row/column gaps, and equal `grid-template-columns` tracks. Pass
`[]Grid.GridItem` for a measured grid. Each item supplies its height and span.
With unset height, the grid reserves the tallest item's height per row plus
inter-row spacing and block padding. Call `Grid.GridStep` on the returned
cursor for each item and draw using `cursor.item`. A negative span fills a row.
Close `result.container.opened` with `End(session)`. A two-argument `Layout.Grid`
continues to place unpositioned children one track at a time. See the
[HTML/CSS mapping](HTML_TO_KRYON.md#author-a-responsive-dashboard).

For a native bound field, initialize `state.bytes` once from application-owned
storage and set `state.length` to the number of UTF-8 bytes in the initial
value. `TextField.Bind(session, *state, props)` draws and applies edits to that
storage. Give each field a stable `key`. The same field keeps focus, selection,
scroll and IME preedit across frames and resize. It consumes editing input from
the focused session and uses the platform clipboard. Use the returned `changed`,
`committed`, `escaped` and `paste_without_text` flags for application actions.
For deterministic input, pass `false` as the fourth argument to `Bind`
and provide the sample in `props.input`. Custom hosts can pass an IME sample
in `props.input.composition_event`. Native platform text requests use the
same bound widget.

```zi
// name.bytes points to persistent application storage.
form := Layout.Draw(session, Layout.Props.{.key = 11, .bounds = bounds,
    .gap = Layout.Gap(.ThemeMetricSpace2)})
if form.opened {
    edited := TextField.Bind(session, *name, TextField.Props.{.key = 42, .label = "Name"})
    if edited.committed {
        SaveName(TextView(name.bytes[:name.length]))
    }
    unused End(session)
}
```

`TextField.State` keeps a field's value and interaction state in caller-owned
storage. Import it and its helpers as `TextField :: #import "kryon/text_field";`.
Give `bytes` a writable slice and set `length`, `cursor`, `anchor`, `scroll_x`
and `focused` as needed. Every byte of the slice is usable capacity; length
does not include a terminator and the helpers do not write one.

```zi
// Keep the bytes and state alive between frames.
name_bytes: [96]u8;
name: TextField.State;
// Initialize once: name.bytes = name_bytes[:]

// In Frame, use the existing widget and keep the application's commit policy.
edited := TextField.Draw(session, TextField.StateProps(name, TextField.Props.{
    .key = cast(u64)42, .bounds = bounds, .label = "Name", .input = input}))
if TextField.StateApplyResult(*name, edited) && edited.committed {
    // Save or navigate according to the application's workflow.
}
```

`TextField.StateProps(state: TextField.State, props: TextField.Props) -> TextField.Props`
fills value, selection, scroll and focus, and limits `max_bytes` to storage
capacity while preserving stricter application limits and all other props.
The value borrows the buffer; consume borrowed result text before mutating it.
An invalid length or UTF-8 value disables the field; zero capacity is read-only.

`TextField.StateApplyEdit(state: *TextField.State, edit: TextField.Edit) -> bool`
accepts only ordered ranges within the existing value, with both ends at
UTF-8 scalar boundaries, valid UTF-8 replacement text, and a result that fits
the slice. The current value must also be valid UTF-8. Replacement can borrow
any part of the destination: a temporary owned join is freed after the call.
Rejected edits, including allocation failures, preserve every byte and state
field. An accepted changed edit collapses selection after the replacement.
An unchanged edit is accepted without changing state. Application-specific
text limits beyond storage capacity are enforced by the widget's props.

`TextField.StateApplyResult(state: *TextField.State, result: TextField.Result) -> bool`
applies the edit and copies cursor, anchor, scroll and focus together. A rejected
edit preserves all of them. Successful application clamps cursor and anchor
to scalar boundaries in the resulting text and scroll to zero or greater;
the widget still owns grapheme navigation. `true` means accepted, including
navigation with no edit; it does not mean committed. Enter, Escape, clipboard
and IME composition remain available on the original result for the application
to handle, with their existing borrowed-text lifetime requirements.

Native TextArea and TextField snapshots retain the supplied cursor and anchor
and return only the changed grapheme-aligned span as their edit. An unchanged
snapshot updates selection without a text edit. This keeps a correction near
the beginning of a long note from revealing the document end on its edit frame.
Applications must apply the edit and preserve the result's cursor and anchor.

A `TextArea` can preview a markup language over its value without changing
it. `hidden` bytes take no width or caret stop, `strong` bytes are bold and
`italic` bytes slanted when the host registers a slant raster (otherwise upright), and `colors` tint spans.
`lines` draws a logical line at its own size (`TextArea.Line.scale`), such as
a heading: its rows wrap, take caret stops and take height at that size.
`blocks` adds boxes under a line, one under another in order, the first in
place of its last row with `cover`, each drawing `image` fitted inside it or
a soft frame with `label` while the image is missing; a press inside a box
is the end of its line. Every list is sorted, with starts at line starts and
at most one size per line, or ignored as a whole; without them the layout is
the plain area's. During an IME composition, lines and
boxes after it move with the preedit.

`MarkdownEditor :: #import "kryon/markdown_editor";` shares the inline
Markdown editor extracted from Inner Breeze Diary. Call
`MarkdownEditor.Draw(session, MarkdownEditor.Props.{.area = area, ...})`
with ordinary `TextArea.Props` and retain its result's edit, cursor, anchor,
scroll and focus. `muted_color` and `link_color` color list/quote markers and
links. Headings, emphasis and links preview while the caret's active lines
show their original syntax. The value and edit offsets always address the
original Markdown; there is no HTML conversion or second document.

The optional `kryon/markdown_preview` module exposes `Scan(preview, text,
cursor, anchor, editing, images)` for hosts that need the formatting ranges.
An app resolves its own media into `ImageSpan` values and corresponding
`area.blocks`; the editor owns only their layout and syntax visibility.
It never reads a file or fetches a URL. `kryon/text_props` exposes the
text/range data types without importing a text renderer.

Import `kryon/pointer_input` for `SamplePointer()`, which also works inside a
hosted widget frame. The host returns the pointer sample for the session passed to `WidgetHostBegin`;
an inactive frame cannot read input, and another session's sample is not exposed.
The pointer value schema lives in `input_protocol`, independently of the native
retained input implementation.

`frame_snapshot.CaptureFrameInput(session: Session) -> FrameInputSnapshot`
returns the host's immutable observations for the current frame. `available`
is false before a host publishes, after it ends the frame, or for an invalid
or different session. The result owns its `keys: [512]u8` array and nested
pointer values: retained copies survive subsequent polls and hosted calls.
Test `snapshot.keys[KeyLeft] & cast(u8)FrameKeyHeld` for sustained movement.
Each entry combines `FrameKeyHeld` (1), `FrameKeyPressed` (2),
`FrameKeyReleased` (4), and `FrameKeyRepeated` (8). A repeat is distinct from
a fresh press; holding a key does not invent repeats. Indices use the existing
uppercase ASCII and named `Key` codes; index zero is unused.

The snapshot also carries `pointer`, `secondary`, `focused`, `modifiers`,
`wheel`, and `elapsed_ms`, `delta_ms`, `sequence`. Desktop and Canvas
sample a monotonic clock once per successful host poll: first delta is zero,
later deltas are nonnegative, and repeated captures have identical timing.
Sequence starts at one for each opened host. Pointer coordinates use the
host's logical zoom.
On focus loss, held keys become released for one poll, held pointer buttons
are released, and pressed/repeated keys, modifiers and wheel are cleared.
Further unfocused polls have no held keys or repeated releases. The first
focused poll after focus loss may report held keys but suppresses fresh
press and repeat flags, preventing a focus change from activating a shortcut.

Hosts publish on the session's owning thread with `FrameInputBegin(session, snapshot) -> bool` and call
`FrameInputEnd(session)` after the callback, including failure paths. Publishing
copies the value and rejects replacement while that session's frame is open.
Kryon's standard Desktop and Canvas app loops do this automatically.
Desktop freezes raw SDL key events for its own window; Canvas freezes existing
DOM held state and edge queues before widget reads. Both clear publication on
callback cancellation, terminal polls and host teardown. Custom Desktop or
Canvas loops call `DesktopFrameInputBegin(session)` or
`CanvasFrameInputBegin(session)` after the matching successful poll, and
`FrameInputEnd(session)` when their callback ends (including failure paths).
The host also ends publication before presentation can yield. Other providers must supply
equivalent gathered state; this API does not infer held keys from consumed
keyboard events. A hosted
bundle requests only `frame_snapshot.CaptureFrameInputHost`; `WidgetHostCall`
validates the session against the active `WidgetHostBegin` before returning
that session's published snapshot. No input is queried outside that frame.

Capturing never consumes `KeyboardTake`, `TypedTextTake`, IME, clipboard, or Escape routing. Applications retain movement,
simulation, repeat cadence, and navigation policy. The existing `FrameInput`
replay record and its apply/field helpers retain their contract.

`Scroll` reveals a newly focused text input and adjusts when the viewport
resizes, such as when a phone keyboard opens. It retains that focus transition
on the scroll node. Keeping an input focused never recenters it after manual
scrolling; applications should not repeat their own focused-field reveal loop.

`BeginFrame` routes Escape to the topmost retained modal or popup, even
while a field has focus. `DismissibleOverlay` and `Popup` report it through
`closed`; a non-dismissible modal consumes Escape without closing. Hosts
send `KeyEscape` with `KeyboardSend`, and application shortcuts check
`TreeEscapeCaptured(session)` before handling that same Escape. `KeyF3` is the
canonical F3 key code (292) for application shortcuts.

`Modal.Props.dismiss_on_outside` defaults to false. Its full-viewport scrim
owns pointer presses that start outside the panel and consumes their releases,
including drags that cross the panel boundary, without reporting `dismissed`.
Set `dismiss_on_outside = true` to opt into outside-release dismissal. Escape
dismisses the modal by default; `dismiss_disabled = true` prevents both Escape
and outside dismissal. `Overlay.Props.outside_dismiss_disabled` blocks outside
releases independently of Escape.
`Overlay.Popup` and context menus keep their separate press-outside dismissal
and click-through behavior.
