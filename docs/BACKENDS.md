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

MediaAudio's Raylib and Canvas providers support individual voice Pause/Resume.
Raylib uses Pause/ResumeMusicStream and Pause/ResumeAudioStream, and reports
GetMusicTimePlayed for asset music. Canvas retains music offsets against the
AudioContext clock and re-creates only that voice's source on explicit Resume;
paused PCM retains at most two immutable buffers and their remaining offsets.
No owner pauses the shared context. See [MediaAudio](API.md) for capability
bits and position availability. Native tests use Raylib/miniaudio's paced null
backend; browser tests use a disposable muted headless context and trusted gesture.
Ogg Opus assets decode through the `audio` package on native Linux and Windows;
those targets link `-lopus -lm` (the `renderer-raylib` artifact ships a static
`lib/libopus.a`). Android portable guests use the platform `MediaPlayer`
service through JNI instead of carrying an audio codec.

## Renderer artifacts in build scripts

The package exports `renderer-raylib`, `renderer-desktop` and `renderer-web`
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
| `terminal` | `src/backend/terminal_run.zi` | 80 × 24 terminal cells via `terminal.zi` |
| `desktop` | `src/backend/desktop_run.zi` | SDL2 window with Cairo raster |
| `raylib` | `src/backend/raylib_run.zi` | raylib SDL2/OpenGL ES 2 window |
| `raylib` with `platform = "web"` | `src/backend/raylib_run.zi` | raylib WebGL 1 page with Emscripten and Asyncify; see [raylib in the browser](#raylib-in-the-browser) |
| `canvas` | `src/backend/canvas_run.zi` | Canvas2D page with Emscripten and Asyncify |
| `android` | app `src/main.zi` → `android_run.zi` | Android NativeActivity package with direct `ANativeWindow` pixel presentation |

The small application surface selects the host from the compile-time target.
Import `kryon/app` and call `App.Run(window, draw)`: C/Linux uses
`kryon/host/desktop`, while C/Web uses `kryon/host/web`. Advanced hosts can
import those named host packages directly. The desktop implementation uses
`cairo_raster.zi` and `desktop_loop.zi`.
After `PollDesktop()`, custom loops call `DesktopInputTake(session)` to transfer
the next ordered editing action or printable-key/text run into that session.
The returned key uses shared codes, including 320–329 for numeric-keypad
digits, and carries the modifiers held when that key arrived.
Standalone `AppContext` applications use the compiled host
`canvas_app_run.zi`.
Polygon-clipped images use native Cairo/Canvas path clips. Raylib caches bounded polygon triangulations and draws them directly on the
GPU, including all Jungle puzzle pieces. Complex polygons and pixel hosts use
clipped spans while preserving the original source mapping.
SDL pixel hosts prefer accelerated texture presentation without blocking on
vertical refresh. `SDL_RENDER_DRIVER` retains explicit driver selection,
including `software`; hosts without an accelerated driver use software
presentation automatically. Pixel rasterization remains on CPU workers.
Desktop windows become visible only after their first complete frame has
been uploaded and presented. A failed initial upload or copy leaves the
window hidden until presentation succeeds.

The `image_preview` module also supplies `ImageCompress` for smaller upright
JPEG copies. It keeps transparent and animated originals intact and returns
no replacement when encoding would increase file size.

## raylib in the browser

A `raylib` profile with `platform = "web"` builds the same raylib host, or a
project's own raylib host, as a WebGL page. raylib's `PLATFORM_WEB` library
(GLFW through Emscripten, OpenGL ES 2 on WebGL 1, with `rmodels` and `raudio`)
builds once into `build/raylib-web/`, and the generated program is compiled
with `PLATFORM_WEB` defined, so no POSIX threads are used.

```toml
[tool.kryon.profiles.web]
backend = "raylib"
platform = "web"
host = "src/host.zi"                 # optional custom raylib host
assets = "assets"                    # mounted at /assets
preload = "client/bin/textures@textures mods"
```

| Key | Meaning |
| --- | --- |
| `platform` | `web` builds a browser page. It needs `backend = "raylib"` with c99 codegen; `library`, `static_archive` and `native_artifacts` are rejected. |
| `host` | A project host as for native raylib. The default is `raylib_run.zi`. |
| `assets` | One project directory, mounted at its own relative path. |
| `preload` | Space-separated project directories, `DIR` or `DIR@MOUNT`, mounted at `/DIR` or `/MOUNT`. It needs `platform = "web"`. |

`ziran tool kryon build --profile web` writes `index.html`, `index.js`,
`index.wasm` and, when files are preloaded, `index.data` to
`build/NAME-PROFILE/`. A new build replaces the directory only after the link
succeeds. Serve that directory over HTTP, not `file://`, so the page can fetch
its module and data. Copy it as a whole, for example to a site's `play/`
directory. The page's working directory is `/`, so relative asset paths
resolve as they do from the project directory on the desktop. Kryon's UI font
is preloaded at `/kryon-font.ttf`. The page's `<title>` and raylib's window
title come from `[tool.kryon.install] name` or else the package name. The page
records how the program ended in `document.body.dataset.kryon` (`exit CODE` or
`abort REASON`).

The program keeps its ordinary blocking loop. Emscripten's ASYNCIFY suspends
the program inside `PollRaylib()`: raylib's web `WindowShouldClose()` sleeps
12 ms, which returns to the browser so it can present the frame and deliver
input. The frame rate therefore depends on that sleep. Kryon sets raylib's
target FPS to 0 in the page, because a frame limit would spin the browser's
only thread. A custom host follows these rules:

- Call `OpenRaylib(width, height, title, flags)` once. Pass
  `FLAG_WINDOW_RESIZABLE` (4, the default) so the canvas follows the browser
  window, and keep the result of `RaylibViewport()` per frame rather than the
  opening size.
- Loop on `while PollRaylib() { ... }` from `main` or a procedure called
  directly by it. Each frame calls `PollRaylib()` once. Do not wait, sleep or
  spin anywhere else in the loop.
- Inside a frame call `RaylibFrameInputBegin(session)`, then
  `BeginRaylibFrame()`. Draw the world after it: clear the background, then
  call `BeginMode3D`, the 3D drawing and `EndMode3D`. Then call
  `BeginFrame(session, ...)`, the app's `Frame`, `EndFrame`,
  `FrameInputEnd(session)` and `EndRaylibFrame()`. Kryon paints above
  whatever the host drew earlier in the frame.
- Use only GLES2/WebGL 1 features: no geometry shaders, `#version 100`
  shaders, and power-of-two textures when mipmaps or repeat wrapping are
  needed.
- Read files through the preloaded mounts. Writes stay in the page's memory
  file system and are lost on reload.
- Leave out native-only libraries: threads, `#system_library` libraries other
  than raylib, libc and libm, and `RunRaylibAppFrame` worker pipelines. WebP assets
  decode with the `webp` package's authored still-image engine; AVIF assets
  do not decode in this host.
- Return from `main` to end the page. `CloseRaylib()` releases the WebGL
  context.

`make raylib-web-project-test` builds such a host, with a 3D cube under a
Kryon `Box` and `Image`, and checks the page in private headless Chromium.

## Experimental SDL3 GPU renderer

Custom Linux x86_64 GNU hosts can import `kryon/pixmap_sdl3` and
`kryon/pixmap_sdl3_gpu`. This is an opt-in rendering prototype, rather than a
new project profile: the host supplies an SDL3 window, input loop and
presentation. Existing desktop, SDL2 and Raylib profiles keep their behavior.
The handwritten renderer is Ziran; SDL3 supplies the GPU API and shaders.

Kryon's `native-sdl3` artifact builds static SDL 3.4.18 from its pinned official
source package. No Git submodule or global SDL3 installation is needed. Build
prerequisites are a GNU C toolchain, CMake, pkg-config, Python 3.11+, X11
development headers and Vulkan headers/loader. Running requires an SDL3 GPU
device, which may be software. On Debian/Ubuntu the additional test packages
are `libvulkan-dev mesa-vulkan-drivers libx11-dev libxext-dev libxtst-dev xvfb xauth`.
From the Kryon checkout, with Ziran available:

```sh
build-queue -- env -u DISPLAY -u WAYLAND_DISPLAY -u XAUTHORITY \
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
The verified local device was **llvmpipe LLVM 19.1.7 / Mesa 25.0.7**, a software
Vulkan implementation. These tests establish rendering behavior, not hardware
performance.

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
which can add draw calls; this does not establish a speedup over SDL2 or Raylib.
See the executable lifecycle example in
[`tests/sdl3_renderer_host_test.zi`](../tests/sdl3_renderer_host_test.zi).

After `make sdl3-test`, the separate comparison harness is available with
`build-queue -- python3 tests/backend_render_benchmark.py --smoke`. It records
device identity, parity and raw samples under ignored
`work/backend-render-benchmark`. The test-only Raylib adapter currently fails
the zoom scene's pixel oracle, so the comparison returns status 2 and excludes
that scene's Raylib timings. This does not make the SDL3 integration test fail.

For SDL2 versus SDL3 on a physical AMD GPU under Linux, use the explicit
hardware mode (choose the render node from `/sys/class/drm/renderD*/device`):

```sh
build-queue -- env -u DISPLAY -u WAYLAND_DISPLAY -u XAUTHORITY \
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
does not infer acceleration from the Vulkan API name. Raylib is excluded
from this hardware comparison. The default software mode still forces
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

The 2026-11-02 hardware run used AMD Radeon Graphics
(RAPHAEL_MENDOCINO), `/dev/dri/renderD129`, PCI `0000:7a:00.0`,
vendor/device `1002:13c0`, kernel `amdgpu`, Mesa 25.0.7. SDL2 used
radeonsi OpenGL 4.6; SDL3 used RADV Vulkan. Both passed 600 parity frames,
with zero failures after the specified checker-boundary allowance. Five
randomized trials per scene used 1,200 timed frames and 240 warmup frames.
Median drained frames/s, SDL2 then SDL3, were:

- Text: 7,709 / 7,115 (SDL3 −7.7%).
- Images: 2,819 / 2,894 (+2.7%).
- Shapes: 4,047 / 4,794 (+18.5%).
- Scroll: 8,270 / 6,633 (−19.8%).
- Zoom: 9,829 / 7,406 (−24.7%).

These are scene-specific observations from this machine, with trial spread
and concurrent workloads; the small image difference does not establish a
general advantage. The old forced-software comparison is not hardware
evidence. Raw frame samples and DRM counters are in ignored
`work/backend-render-benchmark/hardware.json`; the compact
`hardware-summary.json` includes every trial's throughput and CPU latency
p50/p95. Load averages were approximately 6.3/6.1/5.9 at both ends of the run.

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
including extensionless names and packaged bundle/APK assets. Raylib hosts opt
in with `AVIF_RAYLIB` and the pinned archives; builds without it decode no AVIF
and link no codec library. Native hosts
decode with the locked libavif 1.3 and dav1d, its assembly selected at run time
for the CPU (x86 SSE2 to AVX-512, ARMv7 and AArch64 NEON); Android 9 requires
no OS AVIF support. libaom is linked as the explicit comparison oracle, never
as a fallback. The still-image API shows the first complete frame of a
sequence. The ARMv7 oracle keeps libaom's portable kernels, because the locked
AOM/NDK combination emits unaligned NEON loads which fault on real Android
hardware; dav1d's own NEON runs there. The ARMv7 device suite covers both Debug
and Release, including the original large YUV420 crash input and background
image loading.
8/10/12-bit color and monochrome decode to straight 8-bit RGBA; alpha and
container clean aperture, rotation and mirroring are preserved. The renderer
then premultiplies for Cairo or composites normally in Raylib. HDR tone mapping
and ICC display color management are outside the existing 8-bit image surface.

Encoded files are limited to 256 MiB, dimensions to 32768 per axis and pixel
count to 67,108,864 (a 256 MiB RGBA buffer). These are independent axis/count
limits, permitting wide images below the pixel budget. Invalid/truncated
containers, invalid crops and oversized images return no image. 4:2:0 frames
of at least 8 MiP and other frames of at least 2 MiP decode on the process's
usable cores, capped at 4 threads; smaller frames use one, because threading
slowed them by up to 2.7x on the Moto. Thread count never changes output
bytes. Only the first decoded frame is retained.

| target | engine | proof |
|---|---|---|
| Linux x86-64 | dav1d, SSE2..AVX-512 | executed: strict parity, benchmark |
| Android armeabi-v7a | dav1d NEON | executed on the USB Moto (MT6765, Android 9): parity, benchmark |
| Android arm64-v8a | dav1d NEON, dotprod, i8mm | static build under qemu-user: strict parity |
| Android x86, x86_64 | dav1d SSSE3..AVX-512 | static build on the x86-64 host: strict parity |
| Linux AArch64 | dav1d NEON, dotprod, i8mm | qemu-user: strict parity |
| Windows, macOS | dav1d via the same recipe | unverified here |

`mk/raylib-library.mk` builds `AVIF_A`, `AOM_A` and Linux `AVIF_SO` beside the
existing renderer archives; `AVIF_A` contains dav1d. Building it needs Meson,
Ninja and, on x86, NASM; `ZIRAN_AVIF_TOOLS` can name a directory holding the
avif package's pinned Meson/Ninja. `kryon_android_codecs(webp, avif, aom)`
links the static codec for each ABI; older scaffolds receive the source paths
through `KRYON_ANDROID_AVIF_DIR`, `KRYON_ANDROID_AOM_DIR` and `KRYON_DAV1D_DIR`
(otherwise dav1d comes from the locked package graph). The Raylib artifact
includes the static codec archives. The desktop artifact includes the
self-contained `libkryonavif.so`; `renderer_link` retains it as a runtime
dependency so application packaging tools carry it into their lib directory.
Cairo also searches beside the executable and its `lib`/`../lib` directories;
`KRYON_AVIF_LIBRARY` can select an explicit codec file for custom hosts.

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
`make avif-render-test` checks real Cairo and private-display Raylib rendering,
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

The headless package suite, `build-queue -- sh tests/image_integration.sh`,
resolves real native artifacts and tests source and saved IR under ASan, UBSan
and leak detection, including adapter failure cleanup. With development-only
package overrides, set `ZIRAN_ROOT` and `ZIRAN_BIN` to the rebuilt toolchain.

## WebP images

Images, image previews and app media files accept WebP wherever they accept
PNG or JPEG, recognized by the file's `RIFF`/`WEBP` signature rather than
its name. Lossy, lossless and alpha pictures decode to straight RGBA. An
animated WebP decodes to its first complete frame, since Kryon images are
still; `ImageCompress` keeps animated originals unchanged.

- The raylib and Android hosts decode through `webp_image.zi` with libwebp's
  decoder and animation demuxer, built from the locked `libwebp` source
  package into the static `kryonwebp` archive (`cmake/WebP.cmake`, no
  encoder or threads). Images above 67108864 pixels are refused. The static
  adapter retains its 2147483647-byte input cap; Cairo retains its 256 MiB
  encoded-file budget.
	  `mk/raylib-library.mk` builds it as `WEBP_A` beside `RAYLIB_A`;
	  `kryon_android_codecs` builds it for each ABI; the `renderer-raylib`
  artifact lists `lib/libkryonwebp.a` in its archives.
- The desktop (Cairo) host loads the system libwebp demuxer
  (`libwebpdemux.so.2`) on first use, like its other optional codecs.
- The canvas host uses the browser's own WebP decoder.

`make raylib-webp-decode-test` and `make test-focus TEST=ziran_image_decode_linux_test` decode the
`tests/fixtures/webp` pictures natively, `tests/webp_browser_test.py` does the
same in headless Chromium, and `make android-webp-link-test` links the
decoder for every Android ABI.

The PNG byte and pixel rules are Ziran in `png.zi`. PNG supports all standard
color types and depths, transparency, all five filters, and Adam7 interlace.
Sixteen-bit samples use their high byte; ancillary color profiles and gamma
are ignored. `tests/ziran_png_test.sh` compares independent reference pixels
for 33 fixtures through source and saved IR in bundles, C and Go, including
malformed streams and bounded decoder output.

The raylib host uses `raylib_runtime.zi` and the raylib revision pinned as a
Ziran source dependency. The canvas host uses its dedicated `canvas_raster.zi` provider over
a Canvas2D browser ABI; see [Canvas2D browser host](canvas.md). Backend build
dependencies and link flags live in `mk/ziran-project.mk`. Platform calls are
declared with Ziran foreign imports; handwritten host implementation is
Ziran.

On Linux, the Raylib host resolves system typefaces and missing characters
through [Fontconfig](https://fontconfig.pages.freedesktop.org/fontconfig/fontconfig-devel/).
Measurement and drawing use the same fallback face and advances. TrueType and
OpenType collections are extracted into owned standalone font bytes following
the [OpenType table and checksum rules](https://learn.microsoft.com/en-us/typography/opentype/spec/otff).
Glyph atlases remain available for later frames, and queued text is flushed
before replacing an atlas.
Missing characters are checked against the source before rebuilding an atlas.
Text measurement refreshes the base atlas; cached drawing sizes refresh only
when used, so a new character cannot rebuild every cached zoom at once.

`make raylib-typeface-test` decodes real fonts and checks mixed Latin/CJK text,
supplementary characters, collection bounds and checksums, retained sources,
and disposal in C from source and saved IR. It replaces only graphics
submission and texture allocation, so it needs no display and does not verify
rendered pixels. Supply `RAYLIB_A` for an existing Raylib archive (and
`RAYLIB_LIBS` for its link flags); Fontconfig and an installed CJK outline font
are required. Whole-application captures still require a private display.

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
| Raylib | Uploaded texture; the CPU copy is freed after upload |
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
Raylib on a private Xvfb display and Canvas in headless Firefox against
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

A custom raylib Android host defines `KRYON_RAYLIB_HOST` during Ziran generation.
This selects raylib's pointer and clipboard exports while retaining Android
platform services. Direct touch consumers call `AndroidPointerFrame()` and
`AndroidSecondarySample()` from `android_touch`; those samples do not replace
the selected renderer's generic input exports.
JNI also selects raylib's `GetAndroidApp()` for this host, so private files,
Java calls and process preparation use the raylib-owned NativeActivity.
Before initializing graphics, custom hosts can read the announced native window
with `GetAndroidPendingSurfaceSize(width, height)` from `android_surface`. It
returns positive dimensions without consuming or acknowledging lifecycle commands.
Once EGL is current, surface synchronization reads its drawing-buffer extent.
If raylib's fixed buffer request no longer matches the native window after
picture-in-picture, synchronization releases that request. Subsequent EGL
buffers follow the native window size; software hosts use the native window.
The same synchronization maps window-local touch samples into the decorated
surface, including replayed quick taps and logical zoom.

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
the modifiers that were down. Desktop, Canvas and Raylib custom loops call
`DesktopInputTake(session)`, `CanvasInputTake(session)` or
`RaylibInputTake(session)` before each frame. Each take transfers ordered text
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
`make desktop-project-test` and `make raylib-project-test` for graphical
capture and input.
Never run graphical tests against the developer's live display.

The SDL/Cairo desktop host also exposes `DesktopSetTitle(title)` and
`DesktopTakeDroppedFile(output)` through `Desktop`. A file-manager drop queues
up to sixteen complete UTF-8 paths (4095 bytes each), only for the host's own
window. The application consumes and opens them during `Frame`; an output
buffer that is too small leaves the next path queued. Text drops are ignored.

## Portable programmable scene rendering

`scene_render` executes `shader` material programs in Ziran on CPU before
presentation. Cairo, Raylib (including WebAssembly), and Android's
ANativeWindow/Pixmap host consume the resulting managed bitmap through the
standard Image surface. These hosts do not need a shader-language bridge.
This interface does not claim GPU execution, GLSL compilation, vertex-shader
execution or GPU performance. See [programmable scene materials](API.md#programmable-scene-materials)
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
app. `make shader-web-test` exercises the corresponding Raylib WebGL presentation
inside a private headless browser. These targets also load a real PNG through
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

See [GPU scene APIs](API.md#portable-glsl-translation-and-optional-native-gpu-rendering)
for ownership, lifecycle, capability requirements and local test targets.

## Custom application entry

Applications that own their frame lifecycle import `kryon/application_entry`
and export `KryonApplicationMain(argc: s32, argv: **u8) -> s32` with
`#program_export "KryonApplicationMain"`. The adapter owns `main`: native
hosts forward their exact command line and browser hosts pass `(0, null)`.
The callback retains initialization, frame scheduling and shutdown policy;
Android's NativeActivity host can call the same native entry. Do not define
an additional application `main` when importing this adapter.
