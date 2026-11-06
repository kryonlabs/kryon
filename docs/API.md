# Kryon API

Kryon is an ordinary Ziran package. An application imports the modules it
uses; no widget names are recognized by the Ziran compiler or `.zx` loader.
The public package imports are declared in [`ziran.toml`](../ziran.toml).

Rendering hosts that retain pixels can call `CommitFrameDamaged(session)` to
commit the tree and compare paint while leaving changed commands queued. An
unchanged result needs no raster work; a partial result supplies its damage
rectangle. The host clears and clips that area before `PaintFlushArea`, or
clears its whole surface before `PaintFlush`. `EndFrameDamaged` keeps its
existing behavior of emitting a full repaint immediately.

| Import | Surface |
| --- | --- |
| `Kryon` | Geometry, drawing values, sessions, `Text(TextProps)`, and `Button(ButtonProps)` |
| `Widgets` | The core widget catalog, including `Image(ImageProps)`, page structure, fields, controls, layout, and tree input |
| `Page`, `Semantic` | Direct page and semantic-kind exports for document-oriented applications |
| `PageRoute` | Browser route path, hash, and history (`GetRoutePath`, `PushRoute`, `GetRouteOrigin`, ...) for Canvas and DOM profiles |
| `geometry`, `drawing_props`, `session`, `tree`, `surface`, `paint_queue`, `style`, `style_sheet`, `control_props`, `font_metrics`, `scroll`, `semantic`, `tree_input`, `widget_kind` | Building blocks for widget packages |

## Vector path coverage

`vector_path` rasterizes the portable `KVP1` filled-path format and round
polyline strokes into caller-owned 8-bit coverage. `VectorPathFill` accepts an
affine `VectorTransform`, so hosts can render either a complete image or an
integer-translated tile with identical pixels. `VectorPolylineStroke` uses the
same transform, target, and `VectorAdd` / `VectorErase` coverage arithmetic.
File helpers are bounds-checked; callers retain ownership of all bytes and
coverage storage.

Calls share bounded scratch storage and must be serialized. Targets may be up
to 8192 by 16384 pixels; invalid input or exhausted scratch returns `false`.

## Bottom navigation

`NavigationBar(session, props, items)` defaults to a full-width 80-unit tonal
bottom bar. Reserve `NavigationBarHeight(props)` above the bottom of the view.
Each `NavigationBarItem` accepts an optional `icon_shape` (the standard Shape
format) or `image`; the icon is 24 units inside a 64 by 32 selection pill, with
its label below. With neither icon, the label is centered inside the pill.
Item keys, route results, disabled hit testing and custom style rules retain
their existing contracts. Pointer hover/press and keyboard focus draw state
layers; keyboard focus adds a ring. `compact` retains the scrolling dock/rail
layout used by Breeze, including its footer and drag/close behavior.

## Buttons

Installed system palettes give buttons the full (capsule) shape. A button
taller than 56 units is a tile, such as a launcher or keypad tile, and takes
20-unit card corners instead of becoming a stadium; `pill` and `circle` still
force a capsule. Neutral soft buttons are tonal: a light tint of the palette's
accent over the background. A labeled `image` in a tile fills the spare
height instead of staying at the styled icon size: up to 64 units above the
label, or up to 40 units beside it. A vector `icon_shape` still takes
precedence over an `image` and keeps the styled size.

## Cards

`Card(session, CardProps)` draws a filled, elevated or outlined Material surface.
Import it from `Widgets` or `kryon/card_widget`. Set `variant` to `CardFilled`
(the default), `CardElevated` or `CardOutlined`, and supply `bounds` and a stable
`key`. The returned `content` rectangle has 16 units of padding, reduced for
small bounds. Draw your content in that rectangle; Card does not open a scope.

Cards are decorative by default. `clickable` enables the standard Button's
pointer, keyboard, focus and disabled behavior; `activated` reports activation.
An optional `label` is drawn centered and announced by the clickable surface.
Selected and explicit state props retain their usual meaning. Installed Card
style rules apply by id, class and emphasis (soft, filled and outline for the
three variants). Elevated cards and floating menus/dialogs use two fixed,
1-unit contact strips, with no blur, offscreen bitmap or animation timer.

A ListBox with no explicit row height uses 40 units. Its icon size does not
set its row height. Explicit `row_height` and style `offset_y` still win.

## Widget packages

`CarouselProps.logo_max_width` optionally caps the cover logo's logical width;
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
package. Its `SyntaxColorSpan` has the same fields as `TextAreaColorSpan`.

Hosts with stable font measurements call `font_metrics.GlyphMetricsChanged()`
after initializing their font services and whenever fonts or their advances
change. This lets `TextArea` retain wrapped rows in a bounded cache of owned
content. Publish revisions on the UI's owning thread. The pixmap host enables
reuse automatically for its bundled, fixed
font outlines. Hosts that have not published a metrics revision keep measuring
rows on each call; they do not acquire an invalidation requirement implicitly.
Font size, typeface, wrap width and content changes always select a new layout.

`TextAreaProps.manual_scroll` keeps a reader's scroll position independent of
caret and selection changes. Wheel, scrollbar and touch scrolling remain
available. Editors keep the normal caret reveal unless they enable this flag.
`ScrollProps.manual_scroll` similarly suppresses automatic focused-field
reveal for a reader that owns its viewport.

`TextFieldState` keeps a field's value and interaction state in caller-owned
storage. Import it and its helpers from `Widgets` (or `text_input_props`).
Give `bytes` a writable slice and set `length`, `cursor`, `anchor`, `scroll_x`
and `focused` as needed. Every byte of the slice is usable capacity; length
does not include a terminator and the helpers do not write one.

```zi
// Keep the bytes and state alive between frames.
name_bytes: [96]u8;
name: TextFieldState;
// Initialize once: name.bytes = name_bytes[:]

// In Frame, use the existing widget and keep the application's commit policy.
edited := TextField(session, TextFieldStateProps(name, TextFieldProps.{
    .key = cast(u64)42, .bounds = bounds, .label = "Name", .input = input}))
if TextFieldStateApplyResult(*name, edited) && edited.committed {
    // Save or navigate according to the application's workflow.
}
```

`TextFieldStateProps(state: TextFieldState, props: TextFieldProps) -> TextFieldProps`
fills value, selection, scroll and focus, and limits `max_bytes` to storage
capacity while preserving stricter application limits and all other props.
The value borrows the buffer; consume borrowed result text before mutating it.
An invalid length or UTF-8 value disables the field; zero capacity is read-only.

`TextFieldStateApplyEdit(state: *TextFieldState, edit: TextFieldEdit) -> bool`
accepts only ordered ranges within the existing value, with both ends at
UTF-8 scalar boundaries, valid UTF-8 replacement text, and a result that fits
the slice. The current value must also be valid UTF-8. Replacement can borrow
any part of the destination: a temporary owned join is freed after the call.
Rejected edits, including allocation failures, preserve every byte and state
field. An accepted changed edit collapses selection after the replacement.
An unchanged edit is accepted without changing state. Application-specific
text limits beyond storage capacity are enforced by the widget's props.

`TextFieldStateApplyResult(state: *TextFieldState, result: TextFieldResult) -> bool`
applies the edit and copies cursor, anchor, scroll and focus together. A rejected
edit preserves all of them. Successful application clamps cursor and anchor
to scalar boundaries in the resulting text and scroll to zero or greater;
the widget still owns grapheme navigation. `true` means accepted, including
navigation with no edit; it does not mean committed. Enter, Escape, clipboard
and IME composition remain available on the original result for the application
to handle, with their existing borrowed-text lifetime requirements.

A `TextArea` can preview a markup language over its value without changing
it. `hidden` bytes take no width or caret stop, `strong` bytes are bold and
`italic` bytes slanted (backends that register a slant raster, as Raylib
does, draw them leaning; others upright), and `colors` tint spans.
`lines` draws a logical line at its own size (`TextAreaLine.scale`), such as
a heading: its rows wrap, take caret stops and take height at that size.
`blocks` adds boxes under a line, one under another in order, the first in
place of its last row with `cover`, each drawing `image` fitted inside it or
a soft frame with `label` while the image is missing; a press inside a box
is the end of its line. Every list is sorted, with starts at line starts and
at most one size per line, or ignored as a whole; without them the layout is
the plain area's. During an IME composition, lines and
boxes after it move with the preedit.

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
`wheel`, and `elapsed_ms`, `delta_ms`, `sequence`. Desktop, Canvas and Raylib
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
Kryon's standard Desktop, Canvas and Raylib app loops do this automatically.
Desktop freezes raw SDL key events for its own window; Canvas freezes existing
DOM held state and edge queues before widget reads. Both clear publication on
callback cancellation, terminal polls and host teardown. Custom Desktop or
Canvas loops call `DesktopFrameInputBegin(session)` or
`CanvasFrameInputBegin(session)` after the matching successful poll, and
`FrameInputEnd(session)` when their callback ends (including failure paths).
The host also ends publication before presentation can yield. Custom Raylib
loops call `RaylibFrameInputBegin(session)` after `PollRaylib`, then
`FrameInputEnd` when their callback ends. Other providers must supply
equivalent gathered state; this API does not infer held keys from consumed
keyboard events. A hosted
bundle requests only `frame_snapshot.CaptureFrameInputHost`; `WidgetHostCall`
validates the session against the active `WidgetHostBegin` before returning
that session's published snapshot. No input is queried outside that frame.

Capturing never consumes `KeyboardTake`, `TypedTextTake`, Raylib's key/character
queues, IME, clipboard, or Escape routing. Applications retain movement,
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
`TreeEscapeCaptured(session)` before handling that same Escape.

## Style rules

`InstallStyleRules` copies typeface text into Kryon-owned storage and returns
`false` without replacing the active rules when the rule count is outside
`0..320` or the combined typeface text exceeds 65,536 bytes.
`InstallParsedStyleRules` reports the same storage failure.

Use `Image(ImageProps)` for UI images. Its asset path, bounds, fit, and alt
text describe the image at the widget boundary. Backend texture calls are
renderer primitives, not alternate widget APIs.
Asset paths may name PNG, JPEG, WebP or AVIF files on image-loading hosts. WebP is
recognized by its content: lossy, lossless and transparent pictures load as
RGBA, and an animated WebP shows its first frame (Kryon images are still).
See [WebP images](BACKENDS.md#webp-images) and [AVIF images](BACKENDS.md#avif-images) for each host's decoder.
`ImageProps.radius` gives an image rounded corners in native and hosted UI;
a positive value overrides its style radius.

Import `kryon/bitmap_image` to create a runtime RGBA image for the same
`Image(session, ImageProps.{.texture = image, .smooth = true, ...})`:
`BitmapImageCreate(session: Session, width: s32, height: s32, rgba: []u8) -> Texture2D`,
`BitmapImageUpdate(session: Session, image: Texture2D, rgba: []u8) -> bool`, and
`BitmapImageRelease(session: Session, image: Texture2D)`. Bytes are tightly packed,
row-major straight RGBA8 and copied/consumed synchronously; reuse the input buffer
after the call. Create failure returns a zero texture. Update returns true for
accepted unchanged data, and false preserves the old pixels. Only successful
changed data invalidates retained paint. Smooth sampling and alpha use the
ordinary Image renderer on Desktop/Cairo, pixmap and Canvas.

Managed handles belong to the complete Session (slot and epoch) and current
provider. Dimensions must be positive, at most 4096 each and 4,194,304 pixels;
byte count must equal width × height × 4. There are at most 64 images, with a
64 MiB total and 32 MiB per-session budget for straight and premultiplied CPU
copies. Provider copies have the same bounded dimensions/count. Bad, modified,
stale and cross-session handles are rejected; releasing them is harmless.
Closing a Session releases its images. Closing/replacing the Canvas provider
also releases its images. Call on the session/provider's frame thread, outside
an active raster emission. Native renderer texture handles keep their existing
contract. Standard hosted widget policy continues to reject all guest runtime
texture IDs and does not advertise bitmap lifecycle capabilities; an embedding
must provide a separate authenticated ownership policy to grant those effects.

`ImageProps.clip_polygon` clips an image without stretching its content.
Supply 3–4096 points in normalized destination coordinates (0–1); the path
closes automatically. Empty leaves the image unchanged. Invalid coordinates,
too few points, or combining a polygon with a mesh draw nothing. Native,
hosted and browser images retain their own copy of the supplied points.

`ImageProps.mesh` warps the same image over a grid of `columns` × `rows`
screen points, row by row from the source's top left, each cell drawn as two
textured triangles: a cut-out puppet's painted arm can bend at its elbow, or
a picture can follow a curve. The bounds still clip it; fit, origin,
rotation and the style radius do not apply, and a grid with too few points
draws nothing. Points are copied when the image is queued, at most 4096 per
image (256 through a portable bundle's hosted `Image`).

`ImageProps.preview_path` names a small copy of the same picture that draws in
its place, with the image's own bounds, fit, mesh and clip, while the picture
loads or when it is unavailable, so something shows at once and the picture
replaces it when ready. A preview of part of a picture (a nonempty `source`,
such as a jigsaw piece) needs `ImageProps.full_size`, the picture's size in
pixels, to map that rectangle onto the preview; without it the image waits for
the picture. Hosted previews pass the same asset policy as their picture.
`image_showing.ImageShowingOf(session, image)` reports `ImageShowingNone`
(loading), `ImageShowingPreview`, `ImageShowingFull` or
`ImageShowingUnavailable`, natively and through a portable bundle's host, so
an app can wait for a picture before what belongs with it. Asking starts the
host loading the picture, which also fetches pictures shown soon.

`image_mesh.ImageMeshQuad(bounds, camera, points)` constructs a four-corner
mesh for the standard `ImageProps.mesh`. It uses the same 2D `Camera` as
`WorldToScreen`, with caller-owned space for four points; too little space
produces an empty mesh. Rotation and scale stay on the standard image surface.

`TileMapProps.pixel_snap` fits and positions whole-pixel cells. `nearest` selects
nearest-neighbour sampling for the whole map; `nearest_tiles` selects individual
tile indices while the rest remain smooth. Sampling survives the hosted codec.
A hosted map uses batched sampling only when its host advertises
`tile_map.sampling`; older hosts draw the same standard Images individually.
Pixel snapping also works on older hosts because the bundle fits the bounds.

`WidgetPaletteUse` scopes semantic colors along with widget rules. Hosted
`AppThemeColor` calls and compositions using canvas, surface, text, border and
accent see the same palette. `WidgetPaletteRestore` restores the surrounding palette when calls nest,
including its text style and semantic tokens. The host frame unwinds palettes
a guest leaves open. Sixteen nested parents are retained; scopes beyond that
restore to the deepest retained palette, then unwind to the original host.
The outermost restore preserves the complete prior token set, including
host-specific colors and the absence of unset tokens.

Hosts call `PaintResourcesChanged()` when replacing raster resources without
changing widget props. Retained frames then compose and paint again, including
when a browser graphics context resets or an app updates its image assets.

`Line(LineProps)`, `Circle(CircleProps)`, `Ring(RingProps)` and
`Triangle(TriangleProps)` draw shapes as raylib's `DrawLineEx`, `DrawCircle`
and `DrawCircleGradient`, `DrawRing` and `DrawTriangle` do. Like `Box`, each
is decorative: it submits a node covering what it draws and queues one paint
command. Kryon cuts the shape into triangles, clips them to its parent's clip
and sends each colour's triangles to the host in one `RasterTriangles` call;
a radial fade is drawn as rings of one colour each. `BoxProps.fill_bottom`
with `has_fill_bottom` fades a box's fill from top to bottom, as
`DrawRectangleGradientV` does.
Import `kryon/shape_props` and `kryon/shape_widget` to use these shapes without
loading the full widget catalog, including in a portable hosted bundle.

An application using a Kryon host exports a Ziran function with this shape:

```zi
using UI :: #import "Widgets";

#program_export
Frame :: (session: Session, viewport: Rectangle) -> s32 {
    label: TextProps
    label.text = "Hello, Kryon"
    Text(session, label)
    return 0
}
```

The selected host owns the process entry point, calls `BeginFrame` and
`EndFrame`, supplies input to the session, and presents paint operations.
Every host reports the same key codes through `KeyboardTake`: letters as
uppercase ASCII and named keys as `KeyEnter`, `KeyBackspace`, `KeyLeft`, and
the other `Key` constants. A letter is the one the active keyboard layout types,
not its physical position; on Raylib, `RaylibLayoutKey` converts a position from
`RaylibKeyDown`. `TypedTextTake` collects the text typed since the
last frame, and `TextFieldInputFor(key, modifiers, text)` turns both into a
focused `TextField`'s input.
A Raylib app that has nothing to animate can call
`RaylibWaitForActivity(descriptors, milliseconds)` between frames instead of
drawing: it returns `RaylibActivityDescriptor`, `RaylibActivityInput` or
`RaylibActivityNone`, and `RaylibActivityUnsupported` on hosts that must keep
drawing. After `RaylibActivityInput` draw two frames, since a frame uses input
the previous one read. `RaylibPrepareGlyphs(text)` readies many characters at
once, so drawing them one at a time does not rebuild a font for each.

Canvas and DOM app hosts retain unread key presses and repeats across drawing
frames, taking one application key per frame. Pressed/released snapshot flags
still expire at the frame boundary; delivery does not replay those flags.
Focus loss and host reset cancel unread keys. Each queued press or repeat
retains its DOM event's Shift, Control/Meta and Alt modifiers. Custom Canvas
loops read `CanvasPressedKeyModifiers()` immediately after `CanvasPressedKey()`
and submit those bits with `KeyboardModifiersSend(session, modifiers)`.
An empty key take reports current held modifiers. `CanvasModifiers()` and
`CaptureFrameInput(session).modifiers` still report held state at their own
sampling time; consuming a shortcut never changes held keys or snapshot flags.

Application code owns its product state and calls widgets within `Frame`.
The complete manifest and build commands are in [PROJECTS.md](PROJECTS.md).

Individual `.zi` modules under [`src/ui`](../src/ui) can also be imported by
non-graphical Ziran programs. A program that does not import Kryon uses no
Kryon code. A portable `.zx` links only imported modules and needs a host
binding for any platform effect they call.

An application runs another `.zx` inside its own window with
[`hosted_app`](../src/backend/hosted_app.zi): `HostedAppOpen` takes the
bundle's bytes, `HostedAppStart` asks the host about every capability the
bundle requests and binds them to one `HostCall`, and `HostedAppRun` runs it
under a step limit. `HostedAppRestart` keeps the working instance until the
new one passes the host's handshake. It draws that
bundle's widgets with [`widget_host`](../src/backend/widget_host.zi). The
bundle calls each widget's `HostedHost` function; the host binds
`WidgetHostCall` for the functions `WidgetHostSupports` lists, and wraps each
bundle frame in `WidgetHostBegin(session, asset_allowed)` and
`WidgetHostEnd()`. The callback decides which asset paths the bundle may
draw; texture handles never cross. Records match fields by name, so a bundle
and its host may be built against different Kryon layouts.
Host replies copy returned strings into the bundle's memory, including text
inside records and collections. A host may reuse its reply buffers on its
next call; the bundle can retain those strings across calls and instance runs.
[`scripts/generate-widget-codecs.py`](../scripts/generate-widget-codecs.py)
generates the codecs for Kryon's widget records and, with `--uses`, for an
application's own records.

A bundle measures text with its host's fonts through
[`text_measure`](../src/ui/text_measure.zi): `TextWidth(session, props)` is
the width a line takes and `TextHeight(session, props)` the height Text
takes to draw a paragraph wrapped in `props.bounds.width`.

A vector icon that is not a button, such as a symbol beside a name or a coin
beside an amount, is `SvgIcon(session, SvgIconProps.{bounds, shape, color})`
from [`svg_icon_widget`](../src/ui/svg_icon_widget.zi); a bundle submits it
to its host like the other widgets. An icon is rasterized once at the
screen's pixels and kept, with the 255 others painted most recently, as the
rectangles it paints, so a screen with many icons does not rasterize them
again every frame. A host that draws textures registers
`SvgIconUseTextures(make, free)` and then paints each icon as one image of
the same pixels; raylib's `OpenRaylib` does, and a raylib host that opens its
own window calls `RaylibUseIconTextures()` after `InitWindow`.

A native game host using `raylib_runtime` can enable
`RaylibCompletePictureFrames(true)` for its `BeginRaylibFrame` /
`EndRaylibFrame` loop. Two retained screen textures keep the last complete
frame visible while newly requested images decode, including later animation
sprites. Decoding continues while that frame is held. Hosts that want images
to appear progressively keep the default, `false`.

A bundle draws a row of choices, such as a week or calendar view, with
`SegmentedControl(session, props, options)` from
[`segmented_control_widget`](../src/ui/segmented_control_widget.zi); its host
draws it as it draws its own, with up to 32 options. It frames a form's
sections with `Fieldset(session, props)` from
[`fieldset_widget`](../src/ui/fieldset_widget.zi), and places a toggle at a
row's end with the size `ToggleSize(session, props)` from
[`toggle_measure`](../src/ui/toggle_measure.zi) answers, which stays apart from
the toggle widget as `text_measure` stays apart from Text. Lines, circles,
rings and triangles from [`shape_widget`](../src/ui/shape_widget.zi) are
hosted too, so a bundle can draw a chart's lines and dots; their host fills
them with the renderer's `RasterTriangles`. `Slider` and `DiscreteSlider`
from [`slider_widget`](../src/ui/slider_widget.zi) and `Progress` from
[`progress_widget`](../src/ui/progress_widget.zi) are hosted as well: the
host drags the slider with its own pointer and answers the value, and paints
the progress bar.

[`open_uri`](../src/backend/open_uri.zi) opens a link with the platform's
handler: `OpenUri(uri)` passes it unchanged to the desktop opener, Windows'
shell, an Android `ACTION_VIEW` intent or a new browser tab,
and `CanOpenUri(uri)` says whether that platform can open it. Android code
reaches the activity through [`android_jni`](../src/backend/android_jni.zi).

The generated Android activity also exposes `public static synchronized String
takeLaunchUri()` (JNI static signature `()Ljava/lang/String;`). It consumes the
latest nonempty intent data string of at most 4,096 UTF-16 units and returns
`""` when none is pending. Capture occurs before native startup and on
`onNewIntent`; recreation with saved state does not replay the original launch.
The single slot is process memory only; a newer bounded value replaces it.
Null, empty, oversized or unreadable data leaves the slot unchanged. Values
are preserved without decoding or truncation. The application owns URL
allowlisting, UTF-8 copy bounds, user confirmation and any resulting action.
The bridge does not open URLs or validate their origin. Applications own
their manifest intent filters and custom-activity integration.

`kryon/play_billing` exposes `PlayBillingBuy(id, accountID, output)` for
Google Play editions. Pass the signed-in account's stable ID. The bridge sends
`buy:product|account` and sets Play's obfuscated account ID to the lowercase
64-character SHA-256 digest of the account ID's UTF-8 bytes. Empty IDs,
embedded NULs, delimiters and oversized requests are rejected without opening
a purchase sheet. Each queued sheet retains the account from its request.
The application server must verify the paid purchase and its account binding
before crediting it; this client binding alone grants no balance or referral
reward. Other editions/platforms return an empty answer.

Other platform services an application may import:

| Module | Surface |
| --- | --- |
| [`file_export`](../src/backend/file_export.zi) | `ExportBytes` and `ExportFile`: hand a finished file to the user as a browser download or through Android's share sheet |
| [`file_picker`](../src/backend/file_picker.zi) | `PickFile(FilePickerProps, output)`: zenity on Linux and BSD, Windows' common dialog, and the browser's chooser, which copies the file into the page's file system |
| [`window_close`](../src/backend/window_close.zi) | `WindowCloseWatch(window)` and `WindowCloseTake()`: close requests for the application's own SDL or Win32 window, so it can ask first or hide to a tray |
| [`idle_time`](../src/backend/idle_time.zi) | `IdleMilliseconds()` from the session's idle service over D-Bus, and `IsWaylandSession()` |
| [`notification_desktop`](../src/backend/notification_desktop.zi) | The `notification` host for Linux and BSD desktops, through notify-send |
| [`android_picker`](../src/backend/android_picker.zi) | `AndroidPickerOpen`, `AndroidScannerOpen` and `AndroidPickerPoll`: asynchronous document and QR selection. Import accepts one MIME type or a comma-separated allowlist; custom activities use the scaffold's `org.kryonlabs.platform.DocumentTypes.apply` to set Android's primary type and `EXTRA_MIME_TYPES`. Android filters by MIME type rather than filename extension. `AndroidSavePickerOpen` returns a private staging path through the same poll; write it, then `AndroidSavePickerCommit` confirms the selected document was written and closed. The activity implements `openImportPicker`, `openQrScanner`, `openSavePicker` and `savePickedFile`. |
| [`android_text_editor`](../src/backend/android_text_editor.zi) | Input method editing on Android: the activity's input connection sends snapshots of the focused field, so composition, autocorrect and suggestions work; android_device's text input uses it when the activity implements `setTextInputState` and `nativeTextInputState` |
| [`file_download`](../src/backend/file_download.zi) | `FileDownloadStart`, `FileDownloadPoll` and `FileDownloadRelease`: a large file downloaded to a path, appearing only when complete, through curl, the browser's streaming `fetch` into the page's file system, or the Android activity |
| [`http_transfer`](../src/backend/http_transfer.zi) | `HttpTransferStart`, `HttpTransferPoll` and `HttpTransferCancel`: HTTP requests that run while the application draws, through curl on a worker thread, the Android activity's transport, or the browser's `fetch` |
| [`page_socket`](../src/backend/page_socket.zi) | `PageSocketOpen`, `PageSocketPoll` and `PageSocketClose`: a browser WebSocket for text events, polled each frame, with bounded messages and a token carried as a subprotocol |
| [`browser_viewport`](../src/backend/browser_viewport.zi) | `BrowserViewportMeasure` and `BrowserResizeSettled`: the room a canvas application has in the browser, and when a new size has settled enough to resize to, for applications such as raylib web builds that size their own canvas |
| [`raylib_secondary_window`](../src/backend/raylib_secondary_window.zi) | Up to three extra SDL windows for a raylib application, such as a floating timer, each drawing its own Kryon tree with its own pointer (`SecondaryWindowInput`) |
| [`raylib_trace`](../src/backend/raylib_trace.zi) | Where raylib's log goes: `RaylibTraceToConsole`, `RaylibTraceToFile` for Windows applications without a console, and `TraceLog(level, text)` |

After a completed HTTP poll, `transfer.failure` distinguishes a response that
exceeded the caller's buffer (`HTTP_TRANSFER_FAILURE_CAPACITY`) from a transport
failure. Failed responses never publish partial bytes. A new start or cancel
clears the failure. Large logical transfers can use this result to switch to
the service's paged API and preserve progress without enlarging frame memory.

[`src/ui/modules.txt`](../src/ui/modules.txt) is the core build inventory.
`make test` checks source, saved `.zir`, `.zx`, and generated C/Go behavior.
See [FEATURE_MATRIX.md](FEATURE_MATRIX.md) for package and host verification.

Portable apps can import `kryon/input_protocol` for pointer observations,
key codes, modifier bits and `PointInBounds` without loading the native tree
implementation. Existing `tree_input` imports retain that surface.
`kryon/file_picker` exposes the native file chooser; `kryon/app_theme` reads
optional host theme colours from a portable app.

## Cover carousels

`kryon/carousel` provides `Carousel(session, props, items, state)` for a
full-cover browser. Keep a `CarouselState`, pass `CarouselItem` values with
portrait and landscape art, an optional source crop, an optional logo and
caption, and an action label. A zero `source` uses the whole selected image.
It returns the activated item index, or -1. Swipe, wheel, arrows and
Enter/Space share one selection; only that selection exposes an action.
The caller keeps game launching and any footer controls. `CarouselArtwork`
paints the same cover composition for a native loading screen and accepts the
same optional source crop, defaulting to the whole image.

## Loading surfaces

`kryon/loading_screen` provides `LoadingScreen(session, LoadingScreenProps)`
for app starts and longer operations. Supply a title, localized detail and
an app icon (`icon_shape` or `icon_asset`). Its responsive composition uses
the current canvas, surface, border and accent tokens in native and hosted
apps. A quiet arc moves with `now`; `reduced_motion` keeps it still. Set
`has_progress`, `completed` and `total` only for measured work, to show a
slim standard progress bar instead. It never invents a percentage. The same
surface is exported by `Widgets`; title/detail class IDs allow text styling.
Use `loading_indicator` for small inline waits, with the same theme accent.

## Game touch controls

Import `kryon/cursor` for `CursorSet(session, CursorProps) -> bool`,
`CursorSetDefault(CursorProps) -> bool`, `CursorSetPointing(CursorProps) -> bool`
and `CursorHover(session, shape) -> bool`. A request persists until the next
deliberate change: repeating it every frame changes nothing, frames never clear
it, and the empty request `.{}` withdraws it. `CursorSetDefault` sets the
application cursor that every session inherits; it replaces the default, arrow,
clickable and disabled widget shapes, while text, crosshair and resize shapes
still show through. `CursorSetPointing` sets the application's pointing cursor,
shown instead only where a click does something: over clickable widgets and
over targets the app marks with `CursorHover`. Without one, clickable shows the
application cursor. A session's own request wins over all of them. Requests
copy their asset path; disposing or closing a session drops its request.

A target the app draws and hit-tests itself (a painted hotspot, a field)
reports the shape under the pointer with `CursorHover(session, shape)` while it
builds the frame, from the same test that decides its clicks. A frame keeps its
highest-priority mark, and its commit replaces the previous frame's, so the
cursor changes only when the hovered target does; a retained, cancelled or
failed frame keeps the committed mark, and a frame without one shows the
default. A widget with its own shape under the pointer wins over the mark,
except a disabled one the app's mark covers. A `Press` the app refines with its
own hit test, such as a painted shape inside its bounds, sets
`PressProps.cursor` to `CursorArrow` while the pointer is off that shape; only
the cursor changes, never presses, touch or keys. Invalid shapes and inactive
sessions return false.

```zi
#import "kryon/cursor"
// Once at startup: a calm arrow everywhere, a hand only where a click works.
unused CursorSetDefault(CursorProps.{.asset_path = "assets/arrow.webp",
    .width = 32, .height = 32, .hotspot_x = 6, .hotspot_y = 2})
unused CursorSetPointing(CursorProps.{.asset_path = "assets/hand.webp",
    .width = 32, .height = 32, .hotspot_x = 8, .hotspot_y = 1})
// Each frame, from the hit test that opens the painted door.
if DoorAt(pointer.x, pointer.y) { unused CursorHover(session, cast(s32)CursorShape.CursorClickable) }
// A deliberate tool state; .{} returns to the application cursors.
unused CursorSet(session, CursorProps.{.asset_path = "assets/can.webp",
    .width = 32, .height = 32, .hotspot_x = 4, .hotspot_y = 6})
```

`width`, `height` and the integer hotspot are logical units, each size 1–128,
independent of the source image size and UI zoom; the hotspot is measured from
the top left and must be inside. Hosts rasterize at their own pixel density
(desktop `XCURSOR_SIZE`/24, web `devicePixelRatio`), so give a source at least
two to four times the logical size. `asset_path` is at most 4095 bytes with no
NUL. With no asset, use `shape` (the `CursorShape` values 0–10) or `hidden`;
size and hotspot must then be zero. An asset and `hidden` together are invalid.
Invalid requests return false and keep the last valid request. A missing or
undecodable image falls back to the requested system shape; only an explicit
hidden request hides the pointer. `CursorResolve(session, widget_shape)` returns
what a host shows.

The SDL/Cairo desktop host applies a real bitmap cursor scoped to its own
windows, caches decoded cursors, and calls the system only when the resolved
cursor changes, so switching between requests never flickers. X11 receives
premultiplied pixels and other drivers straight alpha, so soft edges show no
light fringe. It restores the system cursor on close. Canvas and DOM draw the
image into a CSS cursor (a 1x image plus an `image-set` density entry) with the
same hotspot on the canvas or root, and DOM widgets inherit it. Other hosts
retain their system pointer; touch feedback is drawn by the application. These
requests never move, lock, grab or warp a pointer.

Portable hosted apps use `CursorSet` and `CursorHover` with `KRYON_HOSTED_UI`
(the application cursors belong to their host). Their only effects are
`cursor.CursorSetHostedHost` and `cursor.CursorHoverHostedHost`, which marks
only the active session's building frame; `WidgetHostCall` requires the active
`WidgetHostBegin` session. Asset requests must name a normalized packed path
under `assets/`, pass that instance's existing asset policy, and resolve through
its existing asset resolver. The host copies the resolved path before returning;
no guest image bytes, pointer or native cursor handle crosses this boundary.
An unavailable resolution returns false. Hosts apply the resolved cursor after
committing the frame with `DesktopCursor(session)` or `CanvasCursor(session)`;
the standard loops already do this. Custom hosts must restore cursors before
teardown.
The generic desktop player extracts packed assets into a private directory under
`TMPDIR` (an existing absolute directory), or the system default when unset,
and removes that directory on normal exit.

`kryon/joystick` provides `SwipeArea` (swipes and taps over a board) and
its `secondary_tap` option reports a right click as a tap at that position.
It is off by default. The same module provides
`Joystick` (a stick to lean). `kryon/direction_pad` provides
`DirectionPad(session, DirectionPadProps)`, a four-way pad like a game
controller's D-pad, drawn in the largest square of its bounds. Directions are
0 left, 1 up, 2 right and 3 down. It returns the arm held (`direction`) and
`step`: true when an arm is pressed or the thumb slides onto another arm,
then while held after twice `repeat_seconds` and at each repeat, like a held
arrow key. The hub stops, and a quick tap steps once. `floating` lays it
over a board: lighter arms and outlined arrows let the pieces under it show
through, and drawn after the board it takes the presses there. It is composed
from `Press`, `Box` and `SvgIcon`, so hosted games can use it.

`DirectionPadBounds(usable_viewport)` places a floating pad at the bottom left
with 24–32 logical units of edge padding and a 168–216 unit side on normal
phone screens, in either orientation. Pass the area inside system insets and
app bars; very small embedded areas shrink the pad to fit.

`joystick.SwipeFeedback(session, SwipeFeedbackProps.{key, gesture, size})`
draws a faint base at a `SwipeAreaResult`'s contact start and a glowing ball
following the finger, clamped to that base. It disappears on release and
does not take input. Draw it after the board; `size` is the base diameter
in logical units (56 by default). The app keeps its movement rules.

## Rotated text

`TextProps.rotation` rotates text clockwise in degrees about `bounds` top left
plus `TextProps.origin` (logical units). Zero keeps horizontal text; `origin =
Vector2.{bounds.width / 2.0, bounds.height / 2.0}` rotates about the box centre.
Layout and font sizes stay unchanged. Native Raylib (including Android), Canvas
and DOM preserve the same pivot; local text clipping rotates with the text,
while ancestor clipping stays in its original coordinate space.

## Reading pages

`text_measure.TextPage(session, props, start)` returns a UTF-8 byte range
(`start`, `end`, `next`) that fits `props.bounds`. Draw `text[start:end]` with
the same Text props and continue at `next`. Only separating whitespace is
skipped; a viewport shorter than a line reports no progress. Apps keep their
own content anchors and page indexes so reflow preserves the reading place.

`TextProps.line_gap` supplies extra units between lines. `typeface` names a
registered face or a font asset. Hosted fonts pass the same asset policy as
images; absolute host paths are refused. Raylib and Canvas select the face
for measurement and for queued paint, restoring the interface font afterward.

`TextProps.wrap = TextWrap.TextWrapAnywhere` wraps at extended Unicode grapheme
boundaries, including inside unbroken usernames. Combining marks, emoji ZWJ
sequences and flags stay whole. Spaces are preserved and CR, LF and CRLF end
lines. A single grapheme wider than the bounds stays whole on its own line;
nonpositive widths disable wrapping. The default `TextWrapAuto` still wraps at
word boundaries and keeps oversized Latin words intact. Supply enough height
for the resulting lines.

## View transforms

`raylib_runtime.RaylibTouches(points: []Vector2) -> s32` reads the most recent
successful `PollRaylib` touch snapshot. It returns the active count clamped to
0–8 and writes only the first `min(count, points.count)` coordinates; an empty
slice can query the count. Coordinates include the Android surface offset
and use the current `RaylibZoom()`, like `PollPointer`. Unwritten entries stay
unchanged. Repeated calls do not poll or consume input, including on the app
worker. Before the first poll, after shutdown or a terminal poll, and while
unfocused the count is zero. Point order follows raylib, without stable finger
identities; applications own gesture tracking and pinch/pan behavior.

`view_transform.ViewFit(viewport, page_width, page_height, maximum)` returns
the state for a page that fills its viewport at zoom 1. The pure mapping and
mutation helpers convert between page and screen coordinates, zoom at a point,
pan, resize while preserving the visible centre, and drive a two-finger pinch.
Every mutation keeps zoom in range and leaves no empty space around the page.
Hosts observe wheel, pointer, and touch input and pass those observations in;
`ViewWheelFactor(notches)` supplies the bounded 1.2-per-notch wheel factor.

## Asset audio

`kryon/Media` retains `MediaOpen`, pause/stop/state/message and text fetching.
It also exports `MediaAudio(session, command: MediaAudioCommand) ->
MediaAudioResult`. A hosted bundle uses `KRYON_HOSTED_UI` and requests only
`media_audio.MediaAudioHost` for this function. The same types/function are
available directly from `kryon/media_audio`.

`MediaAudioCommand` has `operation: MediaAudioOperation`, `voice: s32`,
`asset: string`, `music: bool`, `looping: bool`, and `gain: float32`, plus the PCM fields below.
`MediaAudioOperation` is `Capabilities`, `Load`, `Play`, `Gain`, `Stop`,
`Unload`, `Status`, `CreateStream`, `SubmitStream`, `Pause`, or `Resume`. Voice IDs are caller-selected `1..16`, local to one
actual app instance. Each loaded voice has an independent decoder and gain;
several effects and music voices can play together. `Load` uses `asset`,
`music`, and `gain`; unload an occupied ID before reloading it. `Play` starts
from the beginning and uses `looping`, which is accepted only on a voice
loaded with `music = true`. `Gain` changes only that voice. Gain must be
finite and in `0..1`; out-of-range, NaN and infinite values are rejected,
without clamping or changing playback. Zero-initialized gain means silence.
`Stop` retains a decoded voice for replay; `Unload` stops and frees it.
`Status` updates the stream and reports actual completion. `Capabilities`
does not need a voice ID. There is no queued playback or automatic transition.

`MediaAudioResult` owns `message: string` across the hosted boundary, with
`code: MediaAudioCode`, `state: MediaAudioState`, `capabilities: u32`, and
`writable: bool` for PCM streams, `has_position: bool`, and `position_seconds: float64`.
Codes are `Ok`, `Unsupported` (no provider), `Unavailable` (no device/reader
or an operation already in flight), `Suspended` (browser context cannot play,
including before an autoplay-unlocking gesture), `Invalid`, `Denied`,
`Missing`, `Limit`, `Failed` (provider/decode failure), and `Backpressure`
(a full PCM stream). States are `Unloaded`,
`Ready`, `Playing`, `Stopped`, `Finished`, and `Paused`. A failed command does not
imply playing; inspect `code` first. Capability bits 1, 2 and 4 mean asset
effects, asset music and gain. Providers retain those capability bits when
the device is unavailable or suspended. Bit 8 advertises bounded PCM streams.

Bits 16, 32 and 64 advertise asset pause/resume, asset-music position, and
PCM pause/resume respectively. An older or partial provider need not implement
these features; check its bits and result code. Existing operation/state wire
values are unchanged; Pause, Resume and Paused are appended.

`Pause` changes Playing to Paused, preserving the individual voice's offset,
loop, gain and queued PCM copies. Repeating Pause on Paused succeeds. `Resume`
changes Paused to Playing; repeating it on Playing succeeds only when the
device/context is available. Both operations reject Ready, Stopped and Finished
with Invalid, and unloaded IDs with Missing. Resume never starts a new voice or
replays a stopped one. Play remains an explicit restart from the beginning.
Neither operation suspends the shared device/context or another owner.

Asset-music Status sets `has_position` only when `position_seconds` contains a
finite, nonnegative provider playhead in seconds, within the current loop
iteration (it wraps at the loop boundary). Paused time freezes. Browser time
also freezes during external context suspension, using AudioContext.currentTime;
Status then reports Suspended with its available frozen position. Resume while
suspended returns Suspended and queues nothing; retry explicitly after unlocking.
PCM and effects have `has_position = false`; zero seconds alone never indicates
support. PCM exposes neither a drained counter nor an estimated elapsed duration.
Paused PCM is not writable and rejects submission with Invalid, preserving the
same bounded queued copies until Resume; Stop and Unload clear those copies.

```zi
using Media :: #import "kryon/Media";
// Once, after verifying the capability/result:
loaded := MediaAudio(session, MediaAudioCommand.{.operation = .Load,
    .voice = 1, .asset = "assets/my-game/chime.wav", .gain = .6})
if loaded.code == .Ok {
    played := MediaAudio(session, MediaAudioCommand.{.operation = .Play, .voice = 1})
    // Consume played.code in the application's audio policy.
}
```

`CreateStream` occupies one of the same 16 voices. Set `format: MediaAudioFormat`
to `S16LE` (signed little-endian 16-bit interleaved samples), `sample_rate` to
8000–48000 Hz, `channels` to 1 or 2, `frames_per_block` to 2048–8192, and
`gain` to a finite value in `0..1`. These are the provider's supported bounds;
no other PCM formats are advertised. Creation accepts no samples. `Play`
starts an empty stream; repeating it clears pending samples. `SubmitStream`
uses `pcm: string` as binary bytes, including NULs, and requires exactly
`frames_per_block * channels * 2` bytes (at most 32768). Samples are copied
before return; the guest retains no host pointer. Format/rate/channels are
fixed until `Unload`; submission uses the voice's creation settings.

`Status` returns `writable: bool`, true only for a running stream with room
for a complete block. `SubmitStream` returns `Backpressure` without accepting
any bytes when full; retry the same block later. `Ok` means the entire block
was copied. The provider holds at most two blocks in its existing stream
buffers; no application command queue is added. `Stop` clears queued samples,
`Gain` changes only that voice, and `Unload` frees the stream. Streams remain
`Playing` across frames and underruns, emitting silence while empty: they are
open-ended and do not claim `Finished` or a precise drained count. Asset voices
retain their existing completion semantics. Suspended/unavailable providers
reject creation, play and submission without queuing them for later autoplay;
after a trusted browser gesture the app must retry explicitly. Owner reset,
revocation, close and successful restart dispose streams with asset voices.
Wurm's synthesis, phrase scheduling, crossfades and game policy stay in Wurm;
its existing mono 22050 Hz / 2048-frame transport fits these bounds.

Only packed paths under `assets/` pass: absolute paths, empty path components,
traversal, backslashes and schemes are refused before the host's existing
allowed-asset callback and packed reader. An asset is at most 8 MiB; there
are at most 16 loaded voices per owner and no command queue. Resource bytes
are copied/decoded into provider-owned storage; no native pointer or decoder
handle crosses the VM. The host reply codec uses named fields, and the VM
copies its strings before the host reuses the reply buffers.

Hosts import `kryon/media_audio_raylib` and call `MediaAudioUseRaylib()` once
on their owning thread. It reuses Raylib memory music streams on native
Raylib hosts and the current Canvas WebAudio engine on the web. The decoder
is chosen by content, not name: bytes starting with an Ogg page whose first
packet is `OpusHead` decode through the `audio` package (libopus) on Linux,
Windows and Android, streamed into a Raylib audio stream in two 2048-frame
blocks that `Status` and `MediaAudioOwnerTick` refill. Other bytes, including
Vorbis Ogg, stay on Raylib's decoders. These hosts link libopus: `-lopus -lm`
from Kryon's makefiles, or the static `lib/libopus.a` in the `renderer-raylib`
artifact (Android builds the package's `opus` target). On macOS, Ogg Opus
currently returns `Unsupported`; the browser decodes Opus itself. Linux,
Windows (MinGW under Wine), Android device and browser behavior is tested
with null or muted audio. There is no Seek or duration operation. A browser
reports `Suspended` before its context runs: the app can retry after a real
user gesture; no play request is queued to run in another app later.
The standalone `RaylibBundleOpen` installs this provider. Other hosts that
have not installed one receive `Unsupported`, regardless of platform name.

`HostedApp.audio` stores the owner for the actual VM instance. The existing
`AppContextCall` dispatches audio only in an identified app's active widget
frame, validates its session and existing widget asset policy, and reads
only that runtime's packed resources. `AppContextRun` / `AppContextRunFrame`
service streams; audio commands disable quiet-frame replay so effects cannot
repeat as cached UI. `HostedAppClose` and successful `HostedAppRestart`
release all voices. Failed restart preserves the previous instance's audio.
Before changing an existing context's account/resource ownership, the host
must call `AppContextAudioReset(app)`; it revokes in-flight decoding too.
Never use the outer Session alone as the resource owner.

A custom hosted widget host can supply its own instance's `MediaAudioOwner`
and packed reader as the final optional arguments to
`WidgetHostBegin(session, allowed, resolve, audio, audio_read, audio_context)`.
The original arguments remain sufficient. Without an owner, audio returns
`Unsupported`. `WidgetHostEnd` clears dispatch only; music survives frames.
The custom host must call `MediaAudioOwnerClose(owner)` on actual disposal,
account/resource context changes and instance close. Keep the owner storage
alive until an in-flight synchronous host call returns; a browser decode may
yield. A revoked decode discards its temporary resource before returning.

Native app hosts can use `MediaAudioBegin(owner, session, allowed, read,
context)` / `MediaAudioEnd()` around each callback, and
`MediaAudioOwnerTick(owner)` once per host frame while voices continue.
Binding a changed session or resource context closes previous voices;
close explicitly before replacing a reader or changing its policy in place.
closing an invalid session is also detected by the next tick. Owner close
is explicit and idempotent. Closing one owner never closes the shared device
or another owner's voices. Hosts can register a different actual provider
with `MediaAudioUseProvider`; existing owners retain their provider until
closed. The caller retains cue selection, score/synthesis, volume UI, mute,
phrase scheduling and any single-in-flight music transition policy.

## Document persistence

The existing `kryon/app_context` export provides `DocumentStorageReady() -> bool`
and `DocumentFlush() -> bool`, bound to `app_context:DocumentStorageReadyHost`
and `app_context:DocumentFlushHost` (both zero arguments, bool result).
`AppLocal` and the direct Canvas/DOM app host implement them. Other hosts must
explicitly grant and dispatch these capabilities; importing them grants no access.

Readiness is nonblocking: true requires an open private document namespace and,
in a browser, successful initial IndexedDB hydration. Before readiness, reads
report `DOCUMENT_ERROR`, writes and flush return false. The host opens storage
with `AppFilesOpen` after hydration; the guest does not initialize host storage.

`DocumentWrite`/`WriteStore` retain their queued browser semantics. A true write
result alone does not acknowledge persistence. After successful writes,
`DocumentFlush` returns true only when the backing store has acknowledged a
snapshot including those writes. It waits for any previous snapshot, then
requests and awaits a fresh one. Later writes remain in memory for a subsequent
flush; this operation neither clears a host's pending-write flag nor reloads
files from storage. Failure, unavailable storage, a 15-second browser deadline,
or namespace revocation returns false. False does not roll back queued bytes
or promise that an already running backend transaction has been cancelled.

The native provider syncs file bytes before atomic replacement and syncs parent
directories after rename, including new ancestors. Platforms/filesystems that
cannot sync directories reject that write rather than claim durability. The
browser reuses IDBFS and Asyncify; its acknowledgement is the successful IDBFS
transaction callback, subject to the browser's persistence/eviction policy.
Atomic replacements have distinct mtimes even within one millisecond, as IDBFS
uses mtimes to detect changes.

Hosts use `AppFilesReady(files)` and `AppFilesFlush(files)` for their authorized
`AppFiles`. The no-argument guest operation cannot name or read another node.
The browser backing store is the existing shared `/kryon-apps` mount: the host's
physical sync can include other queued namespaces, without exposing their paths,
bytes, handles, or authority to the guest. An AppFiles outside this mount can still stage packed assets, but reports
false for durable readiness and flush. Initial hydration is never restarted by flush.

Call on the owning thread. Browser flush yields; keep the owner and its AppFiles
alive until it returns and never reenter its running VM while suspended. Close
with `AppFilesClose` before changing account/resource ownership or rebinding the
namespace; close/open increments a generation, and an in-flight flush then
returns false even if its old snapshot succeeds. Do not overwrite live AppFiles
or mutate its root/lock directly. Readiness is availability, not an account grant.

`make -j1 document-storage-test` exercises native and actual disposable headless
IndexedDB storage, source/saved compilation and portable AppLocal dispatch.

## Local notifications

`kryon/notification_schedule` schedules Android OS reminders independently of
push, network access, and the running app loop. `DailyNotificationsSupported()`
reports scheduler support independently of permission. Other native platforms
return false; portable apps use their authenticated host capability.

- `ScheduleDailyNotification(props, hour, minute=0, suppress_on_open=false,
  not_before_unix_ms=0, weekdays=127, skip_local_date=0)` persists one rule.
  Weekday bits are Sunday=1 through Saturday=64. Times use the device's current
  timezone; `not_before_unix_ms` is an absolute UTC lower bound.
  `skip_local_date` excludes one YYYYMMDD date, for example today's completion.
- `ScheduleNotificationAt(props, year, month, day, hour, minute=0)` schedules
  one local date; an elapsed date/time is rejected.
- `ScheduleNotificationDates(props, dates, hour, minute=0)` keeps one alarm for
  a caller-evaluated, finite sequence of at most 3660 strictly increasing valid
  YYYYMMDD dates. Apps own recurrence and refresh this explicit horizon.
- `CancelDailyNotification(tag, id)` cancels any of those rule types and its
  visible notification. Reusing `(tag,id)` replaces the rule.
- `NotificationAppOpened(tag,id)` durably marks that particular rule's local
  date and cancels its visible notice. Games call it on actual game entry,
  resume and local date changes while playing. A collection host must never
  mark its other games opened when the host foregrounds.

Scheduling persists even before permission is granted and never prompts.
Applications request permission through `notification.RequestNotifications`;
Kryon's Android host prompts once and preserves a refusal. System notification
settings remain authoritative. Failed scheduling returns false. Android's
inexact alarms can arrive later during idle; force-stopped applications cannot
receive alarms until relaunched. Reboot, clock, timezone and package-replacement
broadcasts rebuild future alarms from private preferences. Dates already opened
or delivered remain suppressed when the clock moves backward. Android allows at
most 480 active Kryon rules per host package and 64 per authenticated hosted app;
replacements do not consume another slot and cancellation frees it.

The Android scaffold includes the notification receiver and permissions. Existing
Android hosts must include `org.kryonlabs.notifications.NotificationReceiver`
from the canonical scaffold, declare POST_NOTIFICATIONS and
RECEIVE_BOOT_COMPLETED, and merge its receiver actions from the scaffold manifest.
Include `android_notifications` in the native graph so receiver exports survive
linking. `notification_desktop` dispatches immediate notification effects to the
Android implementation when ANDROID_BUILD is defined. Custom activities provide
`requestKryonNotifications()` using the scaffold's UI-thread callback.

In-app `ToastProps.seconds` is independent: a toast's caller chooses display
length. OS notifications have no app-chosen toast deadline and remain subject
to Android's notification settings and user dismissal.

## Compiled canvas host extensions

`canvas_app_run.CanvasAppMainLoop(false)` runs an AppContext app compiled into
its page. Bind `canvas_app_run:CanvasAppMain=app:Main`. Private documents retain
`/kryon-apps/<app id>` on IndexedDB. The bare runner supports mini mode only.

A host may install `canvas_app_hooks.CanvasAppHooks` with `CanvasAppUseHooks`
before entering the loop. Every callback is optional: `start(mini) -> bool`
runs once after persistent storage and the app's documents are open; `pump()`
runs before input each frame; `stop() -> bool` runs after a successful start, before
storage flush and teardown. A false stop result makes the loop fail after cleanup.
A failed start must clean up its own partial work.
`supports(capability)` and `service(name, request)` provide host-owned optional
services. An absent service returns the usual unavailable JSON response.

`frame(session, bounds, key, modifiers, input) -> s32` runs inside the runner's
frame. Return 0 to run the app, 1 to replace its entire frame, or -1 to fail the
loop. Replacement frames exclusively receive input; the app callback is skipped,
including on the frame that closes an overlay. The host draws widgets but never
calls BeginFrame/EndFrame. Callbacks run on the frame thread. Account, payment,
authentication and application navigation policies belong to the embedding
package. A host decides which services it offers in mini mode.
