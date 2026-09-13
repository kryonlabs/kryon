#include "ui_internal.h"
#include "ui_style_internal.h"
#include "runtime/dropdown.h"
#include "runtime/toolbar.h"
#include "ui_style_sheet.h"

static Style
icon_popup_style(void)
{
    ButtonProps props = {0};
    props.tone = ButtonToneNeutral;
    props.emphasis = ButtonEmphasisFilled;
    return ui_style_apply_effects(ui_resolve_button_style_kind(props,
        ButtonStateNormal, StyleKindDropdown()));
}

static void
draw_icon_popup_surface(Rectangle bounds)
{
    Style paint = icon_popup_style();
    ui_draw_material(bounds, (Rectangle){0}, paint.background, paint.border,
        paint.border, paint.radius, paint.border_width, 1, 0, 0,
        paint.focus, 0, paint.opacity, ui_style_fill(paint), paint.material);
}

int
RenderIconSliderPopup(IconSliderPopupProps popup)
{
    IconSliderPopupLayout layout;
    int icon_clicked;
    int was_open;
    Vector2 mouse;

    if(popup.open == NULL || popup.value == NULL)
        return 0;

    layout = IconSliderPopupLayoutFor(popup.x, popup.y, popup.icon_size,
                                      popup.icon_padding, popup.popup_width,
                                      popup.popup_height,
                                      (float)Scale(1000) / 1000.0f);
    was_open = *popup.open;
    icon_clicked = Button((ButtonProps){
        .bounds = layout.button_bounds,
        .icon = popup.icon, .icon_only = true,
        .tone = ButtonToneNeutral, .emphasis = ButtonEmphasisSoft,
        .style = {.normal = {.fields = StyleIconSize,
            .icon_size = (float)layout.icon_size * 1000.0f / Scale(1000)}}
    });
    if(icon_clicked) {
        *popup.open = !was_open;
        if(was_open)
            return 0;
    }

    if(!*popup.open)
        return 0;

    mouse = ui_mouse_world();

    if(!icon_clicked && IsMouseButtonReleased(MOUSE_BUTTON_LEFT) &&
       !CheckCollisionPointRec(mouse, layout.popup_bounds)) {
        *popup.open = 0;
        return 0;
    }

    draw_icon_popup_surface(layout.popup_bounds);

    return ui_render_vertical_slider_active(popup.id, layout.slider_x,
                                            layout.slider_y,
                                            layout.slider_height,
                                            popup.min, popup.max, popup.value);
}

IconRowResult
RenderBottomIconRow(BottomIconRowProps row)
{
    IconRowResult result = {-1, 0, 0};
    int count = row.count;
    BottomIconRowLayout layout;

    if(row.items == NULL || count <= 0)
        return result;

    layout = BottomIconRowLayoutFor(row, (float)Scale(1000) / 1000.0f);
    result.y = layout.y;
    result.button_width = layout.button_width;

    for(int i = 0; i < count; i++) {
        Rectangle bounds = BottomIconRowButtonBoundsFor(layout, i);

        if(row.items[i].disabled)
            continue;
        if(Button((ButtonProps){
            .bounds = bounds,
            .icon = row.items[i].icon, .icon_only = true,
            .tone = ButtonToneNeutral, .emphasis = ButtonEmphasisSoft,
            .style = {.normal = {.fields = StyleIconSize,
                .icon_size = (float)layout.icon_size * 1000.0f / Scale(1000)}}
        }))
            result.clicked_index = i;
    }

    return result;
}
