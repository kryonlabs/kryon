#include "ui_internal.h"
#include "ui_style_internal.h"
#include "dropdown_store.h"
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
    int captured = InputCapturesClick(mouse);
    int active = inside && !disabled && !captured;

    if(hovered != NULL)
        *hovered = active && HoverEffectsEnabled();
    if(inside && !captured) {
        if(disabled)
            MarkDisabled();
        else
            MarkClickable();
    }
    if(mouse_release_activates_rect(bounds, mouse, active)) {
        ConsumeRelease();
        return 1;
    }
    return 0;
}

static void
ui_draw_navigation_bar_icon(Texture2D icon, IconType icon_type, Rectangle dst,
                            Color tint, unsigned char alpha)
{
    Rectangle src;

    tint.a = (unsigned char)((int)tint.a * alpha / 255);
    if(icon_type != ICON_NONE) {
        DrawIcon(icon_type, dst, tint);
        return;
    }
    if(icon.id == 0)
        return;
    if(tint.a == 0)
        tint = WHITE;
    src.x = 0;
    src.y = 0;
    src.width = (float)icon.width;
    src.height = (float)icon.height;
    DrawTexturePro(icon, src, dst, kryon_zero_vector2, 0, tint);
}

static StyleFrame
ui_navigation_bar_surface_frame(int class_name)
{
    ButtonProps props = {0};
    props.class_name = class_name;
    props.tone = ButtonToneNeutral;
    props.emphasis = ButtonEmphasisSoft;
    props.size = ControlSizeLarge;
    props.pill = 1;
    return ui_control_style_frame_kind(props, ButtonStateNormal, 0, 0.0f,
                                       0.0f, 0.0f, StyleKindNavigationBar());
}

static StyleFrame
ui_navigation_bar_item_frame(int active, int disabled, int hovered,
                             int class_name)
{
    ButtonProps props = {0};
    ButtonState state = ButtonStateNormal;
    props.class_name = class_name;
    props.tone = active ? ButtonToneAccent : ButtonToneNeutral;
    props.emphasis = active ? ButtonEmphasisFilled : ButtonEmphasisGhost;
    props.size = ControlSizeMedium;
    props.pill = 1;
    props.disabled = disabled;
    props.selected = active;
    if(disabled)
        state = ButtonStateDisabled;
    else if(active)
        state = ButtonStateSelected;
    else if(hovered)
        state = ButtonStateHover;
    return ui_control_style_frame_kind(props, state, 0, 0.0f, 0.0f, 0.0f,
                                       StyleKindNavigationBarItem());
}

NavigationBarResult
RenderNavigationBar(NavigationBarProps nav)
{
    NavigationBarResult result = {-1, -1, 0, 0};
    int count = nav.count;
    float runtime_scale = (float)Scale(1000) / 1000.0f;
    int height = nav.height > 0 ? nav.height : ui_navigation_bar_height();
    Widget widget;
    Rectangle bounds;
    NavigationBarPaint paint;
    Style bar_style;
    StyleFrame bar_frame;
    Style base_icon_style;
    int icon_size = 0;
    int side_margin = 0;
    int bottom_margin = 0;
    int i;
    Rectangle dst;

    if(nav.items == NULL || count <= 0 || nav.view_width <= 0 || nav.view_height <= 0)
        return result;
    base_icon_style = ui_unpack_style(
        ui_navigation_bar_item_frame(0, 0, 0, nav.class_name).value);
    if(base_icon_style.icon_size > 0.0f)
        icon_size = (int)(base_icon_style.icon_size + 0.5f);
    bar_frame = ui_navigation_bar_surface_frame(nav.class_name);
    bar_style = ui_unpack_style(bar_frame.value);
    if(bar_style.padding_x > 0.0f)
        side_margin = (int)(bar_style.padding_x + 0.5f);
    if(bar_style.padding_y > 0.0f)
        bottom_margin = (int)(bar_style.padding_y + 0.5f);
    paint = NavigationBarPaintFor((NavigationBarSpec){
        .view_width = nav.view_width,
        .view_height = nav.view_height,
        .count = count,
        .height = height,
        .side_margin = side_margin,
        .bottom_margin = bottom_margin,
        .icon_size = icon_size,
        .scale = runtime_scale,
        .bar = bar_frame
    });
    count = paint.count;
    result.y = paint.y;
    result.height = paint.height;
    bounds = paint.bounds;
    widget = BeginWidget("navigation_bar", "tmp:navigation-bar", bounds,
                           WIDGET_READONLY);
    WidgetSetAction(&widget, "RenderNavigationBar");

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
        StyleFrame base_frame = ui_navigation_bar_item_frame(0, item->disabled,
                                                             0, nav.class_name);
        Style base_style = ui_unpack_style(base_frame.value);
        StyleFrame face_frame = ui_navigation_bar_item_frame(item->active,
                                                             item->disabled, 0,
                                                             nav.class_name);
        Style text_style = base_style;
        int label_font = base_style.font_size > 0.0f
            ? (int)(base_style.font_size + 0.5f)
            : GetSmallFontSize();
        int label_h = TextLineHeight(label_font);
        NavigationBarItemPaint item_paint;

        item_paint = NavigationBarItemPaintFor((NavigationBarItemSpec){
            .bar = paint,
            .index = i,
            .active = item->active,
            .disabled = item->disabled,
            .hovered = 0,
            .label_height = label_h,
            .base = base_frame,
            .face = face_frame
        });
        if(ui_navigation_bar_hit(item_paint.bounds, item->disabled, &hover)) {
            result.clicked_index = i;
            result.clicked_route = item->route;
        }
        if(hover) {
            face_frame = ui_navigation_bar_item_frame(item->active,
                                                     item->disabled, hover,
                                                     nav.class_name);
            item_paint = NavigationBarItemPaintFor((NavigationBarItemSpec){
                .bar = paint,
                .index = i,
                .active = item->active,
                .disabled = item->disabled,
                .hovered = hover,
                .label_height = label_h,
                .base = base_frame,
                .face = face_frame
            });
        }
        if(item_paint.draw_face) {
            StyleFrame face_frame = ui_style_apply_effects_frame(item_paint.face);
            Style face_style = ui_unpack_style(face_frame.value);
            text_style = face_style;
            if(face_style.font_size > 0.0f)
                label_font = (int)(face_style.font_size + 0.5f);
            ui_draw_material(item_paint.state_bounds, (Rectangle){0},
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
        ui_draw_navigation_bar_icon(item->icon, item->icon_type, dst,
                                    GetColor(item_paint.icon_color),
                                    (unsigned char)item_paint.icon_alpha);
        if(item->label != NULL && item->label[0] != '\0') {
            DrawFittedTextInRect(item->label, item_paint.label_bounds,
                                 label_font, Text8,
                                 Fade(GetColor(item_paint.text_color),
                                      text_style.opacity));
        }
    }

    EndWidget(&widget);
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
    ScrollArea route_area;
    ScrollView route_view;
    const char *option_labels[16];
    int option_count = modal.option_count;
    int route_count = modal.route_count != NULL ? *modal.route_count : 0;
    int max_route_count = modal.max_route_count > 0 ? modal.max_route_count : route_count;
    int selected[16] = {0};
    float runtime_scale = (float)Scale(1000) / 1000.0f;
    NavigationBarConfigMetrics metrics =
        NavigationBarConfigMetricsFor(runtime_scale);
    NavigationBarConfigLayout layout;
    int y;
    int dropdown_blocks_buttons;
    int i;
    int j;
    Style label_style;
    int label_font;

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
    label_style = ui_unpack_style(ui_style_apply_effects_frame(
        ui_control_style_frame_kind((ButtonProps){0}, ButtonStateNormal, 0,
                                    0.0f, 0.0f, 0.0f,
                                    StyleKindNavigationBarItem())).value);
    label_font = label_style.font_size > 0.0f
        ? (int)(label_style.font_size + 0.5f)
        : GetSmallFontSize();
    for(i = 0; i < route_count; i++)
        selected[i] = navigation_bar_option_index(modal.options, option_count,
                                              modal.routes != NULL ? modal.routes[i] : 0);

    frame = RenderModalFrame(metrics.frame_width,
                                NavigationBarConfigFrameHeight(route_count,
                                                               metrics),
                                modal.title,
                                kryon_zero_texture2d,
                                modal.close_icon);
    if(frame.right_clicked) {
        result.action = 1;
        return result;
    }

    layout = NavigationBarConfigLayoutFor(
        (Rectangle){(float)frame.x, (float)frame.y,
                    (float)frame.w, (float)frame.h},
        (Rectangle){(float)frame.content_x, (float)frame.content_y,
                    (float)frame.content_w, (float)frame.content_h},
        route_count, metrics);
    memset(&route_area, 0, sizeof(route_area));
    route_area.bounds = layout.route_bounds;
    route_area.content_height = layout.route_content_height;
    route_area.content_x = frame.content_x;
    route_area.content_width = frame.content_w;
    route_area.scroll_offset = &route_scroll_offset;
    route_area.wheel_step = layout.wheel_step;
    route_area.scrollbar_x = layout.scrollbar_x;

    route_view = BeginScrollContainer(route_area);
    y = route_view.content_y;
    for(i = 0; i < route_count; i++) {
        NavigationBarConfigRowLayout row = NavigationBarConfigRowLayoutFor(
            (Rectangle){(float)frame.content_x, (float)frame.content_y,
                        (float)frame.content_w, (float)frame.content_h},
            y, TextLineHeight(label_font), metrics);
        const char *slot_label = modal.slot_labels != NULL && modal.slot_labels[i] != NULL
                                     ? modal.slot_labels[i]
                                     : "";
        RenderTextStyled(slot_label, (int)row.label_bounds.x,
                         (int)row.label_bounds.y,
                         (TextStyle){label_font,
                                     Fade(label_style.foreground,
                                          label_style.opacity),
                                     1, 0});
        if(Dropdown((DropdownProps){.id = modal.id + i, .bounds = row.dropdown_bounds,
            .options = option_labels, .option_count = option_count, .selected_index = &selected[i]}) &&
           modal.routes != NULL && selected[i] >= 0 && selected[i] < option_count) {
            modal.routes[i] = modal.options[selected[i]].route;
            result.changed = 1;
        }
        if(Button((ButtonProps){
            .bounds = row.remove_bounds,
            .icon = modal.close_icon, .icon_only = true,
            .tone = ButtonToneNeutral, .emphasis = ButtonEmphasisSoft
        })) {
            for(j = i; j < route_count - 1; j++)
                modal.routes[j] = modal.routes[j + 1];
            route_count--;
            if(modal.route_count != NULL)
                *modal.route_count = route_count;
            result.changed = 1;
            break;
        }
        y += metrics.row_height;
    }
    EndScrollContainer(route_area, route_view);

    dropdown_store_clip(layout.clip_y, layout.clip_height);

    dropdown_blocks_buttons = dropdown_captures(ui_mouse_world());
    if(route_count < max_route_count && modal.routes != NULL) {
        if(Button((ButtonProps){
               .bounds = layout.add_bounds,
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
        if(Button((ButtonProps){.bounds=layout.reset_bounds,
                               .label=modal.reset_label,.tone=ButtonToneNeutral,
                               .emphasis=ButtonEmphasisSoft,.disabled=dropdown_blocks_buttons}))
            result.action = 3;
        if(Button((ButtonProps){.bounds=layout.cancel_bounds,
                               .label=modal.cancel_label,.tone=ButtonToneNeutral,
                               .emphasis=ButtonEmphasisSoft,.disabled=dropdown_blocks_buttons}))
            result.action = 1;
        if(Button((ButtonProps){.bounds=layout.save_bounds,
                               .label=modal.save_label,.disabled=dropdown_blocks_buttons}))
            result.action = 2;
    }

    dropdown_store_clip(0, 0);

    return result;
}
