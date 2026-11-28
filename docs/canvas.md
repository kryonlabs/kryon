# Canvas browser host

A browser integration roots `src/backend/canvas_raster.zi` with the selected
host and defines `PLATFORM_WEB`. Generate native sources with Ziran, then link
them with Emscripten, and Ziran's Web bridge:

```
--js-library $ZIRAN_DIR/web/ziran_web.js
-sEXPORTED_RUNTIME_METHODS=FS
-sASYNCIFY -sSINGLE_FILE=1 -sEXIT_RUNTIME=1 -sENVIRONMENT=web
```

The browser hosts (`page_core.zi`,
`page_paint.zi`, `page_texture.zi`, `page_font.zi`, `page_events.zi`,
`page_files.zi`, `page_input.zi`, `page_mesh.zi`, `canvas_audio_host.zi`) are
written in Ziran and reach the page through `std/web.zi`; the one JavaScript
file is the generic, Kryon-agnostic bridge in the Ziran repository.

The page presents through one WebGL2 context on the visible canvas whose
drawing buffer survives composites: a repaint rasterises only its damage
rectangle and the browser shows the rest of the frame unchanged, with no
second canvas and no full-frame redraw. Shapes stream into one batched
vertex buffer and shade with analytic coverage; text runs and images draw
as textured quads from retained atlases. Hidden 2D canvases only rasterise
the glyph bitmaps that feed those atlases, so no pixel is ever presented
through a 2D context. A browser without WebGL2 leaves the page blank: the
Canvas2D painter is deleted, not kept as a fallback.
The UI host retains its existing back buffer. It compares checked paint plans,
keeps unchanged pixels and clears/replays only the damaged region. Session,
viewport, display-density, background and raster-resource changes repaint as
needed. Ordinary host paint and custom emitters use complete painting. Text
measurement leaves retained pixels intact. Ordinary frames add no extra full-size
surface. When a canvas dimension actually changes, the host temporarily blits
the completed drawing buffer into a GPU target and restores it at the new size
before returning to the browser. That target is immediately freed; resize never
presents the empty buffer while the application prepares its next frame.
The dedicated `canvas_raster.zi` provider
calls the Canvas modules directly, so retained widgets use the same paint
queue, font selection, and clipping path as the native host. Frame-profile builds retain the selected `_run` host and `canvas_raster.zi`
as generation roots for raster and typeface adapters. A program entry owns
its own `main`.

`RequestWindowClose` is currently a raw browser host capability. An application
may declare it against `host_api`; the host records the request and observes it
on the next poll so the current frame can finish committing.

Ziran owns frame timing, image memory, font atlas packing, text layout, file
callbacks, audio decoding and conversion, resource APIs, and every call into
the browser's events, Canvas2D, FontFace, WebAudio, clipboard, and Emscripten
filesystem. Asynchronous browser calls carry
Emscripten async metadata and require Asyncify.

`EndDrawing` yields to the next browser frame for ordinary canvas loops. An
application using `emscripten_set_main_loop` must call
`SetExternalFramePacing(true)` before starting that loop, so only the callback
scheduler waits for frames. `SetTargetFPS` then adjusts that scheduler's RAF
interval; it does not suspend `EndDrawing`. Opening a window resets this mode.

The host finds `#canvas` and fills its `#canvas-frame` container when one is
present, otherwise the canvas parent. It draws at the canvas's own CSS size, so
a frame padded for safe-area insets still matches where touches land, and maps
pointer positions through the canvas's shown box. Include UTF-8 metadata in
HTML shells, including shells using Emscripten's single-file Wasm output.
Only this host's event listeners and resize observer are removed on close.
A subsequent open creates new input and texture state; cached default font
memory is released before closing the canvas.

A `#canvas` that carries the `data-transparent` attribute has no background:
the host clears the window and each frame to alpha 0 instead of Kryon's frame
colour, so the page behind it shows wherever the program draws nothing, and
widgets draw over it as usual. The editor preview sets it; without it every
pixel is opaque.

Applications that handle Escape as navigation can call `CanvasSetExitKey(0)`
through `kryon/Canvas` to disable the host's default Escape-to-exit behavior.
Ordered keys and text remain available through `CanvasInputTake(session)`.

Custom Canvas hosts can use `BeginRasterClip(bounds)` and
`EndRasterClip(saved)` through `kryon/Canvas` to intersect a rendering scope
with an outer viewport, restoring the previous clip afterward.

Images are RGBA8 (`format = 7`). Individual pixel allocations are limited to
64 MiB and 16384 pixels per dimension. Tint caching retains at most 32 entries
and 16 MiB of pixels; releasing or updating a texture invalidates its cached
copies. Frame input queues hold at most 1024 events. Audio streams enqueue at
most two pending buffers and report their actual readiness.

A lost GPU context releases its resources and stops drawing until the browser
restores the context: the program and buffers rebuild, texture owners
re-upload on their next use, and caches holding GPU textures re-rasterise
instead of sampling now-empty uploads.

Browser window position, monitor selection, and native window icons have no
corresponding operation on the embedded canvas. Fullscreen and pointer lock
requests remain subject to browser gesture permissions.

## Focused checks

```
tests/canvas_backend_test.sh
python3 tests/canvas_audio_test.py
sh tests/canvas_text_os_wasm_test.sh
python3 tests/canvas_project_test.py
```

The first command compiles the complete host and checks real pixels in a new
headless Chromium process. It covers camera and nested clipping, UTF-8 input,
texture updates and render targets, asynchronous PNG round trips, a real TTF
font, frame yielding, and close/reopen ownership. Sprite pixels, sampling,
opacity and texture updates run at three display densities on the GPU
presenter and also lose and recreate their graphics context. A separate
no-WebGL run (tests/canvas_no_webgl_test.py) checks that a browser without
WebGL2 shows a blank canvas with no Canvas2D fallback and no errors. It scrubs desktop display
variables and controls only its own browser process. The other tests verify
WebAudio and text/file behavior with deterministic browser-effect fixtures. The
project test builds a real application manifest through Ziran, loads the
resulting single-file page in private headless Chromium, and checks its exit.
`EMCC`, `CHROMIUM`, `ZIRAN_DIR`, and `ZIRAN_BUILD_DIR` can select installed
tools. `ZIRAN_RUNNER` optionally wraps focused compiler calls in a caller's
serialization script; do not set it when that script already holds its lock
around the entire test.

## Canvas host performance

Before changing the host or Ziran browser bridge, preserve a compiled baseline:

```sh
python3 tests/canvas_performance.py build/scratch/canvas-performance/before
# Apply the change, then compile the same workload again.
python3 tests/canvas_performance.py build/scratch/canvas-performance/after
node tests/canvas_performance.mjs build/scratch/canvas-performance/before \
  build/scratch/canvas-performance/after build/scratch/canvas-performance/result.json
```

The runner uses private headless Chromium profiles and interleaves nine samples
per version after warmup. Each sample draws fifty frames of a 512-sprite tile
board, followed by pixel readback. An independent pixel oracle checks the
ordinary board; rotated sprites must match the baseline. Receipts preserve raw
timings and artifact hashes. This measures warm Canvas host throughput on one
shared machine; it does not establish a whole-game frame rate or a speedup on
other devices. The builder refuses to replace an existing compiled artifact.
