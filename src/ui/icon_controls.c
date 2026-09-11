#include "ui_internal.h"
#include "ui_style_internal.h"
#include "runtime/dropdown.h"

static Style
icon_popup_style(void)
{
    ButtonProps props = {0};
    props.tone = ButtonToneNeutral;
    props.emphasis = ButtonEmphasisSoft;
    Style base = ResolveButtonStyle(props, ButtonStateNormal);
    props.tone = ButtonToneAccent;
    props.emphasis = ButtonEmphasisFilled;
    Style accent = ResolveButtonStyle(props, ButtonStateNormal);
    Style panel = ui_unpack_style(Appearance(
        ui_pack_style_states((ControlStyle){.normal = base}).normal,
        ui_pack_style_states((ControlStyle){.normal = accent}).normal,
        ColorToInt(GetThemeSurface()), 1, ButtonStateNormal, 0));
    panel = ui_style_apply_effects(panel);
    panel.radius = GetThemeMetrics().radius_large + 4.0f;
    return panel;
}

static void
draw_icon_popup_surface(Rectangle bounds)
{
    Style paint = icon_popup_style();
    ui_draw_material(bounds, (Rectangle){0}, paint.background, paint.border,
        paint.border, paint.radius, paint.border_width, 0, 0, 0,
        paint.focus, 0, paint.opacity, ui_style_fill(paint), paint.material);
}

int
RenderIconSliderPopup(IconSliderPopupProps popup)
{
    int popup_w;
    int popup_h;
    int popup_x;
    int popup_y;
    int button_w;
    int icon_clicked;
    int was_open;
    Vector2 mouse;

    if(popup.open == NULL || popup.value == NULL)
        return 0;

    was_open = *popup.open;
    button_w = popup.icon_size + popup.icon_padding * 2;
    icon_clicked = Button((ButtonProps){
        .bounds = {popup.x, popup.y, button_w, button_w},
        .icon = popup.icon, .icon_only = true,
        .tone = ButtonToneNeutral, .emphasis = ButtonEmphasisSoft,
        .style = {.normal = {.fields = StyleIconSize,
            .icon_size = (float)popup.icon_size * 1000.0f / Scale(1000)}}
    });
    if(icon_clicked) {
        *popup.open = !was_open;
        if(was_open)
            return 0;
    }

    if(!*popup.open)
        return 0;

    button_w = popup.icon_size + popup.icon_padding * 2;
    popup_w = popup.popup_width > 0 ? popup.popup_width : button_w;
    if(popup_w < button_w)
        popup_w = button_w;
    popup_h = popup.popup_height > 0 ? popup.popup_height : Scale(200);
    popup_x = popup.x + button_w / 2 - popup_w / 2;
    popup_y = popup.y + popup.icon_size + popup.icon_padding * 2 + Scale(4);
    mouse = ui_mouse_world();

    if(!icon_clicked && IsMouseButtonReleased(MOUSE_BUTTON_LEFT) &&
       (mouse.x < popup_x || mouse.x > popup_x + popup_w ||
        mouse.y < popup_y || mouse.y > popup_y + popup_h)) {
        *popup.open = 0;
        return 0;
    }

    draw_icon_popup_surface((Rectangle){popup_x, popup_y, popup_w, popup_h});

    return ui_render_vertical_slider(popup.id, popup_x + popup_w / 2,
                                     popup_y + Scale(14),
                                     popup_h - Scale(24),
                                     popup.min, popup.max, popup.value);
}

IconRowResult
RenderBottomIconRow(BottomIconRowProps row)
{
    IconRowResult result = {-1, 0, 0};
    int count = row.count;
    int icon_size = row.icon_size > 0 ? row.icon_size : Scale(24);
    int icon_padding = row.icon_padding > 0 ? row.icon_padding : Scale(10);
    int gap = row.gap > 0 ? row.gap : Scale(12);
    int side_margin = row.side_margin > 0 ? row.side_margin : Scale(24);
    int bottom_margin = row.bottom_margin > 0 ? row.bottom_margin : Scale(6);
    int min_icon_size = row.min_icon_size > 0 ? row.min_icon_size : Scale(16);
    int min_icon_padding = row.min_icon_padding > 0 ? row.min_icon_padding : Scale(6);
    int min_gap = row.min_gap > 0 ? row.min_gap : Scale(8);
    int available_w;
    int max_btn_w;
    int button_w;
    int row_w;
    int start_x;

    if(row.items == NULL || count <= 0)
        return result;

    available_w = row.view_width - side_margin * 2;
    if(available_w < Scale(120))
        available_w = Scale(120);

    max_btn_w = row.max_button_width > 0 ? row.max_button_width : available_w;
    if(count > 1) {
        int fit_btn_w = (available_w - gap * (count - 1)) / count;
        if(max_btn_w <= 0 || max_btn_w > fit_btn_w)
            max_btn_w = fit_btn_w;
    }

    button_w = icon_size + icon_padding * 2;
    if(button_w > max_btn_w) {
        button_w = max_btn_w;
        icon_padding = button_w / 4;
        icon_size = button_w - icon_padding * 2;
    }

    if(icon_padding < min_icon_padding)
        icon_padding = min_icon_padding;
    if(icon_size < min_icon_size)
        icon_size = min_icon_size;

    button_w = icon_size + icon_padding * 2;
    gap = button_w / 4;
    if(gap < min_gap)
        gap = min_gap;

    row_w = button_w * count + gap * (count - 1);
    start_x = row.center_x - row_w / 2;
    result.y = row.view_height - bottom_margin - button_w;
    result.button_width = button_w;

    for(int i = 0; i < count; i++) {
        int x = start_x + i * (button_w + gap);

        if(row.items[i].disabled)
            continue;
        if(Button((ButtonProps){
            .bounds = {x, result.y, button_w, button_w},
            .icon = row.items[i].icon, .icon_only = true,
            .tone = ButtonToneNeutral, .emphasis = ButtonEmphasisSoft,
            .style = {.normal = {.fields = StyleIconSize,
                .icon_size = (float)icon_size * 1000.0f / Scale(1000)}}
        }))
            result.clicked_index = i;
    }

    return result;
}
