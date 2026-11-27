# Frame capture, replay and inspection

## Native fixture preview

To capture the whole app instead, without a fixture, run
`kryon run desktop --png FILE.png --width PX --height PX --theme light|dark`
([Projects](PROJECTS.md)); it writes the same PNG and JSON.

From a Linux project with a desktop C99 profile, run:

```sh
kryon preview controls --theme light --width 800 --height 480 --output build/controls.png
ziran tool kryon preview controls --theme dark --width 360 --height 640 --output build/controls-dark
```

`--output` accepts a PNG path or a prefix, relative to the project root or
absolute. Its parent directory must exist. Both forms write `<prefix>.png`
and `<prefix>.json`. Each dimension accepts 1–4096 pixels. The project default
profile is used unless `--profile NAME` selects another desktop C99 profile.

Register `previews` in `[package] module_roots`, for example
`module_roots = ["src", "previews"]`. Each fixture is a uniquely named module
at `previews/<fixture>.zi`. Its entry is an ordinary program: `main` runs
`App.Run` with the module that draws the screen:

```zi
App :: #import "kryon/app";
Button :: #import "kryon/button";
#import "kryon/geometry"
#import "kryon/session"

Frame :: (frame: App.Frame) {
    session := Session.{.slot = frame.slot, .epoch = frame.epoch}
    viewport := frame.viewport
    unused Button.Draw(session, .{.key = cast(u64)7,
        .bounds = .{16.0, 16.0, viewport.width - 32.0, 36.0}, .label = "Save"})
}

main :: () -> s32 { return App.Run(App.Window.{.title = "save_button"}, Frame) }
```

Compose the frame from ordinary widget calls. Fixture code can import the project's
ordinary view modules and assets. Use explicit sample data and the supplied
viewport. The preview runs offscreen with the native Cairo renderer, neutral
input, zero frame time and a fixed light or dark palette with reduced motion.
A frame can be recomposed while smooth image copies finish, so keep fixture
state stable across calls. Repeat runs with the same fixture, assets and
renderer/font versions produce the same output when the fixture uses fixed
data and host-supplied time. The command exits after capturing the artifacts.

The JSON contains `version`, `width`, `height` and `nodes` in retained-tree
order. Each node has `index` and `parent` (`-1` at the root), `kind`, `label`
where available, `x`, `y`, `width`, `height`, and clipped `visible_*` bounds.
`role` uses the semantic role when available and the widget kind otherwise.
`key` is an unsigned 64-bit widget key, which a consumer
should parse without losing integer precision. Where supplied, `focus_id`
identifies a focus target. Bounds are viewport pixels reported to one decimal
place. Secure field values are redacted as in test-driver inspection.

## Portable frame replay

`EndFrameCapture(session)` commits a frame and records its retained nodes and
resolved paint commands before emitting raster effects. It replaces
`EndFrame(session)` in a capture-enabled application. Bind all five capture
capabilities declared in `frame_capture.zi`: `RecordFrameStart`, `RecordNode`,
`RecordPaint`, `RecordFrameEnd` and `RecordFrameFailure`. A host that binds
only some fails with `missing host capability: frame_capture:RecordFrameFailure`;
`tools/frame-replay.sh` builds `tools/frame_replay_cli.zi`, which binds all five.
A rejected frame reports its status, message, widget key and kind and emits no
raster effects. Ordinary `EndFrame()` needs no capture bindings.

Run a portable bundle with:

```sh
tools/frame-replay.sh app.zx app trace.txt capture.json
tools/frame-inspect.sh capture.json 3 1
tools/frame-inspect.sh --diff baseline.json candidate.json
```

The inspector prints a frame's inputs and nodes, or a selected node with its
paint commands, colors, font sizes and clips. Comparison reports the first
changed JSON field, ignores object key order and preserves exact integer
identities. Exit codes are 0 for matching/valid frames, 1 for differences or
rejected frames, and 2 for invalid arguments or files. Files are limited to
64 MiB each.

The entry takes no arguments, returns an integer, and samples exactly one
input record per frame. Declare a module-local `PollFrameInput() -> FrameInput` and use the
twenty-column format:

```
x y down pressed released secondary_down secondary_pressed secondary_released key modifiers wheel width height elapsed_ms text_hex ime_phase ime_cursor ime_selection ime_hex clipboard_hex
```

Boolean fields are 0 or 1. Modifiers use Shift=1, Control/Command=2, Alt=4.
Key codes are the same codes accepted by `KeyboardSend`. Composition phases
are none=0, start=1, update=2, commit=3, cancel=4. Composition cursor and
selection lengths count UTF-8 bytes. Text, composition and clipboard payloads
are hexadecimal UTF-8 bytes, with `-` for empty; each payload is bounded to
256 bytes. Coordinates and wheel values must be finite; the viewport is
positive and elapsed milliseconds nonnegative. Blank lines and `#` comments
are accepted. A trace holds at most 1,024 samples within 1 MiB.

`FrameInputApply` supplies session pointer, key, modifier and wheel inputs;
`FrameInputField` converts text and composition into a `TextField.Input`.
Applications explicitly use the recorded viewport and elapsed time rather
than reading wall time. Clipboard reads use the recorded input and clipboard
writes appear in the JSON. Captures use format version 2.

Capture includes focus, read-only state, selection and committed editor text.
Secure editor values, lengths and selections are redacted. A document larger
than 64 KiB retains its length but omits its full value from the capture.
The application still owns state and applies the edits returned by widgets.
Replay uses fixed font measurements and a headless raster sink, so it checks
UI decisions rather than platform-specific pixels or font fallback.

`tests/ziran_frame_capture_test.sh` and `tests/ziran_frame_input_test.sh`
compare source and saved-IR bundles and exercise edits, IME, clipboard,
resizing, rejection diagnostics and inspection. The runners use locked,
stable build directories and scrub the desktop environment.
