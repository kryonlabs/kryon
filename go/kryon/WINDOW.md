# Native Go Window Runtime

`go/kryon.Open` starts a real desktop window on Linux without cgo or the legacy
`go/kryui` bridge. The Linux backend talks to X11 directly over the display
socket, renders the existing Go frame-operation stream with `RenderFrame`, and
presents the pixels with `PutImage`.

The backend feeds X11 mouse clicks and keyboard input into the same runtime
controllers used by `NewHost` tests:

- button press events queue taps
- printable keysyms queue text
- Backspace, Tab, Enter, Delete, Left, Right, Home, and End queue Kryon keys
- Ctrl+A/C/V/X queue Kryon shortcuts
- window close requests make `WindowShouldClose` return true
- resize events update `GetScreenWidth` and `GetScreenHeight`

When no Linux display is available, `Open` falls back to the headless runtime so
tests and non-window tools keep working.

Non-Linux desktop window backends are not implemented in `go/kryon` yet.
