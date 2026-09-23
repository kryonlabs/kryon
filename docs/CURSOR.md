# Cursor input

`src/ui/cursor.zi` owns cursor shape choice and priority. An application keeps
one `CursorFrame` per window. At the start of a frame, call
`CursorBeginFrame(state)`. For hovered widgets, call `CursorRequest(state,
shape)`, optionally using `CursorShapeForWidget(kind, disabled)` to choose the
shape. The latest request at the highest priority wins; resize shapes outrank
clickable, text, and disabled shapes. At the end, call `CursorEndFrame(state)`
and retain the returned state. `CommitCursor(decision)` applies the shape only
when it changed.

The native generated host supplies `ApplyCursorShape(i32)`. Portable `.zib`
hosts bind `CursorBinding` with a `CursorPlatform` callback. The platform maps
the requested shape to its own cursor API; it does not decide which widget
gets which shape. The default, text, clickable, resize, and disabled shape
numbers match the existing Kryon platform mapping (0, 2, 4, 5–9, 10).

This checked path replaces the old `cursor_host.zi` source. Existing
downstream C hosts have not yet been converted to call it.
