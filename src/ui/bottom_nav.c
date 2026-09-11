#include "ui_internal.h"
#include "ui_style_internal.h"
#include "dropdown_store.h"
#include "ui_widget.h"
#include "runtime/navigation_bar.h"

/* zero constants: the native Plan 9 compiler rejects short
 * compound literals like (Type){0}, and a copy of a zero
 * object is equivalent on every platform. */
static const Texture2D kryon_zero_texture2d;
static const Vector2 kryon_zero_vector2;


int
ui_navigation_bar_height(void)
{
    if(ui_classic_style())
        return Scale(40);
    return NavigationBarDefaultHeight((float)Scale(1000) / 1000.0f);
}

static int
ui_navigation_bar_hit(Rectangle bounds, int disabled, int *hovered)
{
    Vector2 mouse = ui_mouse_world();
    int inside = CheckCollisionPointRec(mouse, bounds);
    int captured = UIInputCapturesClick(mouse);
    int active = inside && !disabled && !captured;

    if(hovered != NULL)
        *hovered = active && UIHoverEffectsEnabled();
    if(inside && !captured) {
        if(disabled)
            MarkDisabled();
        else
            MarkClickable();
    }
    if(mouse_release_activates_rect(bounds, mouse, active)) {
        UIConsumeRelease();
        return 1;
    }
    return 0;
}

static void
ui_draw_navigation_bar_icon(Texture2D icon, Rectangle dst, Color tint, unsigned char alpha)
{
    Rectangle src;

    if(icon.id == 0)
        return;
    if(tint.a == 0)
        tint = WHITE;
    tint.a = (unsigned char)((int)tint.a * alpha / 255);
    src.x = 0;
    src.y = 0;
    src.width = (float)icon.width;
    src.height = (float)icon.height;
    DrawTexturePro(icon, src, dst, kryon_zero_vector2, 0, tint);
}

NavigationBarResult
RenderNavigationBar(NavigationBarProps nav)
{
    NavigationBarResult result = {-1, -1, 0, 0};
    int count = nav.count;
    float runtime_scale = (float)Scale(1000) / 1000.0f;
    int height = nav.height > 0 ? nav.height : ui_navigation_bar_height();
    UIWidget widget;
    Rectangle bounds;
    Palette palette;
    Metrics tokens;
    NavigationBarPaint paint;
    StyleFrame bar_frame;
    Style bar_style;
    int i;
    Rectangle dst;

    if(nav.items == NULL || count <= 0 || nav.view_width <= 0 || nav.view_height <= 0)
        return result;
    ui_runtime_theme_values(&palette, &tokens);
    paint = NavigationBarPaintFor((NavigationBarSpec){
        .view_width = nav.view_width,
        .view_height = nav.view_height,
        .count = count,
        .height = height,
        .side_margin = nav.side_margin,
        .bottom_margin = nav.bottom_margin,
        .icon_size = nav.icon_size,
        .scale = runtime_scale,
        .palette = palette,
        .metrics = tokens
    });
    count = paint.count;
    result.y = paint.y;
    result.height = paint.height;
    bounds = paint.bounds;
    widget = BeginUIWidget("navigation_bar", "tmp:bottom-nav", bounds,
                           UI_WIDGET_READONLY);
    UIWidgetSetAction(&widget, "RenderNavigationBar");

    bar_frame = ui_style_apply_effects_frame(paint.bar);
    bar_style = ui_unpack_style(bar_frame.value);
    ui_default_elevation(paint.bar_bounds, bar_style.radius, 2);
    ui_draw_material(paint.bar_bounds, (Rectangle){0},
                     bar_style.background, bar_style.border, bar_style.border,
                     bar_style.radius, bar_style.border_width,
                     0.0f, 0.0f, 0, bar_style.focus, 0.0f,
                     bar_style.opacity, ui_style_apply_effects_fill(bar_frame.fill),
                     bar_style.material);

    for(i = 0; i < count; i++) {
        const NavigationBarItem *item = &nav.items[i];
        int hover = 0;
        int label_font = GetSmallFontSize();
        int label_h = TextLineHeight(label_font);
        NavigationBarItemPaint item_paint;

        item_paint = NavigationBarItemPaintFor((NavigationBarItemSpec){
            .bar = paint,
            .index = i,
            .active = item->active,
            .disabled = item->disabled,
            .hovered = 0,
            .label_height = label_h,
            .palette = palette,
            .metrics = tokens
        });
        if(ui_navigation_bar_hit(item_paint.bounds, item->disabled, &hover)) {
            result.clicked_index = i;
            result.clicked_route = item->route;
        }
        if(hover) {
            item_paint = NavigationBarItemPaintFor((NavigationBarItemSpec){
                .bar = paint,
                .index = i,
                .active = item->active,
                .disabled = item->disabled,
                .hovered = hover,
                .label_height = label_h,
                .palette = palette,
                .metrics = tokens
            });
        }
        if(item_paint.draw_face) {
            StyleFrame face_frame = ui_style_apply_effects_frame(item_paint.face);
            Style face_style = ui_unpack_style(face_frame.value);
            ui_draw_material(item_paint.state_bounds, paint.bar_bounds,
                             face_style.background, face_style.border,
                             face_style.border, face_style.radius,
                             face_style.border_width,
                             hover ? 1.0f : 0.0f, 0.0f, item->disabled,
                             face_style.focus, 0.0f,
                             face_style.opacity,
                             ui_style_apply_effects_fill(face_frame.fill),
                             face_style.material);
        }
        dst = item_paint.icon_bounds;
        ui_draw_navigation_bar_icon(item->icon, dst,
                                nav.icon_color.a == 0 ? GetColor(item_paint.icon_color)
                                                      : nav.icon_color,
                                (unsigned char)item_paint.icon_alpha);
        if(item->label != NULL && item->label[0] != '\0') {
            DrawFittedTextInRect(item->label, item_paint.label_bounds,
                                 label_font, Text8,
                                 GetColor(item_paint.text_color));
        }
    }

    EndUIWidget(&widget);
    return result;
}

static int
navigation_bar_option_index(const NavigationBarOption *options, int option_count,
                        int route)
{
    int i;

    if(options == NULL || option_count <= 0)
        return 0;
    for(i = 0; i < option_count; i++) {
        if(options[i].route == route)
            return i;
    }
    return 0;
}

NavigationBarConfigResult
RenderNavigationBarConfigModal(NavigationBarConfigProps modal)
{
    static int route_scroll_offset = 0;
    NavigationBarConfigResult result = {0, 0};
    UIPanelFrame frame;
    UIScrollArea route_area;
    UIScrollView route_view;
    const char *option_labels[16];
    int option_count = modal.option_count;
    int route_count = modal.route_count != NULL ? *modal.route_count : 0;
    int max_route_count = modal.max_route_count > 0 ? modal.max_route_count : route_count;
    int selected[16] = {0};
    int row_h = Scale(58);
    int dropdown_h = Scale(36);
    int remove_w = Scale(36);
    int add_h = Scale(34);
    int button_h = Scale(36);
    int button_gap = Scale(8);
    int y;
    int button_w;
    int total_button_w;
    int button_y;
    int add_y;
    int add_w;
    int route_view_h;
    int route_content_h;
    int reset_hover = 0;
    int cancel_hover = 0;
    int save_hover = 0;
    int dropdown_blocks_buttons;
    int i;
    int j;

    if(max_route_count > 16)
        max_route_count = 16;
    if(route_count < 0)
        route_count = 0;
    if(route_count > max_route_count)
        route_count = max_route_count;
    if(option_count > 16)
        option_count = 16;
    for(i = 0; i < option_count; i++)
        option_labels[i] = modal.options[i].label;
    for(i = 0; i < route_count; i++)
        selected[i] = navigation_bar_option_index(modal.options, option_count,
                                              modal.routes != NULL ? modal.routes[i] : 0);

    frame = RenderModalFrame(Scale(340),
                                Scale(128) + row_h * route_count + add_h + Scale(58),
                                modal.title,
                                kryon_zero_texture2d,
                                modal.close_icon);
    if(frame.right_clicked) {
        result.action = 1;
        return result;
    }

    button_w = (frame.content_w - button_gap * 2) / 3;
    if(button_w > Scale(92))
        button_w = Scale(92);
    total_button_w = button_w * 3 + button_gap * 2;
    button_y = frame.y + frame.h - button_h - Scale(16);
    add_y = button_y - button_gap - add_h;
    route_view_h = add_y - frame.content_y - Scale(12);
    if(route_view_h < row_h)
        route_view_h = row_h;
    if(frame.content_y + route_view_h > add_y - Scale(8))
        route_view_h = add_y - frame.content_y - Scale(8);
    if(route_view_h < Scale(48))
        route_view_h = Scale(48);
    route_content_h = row_h * route_count;
    memset(&route_area, 0, sizeof(route_area));
    route_area.bounds.x = (float)frame.content_x;
    route_area.bounds.y = (float)frame.content_y;
    route_area.bounds.width = (float)frame.content_w;
    route_area.bounds.height = (float)route_view_h;
    route_area.content_height = route_content_h;
    route_area.content_x = frame.content_x;
    route_area.content_width = frame.content_w;
    route_area.scroll_offset = &route_scroll_offset;
    route_area.wheel_step = row_h;
    route_area.scrollbar_x = frame.content_x + frame.content_w - Scale(8);

    route_view = BeginUIScrollContainer(route_area);
    y = route_view.content_y;
    for(i = 0; i < route_count; i++) {
        const char *slot_label = modal.slot_labels != NULL && modal.slot_labels[i] != NULL
                                     ? modal.slot_labels[i]
                                     : "";
        RenderText(slot_label, frame.content_x, y, GetFontSize(), c_text);
        if(Dropdown((DropdownProps){.id = modal.id + i, .bounds = {frame.content_x, y + Scale(22), frame.content_w - remove_w - Scale(8), dropdown_h},
            .options = option_labels, .option_count = option_count, .selected_index = &selected[i]}) &&
           modal.routes != NULL && selected[i] >= 0 && selected[i] < option_count) {
            modal.routes[i] = modal.options[selected[i]].route;
            result.changed = 1;
        }
        if(Button((ButtonProps){
            .bounds = {frame.content_x + frame.content_w - remove_w,
                       y + Scale(22), remove_w, remove_w},
            .icon = modal.close_icon, .icon_only = true,
            .tone = ButtonToneNeutral, .emphasis = ButtonEmphasisSoft,
            .style = {.normal = {.fields = StyleIconSize, .icon_size = 20}}
        })) {
            for(j = i; j < route_count - 1; j++)
                modal.routes[j] = modal.routes[j + 1];
            route_count--;
            if(modal.route_count != NULL)
                *modal.route_count = route_count;
            result.changed = 1;
            break;
        }
        y += row_h;
    }
    EndUIScrollContainer(route_area, route_view);

    dropdown_store_clip(frame.content_y, add_y - Scale(8));

    y = add_y;
    dropdown_blocks_buttons = dropdown_captures(ui_mouse_world());
    if(route_count < max_route_count && modal.routes != NULL) {
        add_w = frame.content_w < Scale(180) ? frame.content_w : Scale(180);
        if(Button((ButtonProps){
               .bounds = {(float)(frame.content_x + (frame.content_w - add_w) / 2),
                          (float)y, (float)add_w, (float)add_h},
               .label = modal.add_label,
               .tone = ButtonToneNeutral,
               .emphasis = ButtonEmphasisSoft,
               .disabled = dropdown_blocks_buttons
           })) {
            modal.routes[route_count] = option_count > 0 ? modal.options[0].route : 0;
            route_count++;
            if(modal.route_count != NULL)
                *modal.route_count = route_count;
            result.changed = 1;
        }
    }

    {
        int x = frame.x + (frame.w - total_button_w) / 2;
        if(Button((ButtonProps){.bounds={(float)x,(float)button_y,(float)button_w,(float)button_h},
                               .label=modal.reset_label,.tone=ButtonToneNeutral,
                               .emphasis=ButtonEmphasisSoft,.disabled=dropdown_blocks_buttons}))
            result.action = 3;
        x += button_w + button_gap;
        if(Button((ButtonProps){.bounds={(float)x,(float)button_y,(float)button_w,(float)button_h},
                               .label=modal.cancel_label,.tone=ButtonToneNeutral,
                               .emphasis=ButtonEmphasisSoft,.disabled=dropdown_blocks_buttons}))
            result.action = 1;
        x += button_w + button_gap;
        if(Button((ButtonProps){.bounds={(float)x,(float)button_y,(float)button_w,(float)button_h},
                               .label=modal.save_label,.disabled=dropdown_blocks_buttons}))
            result.action = 2;
    }

    dropdown_store_clip(0, 0);

    return result;
}
