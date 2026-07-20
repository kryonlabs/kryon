#ifndef UI_WIDGET_H
#define UI_WIDGET_H

#include "flint_compat.generated.h"

typedef struct UIWidget {
    char id[96];
    char kind[32];
    Rectangle bounds;
    int flags;
    int index;
    int active;
} UIWidget;

enum {
    UI_WIDGET_MOVABLE = 1 << 0,
    UI_WIDGET_RESIZABLE = 1 << 1,
    UI_WIDGET_READONLY = 1 << 2,
    UI_WIDGET_TEMPORARY_ID = 1 << 3
};

UIWidget BeginUIWidget(const char *kind, const char *id, Rectangle bounds,
                       int flags);
void UIWidgetSetBounds(UIWidget *widget, Rectangle bounds);
void UIWidgetSetAction(UIWidget *widget, const char *action);
void EndUIWidget(UIWidget *widget);

#endif
