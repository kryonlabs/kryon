#include "ui_internal.h"
#include "ui_style_internal.h"
#include "runtime/scroll.h"

static ScrollMetrics
ui_scroll_metrics(void)
{
    StyleFrame track = ui_control_style_frame_kind(
        (ButtonProps){.size = ControlSizeSmall, .pill = 1},
        ButtonStateNormal, 0, 0.0f, 0.0f, 0.0f, StyleKindScroll());
    StyleFrame thumb = ui_control_style_frame_kind(
        (ButtonProps){.tone = ButtonToneAccent, .emphasis = ButtonEmphasisFilled,
                      .size = ControlSizeSmall, .pill = 1},
        ButtonStateNormal, 0, 0.0f, 0.0f, 0.0f, StyleKindScrollThumb());
    return ScrollMetricsFor((float)GetScale(), track, thumb);
}

int
GetScrollbarReservedWidth(int max_scroll)
{
    return ScrollReservedWidth(max_scroll, ui_scroll_metrics());
}

int
GetScrollbarContentWidth(int content_width, int max_scroll)
{
    return ScrollContentWidth(content_width, max_scroll,
                              ui_scroll_metrics());
}

int
GetScrollbarSafeContentWidth(int content_x, int content_width,
                                int scrollbar_x, int max_scroll)
{
    return ScrollSafeContentWidth(content_x, content_width, scrollbar_x,
                                  max_scroll,
                                  ui_scroll_metrics());
}

ScrollView
MeasureScrollContainer(ScrollArea area)
{
    ScrollView view;
    ScrollPolicyView policy = ScrollMeasure(area.bounds, area.content_height,
        area.content_x, area.content_width,
        area.scroll_offset != NULL ? *area.scroll_offset : 0,
        area.scrollbar_x, ui_scroll_metrics());

    memset(&view, 0, sizeof(view));
    view.content_x = policy.content_x;
    view.viewport_h = policy.viewport_h;
    view.content_h = policy.content_h;
    view.max_scroll = policy.max_scroll;
    view.content_y = policy.content_y;
    view.content_w = policy.content_w;

    return view;
}

ScrollPage
BeginScrollPage(ScrollPageSpec spec)
{
    ScrollPage page;
    ScrollArea area;
    ScrollView measured;
    int max_content_w = spec.max_content_width;
    int min_content_w = spec.min_content_width;
    int side_padding = spec.side_padding > 0 ? spec.side_padding : GetPageSidePadding();
    int content_x = 0;
    int content_w = 0;
    int draw_w;
    int passes = spec.measure_passes > 0 ? spec.measure_passes : 3;
    int i;

    memset(&page, 0, sizeof(page));

    max_content_w = ScrollPageContentWidthFor(ui_view_width, max_content_w,
                                              min_content_w, side_padding);

    GetCenteredColumn(max_content_w, side_padding, &content_x, &content_w);
    draw_w = content_w;

    for(i = 0; i < passes; i++) {
        int content_h;

        if(spec.content_height != NULL)
            content_h = spec.content_height(draw_w, spec.user_data);
        else
            content_h = 0;
        memset(&area, 0, sizeof(area));
        area.bounds.x = 0.0f;
        area.bounds.y = (float)spec.y;
        area.bounds.width = (float)ui_view_width;
        area.bounds.height = (float)spec.height;
        area.content_height = content_h;
        area.content_x = content_x;
        area.content_width = content_w;
        area.scroll_offset = spec.scroll_offset;
        ScrollMetrics metrics = ui_scroll_metrics();
        area.wheel_step = spec.wheel_step > 0 ? spec.wheel_step :
            metrics.default_wheel_step;
        area.scrollbar_x = spec.scrollbar_x > 0 ? spec.scrollbar_x :
            ui_view_width - metrics.scrollbar_width;
        measured = MeasureScrollContainer(area);
        if(measured.content_w == draw_w)
            break;
        draw_w = measured.content_w;
    }

    if(spec.content_height != NULL)
        area.content_height = spec.content_height(draw_w, spec.user_data);
    page.area = area;
    page.view = BeginScrollContainer(area);
    page.content_x = page.view.content_x;
    page.content_y = page.view.content_y;
    page.content_w = page.view.content_w;
    page.content_h = area.content_height;
    return page;
}

void
EndScrollPage(ScrollPage page)
{
    EndScrollContainer(page.area, page.view);
}

ScreenScaffold
BeginScreenScaffold(ScreenScaffoldSpec spec)
{
    ScreenScaffold scaffold;
    ScrollPageSpec page_spec;
    int title_h = spec.title_height;
    int top_gap = spec.top_gap > 0 ? spec.top_gap : 0;
    int content_y;
    int content_h;

    memset(&scaffold, 0, sizeof(scaffold));

    if(title_h <= 0)
        title_h = GetNodeHeight(NodeTitleBar(0));
    scaffold.title_height = title_h;

    if(spec.draw_title != NULL)
        scaffold.closed = spec.draw_title(spec.title, title_h,
                                          spec.title_user_data != NULL
                                              ? spec.title_user_data
                                              : spec.user_data);
    else
        TitleBar((TitleBarProps){.title = spec.title, .height = title_h});

    content_y = title_h + top_gap;
    content_h = ui_view_height - content_y - spec.bottom_reserved;
    if(content_h < 0)
        content_h = 0;

    scaffold.content_y = content_y;
    scaffold.content_h = content_h;
    memset(&page_spec, 0, sizeof(page_spec));
    page_spec.y = content_y;
    page_spec.height = content_h;
    page_spec.max_content_width = spec.max_content_width;
    page_spec.min_content_width = spec.min_content_width;
    page_spec.side_padding = spec.side_padding;
    page_spec.scroll_offset = spec.scroll_offset;
    page_spec.wheel_step = spec.wheel_step;
    page_spec.scrollbar_x = spec.scrollbar_x;
    page_spec.measure_passes = spec.measure_passes;
    page_spec.content_height = spec.content_height;
    page_spec.user_data = spec.user_data;
    scaffold.page = BeginScrollPage(page_spec);
    scaffold.content_x = scaffold.page.content_x;
    scaffold.content_w = scaffold.page.content_w;
    scaffold.y = scaffold.page.content_y;
    return scaffold;
}

void
EndScreenScaffold(ScreenScaffold scaffold)
{
    EndScrollPage(scaffold.page);
}
ScrollView
BeginScrollContainer(ScrollArea area)
{
    static int content_drag_active = 0;
    static int content_dragging = 0;
    static int content_drag_start_y = 0;
    static int content_drag_start_scroll = 0;
    ScrollView view = MeasureScrollContainer(area);
    Vector2 mouse_world = ui_mouse_world();
    int y = (int)area.bounds.y;
    ScrollMetrics metrics = ui_scroll_metrics();
    int wheel_step = area.wheel_step > 0 ? area.wheel_step :
        metrics.default_wheel_step;
    int inside = CheckCollisionPointRec(mouse_world, area.bounds);
    int captured = InputCapturesClick(mouse_world);
    int drag_threshold = metrics.drag_threshold;
    int scrollbar_w = metrics.scrollbar_width;
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
    Rectangle capture;

    capture.x = 0;
    capture.y = 0;
    capture.width = (float)ui_view_width;
    capture.height = (float)ui_view_height;

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
           g_ui_pointer_owner == POINTER_OWNER_NONE &&
           g_ui_pointer_dragging &&
           ui_pointer_drag_is_horizontal())
            g_ui_pointer_owner = POINTER_OWNER_HORIZONTAL_SLIDER;

        if(g_ui_pointer_owner == POINTER_OWNER_HORIZONTAL_SLIDER ||
           g_ui_pointer_owner == POINTER_OWNER_VERTICAL_SLIDER) {
            content_drag_active = 0;
            content_dragging = 0;
        }

        if(view.max_scroll > 0 &&
           g_ui_pointer_owner == POINTER_OWNER_NONE &&
           IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && inside && !captured &&
           !on_scrollbar) {
            g_ui_scroll_gesture_pending = 1;
            content_drag_active = 1;
            content_dragging = 0;
            content_drag_start_y = (int)mouse_world.y;
            content_drag_start_scroll = *area.scroll_offset;
        }
        if(content_drag_active && IsMouseButtonDown(MOUSE_BUTTON_LEFT) &&
           (g_ui_pointer_owner == POINTER_OWNER_NONE ||
            g_ui_pointer_owner == POINTER_OWNER_SCROLL)) {
            int dy = (int)mouse_world.y - content_drag_start_y;
            if(content_dragging || dy > drag_threshold || dy < -drag_threshold) {
                g_ui_pointer_owner = POINTER_OWNER_SCROLL;
                content_dragging = 1;
                *area.scroll_offset = ScrollDragDeltaOffsetFor(
                    content_drag_start_scroll, dy, view.max_scroll);
                PushInputCapture(capture, 0);
            }
        } else if(content_drag_active) {
            if(content_dragging)
                PushInputCapture(capture, 0);
            content_drag_active = 0;
            content_dragging = 0;
        }
        view.content_y = y - *area.scroll_offset;
    } else {
        view.content_y = y;
    }
    {
        Rectangle screen_bounds = ScrollScreenBoundsFor(
            area.bounds, g_ui_camera.offset, g_ui_camera.zoom);
        ScrollClipGeometry geometry = ScrollClipGeometryFor(
            area.bounds, GetClipEffective(screen_bounds), metrics.visual_bleed,
            g_ui_camera.offset, g_ui_camera.zoom, GetScreenWidth(),
            GetScreenHeight(), ui_view_width, ui_view_height);
        Rectangle clipped_world_bounds = geometry.clipped_world_bounds;
        Rectangle visual_screen_bounds = geometry.visual_screen_bounds;

        PushInputClip(clipped_world_bounds);
        BeginClip((int)visual_screen_bounds.x, (int)visual_screen_bounds.y,
                         (int)visual_screen_bounds.width,
                         (int)visual_screen_bounds.height);
    }
    return view;
}

void
EndScrollContainer(ScrollArea area, ScrollView view)
{
    ScrollMetrics metrics = ui_scroll_metrics();
    int scrollbar_w = metrics.scrollbar_width;
    int scrollbar_x;

    EndClip();
    PopInputClip();

    if(area.scroll_offset == NULL || view.max_scroll <= 0)
        return;

    scrollbar_x = area.scrollbar_x > 0
                      ? area.scrollbar_x
                      : (int)(area.bounds.x + area.bounds.width) - scrollbar_w;
    ui_scrollbar(scrollbar_x,
                      (int)area.bounds.y,
                      (int)area.bounds.height,
                      view.content_h,
                      area.scroll_offset,
                      view.max_scroll, 0);
}

void
EnsureScrollRectVisible(ScrollArea area, Rectangle rect, int margin)
{
    ScrollView view;

    if(area.scroll_offset == NULL)
        return;

    view = MeasureScrollContainer(area);
    if(view.max_scroll <= 0)
        return;

    *area.scroll_offset = ScrollRectVisibleOffsetFor(
        *area.scroll_offset, (int)area.bounds.y, (int)area.bounds.height,
        (int)rect.y, (int)rect.height, margin, view.max_scroll);
}

/* ================================================================
 * SCROLLBAR
 * ================================================================ */

static int scrollbar_drag_active;
static int *scrollbar_drag_offset;
static int scrollbar_drag_start_y;
static int scrollbar_drag_start_scroll;

void
ui_scrollbar_cancel(int *scroll_offset)
{
    if(scrollbar_drag_offset != scroll_offset) return;
    if(scrollbar_drag_active && g_ui_pointer_owner == POINTER_OWNER_SCROLL)
        g_ui_pointer_owner = POINTER_OWNER_NONE;
    scrollbar_drag_active = 0;
    scrollbar_drag_offset = NULL;
}

int
ui_scrollbar(int x, int y, int viewport_h, int content_h, int *scroll_offset, int max_scroll, int overlay)
{
    /* Dont show scrollbar if no scrolling needed */
    if(max_scroll <= 0) {
        ui_scrollbar_cancel(scroll_offset);
        return 0;
    }
    if(viewport_h <= 0 || content_h <= 0 || scroll_offset == NULL)
        return 0;

    ScrollMetrics metrics = ui_scroll_metrics();
    ScrollBarPaint paint = ScrollBarPaintFor(x, y, viewport_h, content_h,
                                             *scroll_offset, max_scroll,
                                             metrics);
    int track_span = paint.track_span;
    Vector2 mouse_pos = ui_mouse_world();
    int my = (int)mouse_pos.y;
    Rectangle thumb_bounds = paint.thumb_bounds;
    int input_captured = overlay ? ui_base_input_captures_click(mouse_pos, 0)
                                 : ui_input_captures_click_internal(mouse_pos, 0);
    int thumb_active = CheckCollisionPointRec(mouse_pos, thumb_bounds) && !input_captured;
    int thumb_hover = thumb_active && HoverEffectsEnabled();

    if(thumb_active)
        MarkClickable();

    /* Handle drag state */
    if(IsMouseButtonDown(MOUSE_BUTTON_LEFT) &&
       (!input_captured ||
        (scrollbar_drag_active && scrollbar_drag_offset == scroll_offset))) {
        if(!scrollbar_drag_active) {
            /* Start drag if clicking on thumb */
            if(thumb_active && g_ui_pointer_owner == POINTER_OWNER_NONE) {
                scrollbar_drag_active = 1;
                scrollbar_drag_offset = scroll_offset;
                g_ui_pointer_owner = POINTER_OWNER_SCROLL;
                scrollbar_drag_start_y = my;
                scrollbar_drag_start_scroll = *scroll_offset;
            }
        } else if(scrollbar_drag_offset == scroll_offset) {
            /* Continue drag */
            int dy = my - scrollbar_drag_start_y;
            *scroll_offset = ScrollThumbDragDeltaOffsetFor(
                scrollbar_drag_start_scroll, dy, max_scroll, paint);
        }
    } else if(scrollbar_drag_offset == scroll_offset) {
        ui_scrollbar_cancel(scroll_offset);
    }

    ButtonState thumb_state = ButtonStateNormal;
    StyleFrame track_frame = ui_control_style_frame_kind(
        (ButtonProps){.size = ControlSizeSmall, .pill = 1},
        ButtonStateNormal, 0, 0.0f, 0.0f, 0.0f, StyleKindScroll());
    StyleFrame thumb_frame;
    Style track_style;
    Style thumb_style;

    if(scrollbar_drag_active && scrollbar_drag_offset == scroll_offset)
        thumb_state = ButtonStatePressed;
    else if(thumb_hover)
        thumb_state = ButtonStateHover;

    thumb_frame = ui_control_style_frame_kind(
        (ButtonProps){.tone = ButtonToneAccent, .emphasis = ButtonEmphasisFilled,
                      .size = ControlSizeSmall, .pill = 1},
        thumb_state, 0, 0.0f, 0.0f, 0.0f, StyleKindScrollThumb());
    track_style = ui_unpack_style(track_frame.value);
    thumb_style = ui_unpack_style(thumb_frame.value);

    if(IsWindowReady()) {
        paint = ScrollBarPaintFor(x, y, viewport_h, content_h,
                                  *scroll_offset, max_scroll, metrics);
        thumb_bounds = paint.thumb_bounds;
        ui_draw_material(paint.track_bounds, (Rectangle){0}, track_style.background,
                         track_style.border, WHITE, track_style.radius,
                         track_style.border_width, 0.0f, 0.0f, 0,
                         track_style.focus, 0.0f, track_style.opacity,
                         track_frame.fill, track_style.material);
        ui_draw_material(thumb_bounds, paint.track_bounds,
                         thumb_style.background, thumb_style.border, WHITE,
                         thumb_style.radius, thumb_style.border_width,
                         thumb_state == ButtonStateHover ? 1.0f : 0.0f,
                         thumb_state == ButtonStatePressed ? 1.0f : 0.0f, 0,
                         thumb_style.focus, 0.0f, thumb_style.opacity,
                         thumb_frame.fill, thumb_style.material);
    }

    return 1;
}

/* ================================================================
 * END OF UI FUNCTIONS
 * ================================================================ */
