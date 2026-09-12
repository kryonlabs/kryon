#ifndef KRYON_PAGER_H
#define KRYON_PAGER_H

#include "kryon_compat.generated.h"
#include "ui_swipe.h"

typedef struct GuidePagerProps {
    Rectangle content_bounds;
    Rectangle footer_bounds;
    SwipeGesture *swipe;
    int page;
    int page_count;
    int focus_id;
    const char *close_label;
    const char *back_label;
    const char *next_label;
    const char *finish_label;
} GuidePagerProps;

typedef struct GuidePagerResult {
    int page;
    int changed;
    int closed;
    int finished;
    SwipeResult swipe;
} GuidePagerResult;

GuidePagerResult GuidePager(GuidePagerProps pager);

#endif
