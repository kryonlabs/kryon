# Kryon hosts

Kryon's reusable widget, layout, input, and composition code is written in
Ziran under `src/ui/`. An application imports the `Kryon` or `Widgets`
package module and exports `Frame(session: Session, viewport: Rectangle) ->
s32`. A selected host under `src/backend/` owns the process entry point,
window or terminal, platform input, rasterization, and presentation. Ziran has
no special knowledge of Kryon widgets.

Select a host with a profile in the application's `ziran.toml`:

```toml
[tool.kryon]
default_profile = "desktop"

[tool.kryon.profiles.desktop]
backend = "desktop"
```

Run `ziran tool kryon build` or `ziran tool kryon run` from the application.
See [Ziran projects](PROJECTS.md) for the complete package manifest.

## Available hosts

| Profile backend | Ziran entry | Presentation |
| --- | --- | --- |
| `terminal` | `src/backend/terminal_run.zi` | 80 × 24 terminal cells via `terminal.zi` |
| `desktop` | `src/backend/desktop_run.zi` | SDL2 window with Cairo raster |
| `libdraw` | `src/backend/libdraw_run.zi` | plan9port devdraw window with the shared Cairo raster |
| `raylib` | `src/backend/raylib_run.zi` | raylib SDL2/OpenGL ES 2 window |
| `canvas` | `src/backend/canvas_run.zi` | Canvas2D page with Emscripten and Asyncify |
| `dom` | `src/backend/dom_run.zi` | Semantic DOM layer over the Canvas2D raster host |

The desktop and libdraw hosts share `cairo_raster.zi`. The DOM host reuses
the Canvas raster provider and adds [semantic DOM reconciliation](dom.md).
`libdraw_native.zi` is a separate experimental native Plan 9 provider: it
uses `initdisplay`, `openfont`, `gengetwindow`, `allocimage`, and `string`
directly without plan9port, Cairo, or `dlsym`. Fonts are selected with the
Plan 9 `font` environment variable, and `KRYON_OFFSCREEN=1` forces an image
with the requested raster dimensions. Rill uses this provider for rectangles,
text, font metrics, image presentation, flush, and RGBA capture. Its native
application host submits keyboard and pointer input; held modifier state and
a selectable `_run` profile remain pending.

Asset-backed `Image` accepts PNG and native Plan 9 image files. The PNG byte
and pixel rules are Ziran in `png.zi`; native decompression uses Ziran's
bounded `std/inflate_plan9` adapter to `libflate`. PNG supports all standard
color types and depths, transparency, all five filters, and Adam7 interlace.
Files are limited to 64 MiB and dimensions to 4096 pixels per axis. Sixteen-bit
samples use their high byte; ancillary color profiles and gamma are ignored.
The sixteen-entry asset cache evicts the least recently used image.

`tests/ziran_png_test.sh` compares independent reference pixels for 33 fixtures
through source and saved IR in bundles, C, C++, and Go. Taiji's private native
gate compiles the decoder and canonical `Image` rendering tests with `8c`/`8l`,
checks malformed streams, cache eviction, alpha composition, and native image
compatibility, and checks Rill's real PNG desktop icons.
The raylib host uses `raylib_runtime.zi` and the raylib revision pinned as a
Ziran source dependency. The canvas host uses its dedicated `canvas_raster.zi` provider over
a Canvas2D browser ABI; see [Canvas2D browser host](canvas.md). Backend build
dependencies and link flags live in `mk/ziran-project.mk`. Platform calls are
declared with Ziran foreign imports; handwritten host implementation is
Ziran.

Browser projects save the selected `_run` host and `canvas_raster.zi` as
generation roots without entry pruning. This keeps both the host `main` and
the raster adapters available to the paint queue. Native projects continue
through the ordinary package entry route.

## Frame and input ownership

A host polls platform input and submits it to the session before calling the
application's `Frame`. The application and widgets consume generic session
input such as `KeyboardTake`, `TypedCodepointTake`,
`PointerWheelTake`, and pointer state. `KeyboardModifiers` reports the held
Shift, Control, and Alt keys as `KeyModifierShift`, `KeyModifierControl`, and
`KeyModifierAlt` bits, so a shortcut such as Ctrl+S is a key press read with
the modifiers that were down. The desktop and raylib hosts submit them with
`KeyboardModifiersSend` before each frame; the other hosts report none.
The host calls `BeginFrame` and
`EndFrame` around application composition, then presents the resulting
paint operations. Widget behavior stays in `src/ui/`.

The terminal host renders one frame when input or output is redirected. With
a TTY, it submits ASCII and UTF-8 text to the same session input API as the
window hosts and ignores terminal escape sequences. The project test drives
this through a private PTY. The other hosts can capture a frame with
`KRYON_CAPTURE_PATH`; the graphical integration tests run on private Xvfb
displays. Run `make test` for the library, browser, and typeface gates, plus
`make desktop-project-test`, `make raylib-project-test`, and
`make libdraw-project-test` for graphical capture and input.
Never run graphical tests against the developer's live display.
