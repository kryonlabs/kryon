# Accessibility

Kryon's retained tree owns semantic labels, kinds, hierarchy, identity,
selection, editable values and focus. Platform actions return through the
same session and widget behavior used by pointer and keyboard input. Tab and
Shift+Tab traverse eligible controls; Enter and Space activate focused
controls while editors keep their normal key handling. Modal scopes reject
outside focus, activation and edit requests.

The Linux desktop host now exposes an optional AT-SPI bridge through GIO.
It registers the application with the accessibility bus, uses retained
generations for object paths, reconciles reordered and removed objects, and
provides Accessible, Application, Component, Action, Text and EditableText
interfaces where applicable. State/name/text changes refresh the AT-SPI
cache. Callbacks run on the UI thread between frames. An unavailable bus does
not prevent an ordinary desktop app from opening.

Text offsets use Unicode scalar counts in AT-SPI and UTF-8 bytes in Kryon.
Requests that would split a grapheme cluster are rejected. Read-only editors
omit EditableText. Passwords expose neither plaintext nor text length or
selection. Edit requests are queued; the widget applies its existing limits
and returns a caller-owned edit for the application to accept. A successful
platform request acknowledges the queue rather than promising application
acceptance. Each request and text read is limited to 64 KiB.

Custom Linux hosts can import `kryon/AccessibilityLinux`, call
`AccessibilityOpen`, poll between frames, synchronize after successful
commits and close before disposing the session. Supply the window's
client-area screen position with `AccessibilityOrigin`; the desktop host
updates it from its own SDL window. The default origin is (0, 0).

`sh tests/native_accessibility_test.sh` builds source and saved-IR fixtures
and drives them through real libatspi on private Xvfb and DBus sessions. It
checks discovery, identity after reorder, focus, Unicode text/selection,
readonly and disabled controls, modal blocking and password redaction.

The DOM host mounts native browser elements. Its private browser test checks
an editor in Chromium's accessibility tree as well as editing and focus.
These checks establish the implemented protocol paths, not complete Orca or
other screen-reader usability. Text boundary/extents methods, list Selection,
table interfaces, and Windows/macOS/mobile accessibility remain unverified
or unsupported. Raylib, libdraw, Canvas2D and terminal hosts do not currently
provide the Linux bridge automatically.
