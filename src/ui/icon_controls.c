#include "ui_internal.h"

int
DrawUIIconSliderPopup(UIIconSliderPopup popup)
{
    int hover = 0;
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
    icon_clicked = DrawUIPaddedIconBtn(popup.x, popup.y, popup.icon_size,
                                       popup.icon_padding, popup.icon, &hover);
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
    popup_h = popup.popup_height > 0 ? popup.popup_height : ScaleUIPx(200);
    popup_x = popup.x + button_w / 2 - popup_w / 2;
    popup_y = popup.y + popup.icon_size + popup.icon_padding * 2 + ScaleUIPx(4);
    mouse = ui_mouse_world();

    if(!icon_clicked && IsMouseButtonReleased(MOUSE_BUTTON_LEFT) &&
       (mouse.x < popup_x || mouse.x > popup_x + popup_w ||
        mouse.y < popup_y || mouse.y > popup_y + popup_h)) {
        *popup.open = 0;
        return 0;
    }

    DrawRectangle(popup_x, popup_y, popup_w, popup_h, c_surface);
    DrawUIBevel(popup_x, popup_y, popup_w, popup_h,
                  LightenUIColor(c_surface, 40), DarkenUIColor(c_surface, 40));

    return DrawUIVerticalSlider(popup.id, popup_x + popup_w / 2,
                                   popup_y + ScaleUIPx(14),
                                   popup_h - ScaleUIPx(24),
                                   popup.min, popup.max, popup.value);
}

UIIconRowResult
DrawUIBottomIconRow(UIBottomIconRow row)
{
    UIIconRowResult result = {-1, 0, 0};
    int count = row.count;
    int icon_size = row.icon_size > 0 ? row.icon_size : ScaleUIPx(24);
    int icon_padding = row.icon_padding > 0 ? row.icon_padding : ScaleUIPx(10);
    int gap = row.gap > 0 ? row.gap : ScaleUIPx(12);
    int side_margin = row.side_margin > 0 ? row.side_margin : ScaleUIPx(24);
    int bottom_margin = row.bottom_margin > 0 ? row.bottom_margin : ScaleUIPx(6);
    int min_icon_size = row.min_icon_size > 0 ? row.min_icon_size : ScaleUIPx(16);
    int min_icon_padding = row.min_icon_padding > 0 ? row.min_icon_padding : ScaleUIPx(6);
    int min_gap = row.min_gap > 0 ? row.min_gap : ScaleUIPx(8);
    int available_w;
    int max_btn_w;
    int button_w;
    int row_w;
    int start_x;

    if(row.items == NULL || count <= 0)
        return result;

    available_w = row.view_width - side_margin * 2;
    if(available_w < ScaleUIPx(120))
        available_w = ScaleUIPx(120);

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
        int hover = 0;
        int x = start_x + i * (button_w + gap);

        if(row.items[i].disabled)
            continue;
        if(DrawUIPaddedIconBtn(x, result.y, icon_size, icon_padding,
                                   row.items[i].icon, &hover))
            result.clicked_index = i;
    }

    return result;
}
