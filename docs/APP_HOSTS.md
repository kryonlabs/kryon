# Application hosts and embedding

The desktop ZX player launcher API, the SDL2/Cairo desktop host's optional
parts, Canvas host extensions and Android frame ownership. Read this when you
embed or launch Kryon bundles, or need host-level window and platform control.

## Linked desktop ZX player

Native launchers import `kryon/desktop_zx` and call
`RunWithBindings(path: string, options: PlayerOptions, bindings: []HostBinding) -> s32`
to play a desktop frame bundle or hosted app. Supply the exact bundle module
and function name in each `std/bundle_host.HostBinding`. Kryon binds its window, frame,
drawing and input capabilities first. The player rejects missing or ambiguous
capabilities before initializing the guest.

`PlayerOptions.{.check = true}` checks bindings without starting the guest or
opening a window. The launcher owns its callback contexts and binding storage
until the call returns. Callbacks receive typed `HostValue` values and their
own context. Use typed values or opaque application handles as the guest
interface. Run on the desktop thread, one instance at a time.

Build the launcher for C with `PLATFORM_DESKTOP` defined. Link the runtime
archives and desktop libraries specified in [`mk/zx-player.mk`](../mk/zx-player.mk)
along with the launcher's own native libraries. Grant each application's
native capabilities only to the bundle the launcher was designed to run.

For a hosted shell that runs other apps, `kryon/nested_app_host` provides a
`NestedAppHost` table owned by the linked launcher. Set its `ChildAppPolicy`
to approve child bytes and supply that child's services and asset resolver.
Use `CollectNestedAppBindings` with the inspected outer bundle. Keep that bundle
and the table alive until playback returns, then call `CloseNestedApps` even
if the outer guest traps. The shell uses `std/bundle_guest` for lifecycle and
packed asset reads and `kryon/nested_app` for frame, command, save and reply.
`Frame` draws into a bounded child scope inside the shell's current widget
frame. It receives the key and modifiers the shell passes, and its input
snapshot exposes pointer data within those bounds. Each child has its own
runtime state and the launcher grants its native services explicitly.

## Desktop host parts

The SDL2/Cairo desktop host (`desktop_runtime`) carries optional parts that
only programs importing them link. Key codes are Kryon's: letters as
uppercase ASCII, named keys from 256 (Escape), as `desktop_keys` maps SDL's.

| Module | Surface |
| --- | --- |
| `desktop_runtime` | Routes each SDL event to the window it names; `DesktopWheelMoveX()` gives the poll's sideways wheel steps (positive to the right) beside `DesktopWheelMove()`. Over another window of the program the main window's pointer is at -1, -1 and presses nothing. On X11 a button held by another client's grab, such as a window-manager move, produces no application press or release. `OpenDesktopWindow(width, height, title, options)` opens the window with any of `DesktopWindowResizable`, `DesktopWindowUndecorated`, `DesktopWindowTopmost` and `DesktopWindowAlwaysRun` (keep the frame cadence while hidden or minimized); `OpenDesktopTitled` is the resizable window. `DesktopFrameRate(fps)` caps drawing at `fps` frames a second, or draws without waiting with 0 (the default is 60, settling to 15 while nothing changes). `DesktopExitKey(key)` makes a key end the loop as closing the window does (0 turns that off). `PollMiddlePointer()` reports the middle button like `PollSecondaryPointer()` the right one. `DesktopPrepareGlyphs(text)` loads the fallback faces non-ASCII text needs before a frame draws it |
| `kryon/desktop_activity` | `DesktopWaitForActivity(descriptors, milliseconds)` sleeps until one of the program's own descriptors (such as a terminal's PTY) is readable, window input is queued, or the time ends, and returns `DesktopActivityDescriptor`, `DesktopActivityInput` or `DesktopActivityNone`. Queued input stays for the next `PollDesktop`. Windows and Android return `DesktopActivityUnsupported`; keep drawing there |
| `kryon/desktop_keyboard` | `DesktopKeyDown(key)`, `DesktopKeyPressed(key)` (the first press), `DesktopKeyRepeated(key)` (keyboard auto-repeat) and `DesktopKeyReleased(key)`, for the last `PollDesktop` |
| `kryon/desktop_window_state` | `DesktopMinimize`, `DesktopMaximize`, `DesktopRestore`, `DesktopRaise` and `DesktopSetAlwaysOnTop(enabled)` ask the window manager; `DesktopMinimized`, `DesktopMaximized` and `DesktopAlwaysOnTop` report what it did. `DesktopSetFullscreen(enabled)` and `DesktopToggleFullscreen()` fill the monitor at the desktop's own resolution and `DesktopFullscreen()` reports it. `DesktopMonitorBounds()` and `DesktopMonitorUsable()` give the window's monitor in desktop pixels, and `DesktopDisplayScale()` its density over 96 dpi (X11's `Xft.dpi` when set) |
| `kryon/desktop_secondary_window` | Up to three more windows, each with its own Cairo surface, presenter and events: `OpenSecondaryWindow(title, x, y, width, height, flags, background, scale)`, `CloseSecondaryWindow`, `BeginSecondaryWindow`/`EndSecondaryWindow` around `BeginSecondaryFrame`/`EndSecondaryFrame`, `SecondaryWindowInput`, `PumpSecondaryWindows`, `IsSecondaryWindowClicked`, `RightClicked`, `Dragged`, `TakeSecondaryWindowClose`, `GetSecondaryWindowSize`, `Position`, `ClickPosition`, `SetSecondaryWindowKeyboardGrab` and `CloseSecondarySession`, plus each window's own keys (`IsSecondaryWindowKeyDown`, `KeyPressed`, `KeyRepeated`, `KeyReleased`), `IsSecondaryWindowFocused` and `SecondaryDesktopWindow` (its SDL window) |
| `kryon/cairo_assets` | `RasterUseAssets(read)` gives the raster the program's own assets, such as files embedded in its executable: a picture whose bytes `read(path)` returns decodes from memory in any format the raster decodes. `RasterAssetsClose()` forgets them. Each asset the decoders read, and each face `kryon/cairo_typefaces` is given as bytes, is a file of its own: an in-memory file on Linux, and on Windows a file in the user's temporary folder, removed when it is closed or when the program exits |
| `kryon/cairo_typefaces` | `kryon/typeface_source`'s `RegisterTypefaceData`, `RegisterTypefaceFile`, `RegisterSystemTypeface`, `UseTypeface` and `ClearTypefaces` on Pango and fontconfig. Registered faces also serve scripts the selected face lacks |
| `kryon/cairo_render_scale` | `RasterSetTransform(scale, offset_x, offset_y)` maps layout points to pixels as `point * scale + offset`. It rejects scales outside `(0, 16]` and nonfinite offsets. The transform survives window resize; `RasterScale()` reads its scale. Custom hosts map their raw pointer with `RasterPointFromPixels(point)` before routing input to the widget tree |

`make desktop-windows-test` checks these on a private Xvfb with XTest input.

## Compiled canvas host extensions

`canvas_app_run.CanvasAppMainLoop()` runs an AppContext app compiled into
its page. Bind `canvas_app_run:CanvasAppMain=app:Main`. Private documents retain
`/kryon-apps/<app id>` on IndexedDB. The bare runner supports mini mode only.

A host may install `canvas_app_hooks.CanvasAppHooks` with `CanvasAppUseHooks`
before entering the loop. Every callback is optional: `start(mini) -> bool`
runs once after persistent storage and the app's documents are open; `pump()`
runs before input each frame; `stop() -> bool` runs after a successful start, before
storage flush and teardown. A false stop result makes the loop fail after cleanup.
A failed start must clean up its own partial work.
`supports(capability)` and `service(name, request)` provide host-owned optional
services. An absent service returns the usual unavailable JSON response.

`frame(session, bounds, key, modifiers, input) -> s32` runs inside the runner's
frame. Return 0 to run the app, 1 to replace its entire frame, or -1 to fail the
loop. Replacement frames exclusively receive input; the app callback is skipped,
including on the frame that closes an overlay. The host draws widgets but never
calls BeginFrame/EndFrame. Callbacks run on the frame thread. Account, payment,
authentication and application navigation policies belong to the embedding
package. A host decides which services it offers in mini mode.

### Android frames owned by the application

Android applications can import `kryon/android_egl` and call
`EnableAndroidGlesComposition()` before starting the first frame. A managed
`Frame` entry can call it on its first invocation. Kryon composes Pixmap paint
into a retained GLES 3 framebuffer and swaps the EGL window surface. It
falls back to CPU Pixmap if GLES setup or presentation fails. Explicit frame
snapshots still return ordinary owned textures and may read back GPU pixels.
The `Kryon GL_RENDERER` log names the driver. The `Kryon` log records the
first successful EGL swap and any software fallback.

`kryon/android_runtime` hosts applications with their own main loop. Select it
as the Android host when generating native sources. `PollAndroidHost(timeout_ms)`
drains lifecycle and input events and returns false only on destruction;
a missing window, pause or lost focus does not end the loop. Query
`AndroidHostPaused()`, `AndroidHostFocused()` and `AndroidHostViewport()` before
composing. `BeginAndroidFrame()` borrows the NativeActivity surface for Pixmap;
`EndAndroidFrame()` presents its device-pixel damage. The app owns its widgets,
state and pacing. Resizing, density, insets, redraw and surface replacement
invalidate retained pixels; an unchanged frame skips presentation.
After polling, call `SetAndroidPausedSurfaceVisible(visible)` if the operating
system reports that a floating window such as picture-in-picture is visible.
This permits drawing on a live surface during an activity pause while
preserving the usual pause and focus reports.

Pixmap's `RasterSetTransform(scale, offset_x, offset_y)` and
`RasterPointFromPixels(point)` use the same camera for painting and input.
The inverse takes physical pixels, including a negative logical origin. The
held camera survives frame borrows and surface resize. Device damage is
already scaled and must not be scaled again by the presenter.

`kryon/pixmap_assets_android` adds owned decoded pictures. A picture the
application ships decodes from its APK assets the first time Pixmap measures
or draws its path; a missing asset is looked up once, until invalidation or
host cleanup. The Android Frame host imports it, and an application that owns
its loop imports it to show pictures.
`PixmapLoadImageBytes(path, data, file_type)` replaces a registered Pixmap
picture only after a successful bounded decode. Preview decoding returns owned,
upright straight RGBA through the ordinary image-preview capabilities.
Path, folder and full image invalidation share the raster-cache API; replacing
or closing owned entries releases their pixels and sampling cache. A borrowed
picture installed with `PixmapSetImage` remains borrowed.

`kryon/pixmap_typefaces_android` provides the ordinary `typeface_source`
capabilities: register a font file, borrowed font bytes or the system face,
then select its registered name. Bind those capabilities to this provider.
Selection installs Pixmap's optional text rasterizer; the default Pixmap font
needs no Android font service. Android Paint shapes and measures the same run
that Canvas rasterizes into coverage for Pixmap's camera, clipping, rotation
and alpha composition. The line box is the requested font size, matching the
desktop layout contract. System font lookup uses `system_typeface_android`,
and Android supplies script fallback for the selected face. Clearing faces
restores the built-in Pixmap font. CPU discard clears raster masks while
retaining registered faces; host teardown releases owned references and masks.
