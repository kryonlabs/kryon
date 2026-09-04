#ifndef UI_PAGER_H
#define UI_PAGER_H

#include "kryon_compat.generated.h"
#include "ui_swipe.h"

typedef struct UIGuidePagerProps {
    Rectangle content_bounds;
    Rectangle footer_bounds;
    UISwipeGesture *swipe;
    int page;
    int page_count;
    int focus_id;
    const char *close_label;
    const char *back_label;
    const char *next_label;
    const char *finish_label;
} UIGuidePagerProps;

typedef struct UIGuidePagerResult {
    int page;
    int changed;
    int closed;
    int finished;
    UISwipeResult swipe;
} UIGuidePagerResult;

UIGuidePagerResult DrawUIGuidePager(UIGuidePagerProps pager);

#endif
