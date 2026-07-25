#include "ui_widgets.h"

#include "kryon_compat.generated.h"  /* DrawRectangleRec, DrawRectangleLinesEx, DrawLine, BLANK, Rectangle */

void
WidgetText(const char *label, int x, int y, int font_size, Color color)
{
    DrawUIText(label, x, y, font_size, color);
}

void
WidgetRect(int x, int y, int w, int h, Color fill, Color border)
{
    DrawRectangleRec((Rectangle){x, y, w, h}, fill);
    /* A border alpha of 0 means "no border", matching the old `rect`
     * keyword which only drew the outline when a border: prop was given. */
    if(border.a != 0)
        DrawRectangleLinesEx((Rectangle){x, y, w, h}, 1, border);
}

void
WidgetLine(int x1, int y1, int x2, int y2, Color color)
{
    DrawLine(x1, y1, x2, y2, color);
}

void
WidgetBackground(Color color)
{
    DrawRectangleRec((Rectangle){0, 0, GetUIViewWidth(), GetUIViewHeight()},
                     color);
}

int
WidgetButton(int x, int y, int w, int h, const char *label,
             UIButtonStyle style)
{
    return DrawUIGenericButton(x, y, w, h, label, style, 0, NULL);
}
