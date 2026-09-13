#ifndef KRYON_DRAW_H
#define KRYON_DRAW_H

#include "kryon_compat.generated.h"
#include "ui_icon_types.h"

typedef struct {
    const char *text;
    Texture2D icon;
    IconType icon_type;
    int icon_size;
    int width;
    int font;
    int line_gap;
    Color color;
    int align;
} ParagraphSpec;

int GetFontSize(void);
int GetSmallFontSize(void);
int GetTitleFontSize(const char *title, int max_width);
int FitFontSize(const char *text, int max_width,
                int preferred_size, int min_size);

#endif
