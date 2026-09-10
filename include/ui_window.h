#ifndef KRYON_NATIVE_WINDOW_H
#define KRYON_NATIVE_WINDOW_H

/*
 * Extra OS windows rendered with the regular UI widgets.
 *
 * raylib owns one core window per process; these are additional plain SDL
 * windows that live next to it. Content is drawn between BeginNativeWindow()
 * and EndNativeWindow() with the normal widget calls (same rule as an offscreen
 * render texture): BeginNativeWindow clears the window to its background color
 * and binds a UI frame sized to the window, EndNativeWindow blits the result to
 * the OS window. Call the pair once per frame while the window is open.
 *
 * Mouse events on these windows are kept out of the core windows input
 * state; poll them with IsNativeWindowClicked().
 *
 * Desktop only; other platforms compile to no-ops. Linux and FreeBSD default
 * to a private X11 connection (secondary windows without touching the apps
 * SDL state); builds that prefer SDL windows, e.g. Wayland without XWayland,
 * compile this translation unit with -DNATIVE_WINDOW_HAVE_SDL.
 */

#include "kryon_compat.generated.h"

typedef struct NativeWindow NativeWindow;

enum {
    NATIVE_WINDOW_BORDERLESS = 0x01,     /* no OS decorations */
    NATIVE_WINDOW_ALWAYS_ON_TOP = 0x02,  /* float above normal windows */
    NATIVE_WINDOW_SKIP_TASKBAR = 0x04,   /* keep the window out of taskbars/docks */
    NATIVE_WINDOW_TOP_RIGHT = 0x08,      /* x/y are margins from the top-right
                                        corner of the primary displays work
                                        area instead of absolute positions */
    NATIVE_WINDOW_CENTER = 0x10,         /* center the window on the primary
                                        displays work area (x/y ignored) */
    NATIVE_WINDOW_STICKY = 0x20          /* keep the window visible across
                                        virtual desktops/workspaces when the
                                        window system supports it */
};

/*
 * Open a window. ui_scale is the UI scale widgets inside the window should
 * use (Scale etc.); pass the callers current combined DPI/user scale.
 * Returns NULL when windows are unsupported or resources ran out.
 */
NativeWindow *OpenNativeWindow(const char *title, int x, int y, int width, int height,
                       int flags, Color background, float ui_scale);

/* Close and free a window opened with OpenNativeWindow. NULL is safe. */
void CloseNativeWindow(NativeWindow *window);

/* Begin drawing this windows content; widgets draw into the window until
 * EndNativeWindow(). Also ends itself safely if the window is NULL. */
void BeginNativeWindow(NativeWindow *window);

/* Finish the window frame and blit it to the OS window. */
void EndNativeWindow(void);

/* Consume "the user clicked this window" (left button release inside it). */
int IsNativeWindowClicked(NativeWindow *window);

/* Consume a right-button click on this window. */
int IsNativeWindowRightClicked(NativeWindow *window);

/* Consume "the user dragged this window" (left press-drag that moved it);
 * also swallows the originating click so it is not reported as a plain
 * click. */
int IsNativeWindowDragged(NativeWindow *window);

/* Once-per-frame pump for the window system: applies drag motion recorded
 * by the SDL event watch and bridges core-window close requests. Called
 * by SetUIFrame; a no-op on backends that need no pumping. */
void PumpWindows(void);

/* Atomically read and clear "the core window was asked to close" (X button,
 * Alt+F4, WM delete). Apps whose event loop cannot rely on the bridged
 * SDL_QUIT poll this next to their tray actions. */
int StealCoreWindowClose(void);

/* Current window position (top-left, screen coordinates). */
void GetNativeWindowPosition(NativeWindow *window, int *x, int *y);

/* Window-relative position of the last button press on this window. */
void GetNativeWindowClickPosition(NativeWindow *window, int *x, int *y);

#endif /* KRYON_NATIVE_WINDOW_H */
