# Window focus

Import `kryon/Window` and call `WindowFocused()` to read whether the application's
own visible window has keyboard focus. The call changes no window, focus, input
or desktop setting. Linux SDL and the desktop portable player inspect only the
window created by their host. Hidden and minimized SDL windows report false.
Raylib and browser canvas hosts use their existing window focus signals.

An embedded pixel-surface player forwards `WindowFocusedHost` to its platform
callback. The callback must return the focus of that child application, rather
than the focus of the entire supervising window. A missing or unsupported focus
signal returns false. Terminal, libdraw and bare pixel-surface hosts currently
have no application focus signal and return false.

Applications can use focus changes to start a local background timer. Keep the
timer and lock policy in the application. Use a monotonic clock, check an expired
timer before accepting a returning focus event, and discard delayed permission
responses after access ends. Window focus does not authorize data access.
