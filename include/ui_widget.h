#ifndef KRYON_WIDGET_H
#define KRYON_WIDGET_H

#include "kryon_compat.generated.h"

typedef struct Widget {
    char id[96];
    char kind[32];
    Rectangle bounds;
    int flags;
    int index;
    int active;
} Widget;

enum {
    WIDGET_MOVABLE = 1 << 0,
    WIDGET_RESIZABLE = 1 << 1,
    WIDGET_READONLY = 1 << 2,
    WIDGET_TEMPORARY_ID = 1 << 3
};

Widget BeginWidget(const char *kind, const char *id, Rectangle bounds,
                   int flags);
void WidgetSetBounds(Widget *widget, Rectangle bounds);
void WidgetSetAction(Widget *widget, const char *action);
void EndWidget(Widget *widget);

#endif
