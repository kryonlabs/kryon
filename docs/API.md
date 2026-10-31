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

## Widget packages

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

`pointer_input.SamplePointer()` also works inside a hosted widget frame. The
host returns the pointer sample for the session passed to `WidgetHostBegin`;
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
`ImageProps.radius` gives an image rounded corners in native and hosted UI;
a positive value overrides its style radius.

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
accent see the same palette. `WidgetPaletteRestore` restores the complete prior
token set, including host-specific colors and the absence of unset tokens.

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
the other `Key` constants. `TypedTextTake` collects the text typed since the
last frame, and `TextFieldInputFor(key, modifiers, text)` turns both into a
focused `TextField`'s input.
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

## Asset audio

`kryon/Media` retains `MediaOpen`, pause/stop/state/message and text fetching.
It also exports `MediaAudio(session, command: MediaAudioCommand) ->
MediaAudioResult`. A hosted bundle uses `KRYON_HOSTED_UI` and requests only
`media_audio.MediaAudioHost` for this function. The same types/function are
available directly from `kryon/media_audio`.

`MediaAudioCommand` has `operation: MediaAudioOperation`, `voice: s32`,
`asset: string`, `music: bool`, `looping: bool`, and `gain: float32`, plus the PCM fields below.
`MediaAudioOperation` is `Capabilities`, `Load`, `Play`, `Gain`, `Stop`,
`Unload`, `Status`, `CreateStream`, or `SubmitStream`. Voice IDs are caller-selected `1..16`, local to one
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
`writable: bool` for PCM streams.
Codes are `Ok`, `Unsupported` (no provider), `Unavailable` (no device/reader
or an operation already in flight), `Suspended` (browser context cannot play,
including before an autoplay-unlocking gesture), `Invalid`, `Denied`,
`Missing`, `Limit`, `Failed` (provider/decode failure), and `Backpressure`
(a full PCM stream). States are `Unloaded`,
`Ready`, `Playing`, `Stopped`, and `Finished`. A failed command does not
imply playing; inspect `code` first. Capability bits 1, 2 and 4 mean asset
effects, asset music and gain. Providers retain those capability bits when
the device is unavailable or suspended. Bit 8 advertises bounded PCM streams.

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
Raylib hosts (including the existing Android build boundary) and the current
Canvas WebAudio engine on the web. Native and browser behavior is tested;
Android/device execution has not been verified by this slice. A browser
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
