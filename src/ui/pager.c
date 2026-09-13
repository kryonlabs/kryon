#include "ui_internal.h"
#include "ui_style_internal.h"
#include "runtime/guide_pager.h"
#include "ui_pager_internal.h"

static const char *
ui_pager_label(const char *label)
{
    return label != NULL ? label : "";
}

static int
ui_pager_button(Rectangle bounds, const char *label, ButtonEmphasis emphasis,
                int focus_id)
{
    return Button((ButtonProps){
        .bounds = bounds,
        .label = ui_pager_label(label),
        .id = focus_id,
        .emphasis = emphasis
    });
}

static Style
ui_pager_style_from_frame(StyleFrame frame)
{
    return ui_unpack_style(frame.value);
}

static StyleFrame
ui_pager_guide_frame(int role)
{
    return ui_control_style_frame_role_kind(
        (ButtonProps){0}, ButtonStateNormal, 0, 0.0f, 0.0f, 0.0f,
        StyleKindGuide(), role);
}

GuidePagerResult
GuidePager(GuidePagerProps pager)
{
    GuidePagerResult result = {0};
    StyleFrame bar_frame = ui_pager_guide_frame(1);
    GuidePagerMetrics metrics = GuidePagerMetricsFor((float)GetScale(),
                                                     bar_frame);
    int page_count = GuidePagerPageCount(pager.page_count);
    int page = GuidePagerPageFor(pager.page, page_count);
    GuidePagerLayout layout;
    GuidePagerPolicy policy;
    int footer_y = (int)pager.footer_bounds.y;
    int footer_x = (int)pager.footer_bounds.x;
    int footer_w = (int)pager.footer_bounds.width;
    int previous = 0;
    int next = 0;
    int close_requested = 0;
    int finish_requested = 0;
    int keyboard_finish = 0;

    result.page = page;
    if(pager.swipe != NULL && page_count > 1) {
        result.swipe = UpdateSwipe(pager.swipe, (SwipeSpec){
            .bounds = pager.content_bounds,
            .directions = SwipeHorizontal,
            .min_distance = metrics.swipe_min_distance,
            .axis_bias = metrics.swipe_axis_bias,
            .max_duration = metrics.swipe_max_duration
        });
        previous = result.swipe.direction == SwipeRight;
        next = result.swipe.direction == SwipeLeft;
    }

    if(IsKeyPressed(KEY_LEFT))
        previous = 1;
    if(IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_ENTER)) {
        next = 1;
        keyboard_finish = result.swipe.direction == SwipeNone;
    }
    if(IsKeyPressed(KEY_BACK) || IsKeyPressed(KEY_ESCAPE)) {
        close_requested = 1;
    }

    layout = GuidePagerLayoutFor(pager.footer_bounds, metrics);
    if(!layout.valid)
        goto finish_policy;

    if(IsWindowReady()) {
        Style bar = ui_pager_style_from_frame(bar_frame);
        Style divider = ui_pager_style_from_frame(ui_pager_guide_frame(18));
        ui_draw_material(pager.footer_bounds, (Rectangle){0},
                         bar.background, bar.border, bar.border, bar.radius,
                         bar.border_width, 0.0f, 0.0f, 0, bar.focus, 0.0f,
                         bar.opacity, ui_style_fill(bar), bar.material);
        DrawLine(footer_x, footer_y, footer_x + footer_w, footer_y,
                 divider.border);
    }

    if(ui_pager_button(layout.left_button,
                       page == 0 ? pager.close_label : pager.back_label,
                       ButtonEmphasisOutline, pager.focus_id)) {
        if(page == 0)
            close_requested = 1;
        else
            previous = 1;
    }
    if(ui_pager_button(layout.right_button,
                       page == page_count - 1
                           ? pager.finish_label
                           : pager.next_label,
                       ButtonEmphasisFilled,
                       pager.focus_id > 0 ? pager.focus_id + 1 : 0)) {
        if(page == page_count - 1)
            finish_requested = 1;
        else
            next = 1;
    }

finish_policy:
    policy = GuidePagerPolicyFor(page, page_count, previous != 0, next != 0,
                                 close_requested != 0, finish_requested != 0,
                                 keyboard_finish != 0);
    result.page = policy.page;
    result.changed = policy.changed;
    result.closed = policy.closed;
    result.finished = policy.finished;
    return result;
}
