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

MediaAudio's Raylib and Canvas providers support individual voice Pause/Resume.
Raylib uses Pause/ResumeMusicStream and Pause/ResumeAudioStream, and reports
GetMusicTimePlayed for asset music. Canvas retains music offsets against the
AudioContext clock and re-creates only that voice's source on explicit Resume;
paused PCM retains at most two immutable buffers and their remaining offsets.
No owner pauses the shared context. See [MediaAudio](API.md) for capability
bits and position availability. Native tests use Raylib/miniaudio's paced null
backend; browser tests use a disposable muted headless context and trusted gesture.

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
| `canvas` | `src/backend/canvas_run.zi` | Canvas2D page with Emscripten and Asyncify |
| `dom` | `src/backend/dom_run.zi` | Semantic DOM layer over the Canvas2D raster host |
| `android` | app `src/main.zi` → `android_run.zi` | Android NativeActivity package, raylib OpenGL ES 2 |

The desktop host uses `cairo_raster.zi`. Its frame loop is `desktop_loop.zi`:
a project host that needs its own window size imports `kryon/desktop_loop`
and returns `RunDesktop(width, height)`, and `DesktopStartBeside(marker)`
starts a program opened from its build folder where its files are.
`DesktopPressedKey()` supplies the frame's last key to ordinary widget loops.
Games with several players can drain `DesktopNextKey()` for every key in arrival
order after `PollDesktop()`, up to 32 presses per poll. This snapshot is consumed
independently from the widget event and resets on the next poll. Numeric-keypad
digits use the shared codes 320–329; `DesktopModifiers()` follows the key most
recently drained.
The DOM host reuses
the Canvas raster provider and adds [semantic DOM reconciliation](dom.md).
Standalone `AppContext` applications can use the same two presentations with
the compiled hosts `canvas_app_run.zi` and `dom_app_run.zi`; the DOM entry
reuses the application frame lifecycle while emitting positioned native HTML
text, images and buttons with CSS boxes, gradients and clipping. Custom drawing,
meshes and texture effects use bounded Canvas tiles in paint order; the full-page
Canvas bitmap is not presented in this mode. The ordinary document-layout DOM
host remains available for browser flow layout.
Polygon-clipped images use native Cairo/Canvas path clips; DOM keeps these
custom image shapes in bounded Canvas tiles. Raylib caches bounded polygon triangulations and draws them directly on the
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
(`ANDROID_HOME`, default `$HOME/Android/Sdk`), and the locked `raylib`
source package.

At runtime a `NativeActivity` subclass owns the Java side: it reports window
insets, display cutouts, IME state, density, dark mode and orientation
through JNI natives registered by the generated `src/android_glue.zi`, and
forwards soft-keyboard text as codepoint, backspace and enter events. The
generated `src/main.zi` delegates to `AndroidRunMain`, which drives the
session with the shared raylib host, Kryon's Android viewport policies, and
the session-owned text input queue. Applications with their own JNI surface
keep their own `JNI_OnLoad` and call `AndroidJniOnLoad` with their activity
class to add the canonical natives. An application without Java code, a plain
`NativeActivity`, calls `PollAndroidWindowInsets` once per frame instead: it
reads the system bar, keyboard and cutout insets from the window through JNI
a few times a second, and stops once Java reports them.

The generation runs `ziran ir --project` for the whole graph, so application
module names may repeat Kryon module names; every package is prefixed in the
emitted C. `tests/ziran_android_device_test.sh` checks the device protocol
headlessly with the Java side stubbed.

Android frames use the shared two-stage pipeline by default. A persistent
app thread handles input, application code, layout and paint recording.
The NativeActivity thread keeps the EGL context, replays the previous
completed frame and presents it while the app thread builds the next one.
Each of the two frame buffers owns its text, asset paths and mesh points.
Retained paint and partial repaint pass through the same recorder.

Custom native hosts call `RunRaylibAppFrame(callback, argument)` once per
frame. The shared host manages the worker and its JNI attachment. Texture
and font resource requests run on the context owner; layout caches copied
measurements instead of borrowing mutable atlas data. Platform input is
polled after both stages finish, giving the next app callback a stable
event snapshot. `FlushRaylibAppFrames` finishes pending paint before an app
or bundle is replaced, and `DiscardRaylibAppFrame` drops paint from a lost
graphics context. Window shutdown joins the worker before closing GL.
An active frame-rate request can interrupt an older frame's idle pacing;
lower rates take effect with their recorded frame.

Image preparation captures encoded bytes on the app thread, before any
frame-scoped resource views expire. At most one source is captured per frame,
with one waiting source and one active decode job. The worker owns its bytes,
applies orientation, and hands owned pixels to the render thread.
Frames omit an image until its texture is ready;
completion invalidates retained paint. Layout reads copied image sizes, and
changing an asset reader discards results from its previous generation.

The web path remains serial. Native Linux hosts can share the dispatcher
when their window and input integration keeps all graphics on one owner.

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
`make desktop-project-test` and `make raylib-project-test` for graphical
capture and input.
Never run graphical tests against the developer's live display.

The SDL/Cairo desktop host also exposes `DesktopSetTitle(title)` and
`DesktopTakeDroppedFile(output)` through `Desktop`. A file-manager drop queues
up to sixteen complete UTF-8 paths (4095 bytes each), only for the host's own
window. The application consumes and opens them during `Frame`; an output
buffer that is too small leaves the next path queued. Text drops are ignored.
