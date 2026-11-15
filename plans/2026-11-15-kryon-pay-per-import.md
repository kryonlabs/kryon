# Pay per import: a program carries only what it imports

Owner, 2026-11-15: "why is kryon empty window generating so much c code it
should be really really small amount of code", "only what is necessary",
"i want clean import structure like import a frame it makes a frame, import a
rectangle i have a rectangle, not importing everything by default", "like in
the examples".

Nothing is imported by default. An empty window (`examples/window.zi`) is the
window, its event loop, clearing and presenting. Each example carries the
window plus what it imports. This overrides the 2026-11-13 plan's choice to
keep accessibility in every program: an empty window keeps only window-level
accessibility, and widget roles and nodes come with the widget imports.

Status: in progress (step A pushed; frame driver and accessibility split in review).

## Measurement

```sh
ziran build --target=c --project --root examples --exe --entry window:main \
    -o build/scratch/ppi/window examples/window.zi
```

| examples/window.zi | C files | C bytes | Headers | Executable |
| --- | --- | --- | --- | --- |
| Before (Kryon 07d74f6d) | 62 | 545 965 | 77 | 265 304 |
| Step A: pictures, video, Lanczos, fonts | 39 | 344 259 | 50 | 178 816 |
| After: frame driver, accessibility split, `App.Show` | 23 | 132 686 | 34 | 117 536 |

After the change `examples/window.zi` holds: window, app, app_types,
app_host_desktop, desktop_loop, desktop_runtime, desktop_keys, pixmap_sdl,
cairo_raster, raster_picture, frame_clock, frame_driver, frame_input_provider,
frame_snapshot, input_delivery, session, accessibility_linux (the application
object), atspi_protocol, std byte text, C strings, timer and UTF-8, and the
executable entry.

The per-example table is below, filled in as the work lands.

## Why an empty window is big

Ziran's pruning works: a procedure is emitted only when something reachable
calls it, a `#foreign host_api` call links the host's export of that name
only when the call is reachable, and a module with nothing reachable gets no
`.c` file. The output is large because the desktop loop calls every optional
feature directly, so each is reachable from `main`:

| Feature in an empty window | The direct call that keeps it |
| --- | --- |
| PNG/WebP/AVIF decoders, asset paths | `DesktopCursor` decodes cursor pictures (`CairoImagePixels`) |
| Lanczos cache, threads, parallel | `BeginRasterFrame` pumps, `CloseRaster` closes, capture waits, open sets its wake |
| Video | `CloseDesktop` calls `VideoStopHost` |
| Tree, paint queue, input routing | the loop runs `BeginFrame`, `EndFrameRetained`, `TreePointerUpdate`, `KeyboardSend` |
| Text input, text requests, bitmaps | `BeginFrame`, `CloseSession` and accessibility reach them |
| Cursor shapes and themes | `DesktopCursor` every frame |
| Scroll devices | `BeginScrollFrame` every frame |
| Gamepad navigation | `PollDesktop` maps controller buttons through it |
| Test driver | `TestDriverPointer`, `TestDriverWheel`, `TestDriverFrame` |
| Accessibility nodes and roles | `AccessibilityOpen` and `AccessibilitySync` publish the tree |
| Program title file | `OpenDesktopSized` reads the title file beside the program |
| Fonts | `OpenRaster` loads the Kryon font before any text |

## Design

One rule: a feature is reached only from the module a program imports to use
it, through registration, never a run-time flag.

1. **Frame driver.** The desktop loop keeps the window, events, clearing and
   presenting. Everything that needs the widget tree (input routing, frame
   begin and end, scroll frames, cursor shape, test-driver input) is one
   portable frame driver that `tree_draw` (the `Frame` export) registers into
   a small slot module from its module initializer. Without a widget import
   the loop clears and presents.
2. **Host-internal features register on first use.** The rasterizer registers
   its Lanczos pump, close and wait when it first makes a smooth copy; video
   registers its stop when a video opens; the font loads with the first text.
3. **Cursor pictures** come with `kryon/cursor` (`cursor_picture` registers a
   decoder that asks the host's `CursorPictureHost`), as in c80e022f. It was
   reverted only because Ziran's Go check treated storing a procedure as
   calling the host; that check is fixed in Ziran first.
4. **Accessibility.** The host keeps the AT-SPI connection and the
   application and window objects. Widget roles, states, children, actions
   and text come from a portable provider that the tree registers.
5. **Program title.** `App.Run` passes the window's title straight to the
   window; only the frame host reads the title file the project tool writes.

## Ziran's part

- `zi2go`'s startup check refused a module initializer that stores a
  procedure which later calls a host. Go binds the host interface before the
  first frame, so only a call made during startup is wrong. The check now
  follows calls only.

## Frame driver: where it registers

The first attempt registered the driver from `kryon/tree`'s module
initializer. Two things broke: a module with an initializer turns any
existing import cycle among the modules that import it (such as `modal` and
`modal_frame`) into a fatal startup cycle, and test builds without `--entry`
keep every `#program_export` of every imported module, so importing `tree`
suddenly required a full raster host. The driver is therefore registered by
an explicit call, `TreeFrameUse()`, from the hosts that run frames: the frame
host `desktop_run` and `App.Run(window, Draw)`. `App.Show(window)` never
makes that call, so an empty window carries none of it. Ziran has no
overloading and no compile-time branch on a generic's type, so the
window-only entry has its own name. Accessibility nodes are registered the
same way (`AccessibilityTreeUse()`).

## Steps

- [x] Ziran: Go startup check follows calls, not procedure values (test).
  Committed in Ziran; push waits for the Ziran integration lane.
- [x] Lanczos, video and fonts register on first use.
- [x] Program title passed by `App.Run`.
- [x] Cursor pictures through `kryon/cursor`. A widget host no longer
  imports the decoder: the embedding application imports `kryon/cursor`.
- [x] Frame driver registered by the hosts that run frames; desktop loop core.
- [x] Gamepad navigation, scroll devices and the test driver in the driver.
- [x] Accessibility: window-level host, node module the frame hosts register.
- [x] Ziran test per example (`make pay-per-import-test`): window, hello, box
  and image.
- [ ] Measure every example (before and after table).
- [ ] Every host: desktop, Canvas, raylib, Android, ZX players, Go.
- [ ] `make check`, CHANGELOG, commit, push.
