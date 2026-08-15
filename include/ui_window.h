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
 * Desktop (SDL) only; other platforms compile to no-ops.
 */

#include "kryon_compat.generated.h"

typedef struct UIWindow UIWindow;

enum {
    UI_WINDOW_BORDERLESS = 0x01,     /* no OS decorations */
    UI_WINDOW_ALWAYS_ON_TOP = 0x02,  /* float above normal windows */
    UI_WINDOW_SKIP_TASKBAR = 0x04,   /* keep the window out of taskbars/docks */
    UI_WINDOW_TOP_RIGHT = 0x08       /* x/y are margins from the top-right
                                        corner of the primary display's work
                                        area instead of absolute positions */
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

#endif /* KRYON_UI_WINDOW_H */
