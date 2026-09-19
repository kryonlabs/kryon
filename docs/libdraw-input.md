# libdraw keyboard input

On Linux with an X11 devdraw window, the libdraw backend observes keyboard and
focus events on that window using a separate X connection. This supplies actual
key-down/key-up edges, modifier state, Control shortcuts, and distinct Delete
and Backspace keys. Losing focus clears held keys. Ordinary text and composed
Unicode still come from plan9port's rune stream.

The backend identifies its window with a temporary process-specific title before
restoring the application title. `GetWindowHandle()` returns that X11 window ID;
`IsWindowFocused()` reports its focus state. Only events delivered to the app's
window are processed. A shortcut press and its modifiers remain available for
the application frame, even if the complete chord arrived between frames.

The X11 observer dynamically loads libX11 on Linux. Other libdraw hosts and
native Plan 9 retain their rune-based input path; this change does not establish
equivalent physical key-release/modifier support on those hosts. It does not add
a Wayland input backend. Mouse buttons continue to come from the host's existing
mouse transport, which can have Plan 9 modifier-button conventions.

`tests/libdraw_input_test.py` drives a real private X11 window and checks Control
and Shift combinations, fast chords, releases, Delete/Backspace and focus loss.
`tests/libdraw_exit_key_test.py` verifies default, custom and disabled exit keys.
Both run from `tests/libdraw_backend_test.sh` when Xvfb and xdotool are available.
