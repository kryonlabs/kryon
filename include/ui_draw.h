#ifndef UI_DRAW_H
#define UI_DRAW_H

#include "kryon_compat.generated.h"
#include "ui_icon_types.h"

typedef struct {
    const char *text;
    Texture2D icon;
    UIIconType icon_type;
    int icon_size;
    int width;
    int font;
    int line_gap;
    Color color;
} UIParagraphSpec;

int GetUIFontSize(void);
int GetUISmallFontSize(void);
int GetUITitleFontSize(const char *title, int max_width);
int GetUIControlTextY(const char *text, int box_y, int box_h, int font);

void DrawCenteredUIControlText(const char *text, int center_x, int center_y,
                               int font, Color color);
void DrawLeftUIControlTextInRect(const char *text, Rectangle rect,
                                 int font_size, Color color);
void DrawFittedTextInRect(const char *text, Rectangle rect,
                            int preferred_size, int min_size, Color color);
#endif
