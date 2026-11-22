# Game touch controls

Import `kryon/cursor` for `CursorSet(session, CursorProps) -> bool`,
`CursorSetDefault(CursorProps) -> bool`, `CursorSetPointing(CursorProps) -> bool`
and `CursorHover(session, shape) -> bool`. A request persists until the next
deliberate change: repeating it every frame changes nothing, frames never clear
it, and the empty request `.{}` withdraws it. `CursorSetDefault` sets the
application cursor that every session inherits; it replaces the default, arrow,
clickable and disabled widget shapes, while text, crosshair and resize shapes
still show through. `CursorSetPointing` sets the application's pointing cursor,
shown instead only where a click does something: over clickable widgets and
over targets the app marks with `CursorHover`. Without one, clickable shows the
application cursor. A session's own request wins over all of them. Requests
copy their asset path; disposing or closing a session drops its request.

A target the app draws and hit-tests itself (a painted hotspot, a field)
reports the shape under the pointer with `CursorHover(session, shape)` while it
builds the frame, from the same test that decides its clicks. A frame keeps its
highest-priority mark, and its commit replaces the previous frame's, so the
cursor changes only when the hovered target does; a retained, cancelled or
failed frame keeps the committed mark, and a frame without one shows the
default. A widget with its own shape under the pointer wins over the mark,
except a disabled one the app's mark covers. A `Press` the app refines with its
own hit test, such as a painted shape inside its bounds, sets
`Press.Props.cursor` to `CursorArrow` while the pointer is off that shape; only
the cursor changes, never presses, touch or keys. Invalid shapes and inactive
sessions return false.

```zi
#import "kryon/cursor"
// Once at startup: a calm arrow everywhere, a hand only where a click works.
unused CursorSetDefault(CursorProps.{.asset_path = "assets/arrow.webp",
    .width = 32, .height = 32, .hotspot_x = 6, .hotspot_y = 2})
unused CursorSetPointing(CursorProps.{.asset_path = "assets/hand.webp",
    .width = 32, .height = 32, .hotspot_x = 8, .hotspot_y = 1})
// Each frame, from the hit test that opens the painted door.
if DoorAt(pointer.x, pointer.y) { unused CursorHover(session, cast(s32)CursorShape.CursorClickable) }
// A deliberate tool state; .{} returns to the application cursors.
unused CursorSet(session, CursorProps.{.asset_path = "assets/can.webp",
    .width = 32, .height = 32, .hotspot_x = 4, .hotspot_y = 6})
```

`width`, `height` and the integer hotspot are logical units, each size 1–128,
independent of the source image size and UI zoom; the hotspot is measured from
the top left and must be inside. Hosts rasterize at their own pixel density
(desktop `XCURSOR_SIZE`/24, web `devicePixelRatio`), so give a source at least
two to four times the logical size. `asset_path` is at most 4095 bytes with no
NUL. With no asset, use `shape` (the `CursorShape` values 0–10) or `hidden`;
size and hotspot must then be zero. An asset and `hidden` together are invalid.
Invalid requests return false and keep the last valid request. A missing or
undecodable image falls back to the requested system shape; only an explicit
hidden request hides the pointer. `CursorResolve(session, widget_shape)` returns
what a host shows.

The SDL/Cairo desktop host applies a real bitmap cursor scoped to its own
windows, caches decoded cursors, and calls the system only when the resolved
cursor changes, so switching between requests never flickers. X11 receives
premultiplied pixels and other drivers straight alpha, so soft edges show no
light fringe. It restores the system cursor on close. Canvas draws the
image into a CSS cursor (a 1x image plus an `image-set` density entry) with the
same hotspot on the canvas. Other hosts
retain their system pointer; touch feedback is drawn by the application. These
requests never move, lock, grab or warp a pointer.

A picture cursor needs the host's image decoder. Importing `kryon/cursor`, as
an application does to request a cursor, registers the decoder through the
`CursorPictureHost(asset_path, width, height, pixels, premultiplied) -> bool`
host capability; the cursor module the hosts themselves import does not, so a
program that requests no cursor links no image decoder. A host that shows no
picture cursors exports the capability and returns false, as the terminal and pixmap hosts do, and a custom host linked with a program
that imports `kryon/cursor` exports it too. An application that hosts widget
bundles imports `kryon/cursor` for its guests' picture cursors.

The test driver (`kryon/test_driver`) is test code and an ordinary build
leaves it out: its procedures then pass the platform's input through
unchanged. A build that a test drives defines `KRYON_TEST_DRIVER`, either with
`--define KRYON_TEST_DRIVER` on a direct `ziran` build or by setting the
variable in the environment of `kryon build`. The program then obeys the
directory the same variable names when it runs. A program built without the
driver and started with the variable set stops at once and says how to build
it, so a test fails with the reason instead of waiting for a reply.
On Android, the private `kryon-test-driver` folder activates a compiled test
driver. Normal builds ignore a folder left by an earlier UI test.

A test captures what the app draws with `DriverCapture(driver, path)` (the
`capture path=` command): the desktop host writes its next finished frame to
that file as a PNG, the same raster it shows. Hosts without a raster to read
reply "this host does not capture frames". A custom host captures by writing
its next finished frame to `TestDriverCapturePath()` when that is not "" and
reporting with `TestDriverCaptured(written)`.

Portable hosted apps use `CursorSet` and `CursorHover` with `KRYON_HOSTED_UI`
(the application cursors belong to their host). Their only effects are
`cursor.CursorSetHostedHost` and `cursor.CursorHoverHostedHost`, which marks
only the active session's building frame; `WidgetHostCall` requires the active
`WidgetHostBegin` session. Asset requests must name a normalized packed path
under `assets/`, pass that instance's existing asset policy, and resolve through
its existing asset resolver. The host copies the resolved path before returning;
no guest image bytes, pointer or native cursor handle crosses this boundary.
An unavailable resolution returns false. Hosts apply the resolved cursor after
committing the frame with `DesktopCursor(session)` or `CanvasCursor(session)`;
the standard loops already do this. Custom hosts must restore cursors before
teardown.
The desktop host selects the native arrow when its window opens, before the
first frame. `DesktopSetIcon(width, height, rgba)` copies straight RGBA into
that host's window icon. Dimensions must be 1–512 and the slice must contain
exactly `width * height * 4` bytes. The caller retains its pixels; theme-aware
applications can pass `GtkIconRaster` output and release it immediately.
The generic desktop player extracts packed assets into a private directory under
`TMPDIR` (an existing absolute directory), or the system default when unset,
and removes that directory on normal exit.

`kryon/joystick` provides `SwipeArea` (swipes and taps over a board) and
its `secondary_tap` option reports a right click as a tap at that position.
It is off by default. The same module provides
`Joystick` (a stick to lean). `kryon/direction_pad` provides
`DirectionPad(session, DirectionPadProps)`, a four-way pad like a game
controller's D-pad, drawn in the largest square of its bounds. Directions are
0 left, 1 up, 2 right and 3 down. It returns the arm held (`direction`) and
`step`: true when an arm is pressed or the thumb slides onto another arm,
then while held after twice `repeat_seconds` and at each repeat, like a held
arrow key. The hub stops, and a quick tap steps once. `floating` lays it
over a board: lighter arms and outlined arrows let the pieces under it show
through, and drawn after the board it takes the presses there. It is composed
from `Press`, `Box` and `SvgIcon`, so hosted games can use it.

`DirectionPadBounds(usable_viewport)` places a floating pad at the bottom left
with 24–32 logical units of edge padding and a 168–216 unit side on normal
phone screens, in either orientation. Pass the area inside system insets and
app bars; very small embedded areas shrink the pad to fit.

`joystick.SwipeFeedback(session, SwipeFeedbackProps.{key, gesture, size})`
draws a faint base at a `SwipeAreaResult`'s contact start and a glowing ball
following the finger, clamped to that base. It disappears on release and
does not take input. Draw it after the board; `size` is the base diameter
in logical units (56 by default). The app keeps its movement rules.
