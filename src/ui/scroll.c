#include "ui.h"

int
GetUIScrollbarReservedWidth(int max_scroll)
{
    return max_scroll > 0 ? ScaleUIPx(16) : 0;
}

int
GetUIScrollbarContentWidth(int content_width, int max_scroll)
{
    int reserved = GetUIScrollbarReservedWidth(max_scroll);

    if(reserved <= 0)
        return content_width;
    if(content_width <= reserved)
        return 0;
    return content_width - reserved;
}

int
GetUIScrollbarSafeContentWidth(int content_x, int content_width,
                                int scrollbar_x, int max_scroll)
{
    int gap = ScaleUIPx(20);
    int safe_width = content_width;

    if(max_scroll <= 0 || scrollbar_x <= 0)
        return content_width;

    safe_width = scrollbar_x - content_x - gap;
    if(safe_width > content_width)
        return content_width;
    if(safe_width < 0)
        return 0;
    return safe_width;
}

UIScrollView
MeasureUIScrollContainer(UIScrollArea area)
{
    UIScrollView view;
    int x = (int)area.bounds.x;
    int y = (int)area.bounds.y;
    int w = (int)area.bounds.width;
    int h = (int)area.bounds.height;
    int scrollbar_w = ScaleUIPx(8);
    int scrollbar_x = area.scrollbar_x > 0
                          ? area.scrollbar_x
                          : x + w - scrollbar_w;
    int content_x = area.content_x > 0 ? area.content_x : x;
    int content_w = area.content_width > 0 ? area.content_width : w;

    memset(&view, 0, sizeof(view));
    view.content_x = content_x;
    view.viewport_h = h;
    view.content_h = area.content_height > 0 ? area.content_height : 0;
    view.max_scroll = view.content_h - h;
    if(view.max_scroll < 0)
        view.max_scroll = 0;

    if(area.scroll_offset != NULL) {
        int scroll_offset = ui_clampi(*area.scroll_offset, 0, view.max_scroll);
        view.content_y = y - scroll_offset;
    } else {
        view.content_y = y;
    }
    view.content_w = GetUIScrollbarSafeContentWidth(content_x, content_w,
                                                     scrollbar_x, view.max_scroll);

    return view;
}

UIScrollPage
BeginUIScrollPage(UIScrollPageSpec spec)
{
    UIScrollPage page;
    UIScrollArea area;
    UIScrollView measured;
    int max_content_w = spec.max_content_width;
    int min_content_w = spec.min_content_width;
    int side_padding = spec.side_padding > 0 ? spec.side_padding : GetUIPageSidePadding();
    int content_x = 0;
    int content_w = 0;
    int draw_w;
    int passes = spec.measure_passes > 0 ? spec.measure_passes : 3;

    memset(&page, 0, sizeof(page));

    if(max_content_w <= 0)
        max_content_w = ui_view_width;
    if(max_content_w > ui_view_width - side_padding * 2)
        max_content_w = ui_view_width - side_padding * 2;
    if(min_content_w > 0 && max_content_w < min_content_w)
        max_content_w = min_content_w;
    if(max_content_w < 0)
        max_content_w = 0;

    GetUICenteredColumn(max_content_w, side_padding, &content_x, &content_w);
    draw_w = content_w;

    for(int i = 0; i < passes; i++) {
        int content_h;

        if(spec.content_height != NULL)
            content_h = spec.content_height(draw_w, spec.user_data);
        else
            content_h = 0;
        area = (UIScrollArea){
            .bounds = {0.0f, (float)spec.y, (float)ui_view_width, (float)spec.height},
            .content_height = content_h,
            .content_x = content_x,
            .content_width = content_w,
            .scroll_offset = spec.scroll_offset,
            .wheel_step = spec.wheel_step > 0 ? spec.wheel_step : ScaleUIPx(42),
            .scrollbar_x = spec.scrollbar_x > 0 ? spec.scrollbar_x : ui_view_width - ScaleUIPx(8)
        };
        measured = MeasureUIScrollContainer(area);
        if(measured.content_w == draw_w)
            break;
        draw_w = measured.content_w;
    }

    if(spec.content_height != NULL)
        area.content_height = spec.content_height(draw_w, spec.user_data);
    page.area = area;
    page.view = BeginUIScrollContainer(area);
    page.content_x = page.view.content_x;
    page.content_y = page.view.content_y;
    page.content_w = page.view.content_w;
    page.content_h = area.content_height;
    return page;
}

void
EndUIScrollPage(UIScrollPage page)
{
    EndUIScrollContainer(page.area, page.view);
}
UIScrollView
BeginUIScrollContainer(UIScrollArea area)
{
    static int content_drag_active = 0;
    static int content_dragging = 0;
    static int content_drag_start_y = 0;
    static int content_drag_start_scroll = 0;
    UIScrollView view = MeasureUIScrollContainer(area);
    Vector2 mouse_world = ui_mouse_world();
    int y = (int)area.bounds.y;
    int wheel_step = area.wheel_step > 0 ? area.wheel_step : ScaleUIPx(42);
    int inside = CheckCollisionPointRec(mouse_world, area.bounds);
    int captured = UIInputCapturesClick(mouse_world);
    int drag_threshold = ScaleUIPx(5);
    int scrollbar_w = ScaleUIPx(8);
    int scrollbar_x = area.scrollbar_x > 0
                          ? area.scrollbar_x
                          : (int)(area.bounds.x + area.bounds.width) - scrollbar_w;
    Rectangle scrollbar_bounds = {
        (float)scrollbar_x,
        area.bounds.y,
        (float)scrollbar_w,
        area.bounds.height
    };
    int on_scrollbar = view.max_scroll > 0 &&
                       CheckCollisionPointRec(mouse_world, scrollbar_bounds);

    if(area.scroll_offset != NULL) {
        *area.scroll_offset = ui_clampi(*area.scroll_offset, 0, view.max_scroll);
        if(view.max_scroll > 0 && inside && !captured) {
            float wheel = GetMouseWheelMove();
            if(wheel != 0.0f) {
                *area.scroll_offset -= (int)(wheel * (float)wheel_step);
                *area.scroll_offset = ui_clampi(*area.scroll_offset, 0, view.max_scroll);
            }
        }

        if(g_ui_slider_active_id != 0 &&
           g_ui_pointer_owner == UI_POINTER_OWNER_NONE &&
           g_ui_pointer_dragging &&
           ui_pointer_drag_is_horizontal())
            g_ui_pointer_owner = UI_POINTER_OWNER_HORIZONTAL_SLIDER;

        if(g_ui_pointer_owner == UI_POINTER_OWNER_HORIZONTAL_SLIDER ||
           g_ui_pointer_owner == UI_POINTER_OWNER_VERTICAL_SLIDER) {
            content_drag_active = 0;
            content_dragging = 0;
        }

        if(view.max_scroll > 0 &&
           g_ui_pointer_owner == UI_POINTER_OWNER_NONE &&
           IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && inside && !captured &&
           !on_scrollbar) {
            content_drag_active = 1;
            content_dragging = 0;
            content_drag_start_y = (int)mouse_world.y;
            content_drag_start_scroll = *area.scroll_offset;
        }
        if(content_drag_active && IsMouseButtonDown(MOUSE_BUTTON_LEFT) &&
           g_ui_pointer_owner != UI_POINTER_OWNER_HORIZONTAL_SLIDER &&
           g_ui_pointer_owner != UI_POINTER_OWNER_VERTICAL_SLIDER) {
            int dy = (int)mouse_world.y - content_drag_start_y;
            if(content_dragging || dy > drag_threshold || dy < -drag_threshold) {
                g_ui_pointer_owner = UI_POINTER_OWNER_SCROLL;
                content_dragging = 1;
                *area.scroll_offset = content_drag_start_scroll - dy;
                *area.scroll_offset = ui_clampi(*area.scroll_offset, 0, view.max_scroll);
                PushUIInputCapture((Rectangle){0, 0, (float)ui_view_width,
                                                  (float)ui_view_height}, 0);
            }
        } else if(content_drag_active) {
            if(content_dragging)
                PushUIInputCapture((Rectangle){0, 0, (float)ui_view_width,
                                                  (float)ui_view_height}, 0);
            content_drag_active = 0;
            content_dragging = 0;
        }
        view.content_y = y - *area.scroll_offset;
    } else {
        view.content_y = y;
    }
    {
        Rectangle screen_bounds = {
            g_ui_camera.offset.x + area.bounds.x * g_ui_camera.zoom,
            g_ui_camera.offset.y + area.bounds.y * g_ui_camera.zoom,
            area.bounds.width * g_ui_camera.zoom,
            area.bounds.height * g_ui_camera.zoom
        };
        Rectangle clipped_screen_bounds = GetUIClipEffective(screen_bounds);
        Rectangle clipped_world_bounds = {
            (clipped_screen_bounds.x - g_ui_camera.offset.x) / g_ui_camera.zoom,
            (clipped_screen_bounds.y - g_ui_camera.offset.y) / g_ui_camera.zoom,
            clipped_screen_bounds.width / g_ui_camera.zoom,
            clipped_screen_bounds.height / g_ui_camera.zoom
        };

        PushUIInputClip(clipped_world_bounds);
        BeginUIClip((int)screen_bounds.x, (int)screen_bounds.y,
                         (int)screen_bounds.width, (int)screen_bounds.height);
    }
    return view;
}

void
EndUIScrollContainer(UIScrollArea area, UIScrollView view)
{
    int scrollbar_w = ScaleUIPx(8);
    int scrollbar_x;

    EndUIClip();
    PopUIInputClip();

    if(area.scroll_offset == NULL || view.max_scroll <= 0)
        return;

    scrollbar_x = area.scrollbar_x > 0
                      ? area.scrollbar_x
                      : (int)(area.bounds.x + area.bounds.width) - scrollbar_w;
    DrawUIScrollbar(scrollbar_x,
                      (int)area.bounds.y,
                      (int)area.bounds.height,
                      view.content_h,
                      area.scroll_offset,
                      view.max_scroll);
}

/* ================================================================
 * SCROLLBAR
 * ================================================================ */

static int
ui_scrollbar_abs_diff(int a, int b)
{
    return a > b ? a - b : b - a;
}

static int
ui_scrollbar_luminance(Color color)
{
    return ((int)color.r * 77 + (int)color.g * 150 + (int)color.b * 29) / 256;
}

static int
ui_scrollbar_color_delta(Color a, Color b)
{
    return ui_scrollbar_abs_diff(a.r, b.r) +
           ui_scrollbar_abs_diff(a.g, b.g) +
           ui_scrollbar_abs_diff(a.b, b.b);
}

static Color
ui_scrollbar_contrast_from(Color color, int amount)
{
    return ui_scrollbar_luminance(color) < 128
               ? LightenUIColor(color, amount)
               : DarkenUIColor(color, amount);
}

static Color
ui_scrollbar_ensure_visible(Color color, Color against,
                            int amount, int min_delta, int min_luminance_delta)
{
    int delta = ui_scrollbar_color_delta(color, against);
    int luminance_delta = ui_scrollbar_abs_diff(ui_scrollbar_luminance(color),
                                                ui_scrollbar_luminance(against));

    if(delta >= min_delta && luminance_delta >= min_luminance_delta)
        return color;

    return ui_scrollbar_contrast_from(against, amount);
}

int
DrawUIScrollbar(int x, int y, int viewport_h, int content_h, int *scroll_offset, int max_scroll)
{
    /* Don't show scrollbar if no scrolling needed */
    if(max_scroll <= 0)
        return 0;
    if(viewport_h <= 0 || content_h <= 0 || scroll_offset == NULL)
        return 0;

    static int scrollbar_drag_active = 0;
    static int scrollbar_drag_start_y = 0;
    static int scrollbar_drag_start_scroll = 0;

    int scrollbar_width = ScaleUIPx(8);
    int scrollbar_min_thumb = ScaleUIPx(24);
    int track_padding = 0;

    /* Calculate thumb size and position */
    float content_ratio = (float)viewport_h / (float)content_h;
    int thumb_height = (int)(viewport_h * content_ratio);
    if(thumb_height < scrollbar_min_thumb)
        thumb_height = scrollbar_min_thumb;
    if(thumb_height > viewport_h)
        thumb_height = viewport_h;

    float scroll_ratio = max_scroll > 0 ? (float)*scroll_offset / (float)max_scroll : 0.0f;
    int track_span = viewport_h - thumb_height;
    int thumb_y = y + (int)(scroll_ratio * track_span);

    Vector2 mouse_pos = ui_mouse_world();
    int my = (int)mouse_pos.y;
    Rectangle thumb_bounds = {x + track_padding, thumb_y, scrollbar_width - track_padding * 2, thumb_height};
    int input_captured = ui_input_captures_click_internal(mouse_pos, 0);
    int thumb_active = CheckCollisionPointRec(mouse_pos, thumb_bounds) && !input_captured;
    int thumb_hover = thumb_active && UIHoverEffectsEnabled();

    /* Handle drag state */
    if(IsMouseButtonDown(MOUSE_BUTTON_LEFT) &&
       (!input_captured || scrollbar_drag_active)) {
        if(!scrollbar_drag_active) {
            /* Start drag if clicking on thumb */
            if(thumb_active && g_ui_pointer_owner == UI_POINTER_OWNER_NONE) {
                scrollbar_drag_active = 1;
                g_ui_pointer_owner = UI_POINTER_OWNER_SCROLL;
                scrollbar_drag_start_y = my;
                scrollbar_drag_start_scroll = *scroll_offset;
            }
        } else {
            /* Continue drag */
            int dy = my - scrollbar_drag_start_y;
            float scroll_per_pixel = track_span > 0
                                         ? (float)max_scroll / (float)track_span
                                         : 0.0f;
            int new_scroll = scrollbar_drag_start_scroll + (int)(dy * scroll_per_pixel);
            *scroll_offset = new_scroll;
            if(*scroll_offset < 0) *scroll_offset = 0;
            if(*scroll_offset > max_scroll) *scroll_offset = max_scroll;
        }
    } else {
        if(scrollbar_drag_active && g_ui_pointer_owner == UI_POINTER_OWNER_SCROLL)
            g_ui_pointer_owner = UI_POINTER_OWNER_NONE;
        scrollbar_drag_active = 0;
    }

    Color track_color = ui_scrollbar_ensure_visible(
        ui_scrollbar_contrast_from(c_bg, 22), c_bg, 34, 36, 16);
    DrawRectangle(x, y, scrollbar_width, viewport_h, track_color);

    Color thumb_color = thumb_hover || scrollbar_drag_active ? c_button_hover : c_button;
    thumb_color = ui_scrollbar_ensure_visible(thumb_color, track_color, 72, 78, 28);
    thumb_color = ui_scrollbar_ensure_visible(thumb_color, c_bg, 64, 62, 22);
    DrawRectangleRec(thumb_bounds, thumb_color);
    DrawRectangleLinesEx(thumb_bounds, (float)ScaleUIPx(1),
                         ui_scrollbar_contrast_from(thumb_color, 34));

    return 1;
}

/* ================================================================
 * END OF UI FUNCTIONS
 * ================================================================ */
