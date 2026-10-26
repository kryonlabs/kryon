# Canvas2D browser host

A browser integration roots `src/backend/canvas_raster.zi` with the selected
host and defines `PLATFORM_WEB`. Generate native sources with Ziran, then link
them with Emscripten, and Ziran's Web bridge:

```
--js-library $ZIRAN_DIR/web/ziran_web.js
-sEXPORTED_RUNTIME_METHODS=FS
-sASYNCIFY -sSINGLE_FILE=1 -sEXIT_RUNTIME=1 -sENVIRONMENT=web
```

Kryon contains no JavaScript or C. The browser hosts (`page_core.zi`,
`page_paint.zi`, `page_texture.zi`, `page_font.zi`, `page_events.zi`,
`page_files.zi`, `page_input.zi`, `page_mesh.zi`, `canvas_audio_host.zi`, `dom_host.zi`) are
written in Ziran and reach the page through `std/web.zi`; the one JavaScript
file is the generic, Kryon-agnostic bridge in the Ziran repository.

The page uses a Canvas2D context with a drawing preference that survives
occasional pixel reads. When WebGL is available, `Image` meshes share
a private surface around their visible bounds and transfer to the page together.
Indexed grids reuse shared vertices, and small size changes reuse the drawing
buffer. Texture changes retain their painter order within the batch. Other
paint, clipping, target changes, readback and presentation flush that batch
in order. Without WebGL, the same API uses the Canvas2D triangle painter.
Ordinary sprites without local rotation or mirroring draw directly in the
current camera coordinates. They preserve the caller's opacity and sampling
without copying the complete canvas state for each tile. Opacity alone does
not allocate a tinted texture.
The UI host retains its existing back buffer. It compares checked paint plans,
keeps unchanged pixels and clears/replays only the damaged region. Session,
viewport, display-density, background and raster-resource changes repaint as
needed. Ordinary host paint and custom emitters use complete painting. Text
measurement leaves retained pixels intact. This adds no extra full-size surface.
The dedicated `canvas_raster.zi` provider
calls the Canvas modules directly, so retained widgets use the same paint
queue, font selection, and clipping path as the native host. Project builds do
not pass an entry to IR or C generation: the selected `_run` module owns
`main`, while `canvas_raster.zi` remains reachable as a second root for raster
and typeface adapters.

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

The host finds `#canvas` and measures its `#canvas-frame` container when one is
present. Otherwise it measures the canvas parent. Include UTF-8 metadata in
HTML shells, including shells using Emscripten's single-file Wasm output.
Only this host's event listeners and resize observer are removed on close.
A subsequent open creates new input and texture state; cached default font
memory is released before closing the canvas.

Applications that handle Escape as navigation can call `CanvasSetExitKey(0)`
through `kryon/Canvas` to disable the host's default Escape-to-exit behavior.
Other keys remain available through `CanvasPressedKey`.

Images are RGBA8 (`format = 7`). Individual pixel allocations are limited to
64 MiB and 16384 pixels per dimension. Tint caching retains at most 32 entries
and 16 MiB of pixels; releasing or updating a texture invalidates its cached
copies. Frame input queues hold at most 1024 events. Audio streams enqueue at
most two pending buffers and report their actual readiness.

Mesh acceleration retains at most 32 source textures and 16 MiB of source
pixels; its drawing surface is limited to four million pixels and the device's
own limits. Texture updates refresh cached pixels. A lost context releases
its resources and invalidates retained paint before the next frame.

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
font, frame yielding, and close/reopen ownership. Mesh pixels, sampling,
opacity and texture updates run at three display densities with acceleration
both available and unavailable; the accelerated runs also lose and recreate
their graphics context. It scrubs desktop display
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
