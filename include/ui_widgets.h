#ifndef UI_WIDGETS_H
#define UI_WIDGETS_H

/*
 * Widget library — ordinary C functions that provide the verb-free
 * widget vocabulary for .kry source files.
 *
 * These exist so that .kry code can call Text(...), Rect(...), Line(...),
 * Background(...), and Button(...) as plain function calls instead of
 * through special-case compiler keywords. The compiler (kc) treats them
 * exactly like any other call; there is no built-in widget syntax.
 *
 * Every function here lowers to the existing immediate-mode draw calls
 * (DrawUIText, DrawRectangleRec, DrawLine, DrawUIGenericButton). Defaults
 * match what the removed keyword forms used to provide.
 */

#include "ui_color.h"        /* Color */
#include "ui_controls.h"     /* DrawUIGenericButton, UIButtonStyle */
#include "ui_layout.h"       /* GetUIViewWidth, GetUIViewHeight */
#include "theme.h"           /* GetThemeText, GetThemeBackground, ... */

/* Draw a piece of text. Mirrors the old `text` keyword. */
void WidgetText(const char *label, int x, int y, int font_size, Color color);

/* Draw a filled rectangle, with an optional 1px border when border is
 * non-transparent. Mirrors the old `rect` keyword. */
void WidgetRect(int x, int y, int w, int h, Color fill, Color border);

/* Draw a line. Mirrors the old `line` keyword. */
void WidgetLine(int x1, int y1, int x2, int y2, Color color);

/* Clear the whole view with a background color. Mirrors the old
 * `background` keyword. */
void WidgetBackground(Color color);

/* Draw a button and return 1 if it was clicked this frame, 0 otherwise.
 * Mirrors the old `button` keyword's positional form. */
int WidgetButton(int x, int y, int w, int h, const char *label,
                 UIButtonStyle style);

#endif /* UI_WIDGETS_H */
