# Frame capture, replay and inspection

`EndFrameCapture(session)` commits a frame and records its retained nodes and
resolved paint commands before emitting raster effects. It replaces
`EndFrame(session)` in a capture-enabled application. Bind the capture
capabilities declared in `frame_capture.zi`, including `RecordFrameFailure`.
A rejected frame reports its status, message, widget key and kind and emits no
raster effects. Ordinary `EndFrame()` needs no capture bindings.

Run a portable bundle with:

```sh
tools/frame-replay.sh app.zib app trace.txt capture.json
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
input record per frame. Existing entries may keep a module-local
`PollPointer() -> PointerFrame`. Five-column traces remain supported:

```
x y down pressed released
```

For keyboard, text, composition, wheel, resize, clipboard and time, declare a
module-local `PollFrameInput() -> FrameInput` and use the twenty-column format:

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
`FrameInputField` converts text and composition into a `TextFieldInput`.
Applications explicitly use the recorded viewport and elapsed time rather
than reading wall time. Clipboard reads use the recorded input and clipboard
writes appear in the JSON. Rich traces produce format version 2; legacy
pointer traces keep version 1.

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
