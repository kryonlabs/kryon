# Canvas2D browser host

A browser integration roots `src/backend/canvas_raster.zi` with the selected
host and defines `PLATFORM_WEB`. Generate native sources with Ziran, then link
them with Emscripten, Ziran's Web bridge, and the varargs shim:

```
--js-library $ZIRAN_DIR/web/ziran_web.js
-sEXPORTED_RUNTIME_METHODS=FS
src/backend/canvas_varargs.c
-sASYNCIFY -sSINGLE_FILE=1 -sEXIT_RUNTIME=1 -sENVIRONMENT=web
```

Kryon contains no JavaScript. The browser hosts (`page_core.zi`,
`page_paint.zi`, `page_texture.zi`, `page_font.zi`, `page_events.zi`,
`page_files.zi`, `page_input.zi`, `canvas_audio_host.zi`, `dom_host.zi`) are
written in Ziran and reach the page through `std/web.zi`; the one JavaScript
file is the generic, Kryon-agnostic bridge in the Ziran repository.

The backend uses a visible Canvas2D context directly. It does not use WebGL or
a separate presentation canvas. The dedicated `canvas_raster.zi` provider
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

The host finds `#canvas` and measures its `#canvas-frame` container when one is
present. Otherwise it measures the canvas parent. Include UTF-8 metadata in
HTML shells, including shells using Emscripten's single-file Wasm output.
Only this host's event listeners and resize observer are removed on close.
A subsequent open creates new input and texture state; cached default font
memory is released before closing the canvas.

Images are RGBA8 (`format = 7`). Individual pixel allocations are limited to
64 MiB and 16384 pixels per dimension. Tint caching retains at most 32 entries
and 16 MiB of pixels; releasing or updating a texture invalidates its cached
copies. Frame input queues hold at most 1024 events. Audio streams enqueue at
most two pending buffers and report their actual readiness.

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
font, frame yielding, and close/reopen ownership. It scrubs desktop display
variables and controls only its own browser process. The other tests verify
WebAudio and text/file behavior with deterministic browser-effect fixtures. The
project test builds a real application manifest through Ziran, loads the
resulting single-file page in private headless Chromium, and checks its exit.
`EMCC`, `CHROMIUM`, `ZIRAN_DIR`, and `ZIRAN_BUILD_DIR` can select installed
tools. `ZIRAN_RUNNER` optionally wraps focused compiler calls in a caller's
serialization script; do not set it when that script already holds its lock
around the entire test.
