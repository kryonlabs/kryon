# Cursor input

`src/ui/cursor.zi` owns cursor shape choice and priority. An application keeps
one `CursorFrame` per window. At the start of a frame, call
`CursorBeginFrame(state)`. For hovered widgets, call `CursorRequest(state,
shape)`, optionally using `CursorShapeForWidget(kind, disabled)` to choose the
shape. The latest request at the highest priority wins; resize shapes outrank
clickable, text, and disabled shapes. At the end, call `CursorEndFrame(state)`
and retain `decision.state`. When `decision.apply` is true, the application
applies `decision.state.applied` to its own cursor API.

No Kryon host callback is required. The platform maps the requested shape to
its own cursor API; it does not decide which widget gets which shape. The
default, text, clickable, resize, and disabled shape numbers are 0, 2, 4,
5–9, and 10.

This checked path is ordinary Ziran and returns only values.
