#ifndef UI_WIDGETS_H
#define UI_WIDGETS_H

/*
 * Widget library: the declaration vocabulary for .kry source files.
 *
 * These exist so that .kry code can call Text(...), Rect(...), Line(...),
 * Background(...), and Button(...) as plain function calls instead of
 * through special-case compiler keywords. The compiler (kc) treats them
 * exactly like any other call; the runtime records them as widget-tree nodes.
 */

#include "ui_color.h"        /* Color */
#include "ui_controls.h"     /* UIButtonStyle */
#include "ui_nav.h"          /* UITab */
#include "ui_layout.h"       /* GetUIViewWidth, GetUIViewHeight */
#include "ui_sprite.h"       /* UISprite */
#include "theme.h"           /* GetThemeText, GetThemeBackground, ... */

/* Declare a piece of text. */
void WidgetText(const char *label, int x, int y, int font_size, Color color);

/* Declare a filled rectangle with an optional 1px border. */
void WidgetRect(int x, int y, int w, int h, Color fill, Color border);

/* Declare a line. */
void WidgetLine(int x1, int y1, int x2, int y2, Color color);

/* Declare the screen background. */
void WidgetBackground(Color color);

/* Declare a button and return 1 if it was clicked this frame. */
int WidgetButton(int x, int y, int w, int h, const char *label,
                 UIButtonStyle style);

/* Declare an image-backed sprite from a project-relative or embedded asset path. */
void WidgetSprite(const char *asset_path, int x, int y, int w, int h);
void WidgetSpriteEx(UISprite sprite);

/* Declare a themed tab bar, update selected_index on click, and return the clicked tab or -1. */
int WidgetTabBar(int x, int y, int w, int h, const UITab *tabs, int count,
                 int *selected_index);

#endif /* UI_WIDGETS_H */
