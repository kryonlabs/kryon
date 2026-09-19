# Native input verification

Linux native Go receives X11 pointer/keyboard events and IBus composition without
cgo. The IBus context belongs to the current eligible editable field. Focus
changes reset pending composition, disabled/read-only/secure fields do not
activate it, and late unfocused signals are ignored. Candidate coordinates use
the same measured rows, font and scrolling as the painted caret.

`Text(TextProps{Selectable: true, ...})` supports drag selection across wrapped
lines, Ctrl+A and Ctrl+C. Unicode grapheme boundaries keep combining characters
and emoji intact. Copied text preserves source whitespace even when paragraph
layout collapses display whitespace. Popup capture and editor focus take input
ownership away from selectable text.

Run the integration checks with:

```sh
sh tests/native_go_input_test.sh
```

The script needs Go, Xvfb, xauth, xdotool, dbus-run-session, IBus and its Hangul
engine. It creates a private X display, D-Bus session and configuration directory.
It does not switch the user's desktop input engine. Tests open a real native Go
window, drag a range with X11 events, copy it, and compose/commit `가` using IBus.
Set `KRYON_INPUT_CAPTURE=/tmp/kryon-input.png` to save the presented selection
frame. Never enable the live Hangul test directly on a user's session: it changes
the test bus's global engine.

Ordinary Go tests cover protocol serialization, late composition rejection,
wrapped/scrolled caret coordinates, Unicode selection, clipping, popup capture,
and the rule that releasing the mouse must not inject a second press.

This establishes Linux/X11 native Go delivery. It does not establish macOS,
Windows or Wayland input adapters, nor detailed per-character preedit styling.
