# Kryon hosts

Kryon's reusable widget, layout, input, and composition code is written in
Ziran under `src/ui/`. An application imports the widget packages it uses,
such as `Text :: #import "kryon/text";`, and exports `Frame(session: Session, viewport: Rectangle) ->
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

## Renderer artifacts in build scripts

The package exports `renderer-desktop`, `renderer-desktop-windows` and `renderer-web`
artifacts for the supported native and browser targets. Request them through
`ziran pkg artifact kryon:NAME --platform OS-ARCH-ABI`, using the application's
package wrapper and locked graph. Ziran's `scripts/ziran_artifacts.py` verifies
the descriptor and its declared outputs; see its package guide for the Python
`Artifact` and `request` API.

Kryon's `scripts/renderer_artifact.py` provides the renderer-specific consumer
functions alongside its provider. Load it from the resolved `pkg path kryon`
directory and call `renderer_metadata(artifact)` with that verified artifact.
It validates the renderer/platform combination, compile/link argument types,
resources, include directories and virtual preload mounts, then returns
metadata with absolute paths. Every archive, resource, JavaScript library and
preload must be a declared artifact output. Paths resolve beside that
artifact, regardless of the build script's working directory.

`renderer_link(metadata, preloads=True)` returns ordered linker arguments,
including JavaScript libraries and browser preloads. Set `preloads=False`
when a host supplies those files separately. `compile_flags`, `defines` and
`include_directories` remain separate metadata fields for the host compiler
or CMake invocation. Application editions, asset lists and output names stay
in the application.

## Available hosts

| Profile backend | Ziran entry | Presentation |
| --- | --- | --- |
| `terminal` | `frames_terminal.zi` | 80 × 24 terminal cells via `terminal.zi` |
| `pixmap` | `frames_pixmap.zi` | Explicit headless profile; renders two frames and writes the final PPM image |
| `desktop` | `frames_desktop.zi` | SDL2 window with Cairo raster |
| `canvas` | `frames_web.zi` | Canvas2D page with Emscripten and Asyncify |
| `android` | app `src/main.zi` → `android_run.zi` | Android NativeActivity package with direct `ANativeWindow` pixel presentation |

The small application surface selects the host from the compile-time target.
Import `kryon/app` and call `App.Run(window, draw)`: C/Linux uses
`kryon/host/desktop`, while C/Web uses `kryon/host/web`. Advanced hosts can
import those named host packages directly. The desktop implementation uses
`cairo_raster.zi` and `desktop_loop.zi`.

Direct native Windows builds need a Windows host DLL even when compiled on
Linux. With MinGW installed, run `make install-windows-host` from the Kryon
checkout. This builds the current source and registers
`kryon-desktop-windows.library` alongside the Linux provider, keeping its
runtime DLLs together under `~/.local/lib/kryon/windows`. A PE executable
needs its imported DLLs beside it or on the Windows DLL search path; the
Ziran extension copies the required registered DLLs beside its PE output.

After `PollDesktop()`, custom loops call `DesktopInputTake(session)` to transfer
the next ordered editing action or printable-key/text run into that session.
The returned key uses shared codes, including 320–329 for numeric-keypad
digits, and carries the modifiers held when that key arrived.
Standalone `AppContext` applications use the compiled host
`canvas_app_run.zi`.
Polygon-clipped images use native Cairo/Canvas path clips. Complex polygons and pixel hosts use
clipped spans while preserving the original source mapping.
SDL pixel hosts use software presentation by default. `SDL_RENDER_DRIVER`
retains explicit driver selection; `KRYON_PIXMAP_GPU=1` requests accelerated
presentation. Pixel rasterization remains on CPU workers unless the host
explicitly enables a GPU painter.
Desktop windows become visible only after their first complete frame has
been uploaded and presented. A failed initial upload or copy leaves the
window hidden until presentation succeeds.

The `image_preview` module also supplies `ImageCompress` for smaller upright
JPEG copies. It keeps transparent and animated originals intact and returns
no replacement when encoding would increase file size.

## Hosted ZX software surface

Import `kryon/host/surface` in a portable app and call `Run(window, draw)`
with the same `Window` and `Draw` types as `kryon/app`. A `Frame` identifies
the current session by slot and epoch. The bundle runs the widget tree,
layout, hit testing, paint queue and pixmap raster, including text measurement
and rasterization with Kryon's bundled fonts. Compile ordinary widget imports
and bind their raster calls to `pixmap` inside the bundle.

`RunRetainedRows(window, draw, present, idle_ms, clock)` lets the host retain
pixels while the app keeps only one strip of output. The `SurfaceRowsPresenter`
callback takes `(width: s32, height: s32, first: s32, rgba: string) -> bool`.
Each call supplies up to 32 complete opaque RGBA8 rows beginning at zero-based
`first`. Consume or copy those bytes before returning. Initial draw, resize
and expose deliver every row in order. Ordinary updates deliver affected rows
and unchanged frames deliver none. Use built-in queued paint with this entry
point. A presenter that returns `false` ends the loop with an error.

`RunRetainedPixels(window, draw, present, idle_ms, clock)` uses the same strip
renderer and damage tracking, but its `SurfacePixelsPresenter` receives
`(width: s32, height: s32, first: s32, pixels: []u32) -> bool`. Each pixel is
packed `0xRRGGBB`; ignore its high byte. Consume these borrowed words before
returning. This avoids the RGBA conversion loop and output buffer when a host
can upload the raster's packed pixels directly. Input, resize, expose and
failure behavior match `RunRetainedRows`.

The `hosted_surface` example's packed and scheduled packed smoke variants
match the full renderer's frame hashes and input results with 32-row batches.

All surface loops accept an optional `wake: SurfaceWake` callback.
It receives the host clock sample (or -1 without a clock) before each event
is handled and returns `SurfaceSchedule { wait_ms, redraw }`. An idle timeout
only composes when `redraw` is true or `idle_ms` is positive. This allows a
cheap state poll to run without rebuilding the widget tree. The next wait is
bounded to 1–60000 ms; zero uses the default wait, and a positive `idle_ms`
also bounds that wait to its existing interval of at most one second. Input,
expose, resize and queued text continue to compose immediately.

Define `KRYON_PIXMAP_EXTERNAL_BUFFER` when the portable app always uses
caller-supplied pixmap storage. This also omits the static fallback framebuffer.

From the Kryon checkout, using canonical Ziran:

```sh
mkdir -p build/scratch/zx-surface
ziran bundle --project --bind-host pixmap --entry hosted_surface:main \
  -o build/scratch/zx-surface/hosted_surface.zx examples/hosted_surface.zi
```

A host supplies typed `HostBinding` callbacks for the four capabilities in
[`zx_surface.zi`](../src/backend/zx_surface.zi). Bind the exact module name
from the bundle's capability metadata. The module is `zx_surface` in the
in-tree build and may be package-qualified in a consumer build.
For a consumer project whose host imports the raster, use `--bind-host
kryon__pixmap`: dependency modules bundle under the dependency's module
prefix (`kryon__` for the usual `kryon` dependency alias), not the package id
in `ziran.lock`. A bundle without a project host runs on the ZX player, which
supplies rendering, so it binds nothing.

| Function | Signature | Host behavior |
| --- | --- | --- |
| `Open` | `(title: string, width: s32, height: s32) -> bool` | Create one hidden window per instance. The dimensions establish the initial pixel viewport. |
| `Event` | `(wait_ms: s32) -> SurfaceEvent` | Return one event in FIFO order. Wait at most `wait_ms` when the queue is empty. Zero polls immediately. |
| `Present` | `(width: s32, height: s32, rgba: string) -> bool` | Consume or copy exactly `width * height * 4` bytes during the call. A successful first frame shows the window. |
| `Close` | `()` | Release the window and its input queue. Closing is idempotent. The host also releases them when the instance exits or traps. |

`Present` uses tightly packed top-down RGBA8 with alpha 255. Its `string`
is a byte span and can contain zeros. If a resize arrives during painting, the
host can discard the stale frame and return success while delivering the resize
event for the next frame. The app exits with status 1 if `Open` or `Present`
fails or a resize provides unsupported dimensions. A close request exits
with status 0. The bundle accepts up to 4,194,304 pixels per surface and up to
16,384 pixels per dimension.

`SurfaceEvent` has signed 32-bit fields `kind`, `x`, `y`, `button`, `key`,
`modifiers`, `width` and `height`, plus `text: string`. Set unused fields to
zero or the empty string. Return the event's declared record type from the
capability metadata.

| `kind` | Meaning | Payload |
| --- | --- | --- |
| 0 | Idle or timeout | |
| 1 | Close requested or window lost | |
| 2 | Redraw | |
| 3 | Resize | Positive `width`, `height` in pixels |
| 4 | Pointer move | Window-local `x`, `y` in pixels |
| 5, 6 | Button down, up | `x`, `y`, `button` (1 left, 2 middle, 3 right) |
| 7, 8 | Key down or repeat, up | `key`, `modifiers` |
| 9 | Committed text | UTF-8 `text` |
| 10 | Reserved for Taiji visibility | |
| 11 | Vertical wheel | Window-local `x`, `y` and signed milli-detents in `button` |

Use the portable [key codes](../src/ui/input_protocol.zi), including uppercase
ASCII for letters and modifier bits 1 Shift, 2 Control and 4 Alt. Deliver text
as a commit independently of the key press. Kryon routes left and right
buttons through its primary and secondary widget input. Frames run on input,
expose and resize. Pass a positive `idle_ms` to also redraw on timeouts at an
interval of up to one second. For example, a stopwatch can redraw every 250 ms
and read elapsed time from its host clock. Platform services such as clocks,
clipboard and live IME composition require additional capabilities.

Send one kind-11 event per vertical wheel sample. Positive `button` values
scroll up and negative values scroll down. Use `1000` units per detent and
leave the other payload fields zero. The wheel sample keeps held buttons and
keys in their current state and clears before the next callback.

`make zx-surface-test` builds and runs the example in the Ziran instruction
engine with a typed host. It verifies the RGBA bytes, rendered count changing
after a click, UTF-8 editing, resize, close and opt-in timeout redraw. An
emulator can launch the same bundle by registering the four capabilities.

## Shared ZX libraries

A program that imports Kryon's widgets compiles the pixel, session, tree and
text code into its own bundle. `make zx-libraries` builds that code once as
four ZX libraries that a program links when it is built and loads when it
runs, so Kryon is linked, not copied:

| Library | Export root | Exports |
| --- | --- | --- |
| `kryon-pixmap.zx` | `pixmap_library` ([`src/backend/pixmap_library.zi`](../src/backend/pixmap_library.zi)) | `Background`, `Panel`, `Round`, `Mix` |
| `kryon-session.zx` | `session_library` ([`src/backend/session_library.zi`](../src/backend/session_library.zi)) | `SessionOpen`, `SessionValid`, `SessionIndex`, `SessionClose`, `WidgetPartKey` |
| `kryon-tree.zx` | `tree_library` ([`src/backend/tree_library.zi`](../src/backend/tree_library.zi)) | `DrawButton`, `TreeIdentityHash`, `FrameStatusMessage` |
| `kryon-text.zx` | `text_library` ([`src/backend/text_library.zi`](../src/backend/text_library.zi)) | `DrawLabel`, `TextSpan` |

Each root delegates to the ordinary modules under `src/ui/` and
`src/backend/pixmap.zi`; no widget is implemented twice. `ziran.toml` exports
the roots and `zx_frame` by name, so a project build imports them like any
other Kryon module, and the `zx-libraries` rules of the `Makefile` build each
library with `ziran bundle --project --library NAME --export root:Function ...`
into `build/zx-libraries/NAME.zx`. The drawing libraries bind the software
rasterizer with `--bind-host pixmap`, as the hosted surface above does, so a
library needs no host grants and loads under a plain `ziran run`. The
raster is compiled into each drawing library (about 2.8 MB for pixmap, 5.2 MB
for tree and 4.9 MB for text; the session library is about 30 KB). A host
that paints with its own rasterizer would grant the libraries its pixmap
capabilities per library instead; that is not what these bundles do.

A program links the libraries when it is built and loads them when it runs.
[`examples/zx_libraries.zi`](../examples/zx_libraries.zi) is the consumer;
from the Kryon checkout, with `ZIRAN` the Ziran source directory:

```sh
ziran bundle --root examples --module-path $ZIRAN/std \
  --module-path src/ui --module-path src/backend --define KRYON_ZX_LIBRARIES \
  --require-module KryonPixmap.v1=kryon-pixmap \
  --require-module KryonSession.v1=kryon-session \
  --require-module KryonTree.v1=kryon-tree \
  --require-module KryonText.v1=kryon-text \
  --import zx_libraries:LibPanel=KryonPixmap.v1:Panel ... \
  --entry zx_libraries:main -o app.zx examples/zx_libraries.zi
ziran run --module-root build/zx-libraries app.zx
```

`--require-module` names a library interface and the module file that
provides it under `--module-root`; `--import` binds one foreign declaration
of the program to one export of a library, one flag per declaration (the
example binds fourteen). Every such declaration takes the `u64` handle that
`Modules.Load("KryonPixmap.v1")` of `std/zx_module` returns as its first parameter,
and the program releases the handles with `Modules.Release` before it exits.
The complete command is the `zx-libraries-example` rule of the `Makefile`.

Values cross the boundary by value, never through a pointer or shared
state. A drawing library receives the complete frame, `ZXFrame` of
[`kryon/zx_frame`](../src/backend/zx_frame.zi) (240 × 144 pixels of
`0xRRGGBB`), and returns the frame it painted; the session library takes
and returns Kryon's `Session` of `kryon/session`. An import is checked
against the library's export by record name and field layout, and the name
is module-qualified, so the program must import both records from modules
named `zx_frame` and `session`, as the libraries do, and not declare its
own copies. Two build rules follow from that:

- Build the linked program from plain module paths only. A package path such
  as `--module-path kryon=src/ui` renames the modules it supplies
  (`session` becomes `p<hash>_session`), and the records no longer match.
- Build it without `--project`. Under a project the standard library's
  `zx_module` is named `std_zx_module`; its `Load` and `Release` are then
  not recognized as runtime services, and `ziran run` hands the program to
  an installed runtime provider of the same toolchain build instead of
  running it itself.

Libraries and the programs that load them must come from the same Ziran
toolchain. `make zx-libraries` builds the four libraries from the current
tree, then `make zx-libraries-example` builds the example twice, statically
linked with the four modules compiled in and linked against the libraries
from `--module-root`, runs both, and requires their printed frame hashes to
agree. `make test` runs the same check.

## Experimental SDL3 GPU renderer

Custom Linux x86_64 GNU hosts can import `kryon/pixmap_sdl3` and
`kryon/pixmap_sdl3_gpu`. This is an opt-in rendering prototype, rather than a
new project profile: the host supplies an SDL3 window, input loop and
presentation. Desktop profiles keep their behavior.
The handwritten renderer is Ziran; SDL3 supplies the GPU API and shaders.

Kryon's `native-sdl3` artifact builds static SDL 3.4.18 from its pinned official
source package. No Git submodule or global SDL3 installation is needed. Build
prerequisites are a GNU C toolchain, CMake, pkg-config, Python 3.11+, X11
development headers and Vulkan headers/loader. Running requires an SDL3 GPU
device, which may be software. On Debian/Ubuntu the additional test packages
are `libvulkan-dev mesa-vulkan-drivers libx11-dev libxext-dev libxtst-dev xvfb xauth`.
From the Kryon checkout, with Ziran available:

```sh
env -u DISPLAY -u WAYLAND_DISPLAY -u XAUTHORITY \
  -u DBUS_SESSION_BUS_ADDRESS make sdl3-test
```

This resolves and builds the locked SDK through Ziran, verifies its declared
outputs, and runs shared scene, geometry adapter and real private-Xvfb renderer
tests. Package metadata and the generated import smoke test stay under ignored
`work/sdl3-integration`; there is
no copied source checkout or hard-coded SDK cache key. The Linux CI hosts job
runs the same target with Mesa's software Vulkan device. Missing GPU support
fails the test instead of skipping it.

A host's minimal rendering sequence uses the ordinary package exports:

```zi
Sdl3 :: #import "kryon/pixmap_sdl3";
Sink :: #import "kryon/pixmap_sdl3_gpu";
// After SDL3 initialization and window creation, on the window's thread:
renderer := Sdl3.CreatePixmapSdl3Renderer(window)
// Check renderer != null, then Sink.PixmapSdl3Open(renderer).
// Create and retain a target with Sink.PixmapSdl3CreateTarget(width, height).
// Record normal pixmap painting, then check Sink.PixmapSdl3Draw().
// Copy the target to the window with SDL_RenderTexture and SDL_RenderPresent.
// Close the sink before destroying host-owned targets, renderer and window.
```

`CreatePixmapSdl3Renderer` explicitly requests SDL's `gpu` renderer and returns
null on failure, without trying another renderer. Inspect
`PixmapSdl3DescribeRenderer(renderer)` for the renderer name, API, device name
and driver information; its strings are borrowed until renderer destruction.
A `gpu` renderer using `vulkan` does not establish hardware acceleration.
These tests establish rendering behavior, not hardware performance.

The sink shares `pixmap_gpu`'s clipped geometry, glyph atlas, paint order and
single active recorder. Image publications and sink calls must use the renderer
thread. After a failed draw, close/open and repaint fully; after an SDL render
device reset, call `PixmapSdl3DeviceLost`, recreate the host-owned target and
repaint fully. The sink does not destroy host-owned render targets.

The CPU parity tests cover five scenes and three frames each. Blending has small
rounding differences (observed maximum channel difference 3). Exact nearest
texel ties in the 1.25× zoom scene can choose the adjacent texel, as in the SDL2
reference; the test accepts only analytically identified checker boundaries
within the image and clip. The shared sampler is therefore not byte-exact at
those ties. SDL3 also converts byte colors to float colors in 1536-vertex chunks,
which can add draw calls; this does not establish a speedup over SDL2.
See the executable lifecycle example in
[`tests/sdl3_renderer_host_test.zi`](../tests/sdl3_renderer_host_test.zi).

For SDL2 versus SDL3 on a physical AMD GPU under Linux, use the explicit
hardware mode (choose the render node from `/sys/class/drm/renderD*/device`):

```sh
env -u DISPLAY -u WAYLAND_DISPLAY -u XAUTHORITY \
  -u DBUS_SESSION_BUS_ADDRESS -u SESSION_MANAGER YUE_DESKTOP_RECOVERY=0 \
  python3 tests/backend_render_benchmark.py --mode hardware \
  --device /dev/dri/renderD129 --trials 5 --frames 1200 --warmup 240 \
  --output work/backend-render-benchmark/hardware.json
```

Hardware mode uses SDL2 offscreen EGL/OpenGL and SDL3 offscreen Vulkan with
a headless surface, without the desktop compositor. It removes inherited
software-driver overrides and selects Mesa's Vulkan device by vendor/device
ID. Selection is only a request: both hosts must report a physical renderer
and nonzero Linux DRM graphics-engine execution on the requested PCI device.
The report records that device's render node, PCI address, vendor/device IDs,
kernel driver, renderer/API and Mesa versions. A mismatched device, software
renderer, missing execution evidence or failed pixel oracle blocks hardware
timing. This qualification currently requires AMD's Linux DRM counters; it
does not infer acceleration from the Vulkan API name. The default software mode still forces
llvmpipe on private Xvfb; `--backends sdl2 sdl3` restricts its smoke test.

Every selected backend must pass all 120 frames of all five 480×320 scenes
before hardware timing starts, using the same channel tolerance and narrow
checker-boundary rule described above. Parity exercises presentation as well
as target readback. Repeated trials randomize backend order per scene and
record CPU compose/submit/present p50/p95, individual frame samples, per-trial
drained wall throughput and load averages. Drained wall throughput includes
the final synchronous readback and CPU conversion; it is not a GPU timestamp
measurement. Offscreen results do not measure visible-window/compositor
performance, and other running workloads and GPU clock changes remain
uncontrolled. No backend default or SDL2 software fallback is changed.

## AVIF images

Native AVIF and WebP decoding is supplied by the standalone Ziran `avif`,
`webp` and `image_codec` packages. Kryon selects the platform engine and adapts
caller-owned straight RGBA8 into host pixels, surfaces and textures. Codec
parsing, native ABI and frame composition belong to those packages. The current
native engines explicitly bridge pinned external libraries; this integration
makes no authored AV1 decoder or performance claim.

Content routing uses the packages' bounded prefix predicates; their decoders
perform complete container validation. Kryon's CMake entry points consume the
packages' shared native build recipes and retain the existing target names and
archive locations. Package directories come from `ziran pkg path`; renderer
artifacts pass their resolved dependency paths explicitly. Custom CMake hosts
can set `KRYON_AVIF_PACKAGE_DIR` and `KRYON_WEBP_PACKAGE_DIR` to those paths.

The adapters allocate the final host buffer once. AVIF probes metadata, then
revalidates and decodes the same immutable encoded buffer into that storage;
this repeats bounded metadata parsing without copying a full owned output.
Failed decode, cancellation and changed output dimensions release the buffer
before publishing any pixels. Browser codecs and pixel-renderer inputs retain
their existing paths.

`Image.Draw(Image.Props)` accepts AVIF assets through the same path as PNG/JPEG/WebP,
including extensionless names and packaged bundle/APK assets. Native hosts
decode with the locked libavif 1.3 and dav1d, its assembly selected at run time
for the CPU (x86 SSE2 to AVX-512, ARMv7 and AArch64 NEON); Android 9 requires
no OS AVIF support. libaom is linked as the explicit comparison oracle and as
the AVIF encoder, never as a fallback. The default `kryon:renderer-desktop`
artifact therefore carries dav1d decode only; an app that compresses AVIF
selects `kryon:renderer-desktop-encode`, which adds libaom (`make
desktop-codec-encode-artifact-test`). The still-image API shows the first
complete frame of a sequence. The ARMv7 oracle keeps libaom's portable
kernels, because the locked AOM/NDK combination emits unaligned NEON loads
which fault on real Android hardware; dav1d's own NEON runs there.

Desktop hosts load the AVIF bridge (`libkryonavif.so`, `kryonavif.dll`) on
first use, from `KRYON_AVIF_LIBRARY`, the library search path or beside the
executable, and WebP from the system libwebp. A program that links the codecs
itself builds with `--define KRYON_AVIF_LINKED` (libavif and its AV1 codec)
and `--define KRYON_WEBP_LINKED` (libwebp and libwebpdemux), and loads
neither; `make avif-decode-test` runs such a build, linked with the static
archives, where no `libkryonavif` exists.
8/10/12-bit color and monochrome decode to straight 8-bit RGBA; alpha and
container clean aperture, rotation and mirroring are preserved. The renderer
then premultiplies for Cairo. HDR tone mapping
and ICC display color management are outside the existing 8-bit image surface.

Encoded files are limited to 256 MiB, dimensions to 32768 per axis and pixel
count to 67,108,864 (a 256 MiB RGBA buffer). These are independent axis/count
limits, permitting wide images below the pixel budget. Invalid/truncated
containers, invalid crops and oversized images return no image. 4:2:0 frames
of at least 8 MiP and other frames of at least 2 MiP decode on the process's
usable cores, capped at 4 threads; smaller frames use one, because extra
threads slowed them in measurements. Thread count never changes output
bytes. Only the first decoded frame is retained.

| target | engine | proof |
|---|---|---|
| Linux x86-64 | dav1d, SSE2..AVX-512 | executed: strict parity, benchmark |
| Android armeabi-v7a | dav1d NEON | executed on the USB Moto (MT6765, Android 9): parity, benchmark |
| Android arm64-v8a | dav1d NEON, dotprod, i8mm | static build under qemu-user: strict parity |
| Android x86, x86_64 | dav1d SSSE3..AVX-512 | static build on the x86-64 host: strict parity |
| Linux AArch64 | dav1d NEON, dotprod, i8mm | qemu-user: strict parity |
| Windows, macOS | dav1d via the same recipe | unverified here |

Canvas uses the browser AVIF decoder through `PageImageDecode`; supported
browsers (Firefox 93+, Chromium 85+, Safari 16.4+) supply their native AVIF
implementation. In a browser without AVIF, every AVIF decode fails cleanly:
no pixels and zero size, so the image is treated like any undecodable file,
while PNG, JPEG and WebP keep working. Ship a fallback format for such
browsers. The pure pixel renderer accepts decoded pixel buffers through
`PixmapSetImage`, as for other formats.

`make avif-decode-test` runs independent RGBA goldens from source and saved IR,
and checks dav1d (automatic, 1 and 8 threads) and the libaom oracle against the
strict libavif digests in `tests/avif_strict_digests.zi`, tolerance 0. The
independent goldens keep small documented tolerances: ±1 for 10/12-bit and
grayscale (8-bit rounding of the source pattern), ±4 premultiplied (unpremultiply
rounding) and ±3 for Jungle title samples (YUV420 conversion versus Pillow).
`make avif-render-test` checks native rendering,
including a portable bundle using the public Image widget. `make android-avif-test
ANDROID_NDK=... ANDROID_SERIAL=ZE2223BQZT` builds all four ABIs and runs the
ARMv7 strict-parity/golden-pixel tests on the Android 9 USB Moto, asserting each
ABI's dav1d assembly. `make avif-cross-test [AVIF_CROSS_ROOT=...] [ANDROID_NDK=...]`
runs the strict parity test for Linux AArch64 and static Android arm64-v8a
under qemu-user (Cortex-A53 and every emulated feature) and Android x86/x86_64
on the host; `AVIF_CROSS_ROOT` is a prefix holding the AArch64 GCC and
qemu-aarch64. `make avif-browser-test` exercises the Canvas image
boundary in private headless Firefox, reporting the AVIF capability, and repeats
it as a browser without AVIF. `make avif-benchmark [ANDROID_SERIAL=ZE2223BQZT]
[AVIF_BENCHMARK_BROWSER=1]` records ms/image, MP/s and peak RSS for libaom,
dav1d threads and SIMD tiers, and Firefox, in `build/scratch/avif-benchmark`.

The headless package suite, `sh tests/image_integration.sh`,
resolves real native artifacts and tests source and saved IR under ASan, UBSan
and leak detection, including adapter failure cleanup. With development-only
package overrides, set `ZIRAN_ROOT` and `ZIRAN_BIN` to the rebuilt toolchain.

## AVIF compression and RGBA previews

The native `image_preview` module provides
`ImageCompressAVIF(data: string, file_type: string, orientation: s32,
quality: s32 = 35) -> ImageAVIFResult`. On success, `data` owns actual AVIF
bytes and `source_bytes`/`encoded_bytes` support a real size comparison.
`width`/`height` are the full stored dimensions. Call `ImageAVIFFree(*result)`
when finished. Encoding does not resize or discard a larger result. An artifact
consumer selects `kryon:renderer-desktop-encode` for compression: the default
renderer artifact carries dav1d decode only.
`ImageAVIFEncodingAvailable()` reports whether this host supports encoding.
Browser hosts return false, and compression returns `UnsupportedCapability`
without linking native codec archives. Browser RGBA previews remain available.

Accepted still 8-bit JPEG, PNG and WebP sources retain ICC, Exif and XMP when
the container metadata is supported; AVIF uses the headless package's direct
re-encode path. Alpha stays lossless. Animated, high-depth, gain-map and
unsupported color/metadata cases return an explicit shared `DecodeError`.
PNG compressed ICC and compressed text profiles currently return
`UnsupportedCapability`. A host that already baked a nonidentity orientation
must provide raw pixels before compression can preserve the stored orientation.

`ImagePreviewRGBA(data, file_type, max_edge, orientation)` returns owned upright
straight RGBA8 with `pixels`, `width`, `height` and `stride`. It uses Lanczos-3
and retains transparency. `ImagePreviewRGBAPixels` accepts already decoded
pixels with 1..4 channels. Call `ImagePreviewRGBAFree(*result)` to release the
output. These RGBA previews avoid temporary JPEG files and preserve transparent
edges for before/after comparison. Existing JPEG preview callers retain their
existing API.

## WebP images

Images, image previews and app media files accept WebP wherever they accept
PNG or JPEG, recognized by the file's `RIFF`/`WEBP` signature rather than
its name. Lossy, lossless and alpha pictures decode to straight RGBA. An
animated WebP decodes to its first complete frame, since Kryon images are
still; `ImageCompress` keeps animated originals unchanged.

- The desktop (Cairo) host loads the system libwebp demuxer
  (`libwebpdemux.so.2`) on first use, like its other optional codecs.
- The canvas host uses the browser's own WebP decoder.

`make test-focus TEST=ziran_image_decode_linux_test` decode the
`tests/fixtures/webp` pictures natively, `tests/webp_browser_test.py` does the
same in headless Chromium, and `make android-webp-link-test` links the
decoder for every Android ABI.

The PNG byte and pixel rules are Ziran in `png.zi`. PNG supports all standard
color types and depths, transparency, all five filters, and Adam7 interlace.
Sixteen-bit samples use their high byte; ancillary color profiles and gamma
are ignored. `tests/ziran_png_test.sh` compares independent reference pixels
for 33 fixtures through source and saved IR in bundles, C and Go, including
malformed streams and bounded decoder output.

Browser projects save the selected `_run` host and `canvas_raster.zi` as
generation roots without entry pruning. This keeps both the host `main` and
the raster adapters available to the paint queue. Native projects continue
through the ordinary package entry route.

## Smooth images (Lanczos)

A smooth picture (`.smooth = true`) is shown at the device pixels it
covers through one shared cache, `src/backend/lanczos_cache.zi`. The copy is
std/resample's Lanczos-3, which gives the same bytes as Pillow's
`Image.resize(size, LANCZOS, box)`. It is computed over premultiplied
pixels and drawn pixel for pixel. The key is the picture's content, its
revision, the source rectangle, the device-pixel size and the session.
Until a copy is ready, the backend draws the previous copy of the same
picture stretched, or uses its own filter if there is none. The finished
copy replaces it in one step. A changed or released picture forgets its
copies. Distinct sizes of the same source and revision coexist within the
LRU budgets, so a large cover and its small marker do not evict each other.

- Only axis-aligned draws under a plain translation are cached. Rotation,
  scaled transforms, meshes, polygon strips and render targets keep the
  backend's own filter. A whole-pixel draw at the source's own size needs no
  copy.
- On a GPU pixmap host, managed runtime bitmaps also keep the GPU's linear
  filter. Animated bitmaps otherwise spend CPU time copying and resizing
  revisions that the next frame discards. Static asset pictures and CPU
  pixmap hosts retain their Lanczos copies.
- Limits: a copy is at most 4096 pixels per side and 4,194,304 pixels; all
  copies together take at most 64 MiB, and one session at most 32 MiB.
- Like Pillow, filtering works on the stored sRGB bytes, not in linear
  light.
- Copies are made on a worker thread with a std/parallel pool in builds
  with `POSIX_THREADS`, `ANDROID_BUILD` or `KRYON_FRAME_THREADS`. A browser
  build without threads makes a slice of rows each frame instead (about two
  million filter taps). Other builds keep the backend's filter.

| Backend | Lanczos copy |
| --- | --- |
| Desktop/Cairo | Cached surface, drawn 1:1 |
| pixmap and SDL3 GPU | A copy slot drawn 1:1 (threaded builds only, `PIXMAP_LANCZOS`), so the CPU rows and GPU hosts show the same bytes |
| Canvas | Ziran resampler in slices, from the texture's pixels read back, then uploaded |

Firefox has no `imageSmoothingQuality`. Canvas therefore does not rely on
it, and still sets it to `high` for upright smooth draws that it does not
cache; meshes and images that the draw or a camera turns keep the browser's
default.
The cached Canvas copy matches Pillow for opaque pictures. Translucent
pictures differ by the browser's 8-bit premultiplied blend, up to about
3.5 levels on average.

A web profile's page carries two builds of the program: one compiled with
`-msimd128` and a baseline one. A small script in the page starts the
SIMD128 build where `WebAssembly.validate` accepts a SIMD probe module, so
Canvas copies use std/resample's SIMD128 kernels, and the baseline build
elsewhere. Both builds give the same bytes. The page stays one file, and the
font is carried once for both builds (`mk/web-page.sh`).
`tests/web_simd_page_test.py`, part of `make canvas-project-test`, checks
this in headless Firefox and Chromium.

A host that sleeps between events calls `PixmapLanczosPending` or
`LanczosSetWake` so it draws again when a copy finishes. The desktop loop
already does this by pushing a wake event. `PixmapLanczosWait` and
`LanczosWait` finish the current copy; captures use them so every
smooth picture is final.

`make lanczos-goldens-test` (part of `make test`) compares Cairo, pixmap,
Canvas in a private headless browser against
Pillow goldens. `make lanczos-benchmark` times Kryon against Pillow,
Pillow-SIMD, libvips and stb_image_resize2. It reports ms per output
megapixel, extra peak memory and PSNR against a float64 reference, on
x86-64 and over ADB on an ARMv7 phone. It writes
`build/scratch/lanczos-bench/receipt.md`. The comparison libraries are
test-only oracles.

## Android host

`backend = "android"` builds an installable APK instead of a native binary.
Add the profile and the application id to `ziran.toml`:

```toml
[tool.kryon.profiles.android]
backend = "android"

[tool.kryon.android]
package = "com.example.app"
min_sdk = "21"        # optional, 21 by default
abis = "arm64-v8a"    # optional, arm64-v8a by default
```

`kryon build --profile android` generates a `droid/` Gradle project beside
the application sources on the first build (never overwriting existing
files), then assembles a debug APK through the Android Gradle plugin, the
NDK, and CMake. `kryon run --profile android` additionally installs and
launches it on a connected device through adb, and `kryon check --profile
android` checks the whole Zi graph with the Android defines. The build needs
a JDK, the Android SDK with NDK `28.2.13676358` and CMake `3.22.1`
(`ANDROID_HOME`, default `$HOME/Android/Sdk`), and the locked `libwebp`,
`libavif` and `libaom` source packages. Scaffolds pass their source roots to
`kryon_android_codecs`; the packaged host links no renderer or audio library.

At runtime a `NativeActivity` subclass owns the Java side: it reports window
insets, display cutouts, IME state, density, dark mode and orientation
through JNI natives registered by the generated `src/android_glue.zi`, and
forwards soft-keyboard text as codepoint, backspace and enter events. The
generated `src/main.zi` delegates to `AndroidRunMain`, which owns Android's
looper and lifecycle, drives the session with the current Ziran VM and Pixmap
runtime, and applies Kryon's Android viewport policies to the session-owned
text input queue. `Frame` always receives the safe area, clear
of the system bars and cutouts; a backdrop meant to run behind them draws into
`AndroidWindowBounds()`. Applications with their own JNI surface
keep their own `JNI_OnLoad` and call `AndroidJniOnLoad` with their activity
class to add the canonical natives. An application without Java code, a plain
`NativeActivity`, calls `PollAndroidWindowInsets` once per frame instead: it
reads the system bar, keyboard and cutout insets from the window through JNI
a few times a second, and stops once Java reports them.

The generation runs `ziran ir --project` for the whole graph, so application
module names may repeat Kryon module names; every package is prefixed in the
emitted C. `tests/ziran_android_device_test.sh` checks the device protocol
headlessly with the Java side stubbed.

`android_device.AndroidRequestOrientation(policy)` queues an ActivityInfo
screen-orientation policy through `requestOrientation(int)` in the scaffold
activity, which calls `setRequestedOrientation` on the UI thread. Custom
activities must provide that method. `AndroidOrientationSensor` (4) permits
sensor portrait and landscape; `AndroidOrientationSensorLandscape` (6) keeps
landscape. The caller owns route policy and deduplication. A true result means
the Java call succeeded, not that the OS has applied a rotation. Configuration
observations still use `AndroidOrientation()` and are independent of requests.

Android runs the native host loop on NativeActivity's app thread. Each poll
drains lifecycle and input events, snapshots the current touch/key state,
runs one VM frame through Pixmap, and presents the damaged viewport by locking
and unlocking `ANativeWindow`; there is no EGL, OpenGL ES or second render
thread. Rotation, surface destruction and redraw requests invalidate the
retained bitmap, while consecutive quiet frames can pace at 100 ms instead of
16 ms until the next event. Pausing stops presentation and platform media;
resuming restores the existing guest state.

Touch, pointer, soft-keyboard and hardware-key events are consumed only inside
that frame boundary, so guest code receives a stable input snapshot and no
platform callback reenters the VM. The same viewport transform maps single
and secondary pointers into guest coordinates.

Portable ZX images are decoded before a guest is activated and retained in two
bounded host slots. PNG, WebP and AVIF use native codecs; JPEG can use the
platform decoder and all camera images apply EXIF orientation. A valid
replacement promotes candidate pixels only after its services and images load;
an invalid document leaves the previous guest, pixels and private data intact.
MediaPlayer reads a private file written from bundle bytes and supports the
portable play, pause, resume, stop, gain and completion surface.

## Frame and input ownership

A host polls platform input and submits it to the session before calling the
application's `Frame`. The application and widgets consume generic session
input such as `KeyboardTake`, `TypedCodepointTake`,
`PointerWheelTake`, and pointer state. `KeyboardModifiers` reports
Shift, Control, and Alt keys as `KeyModifierShift`, `KeyModifierControl`, and
`KeyModifierAlt` bits, so a shortcut such as Ctrl+S is a key press read with
the modifiers that were down. Desktop and Canvas custom loops call
`DesktopInputTake(session)` or `CanvasInputTake(session)` before each frame. Each take transfers ordered text
to that session and returns the next key with the modifiers retained when the
press or repeat arrived, so shortcuts remain intact even when every physical
key is released before a frame. `CanvasModifiers()` and the immutable frame
snapshot keep their held-state semantics. Other hosts report none.
The host calls `BeginFrame` and
`EndFrame` around application composition, then presents the resulting
paint operations. Widget behavior stays in `src/ui/`.

The terminal host renders one frame when input or output is redirected. With
a TTY, it submits ASCII and UTF-8 text to the same session input API as the
window hosts and ignores terminal escape sequences. The project test drives
this through a private PTY. The other hosts can capture a frame with
`KRYON_CAPTURE_PATH`; the graphical integration tests run on private Xvfb
displays. Run `make test` for the library, browser, and typeface gates, plus
`make desktop-project-test` for graphical
capture and input.
Never run graphical tests against the developer's live display.

The SDL/Cairo desktop host also exposes `DesktopSetTitle(title)` and
`DesktopTakeDroppedFile(output)` through `Desktop`. A file-manager drop queues
up to sixteen complete UTF-8 paths (4095 bytes each), only for the host's own
window. The application consumes and opens them during `Frame`; an output
buffer that is too small leaves the next path queued. Text drops are ignored.

## Portable programmable scene rendering

`scene_render` executes `shader` material programs in Ziran on CPU before
presentation. Cairo and Android's
ANativeWindow/Pixmap host consume the resulting managed bitmap through the
standard Image surface. These hosts do not need a shader-language bridge.
This interface does not claim GPU execution, GLSL compilation, vertex-shader
execution or GPU performance. See [programmable scene materials](SCENE_RENDERING.md#programmable-scene-materials)
for binding ownership, colour space, clipping, alpha and explicit scratch release.
Programs and pictures contain no context-bound handles. Recreate the host
presentation context as usual; keep or re-upload the owned bitmap and render
again as needed. Shader discard affects colour rendering, not depth-map shadows.
Host lifecycle tests must verify bitmap re-upload after teardown; checking a
shader declaration alone does not establish host execution.

The physical-device regression `make shader-android-test` executes Ziran shader
materials and the Pixmap-to-ANativeWindow presentation path on the USB Moto. It
uses a private offscreen AImageReader window, verifies posted RGBA pixels,
clipping, alpha and binding changes, and destroys/recreates that window while
keeping the CPU program and bitmap alive. It does not launch or alter an owner
app. These targets also load a real PNG through
`kryon/image_pixels`; activity-bound Android asset/JNI decoding is a separate
application integration responsibility.

Native scene frames can render on the shared Ziran range-worker capability
when source generation enables `POSIX_THREADS`. Immutable transformed geometry
and ordered triangle commands are prepared once; each lane clears, draws and
resolves disjoint rows, joining once per frame. The scene owns its pool
separately from the presentation raster; `RenderSceneRelease` joins and
closes it. This is CPU row parallelism and does not change texture resolution,
four-sample coverage, shader operations or draw ordering. WebAssembly without
pthread support retains the same serial implementation. Application code must
keep all borrowed material inputs unchanged through the render call.

## Optional native GPU scene rendering

`kryon/scene_gpu` is an explicit native EGL/GLES3 host for the same Scene and
material APIs. It renders offscreen with four MSAA samples, depth-map shadows,
retained geometry/programs/textures and an Image readback bridge. It creates no
window and preserves the caller's current graphics context. Link only consumers
of this module with `-lEGL -lGLESv2 -lGLX -lm`; core UI and `scene_render` imports retain
their existing portable and WebAssembly dependencies.

`kryon/shader_glsl` supplies portable material translation used by this host.
The translator alone has no EGL dependency. GPU execution uses highp floats
and hardware coverage, so it has pixel tolerances relative to the binary64 CPU
oracle. Capability failures are explicit; the separate `SceneGpuRenderOrCPU`
call permits a full-quality CPU frame and reports `accelerated=false`.
Probe/render/resource counters must establish acceleration and retained upload
behavior before reporting GPU timing. A module export or successful link alone
does not establish application integration or deployment.

See [GPU scene APIs](SCENE_RENDERING.md#portable-glsl-translation-and-optional-native-gpu-rendering)
for ownership, lifecycle, capability requirements and local test targets.

## Custom application entry

`main` is the only program entry. An application that owns its frame
lifecycle defines `main` itself and drives the host APIs directly (open the
desktop runtime, poll and draw), instead of handing its frames to
`App.Run`. `#program_export` stays a Ziran language feature for exporting
plain C-ABI symbols to foreign hosts; Kryon has no frame-entry binding, so
a UI program always enters through `main`. Android's NativeActivity package
keeps `android_run.zi`: the generated `src/main.zi` delegates to
`AndroidRunMain`, which owns Android's looper, window and lifecycle, while
the application's `main` drives it through `App.Run` (`kryon/app`).
