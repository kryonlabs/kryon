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

Run `ziran tool Kryon build` or `ziran tool Kryon run` from the application.
See [Ziran projects](PROJECTS.md) for the complete package manifest.

## Available hosts

| Profile backend | Ziran entry | Presentation |
| --- | --- | --- |
| `terminal` | `src/backend/terminal_run.zi` | 80 × 24 terminal cells via `terminal.zi` |
| `desktop` | `src/backend/desktop_run.zi` | SDL2 window with Cairo raster |
| `libdraw` | `src/backend/libdraw_run.zi` | plan9port devdraw window with the shared Cairo raster |
| `raylib` | `src/backend/raylib_run.zi` | raylib SDL2/OpenGL ES 2 window |
| `canvas` | `src/backend/canvas_run.zi` | Canvas2D page with Emscripten and Asyncify |

The desktop and libdraw hosts share `cairo_raster.zi`. The raylib host uses
`raylib_runtime.zi` and Kryon's `vendor/raylib` revision. The canvas host uses
its dedicated `canvas_raster.zi` provider over a Canvas2D browser ABI; see
[Canvas2D browser host](canvas.md). Backend build dependencies and link flags
live in `mk/ziran-project.mk`. Platform calls are declared with Ziran foreign
imports; handwritten host implementation is Ziran.

## Frame and input ownership

A host polls platform input and submits it to the session before calling the
application's `Frame`. The application and widgets consume generic session
input such as `KeyboardTake`, `TypedCodepointTake`,
`PointerWheelTake`, and pointer state. The host calls `BeginFrame` and
`EndFrame` around application composition, then presents the resulting
paint operations. Widget behavior stays in `src/ui/`.

The terminal host renders one frame when input or output is redirected. With
a TTY, it submits ASCII and UTF-8 text to the same session input API as the
window hosts and ignores terminal escape sequences. The project test drives
this through a private PTY. The other hosts can capture a frame with
`KRYON_CAPTURE_PATH`; the graphical integration tests run on private Xvfb
displays. Run `make test` for the nonvisual library gate, plus
`make desktop-project-test`,
`make raylib-project-test`, and `make libdraw-project-test` for graphical
capture and input.
Never run graphical tests against the developer's live display.

The old C backend contracts, generated compatibility headers, and
`KRYON_BACKEND` switch do not describe the current Ziran project route.
