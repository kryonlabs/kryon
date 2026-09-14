#ifndef KRYON_WIDGET_INTERNAL_H
#define KRYON_WIDGET_INTERNAL_H

#include "kryon_compat.generated.h"
#include "runtime/widget_kind.h"

typedef struct Widget {
    char id[96];
    char kind[32];
    Rectangle bounds;
    WidgetFlag flags;
    int index;
    int active;
} Widget;

Widget BeginWidget(const char *kind, const char *id, Rectangle bounds,
                   WidgetFlag flags);
void WidgetSetBounds(Widget *widget, Rectangle bounds);
void WidgetSetAction(Widget *widget, const char *action);
void EndWidget(Widget *widget);

#endif
