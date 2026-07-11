#include "ui.h"

typedef struct UIReorderState {
    int list_id;
    int item_id;
    int from_index;
    int press_y;
    int press_offset_y;
    int scroll_start;
    int dragging;
} UIReorderState;

static UIReorderState g_ui_reorder_state = {0};

static int
ui_reorder_find_index(const UIReorderList *list, int item_id)
{
    if(list == NULL || list->items == NULL)
        return -1;
    for(int i = 0; i < list->item_count; i++) {
        if(list->items[i].id == item_id)
            return i;
    }
    return -1;
}

static int
ui_reorder_target_index(const UIReorderList *list, int active_index,
                        int pointer_y)
{
    int target = 0;

    if(list == NULL || list->items == NULL || list->item_count <= 0)
        return -1;

    for(int i = 0; i < list->item_count; i++) {
        const UIReorderItem *item = &list->items[i];
        int center_y;

        if(i == active_index)
            continue;
        center_y = (int)(item->bounds.y + item->bounds.height / 2.0f);
        if(pointer_y > center_y)
            target++;
    }

    return ui_clampi(target, 0, list->item_count - 1);
}

static void
ui_reorder_cancel(void)
{
    memset(&g_ui_reorder_state, 0, sizeof(g_ui_reorder_state));
    if(g_ui_pointer_owner == UI_POINTER_OWNER_REORDER)
        g_ui_pointer_owner = UI_POINTER_OWNER_NONE;
}

UIReorderListResult
UpdateUIReorderList(UIReorderList list)
{
    UIReorderListResult result = {0};
    Vector2 mouse = ui_mouse_world();
    int pointer_y = (int)mouse.y;
    int captured = ui_input_captures_click_internal(mouse, 0);
    int threshold = list.drag_threshold > 0 ? list.drag_threshold : ScaleUIPx(5);
    int handle_w = list.handle_width > 0 ? list.handle_width : ScaleUIPx(36);

    result.from_index = -1;
    result.to_index = -1;
    result.active_index = -1;
    result.target_index = -1;
    result.active_id = 0;
    result.pointer_y = pointer_y;

    if(list.item_count < 0)
        list.item_count = 0;

    if(g_ui_reorder_state.list_id != 0 &&
       g_ui_reorder_state.list_id != list.id) {
        if(!IsMouseButtonDown(MOUSE_BUTTON_LEFT))
            ui_reorder_cancel();
        return result;
    }

    if(g_ui_reorder_state.list_id == list.id) {
        int active_index = ui_reorder_find_index(&list,
                                                 g_ui_reorder_state.item_id);
        int dy = pointer_y - g_ui_reorder_state.press_y;

        if(active_index < 0 || active_index >= list.item_count ||
           list.items == NULL || list.items[active_index].disabled) {
            ui_reorder_cancel();
            return result;
        }

        result.active = 1;
        result.from_index = g_ui_reorder_state.from_index;
        result.active_index = active_index;
        result.active_id = g_ui_reorder_state.item_id;
        result.drag_delta_y = dy;
        result.target_index = ui_reorder_target_index(&list, active_index,
                                                      pointer_y);
        result.to_index = result.target_index;

        if(IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            int abs_dy = dy < 0 ? -dy : dy;

            if(!g_ui_reorder_state.dragging && abs_dy >= threshold) {
                g_ui_reorder_state.dragging = 1;
                g_ui_pointer_owner = UI_POINTER_OWNER_REORDER;
            }
            if(g_ui_reorder_state.dragging) {
                int view_top = list.viewport_top > 0
                                   ? list.viewport_top
                                   : (int)list.bounds.y;
                int view_bottom = list.viewport_bottom > 0
                                      ? list.viewport_bottom
                                      : (int)(list.bounds.y + list.bounds.height);
                int margin = list.auto_scroll_margin > 0
                                 ? list.auto_scroll_margin
                                 : ScaleUIPx(34);
                int step = list.auto_scroll_step > 0
                               ? list.auto_scroll_step
                               : ScaleUIPx(12);

                result.dragging = 1;
                PushUIInputCapture((Rectangle){0, 0, (float)ui_view_width,
                                               (float)ui_view_height}, 0);

                if(list.scroll_offset != NULL && list.max_scroll > 0) {
                    if(pointer_y < view_top + margin)
                        *list.scroll_offset -= step;
                    else if(pointer_y > view_bottom - margin)
                        *list.scroll_offset += step;
                    *list.scroll_offset = ui_clampi(*list.scroll_offset, 0,
                                                    list.max_scroll);
                }
            }
            return result;
        }

        if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT) &&
           g_ui_reorder_state.dragging) {
            result.dragging = 0;
            result.committed = result.to_index >= 0 &&
                               result.to_index < list.item_count &&
                               result.to_index != active_index;
            result.from_index = active_index;
            PushUIInputCapture((Rectangle){0, 0, (float)ui_view_width,
                                           (float)ui_view_height}, 0);
        }
        ui_reorder_cancel();
        return result;
    }

    if(list.id == 0 || list.items == NULL || list.item_count <= 0 ||
       !IsMouseButtonPressed(MOUSE_BUTTON_LEFT) || captured ||
       g_ui_pointer_owner != UI_POINTER_OWNER_NONE ||
       !CheckCollisionPointRec(mouse, list.bounds))
        return result;

    for(int i = 0; i < list.item_count; i++) {
        const UIReorderItem *item = &list.items[i];
        Rectangle handle;

        if(item->disabled)
            continue;
        handle = item->bounds;
        handle.width = (float)handle_w;
        if(CheckCollisionPointRec(mouse, handle)) {
            g_ui_reorder_state.list_id = list.id;
            g_ui_reorder_state.item_id = item->id;
            g_ui_reorder_state.from_index = i;
            g_ui_reorder_state.press_y = pointer_y;
            g_ui_reorder_state.press_offset_y =
                pointer_y - (int)item->bounds.y;
            g_ui_reorder_state.scroll_start =
                list.scroll_offset != NULL ? *list.scroll_offset : 0;
            g_ui_reorder_state.dragging = 0;
            result.active = 1;
            result.from_index = i;
            result.to_index = i;
            result.active_index = i;
            result.target_index = i;
            result.active_id = item->id;
            PushUIInputCapture((Rectangle){0, 0, (float)ui_view_width,
                                           (float)ui_view_height}, 0);
            return result;
        }
    }

    return result;
}

void
DrawUIReorderHandle(int x, int y, int w, int h, int active)
{
    int dot = ScaleUIPx(3);
    int gap = ScaleUIPx(4);
    int col_gap = ScaleUIPx(8);
    int total_w = dot * 2 + col_gap;
    int total_h = dot * 3 + gap * 2;
    int start_x = x + (w - total_w) / 2;
    int start_y = y + (h - total_h) / 2;
    Color color = active ? LightenUIColor(c_icon, 20) : DarkenUIColor(c_icon, 18);

    if(w <= 0 || h <= 0)
        return;
    MarkUIClickable();
    for(int row = 0; row < 3; row++) {
        for(int col = 0; col < 2; col++) {
            DrawRectangle(start_x + col * (dot + col_gap),
                          start_y + row * (dot + gap),
                          dot, dot, color);
        }
    }
}

void
DrawUIReorderPlaceholder(Rectangle bounds)
{
    int x = (int)bounds.x;
    int y = (int)bounds.y;
    int w = (int)bounds.width;
    int h = (int)bounds.height;
    int line_h = ScaleUIPx(2);
    Color color = LightenUIColor(c_button_hover, 10);

    if(w <= 0 || h <= 0)
        return;
    if(line_h < 1)
        line_h = 1;
    DrawRectangle(x, y + h / 2 - line_h / 2, w, line_h, color);
}
