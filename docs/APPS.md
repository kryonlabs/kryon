# Applications and window hosts

How a Kryon application starts, which host draws it on each platform, and how
input reaches the session. Read this when creating an app entry, embedding a
bundle, or wiring custom input.

The smallest migrated executable application is `examples/window.tsx`:

```tsx
import * as App from "kryon/app";

export function main(): s32 {
    return <App.Show title="Window" width={s32(320)} height={s32(160)}/>;
}
```

Such a file is a *program*: it owns `main`, and `kryon/app` selects the desktop
host for a Linux C build and the Canvas host for a web build. Build it directly
from this checkout:

```sh
ziran build --project --target=c --exe --root examples \
  --entry window:main -o build/examples/window-native examples/window.tsx
```

Kryon's `[package] links` names the desktop host's system libraries, so no
caller `LDLIBS` is needed. Ziran applies those links to the
root manifest only; an application that depends on Kryon builds through the
project tool, which supplies them. Native declarations supply the JSX props;
the compiler has no widget catalogue. The full TSX widget and state migration
is tracked in `plans/tsx-migration.md`.

A program carries only what it uses. `kryon/app` imports each platform's
window (`desktop_window`, `page_window`) and frame loop (`frames_desktop`,
`frames_web`) as separate modules, so `<App.Show title="Window"/>` reaches only the
window: on Linux the checked program of `examples/window.tsx` is the window,
`kryon/app`, `app_types`, `desktop_window`, `desktop_process_linux`,
`sdl_event`, `pixmap_sdl` and C strings. In a portable bundle the window is `zx_window`: the checked program is
the window, `kryon/app`, `app_types` and `zx_window`, and the bundle asks its
host only for the window, its events, clearing and presenting
(`ZxWindowOpenHost`, `ZxWindowEventHost`, `ZxWindowClearHost`,
`ZxWindowPresentHost`, `ZxWindowCloseHost`). Kryon's desktop zx player binds
them to the same SDL window and runs the bundle's `main` as native code, so
with the player installed (`make install-user`) `ziran run window.zx` opens
the window (`zx window.zx` is the standalone ZX Runner). The desktop host library, `libkryon-desktop.so` from `zx_window_host.zi` (`make zx-window-host`),
is registered by `make install-user` as `kryon-desktop.library` and exports
both the bare `zx_window` capabilities (`App.Show`) and the full native
SDL2/GPU/Cairo frame-loop, raster, text-measurement (`MeasureGlyphWidth`),
input, clipboard, clock and service capabilities (`App.Run`), so `zx2native`
writes a Linux executable that links the real desktop SDL2/GPU stack directly.
While pure/console/file/clock `zx2native` ELFs stay fully static (no
`PT_INTERP` or `PT_DYNAMIC`), graphical desktop `zx2native` executables link
`libkryon-desktop.so` dynamically so portable `.zx` UI programs run at native
speed on the platform's real SDL2 hardware GPU 2D renderer (`desktop_gpu.zi`
using `SDL_RenderGeometry`, retained GPU target textures, and a GPU atlas for
rounded corners, cached Pango/Cairo text runs, and images) instead of a
CPU-only software rasterizer. `make zx-window-test`
checks `window.tsx` on a private Xvfb, and that with no library registered
`zx2native` stops at the gap naming `ZxWindowOpenHost`. In
the browser, a project profile with `backend = "canvas"` and `codegen = "zx"`
takes a program entry too: its bundle is the program's own `main`, and the ZX
canvas player binds the window to the page's canvas (`window_page_player`),
lowers the bundle to Wasm in the page and runs it, each wait for an event
yielding to the page. `App.Run(window, Draw)` brings the frame loop, the widget tree's frame
driver (`TreeFrameUse`) and, on Linux, the accessibility tree
(`DesktopAccessibilityUse`). The desktop raster loads pictures, smooth copies and
fonts when a frame first draws them, video when it opens, and the test driver
only with `KRYON_TEST_DRIVER`. `make pay-per-import-test` checks the examples'
generated C, the window's checked programs and its bundle's modules and host
capabilities.

The project tool compiles a program entry through checked IR.
`kryon build --profile NAME` asks the compiler to emit the checked process
entry for native and Canvas C builds; it does not generate a Ziran source
wrapper or scan the source for an entry signature. A program entry uses
`codegen = "c99"` for native source builds. Desktop and Canvas
`codegen = "zx"` profiles bundle the program's `main`; the player binds its
window capabilities. Native custom hosts remain Ziran implementation modules.

The selected host owns the process entry point, calls `BeginFrame` and
`EndFrame`, supplies input to the session, and presents paint operations.
Every host reports the same key codes through `KeyboardTake`: letters as
uppercase ASCII and named keys as `KeyEnter`, `KeyBackspace`, `KeyLeft`, and
the other `Key` constants. A letter is the one the active keyboard layout types,
not its physical position. `TypedTextTake` collects pending text into the caller's byte
buffer, and `TextFieldInputFor(key, modifiers, text)` turns both into a
focused `TextField`'s input.
An entire Unicode scalar must fit before it is consumed; a buffer of at least
four bytes guarantees progress. `tree_input.TypedCodepointPeek(session)` reads
the next scalar without consuming it, and `TypedCodepointCapacity(session)`
reports free slots in the bounded 32-scalar session queue.

Every graphical host owns an ordered key/text queue and exposes one session
delivery call: `DesktopInputTake(session)` or `CanvasInputTake(session)`. Poll the host every frame, including while session
text remains unread, then call its take once before composition. The take
transfers one editing action or a printable-key/text run into that session and
returns `(key, modifiers)` using portable key and modifier codes. Unread text
delays later editing actions; Return can commit a text run delivered in the
same call. Focus loss cancels queued keys so a queued shortcut cannot fire after
focus returns, while text already committed by the platform remains ordered.

Desktop's bounded 256-record delivery queue backpressures SDL key and text
events without blocking window lifecycle events. Run
`build-queue -- make test-focus TEST=desktop_input_backpressure` for the native
SDL source and saved-IR regression.
The Canvas app host retains unread key presses, repeats, IME commits and
gamepad navigation across drawing frames. Pressed/released snapshot flags still
expire at the frame boundary; delivery does not replay them. Each queued press
or repeat retains its browser event's Shift, Control/Meta and Alt modifiers.
Custom loops call `CanvasInputTake(session)`; an empty take reports current held
modifiers. `CanvasModifiers()` and `CaptureFrameInput(session).modifiers` report held
state at their own sampling time, so consuming a shortcut never changes held
keys or snapshot flags.

Application code owns its product state and calls widgets within `Frame`.
The complete manifest and build commands are in [PROJECTS.md](PROJECTS.md).

Individual `.zi` modules under [`src/ui`](../src/ui) can also be imported by
non-graphical Ziran programs. A program that does not import Kryon uses no
Kryon code. A portable `.zx` links only imported modules and needs a host
binding for any platform effect they call.

To run a portable `.zx` as a windowed app in a host such as TaijiOS, use
[`kryon/host/surface`](BACKENDS.md#hosted-zx-software-surface). The bundle runs
ordinary Kryon widgets with its own session and software raster. The host
supplies the window, pixel presentation and ordered input events. Compose the
screen and wire widget events in TSX, as in
[`hosted_surface.tsx`](../examples/hosted_surface.tsx). Its Ziran helper owns
domain state and native scheduling/pixel callbacks; see the
[build command](BACKENDS.md#hosted-zx-software-surface).

Call `Surface.Run(window, Draw, 250)` to compose on input and every 250 ms
when idle. Apps that draw through Kryon's paint queue can call
`Surface.RunRetained(window, Draw, 250)` with the same window and callback.
It keeps the last RGBA frame, repaints damage in bounded strips, and presents
on damage or a host redraw request. Custom paint emitters use a full replay.

Use `wake = Wake` with `idle_ms = 0` to schedule state checks independently
of composition. `Wake(now_ms: s64) -> Surface.SurfaceSchedule` returns the
next `wait_ms` and whether an idle event needs a `redraw`. Pass a clock to
receive its sample; otherwise `now_ms` is -1. Nonzero waits are bounded to
1–60000 ms; zero uses the default wait.
Unchanged timeout checks skip composition; input and host redraws still draw.

Surface wheel events reach Kryon scroll widgets and `CaptureFrameInput` for
the current callback. Pass `clock = Clock.Milliseconds` to either entry point
to sample the monotonic `kryon/clock` service on each callback. This fills
`elapsed_ms` and `delta_ms` and lets scroll widgets animate wheel motion.
With the default clock, wheel motion is immediate and the four Surface host
bindings suffice.

Import `kryon/gtk_style_types` for GTK style nodes, state flags, metrics and
bitmap records in portable code. The records are shared with `GtkStyleLinux`.
An empty metrics record has `available = false` so portable callers can use
their own palette and layout when native GTK measurement is unavailable.

Import `kryon/window_chrome` to draw a themed caption and window controls.
Pass the current `Session` and `Chrome.Props` to `Chrome.Draw`, with a stable
key, bounds, title, focus flag, and a bit mask of enabled controls. The host
can use `Chrome.PointerPress` with a per-window `Chrome.State`, position,
and millisecond timestamp. Call `Chrome.CancelClick` when a press becomes a
drag or pointer capture ends. `Chrome.KeyPress` maps unhandled keys for the
focused window. The caller dispatches the returned `Chrome.Action`.
