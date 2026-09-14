#include "ui_internal.h"
#include "ui_style_internal.h"
#include "runtime/reorder.h"

static ReorderState g_ui_reorder_state = {0};

static int
ui_reorder_find_index(const ReorderList *list, int item_id)
{
    if(list == NULL || list->items == NULL)
        return -1;
    for(int i = 0; i < list->item_count; i++) {
        if(ReorderItemMatches(list->items[i].id, item_id))
            return i;
    }
    return -1;
}

static int
ui_reorder_target_index(const ReorderList *list, int active_index,
                        int pointer_y)
{
    int target = 0;

    if(list == NULL || list->items == NULL || list->item_count <= 0)
        return -1;

    for(int i = 0; i < list->item_count; i++) {
        target = ReorderTargetStep(target, i, active_index, pointer_y,
                                   list->items[i].bounds);
    }

    return ReorderTargetIndexFor(target, list->item_count);
}

static StyleFrame
ui_reorder_frame(int role, ButtonState state, int active)
{
    return ui_control_style_frame_role_kind(
        (ButtonProps){.tone = active ? ButtonToneAccent : ButtonToneNeutral,
                      .emphasis = ButtonEmphasisSoft,
                      .selected = active},
        state, 0, 0.0f, 0.0f, 0.0f, StyleKindReorder(), role);
}

static ReorderMetrics
ui_reorder_metrics_for_list(ReorderList list)
{
    return ReorderMetricsFor(GetScale(), list.handle_width,
                             list.drag_threshold,
                             list.auto_scroll_margin,
                             list.auto_scroll_step,
                             ui_reorder_frame(12, ButtonStateNormal, 0),
                             ui_reorder_frame(25, ButtonStateFocus, 1));
}

static void
ui_reorder_cancel(void)
{
    memset(&g_ui_reorder_state, 0, sizeof(g_ui_reorder_state));
    if(g_ui_pointer_owner == POINTER_OWNER_REORDER)
        g_ui_pointer_owner = POINTER_OWNER_NONE;
}

ReorderListResult
UpdateReorderList(ReorderList list)
{
    Vector2 mouse = ui_mouse_world();
    int pointer_y = (int)mouse.y;
    ReorderListResult result = ReorderListResultDefault(pointer_y);
    int captured = ui_input_captures_click_internal(mouse, 0);
    ReorderMetrics metrics = ui_reorder_metrics_for_list(list);

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
        int dragged_center_y;

        if(active_index < 0 || active_index >= list.item_count ||
           list.items == NULL || list.items[active_index].disabled) {
            ui_reorder_cancel();
            return result;
        }

        dragged_center_y = ReorderDraggedCenterY(
            pointer_y, g_ui_reorder_state.press_offset_y,
            list.items[active_index].bounds.height);
        result = ReorderListActiveResultFor(
            g_ui_reorder_state.from_index, active_index,
            ui_reorder_target_index(&list, active_index, dragged_center_y),
            g_ui_reorder_state.item_id, pointer_y, g_ui_reorder_state.press_y);

        if(IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            ReorderDragMotion motion = ReorderDragMotionFor(
                pointer_y, g_ui_reorder_state.press_y,
                g_ui_reorder_state.dragging, list.bounds, list.viewport_top,
                list.viewport_bottom,
                list.scroll_offset != NULL ? *list.scroll_offset : 0,
                list.max_scroll, metrics);
            if(!g_ui_reorder_state.dragging && motion.dragging) {
                g_ui_reorder_state.dragging = 1;
                g_ui_pointer_owner = POINTER_OWNER_REORDER;
            }
            if(g_ui_reorder_state.dragging) {
                result.dragging = 1;
                PushInputCapture((Rectangle){0, 0, (float)ui_view_width,
                                               (float)ui_view_height}, 0);
                if(list.scroll_offset != NULL)
                    *list.scroll_offset = motion.scroll_offset;
            }
            return result;
        }

        if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
            PushInputCapture((Rectangle){0, 0, (float)ui_view_width,
                                           (float)ui_view_height}, 0);
            if(g_ui_reorder_state.dragging) {
                result = ReorderListCommitResultFor(result, list.item_count);
            }
        }
        ui_reorder_cancel();
        return result;
    }

    if(list.id == 0 || list.items == NULL || list.item_count <= 0 ||
       !IsMouseButtonPressed(MOUSE_BUTTON_LEFT) || captured ||
       g_ui_pointer_owner != POINTER_OWNER_NONE ||
       !CheckCollisionPointRec(mouse, list.bounds))
        return result;

    for(int i = 0; i < list.item_count; i++) {
        const ReorderItem *item = &list.items[i];
        Rectangle handle;

        if(item->disabled)
            continue;
        handle = ReorderHandleBounds(item->bounds, metrics.handle_width,
                                     list.handle_height);
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
            result = ReorderListPressResultFor(i, item->id, pointer_y);
            PushInputCapture((Rectangle){0, 0, (float)ui_view_width,
                                           (float)ui_view_height}, 0);
            return result;
        }
    }

    return result;
}

void
RenderReorderHandle(int x, int y, int w, int h, int active)
{
    ReorderHandlePaint paint;
    StyleFrame frame = ui_reorder_frame(12,
        active ? ButtonStateSelected : ButtonStateNormal, active);
    Style style = ui_unpack_style(frame.value);
    Color color = style.foreground;

    if(w <= 0 || h <= 0)
        return;
    MarkClickable();
    paint = ReorderHandlePaintFor((Rectangle){(float)x, (float)y,
                                              (float)w, (float)h},
                                  (float)GetScale(), frame);
    if(paint.dot_count <= 0)
        return;
    DrawRectangleRec(paint.dot0, color);
    DrawRectangleRec(paint.dot1, color);
    DrawRectangleRec(paint.dot2, color);
    DrawRectangleRec(paint.dot3, color);
    DrawRectangleRec(paint.dot4, color);
    DrawRectangleRec(paint.dot5, color);
}

void
RenderReorderPlaceholder(Rectangle bounds)
{
    ReorderPlaceholderPaint paint;
    StyleFrame frame = ui_reorder_frame(25, ButtonStateFocus, 1);
    Style style = ui_unpack_style(frame.value);
    Color color = style.border.a != 0 ? style.border : style.foreground;

    if(bounds.width <= 0 || bounds.height <= 0)
        return;
    paint = ReorderPlaceholderPaintFor(bounds, (float)GetScale(), frame);
    if(paint.use_slot) {
        if(paint.slot_bounds.width <= 0 || paint.slot_bounds.height <= 0)
            return;
        DrawRectangleRounded(paint.slot_bounds, paint.radius, paint.segments,
                             Fade(color, paint.fill_alpha));
        if(paint.stroke_width > 0.0f) {
            DrawRectangleRoundedLinesEx(paint.slot_bounds, paint.radius,
                                        paint.segments, paint.stroke_width,
                                        color);
        }
        return;
    }
    if(paint.line_bounds.width <= 0 || paint.line_bounds.height <= 0)
        return;
    DrawRectangleRec(paint.line_bounds, color);
}
