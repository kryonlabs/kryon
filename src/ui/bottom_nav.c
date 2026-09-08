#include "ui_internal.h"
#include "dropdown_store.h"
#include "ui_widget.h"

/* zero constants: the native Plan 9 compiler rejects short
 * compound literals like (Type){0}, and a copy of a zero
 * object is equivalent on every platform. */
static const Texture2D kryon_zero_texture2d;
static const Vector2 kryon_zero_vector2;


int
ui_bottom_nav_height(void)
{
    if(ui_classic_style())
        return Scale(40);
    return Scale(106);
}

static int
ui_bottom_nav_hit(Rectangle bounds, int disabled, int *hovered)
{
    Vector2 mouse = ui_mouse_world();
    int inside = CheckCollisionPointRec(mouse, bounds);
    int captured = UIInputCapturesClick(mouse);
    int active = inside && !disabled && !captured;

    if(hovered != NULL)
        *hovered = active && UIHoverEffectsEnabled();
    if(inside && !captured) {
        if(disabled)
            MarkUIDisabled();
        else
            MarkUIClickable();
    }
    if(mouse_release_activates_rect(bounds, mouse, active)) {
        UIConsumeRelease();
        return 1;
    }
    return 0;
}

static void
ui_draw_bottom_nav_icon(Texture2D icon, Rectangle dst, Color tint, unsigned char alpha)
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

static Color
ui_bottom_nav_mix(Color a, Color b, float t)
{
    if(t < 0.0f)
        t = 0.0f;
    if(t > 1.0f)
        t = 1.0f;
    return (Color){
        (unsigned char)((float)a.r + ((float)b.r - (float)a.r) * t),
        (unsigned char)((float)a.g + ((float)b.g - (float)a.g) * t),
        (unsigned char)((float)a.b + ((float)b.b - (float)a.b) * t),
        (unsigned char)((float)a.a + ((float)b.a - (float)a.a) * t)
    };
}

BottomNavResult
DrawUIBottomNav(BottomNavProps nav)
{
    BottomNavResult result = {-1, -1, 0, 0};
    int count = nav.count;
    int height = nav.height > 0 ? nav.height : ui_bottom_nav_height();
    int bottom_margin = nav.bottom_margin > 0 ? nav.bottom_margin : Scale(12);
    int side_margin = nav.side_margin > 0 ? nav.side_margin : Scale(14);
    int icon_size = nav.icon_size > 0 ? nav.icon_size : Scale(28);
    int y = nav.view_height - bottom_margin - height;
    int available_w = nav.view_width - side_margin * 2;
    int tab_w;
    int group_w;
    int start_x;
    UIWidget widget;
    Rectangle bounds;
    Color bg = GetThemeBackground();
    Color surface = c_surface.a != 0 ? c_surface : DarkenUIColor(bg, 5);
    Color accent = c_button_hover.a != 0 ? c_button_hover : c_circle;
    Color inactive = ui_bottom_nav_mix(c_text, bg, GetEffectiveThemeDarkMode() ? 0.30f : 0.46f);
    Color glass_top = ui_bottom_nav_mix(surface, WHITE, GetEffectiveThemeDarkMode() ? 0.06f : 0.30f);
    Color glass_bottom = ui_bottom_nav_mix(surface, BLACK, GetEffectiveThemeDarkMode() ? 0.28f : 0.04f);
    Color border = ui_bottom_nav_mix(accent, WHITE, GetEffectiveThemeDarkMode() ? 0.34f : 0.12f);
    int i;
    Rectangle dst;
    Rectangle rounded;
    Rectangle bar;

    result.y = y;
    result.height = height;
    if(nav.items == NULL || count <= 0 || nav.view_width <= 0 || nav.view_height <= 0)
        return result;
    if(count > 8)
        count = 8;
    if(available_w < Scale(96))
        available_w = Scale(96);

    tab_w = available_w / count;
    if(tab_w < Scale(56))
        tab_w = Scale(56);
    group_w = tab_w * count;
    if(group_w > available_w)
        group_w = available_w;
    tab_w = group_w / count;
    start_x = side_margin + (available_w - group_w) / 2;
    bounds.x = 0;
    bounds.y = (float)y;
    bounds.width = (float)nav.view_width;
    bounds.height = (float)height;
    widget = BeginUIWidget("bottom_nav", "tmp:bottom-nav", bounds,
                           UI_WIDGET_READONLY);
    UIWidgetSetAction(&widget, "DrawUIBottomNav");

    bar.x = (float)side_margin;
    bar.y = (float)(y + Scale(8));
    bar.width = (float)available_w;
    bar.height = (float)(height - Scale(16));
    ui_default_elevation(bar, 0.38f, 3);
    DrawRectangleRounded(bar, 0.38f, 18, ui_alpha(BLACK, GetEffectiveThemeDarkMode() ? 70 : 22));
    DrawRectangleRounded(bar, 0.38f, 18, glass_bottom);
    DrawRectangleGradientV((int)bar.x, (int)bar.y, (int)bar.width,
                           (int)(bar.height * 0.55f),
                           ui_alpha(glass_top, 210), ui_alpha(glass_top, 40));
    DrawRectangleRoundedLinesEx(bar, 0.38f, 18, Scale(1), ui_alpha(border, 96));

    for(i = 0; i < count; i++) {
        const BottomNavItem *item = &nav.items[i];
        int x = start_x + i * tab_w;
        int w = i == count - 1 ? start_x + group_w - x : tab_w;
        int icon_x;
        int icon_y;
        int hover = 0;
        unsigned char icon_alpha = item->disabled ? 150 : 255;
        Rectangle item_bounds = {(float)x, (float)y, (float)w, (float)height};
        int label_font = GetSmallFontSize();
        int label_h = TextLineHeight(label_font);
        int label_gap = Scale(5);
        int active_h = Scale(76);
        int active_w = w - Scale(10);
        int active_x = x + (w - active_w) / 2;
        int active_y = y + Scale(8);
        int content_y = y + Scale(19);
        int label_y = content_y + icon_size + label_gap;
        int label_pad = Scale(3);
        Color active_foreground = GetThemeButtonText();
        Color text_tint = item->active ? active_foreground : inactive;
        Color icon_tint = item->active ? active_foreground : inactive;

        if(item->disabled) {
            text_tint = ui_alpha(inactive, 112);
        }
        if(ui_bottom_nav_hit(item_bounds, item->disabled, &hover)) {
            result.clicked_index = i;
            result.clicked_route = item->route;
        }
        if(hover && !item->active) {
            rounded.x = (float)(x + Scale(5));
            rounded.y = (float)(y + Scale(14));
            rounded.width = (float)(w - Scale(10));
            rounded.height = (float)active_h;
            DrawRectangleRounded(rounded, 0.24f, 14, ui_alpha(c_text, 22));
        }
        if(item->active) {
            Color glow = LightenUIColor(accent, 30);
            rounded.x = (float)active_x;
            rounded.y = (float)active_y;
            rounded.width = (float)active_w;
            rounded.height = (float)active_h;
            DrawRectangleRounded((Rectangle){rounded.x - Scale(2), rounded.y + Scale(4),
                                             rounded.width + Scale(4), rounded.height},
                                 0.24f, 16, ui_alpha(glow, 38));
            DrawRectangleRounded(rounded, 0.24f, 16,
                                 ui_alpha(ui_bottom_nav_mix(accent, surface, 0.56f), 174));
            DrawRectangleRoundedLinesEx(rounded, 0.24f, 16, Scale(1),
                                        ui_alpha(LightenUIColor(accent, 42), 150));
        }
        icon_x = x + (w - icon_size) / 2;
        icon_y = content_y;
        dst.x = (float)icon_x;
        dst.y = (float)icon_y;
        dst.width = (float)icon_size;
        dst.height = (float)icon_size;
        ui_draw_bottom_nav_icon(item->icon, dst, nav.icon_color.a == 0 ? icon_tint : nav.icon_color, icon_alpha);
        if(item->label != NULL && item->label[0] != '\0') {
            Rectangle label_rect = {
                (float)(x + label_pad),
                (float)label_y,
                (float)(w - label_pad * 2),
                (float)label_h
            };
            DrawFittedTextInRect(item->label, label_rect, label_font,
                                 Text8, text_tint);
        }
    }

    EndUIWidget(&widget);
    return result;
}

static int
bottom_nav_option_index(const BottomNavOption *options, int option_count,
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

BottomNavConfigResult
DrawUIBottomNavConfigModal(BottomNavConfigProps modal)
{
    static int route_scroll_offset = 0;
    BottomNavConfigResult result = {0, 0};
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
        selected[i] = bottom_nav_option_index(modal.options, option_count,
                                              modal.routes != NULL ? modal.routes[i] : 0);

    frame = DrawUIModalFrame(Scale(340),
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
        int remove_hover = 0;
        DrawUIText(slot_label, frame.content_x, y, GetFontSize(), c_text);
        if(draw_dropdown(modal.id + i, frame.content_x,
                          y + Scale(22),
                          frame.content_w - remove_w - Scale(8),
                          dropdown_h, option_labels, option_count,
                          &selected[i]) &&
           modal.routes != NULL && selected[i] >= 0 && selected[i] < option_count) {
            modal.routes[i] = modal.options[selected[i]].route;
            result.changed = 1;
        }
        if(DrawUIPaddedIconBtn(frame.content_x + frame.content_w - remove_w,
                                  y + Scale(22), Scale(20),
                                  Scale(8), modal.close_icon,
                                  &remove_hover)) {
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
