#include "ui_internal.h"

static const char *
ui_pager_label(const char *label)
{
    return label != NULL ? label : "";
}

static int
ui_pager_button(Rectangle bounds, const char *label, ButtonStyle style,
                int focus_id)
{
    int hover = 0;
    int clicked;
    int focused = focus_id > 0 && RegisterUIFocus(focus_id, bounds);

    clicked = RenderStyledButton((int)bounds.x, (int)bounds.y,
                                 (int)bounds.width, (int)bounds.height,
                                 ui_pager_label(label), style, 0, &hover);
    if(focused) {
        if(IsWindowReady())
            DrawUIFocus(bounds);
        if(IsUIFocusActivatePressed(focus_id)) {
            UIConsumeRelease();
            clicked = 1;
        }
    }
    return clicked;
}

UIGuidePagerResult
DrawUIGuidePager(UIGuidePagerProps pager)
{
    UIGuidePagerResult result = {0};
    int page_count = pager.page_count > 0 ? pager.page_count : 1;
    int page = ui_clampi(pager.page, 0, page_count - 1);
    int pad = ScaleUIPx(12);
    int gap = ScaleUIPx(12);
    int button_h = ScaleUIPx(48);
    int footer_y = (int)pager.footer_bounds.y;
    int footer_h = (int)pager.footer_bounds.height;
    int footer_x = (int)pager.footer_bounds.x;
    int footer_w = (int)pager.footer_bounds.width;
    int button_y;
    int button_w;
    Rectangle left;
    Rectangle right;
    int previous = 0;
    int next = 0;

    result.page = page;
    if(pager.swipe != NULL && page_count > 1) {
        result.swipe = UpdateUISwipe(pager.swipe, (UISwipeSpec){
            .bounds = pager.content_bounds,
            .directions = UI_SWIPE_HORIZONTAL,
            .min_distance = (float)ScaleUIPx(48),
            .axis_bias = 1.25f,
            .max_duration = 0.8f
        });
        previous = result.swipe.direction == UI_SWIPE_RIGHT;
        next = result.swipe.direction == UI_SWIPE_LEFT;
    }

    if(IsKeyPressed(KEY_LEFT))
        previous = 1;
    if(IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_ENTER))
        next = 1;
    if(IsKeyPressed(KEY_BACK) || IsKeyPressed(KEY_ESCAPE)) {
        result.closed = 1;
        return result;
    }

    if(footer_w <= 0 || footer_h <= 0)
        return result;
    if(button_h > footer_h - pad * 2)
        button_h = footer_h - pad * 2;
    if(button_h < 1)
        return result;
    button_y = footer_y + (footer_h - button_h) / 2;
    button_w = (footer_w - pad * 2 - gap) / 2;
    if(button_w < 1)
        return result;

    if(IsWindowReady()) {
        DrawRectangleRec(pager.footer_bounds, GetThemeBackground());
        DrawLine(footer_x, footer_y, footer_x + footer_w, footer_y,
                 Fade(GetThemeText(), 0.16f));
    }
    left = (Rectangle){(float)(footer_x + pad), (float)button_y,
                       (float)button_w, (float)button_h};
    right = (Rectangle){(float)(footer_x + pad + button_w + gap),
                        (float)button_y, (float)button_w, (float)button_h};

    if(ui_pager_button(left,
                       page == 0 ? pager.close_label : pager.back_label,
                       ButtonStyleOutline, pager.focus_id)) {
        if(page == 0)
            result.closed = 1;
        else
            previous = 1;
    }
    if(ui_pager_button(right,
                       page == page_count - 1
                           ? pager.finish_label
                           : pager.next_label,
                       ButtonStylePrimary,
                       pager.focus_id > 0 ? pager.focus_id + 1 : 0)) {
        if(page == page_count - 1)
            result.finished = 1;
        else
            next = 1;
    }

    if(previous && page > 0) {
        result.page = page - 1;
        result.changed = 1;
    } else if(next && page < page_count - 1) {
        result.page = page + 1;
        result.changed = 1;
    } else if(next && page == page_count - 1 &&
              result.swipe.direction == UI_SWIPE_NONE &&
              (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_ENTER))) {
        result.finished = 1;
    }
    return result;
}
