#ifndef UI_REORDER_H
#define UI_REORDER_H

#include "kryon_compat.generated.h"

typedef struct {
    int id;
    Rectangle bounds;
    int disabled;
} ReorderItem;

typedef struct {
    int id;
    Rectangle bounds;
    const ReorderItem *items;
    int item_count;
    int handle_width;
    int handle_height;
    int drag_threshold;
    int *scroll_offset;
    int max_scroll;
    int viewport_top;
    int viewport_bottom;
    int auto_scroll_margin;
    int auto_scroll_step;
} ReorderList;

typedef struct {
    int active;
    int dragging;
    int committed;
    int from_index;
    int to_index;
    int active_index;
    int target_index;
    int active_id;
    int pointer_y;
    int drag_delta_y;
} ReorderListResult;

ReorderListResult UpdateReorderList(ReorderList list);

#endif
