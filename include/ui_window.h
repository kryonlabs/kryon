#ifndef KRYON_UI_WINDOW_H
#define KRYON_UI_WINDOW_H

/*
 * Extra OS windows rendered with the regular UI widgets.
 *
 * raylib owns one core window per process; these are additional plain SDL
 * windows that live next to it. Content is drawn between BeginUIWindow()
 * and EndUIWindow() with the normal widget calls (same rule as an offscreen
 * render texture): BeginUIWindow clears the window to its background color
 * and binds a UI frame sized to the window, EndUIWindow blits the result to
 * the OS window. Call the pair once per frame while the window is open.
 *
 * Mouse events on these windows are kept out of the core window's input
 * state; poll them with IsUIWindowClicked().
 *
 * Desktop only; other platforms compile to no-ops. Linux and FreeBSD default
 * to a private X11 connection (secondary windows without touching the app's
 * SDL state); builds that prefer SDL windows, e.g. Wayland without XWayland,
 * compile this translation unit with -DUI_WINDOW_HAVE_SDL.
 */

#include "kryon_compat.generated.h"

typedef struct UIWindow UIWindow;

enum {
    UI_WINDOW_BORDERLESS = 0x01,     /* no OS decorations */
    UI_WINDOW_ALWAYS_ON_TOP = 0x02,  /* float above normal windows */
    UI_WINDOW_SKIP_TASKBAR = 0x04,   /* keep the window out of taskbars/docks */
    UI_WINDOW_TOP_RIGHT = 0x08,      /* x/y are margins from the top-right
                                        corner of the primary display's work
                                        area instead of absolute positions */
    UI_WINDOW_CENTER = 0x10          /* center the window on the primary
                                        display's work area (x/y ignored) */
};

/*
 * Open a window. ui_scale is the UI scale widgets inside the window should
 * use (ScaleUIPx etc.); pass the caller's current combined DPI/user scale.
 * Returns NULL when windows are unsupported or resources ran out.
 */
UIWindow *OpenUIWindow(const char *title, int x, int y, int width, int height,
                       int flags, Color background, float ui_scale);

/* Close and free a window opened with OpenUIWindow. NULL is safe. */
void CloseUIWindow(UIWindow *window);

/* Begin drawing this window's content; widgets draw into the window until
 * EndUIWindow(). Also ends itself safely if the window is NULL. */
void BeginUIWindow(UIWindow *window);

/* Finish the window frame and blit it to the OS window. */
void EndUIWindow(void);

/* Consume "the user clicked this window" (left button release inside it). */
int IsUIWindowClicked(UIWindow *window);

/* Consume a right-button click on this window. */
int IsUIWindowRightClicked(UIWindow *window);

/* Consume "the user dragged this window" (left press-drag that moved it);
 * also swallows the originating click so it is not reported as a plain
 * click. */
int IsUIWindowDragged(UIWindow *window);

/* Once-per-frame pump for the window system: applies drag motion recorded
 * by the SDL event watch and bridges core-window close requests. Called
 * by SetUIFrame; a no-op on backends that need no pumping. */
void PumpUIWindows(void);

/* Current window position (top-left, screen coordinates). */
void GetUIWindowPosition(UIWindow *window, int *x, int *y);

/* Window-relative position of the last button press on this window. */
void GetUIWindowClickPosition(UIWindow *window, int *x, int *y);

#endif /* KRYON_UI_WINDOW_H */
