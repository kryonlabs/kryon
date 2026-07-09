#include "ui.h"

int
GetUIBottomNavHeight(void)
{
    return ScaleUIPx(40);
}

UIBottomNavResult
DrawUIBottomNav(UIBottomNav nav)
{
    UIBottomNavResult result = {-1, -1, 0, 0};
    int count = nav.count;
    int height = nav.height > 0 ? nav.height : GetUIBottomNavHeight();
    int bottom_margin = nav.bottom_margin > 0 ? nav.bottom_margin : 0;
    int side_margin = nav.side_margin > 0 ? nav.side_margin : 0;
    int icon_size = nav.icon_size > 0 ? nav.icon_size : ScaleUIPx(22);
    int y = nav.view_height - bottom_margin - height;
    int available_w = nav.view_width - side_margin * 2;
    int tab_w;
    int group_w;
    int start_x;

    result.y = y;
    result.height = height;
    if(nav.items == NULL || count <= 0 || nav.view_width <= 0 || nav.view_height <= 0)
        return result;
    if(count > 8)
        count = 8;
    if(available_w < ScaleUIPx(96))
        available_w = ScaleUIPx(96);

    tab_w = available_w / count;
    if(tab_w < ScaleUIPx(56))
        tab_w = ScaleUIPx(56);
    group_w = tab_w * count;
    if(group_w > available_w)
        group_w = available_w;
    tab_w = group_w / count;
    start_x = side_margin + (available_w - group_w) / 2;

    DrawRectangle(0, y, nav.view_width, height, DarkenUIColor(c_bg, 10));
    DrawLine(0, y, nav.view_width, y, DarkenUIColor(c_bg, 42));

    for(int i = 0; i < count; i++) {
        const UIBottomNavItem *item = &nav.items[i];
        int x = start_x + i * tab_w;
        int w = i == count - 1 ? start_x + group_w - x : tab_w;
        int icon_x = x + (w - icon_size) / 2;
        int icon_y = y + (height - icon_size) / 2;
        int hover = 0;
        UIButtonStyle style = item->active
                                  ? UI_BUTTON_STYLE_TAB_SELECTED
                                  : UI_BUTTON_STYLE_TAB;
        Color icon_color = item->disabled ? DarkenUIColor(c_icon, 40) : c_icon;

        if(DrawUIGenericButton(x, y, w, height, "", style,
                                  item->disabled, &hover)) {
            result.clicked_index = i;
            result.clicked_route = item->route;
        }

        if(item->icon.id != 0) {
            DrawTexturePro(item->icon,
                           (Rectangle){0, 0, item->icon.width, item->icon.height},
                           (Rectangle){icon_x, icon_y, icon_size, icon_size},
                           (Vector2){0}, 0, icon_color);
        }
    }

    return result;
}

static int
bottom_nav_option_index(const UIBottomNavOption *options, int option_count,
                        int route)
{
    if(options == NULL || option_count <= 0)
        return 0;
    for(int i = 0; i < option_count; i++) {
        if(options[i].route == route)
            return i;
    }
    return 0;
}

UIBottomNavConfigResult
DrawUIBottomNavConfigModal(UIBottomNavConfigModal modal)
{
    static int route_scroll_offset = 0;
    UIBottomNavConfigResult result = {0, 0};
    UIPanelFrame frame;
    UIScrollArea route_area;
    UIScrollView route_view;
    const char *option_labels[16];
    int option_count = modal.option_count;
    int route_count = modal.route_count != NULL ? *modal.route_count : 0;
    int max_route_count = modal.max_route_count > 0 ? modal.max_route_count : route_count;
    int selected[16] = {0};
    int row_h = ScaleUIPx(58);
    int dropdown_h = ScaleUIPx(36);
    int remove_w = ScaleUIPx(36);
    int add_h = ScaleUIPx(34);
    int button_h = ScaleUIPx(36);
    int button_gap = ScaleUIPx(8);
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

    if(max_route_count > 16)
        max_route_count = 16;
    if(route_count < 0)
        route_count = 0;
    if(route_count > max_route_count)
        route_count = max_route_count;
    if(option_count > 16)
        option_count = 16;
    for(int i = 0; i < option_count; i++)
        option_labels[i] = modal.options[i].label;
    for(int i = 0; i < route_count; i++)
        selected[i] = bottom_nav_option_index(modal.options, option_count,
                                              modal.routes != NULL ? modal.routes[i] : 0);

    frame = DrawUIModalFrame(ScaleUIPx(340),
                                ScaleUIPx(128) + row_h * route_count + add_h + ScaleUIPx(58),
                                modal.title,
                                (Texture2D){0},
                                modal.close_icon);
    if(frame.right_clicked) {
        result.action = 1;
        return result;
    }

    button_w = (frame.content_w - button_gap * 2) / 3;
    if(button_w > ScaleUIPx(92))
        button_w = ScaleUIPx(92);
    total_button_w = button_w * 3 + button_gap * 2;
    button_y = frame.y + frame.h - button_h - ScaleUIPx(16);
    add_y = button_y - button_gap - add_h;
    route_view_h = add_y - frame.content_y - ScaleUIPx(12);
    if(route_view_h < row_h)
        route_view_h = row_h;
    if(frame.content_y + route_view_h > add_y - ScaleUIPx(8))
        route_view_h = add_y - frame.content_y - ScaleUIPx(8);
    if(route_view_h < ScaleUIPx(48))
        route_view_h = ScaleUIPx(48);
    route_content_h = row_h * route_count;
    route_area = (UIScrollArea){
        .bounds = {
            (float)frame.content_x,
            (float)frame.content_y,
            (float)frame.content_w,
            (float)route_view_h
        },
        .content_height = route_content_h,
        .content_x = frame.content_x,
        .content_width = frame.content_w,
        .scroll_offset = &route_scroll_offset,
        .wheel_step = row_h,
        .scrollbar_x = frame.content_x + frame.content_w - ScaleUIPx(8)
    };

    route_view = BeginUIScrollContainer(route_area);
    y = route_view.content_y;
    for(int i = 0; i < route_count; i++) {
        const char *slot_label = modal.slot_labels != NULL && modal.slot_labels[i] != NULL
                                     ? modal.slot_labels[i]
                                     : "";
        int remove_hover = 0;
        DrawUIText(slot_label, frame.content_x, y, GetUIFontSize(), c_text);
        DrawUIDropdownButton(modal.id + i, frame.content_x,
                                y + ScaleUIPx(22), frame.content_w - remove_w - ScaleUIPx(8),
                                dropdown_h, option_labels, option_count,
                                &selected[i]);
        if(DrawUIPaddedIconBtn(frame.content_x + frame.content_w - remove_w,
                                  y + ScaleUIPx(22), ScaleUIPx(20),
                                  ScaleUIPx(8), modal.close_icon,
                                  &remove_hover)) {
            for(int j = i; j < route_count - 1; j++)
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

    SetUIDropdownClipTop(frame.content_y);
    SetUIDropdownClipBottom(add_y - ScaleUIPx(8));

    y = add_y;
    dropdown_blocks_buttons = UIDropdownCapturesClick(ui_mouse_world());
    if(route_count < max_route_count && modal.routes != NULL) {
        int add_hover = 0;
        add_w = frame.content_w < ScaleUIPx(180) ? frame.content_w : ScaleUIPx(180);
        if(DrawUIGenericButton(frame.content_x + (frame.content_w - add_w) / 2,
                                  y, add_w, add_h, modal.add_label,
                                  UI_BUTTON_STYLE_SECONDARY,
                                  dropdown_blocks_buttons, &add_hover)) {
            modal.routes[route_count] = option_count > 0 ? modal.options[0].route : 0;
            route_count++;
            if(modal.route_count != NULL)
                *modal.route_count = route_count;
            result.changed = 1;
        }
    }

    {
        int x = frame.x + (frame.w - total_button_w) / 2;
        if(DrawUIGenericButton(x, button_y, button_w, button_h,
                                  modal.reset_label, UI_BUTTON_STYLE_SECONDARY,
                                  dropdown_blocks_buttons, &reset_hover))
            result.action = 3;
        x += button_w + button_gap;
        if(DrawUIGenericButton(x, button_y, button_w, button_h,
                                  modal.cancel_label, UI_BUTTON_STYLE_SECONDARY,
                                  dropdown_blocks_buttons, &cancel_hover))
            result.action = 1;
        x += button_w + button_gap;
        if(DrawUIGenericButton(x, button_y, button_w, button_h,
                                  modal.save_label, UI_BUTTON_STYLE_PRIMARY,
                                  dropdown_blocks_buttons, &save_hover))
            result.action = 2;
    }

    for(int i = 0; i < route_count; i++) {
        if(DrawUIDropdownMenu(modal.id + i) &&
           modal.routes != NULL && selected[i] >= 0 && selected[i] < option_count) {
            modal.routes[i] = modal.options[selected[i]].route;
            result.changed = 1;
        }
    }
    SetUIDropdownClipTop(0);
    SetUIDropdownClipBottom(0);

    return result;
}
