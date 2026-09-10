#include "ui_internal.h"

static void
ui_top_nav_background(Rectangle bounds)
{
    if(ui_default_style()) {
        ThemeScheme scheme = ui_default_scheme();
        DrawRectangleRec(bounds, scheme.surface_container);
        DrawLine((int)bounds.x, (int)(bounds.y + bounds.height - 1),
                 (int)(bounds.x + bounds.width),
                 (int)(bounds.y + bounds.height - 1),
                 scheme.outline);
        return;
    }
    DrawRectangleRec(bounds, c_bg);
    DrawLine((int)bounds.x, (int)(bounds.y + bounds.height - 1),
             (int)(bounds.x + bounds.width),
             (int)(bounds.y + bounds.height - 1),
             DarkenUIColor(c_button, 18));
}

static void
ui_top_nav_title(const char *title, Rectangle bounds, int side_reserved)
{
    int font = GetFontSize();
    int max_w = (int)bounds.width - side_reserved * 2;
    int title_w;

    if(title == NULL)
        title = "";
    if(max_w < Scale(48))
        max_w = (int)bounds.width - Scale(16);
    title_w = TextWidth(title, font);
    while(font > Text12 && title_w > max_w) {
        font--;
        title_w = TextWidth(title, font);
    }
    RenderText(title, (int)bounds.x + ((int)bounds.width - title_w) / 2,
                 (int)bounds.y + GetUIControlTextY(title, 0,
                                                   (int)bounds.height, font),
                 font, c_text);
}

TopNavResult
RenderTopNav(TopNavProps nav)
{
    TopNavResult result = {-1, -1};
    int x = nav.x;
    int y = nav.y;
    int w = nav.width > 0 ? nav.width : ui_view_width;
    int h = nav.height > 0 ? nav.height : ui_tab_bar_height();
    int pad = nav.side_padding > 0 ? nav.side_padding : Scale(8);
    int action_icon = nav.action_icon_size > 0 ? nav.action_icon_size
                                               : Scale(18);
    int action_pad = nav.action_icon_padding > 0 ? nav.action_icon_padding
                                                 : Scale(6);
    int action_gap = nav.action_gap > 0 ? nav.action_gap : Scale(4);
    int action_total = action_icon + action_pad * 2;
    int actions_w = 0;
    int dropdown_h = nav.dropdown_height > 0 ? nav.dropdown_height
                                             : Scale(32);
    int dropdown_x = x + pad;
    int dropdown_y = y + (h - dropdown_h) / 2;
    int previous = nav.selected_index != NULL ? *nav.selected_index : -1;
    Rectangle bounds = {(float)x, (float)y, (float)w, (float)h};
    UIWidget widget;

    if(w <= 0 || h <= 0)
        return result;

    widget = BeginUIWidget("top_nav", "tmp:top-nav", bounds,
                           UI_WIDGET_READONLY);
    UIWidgetSetAction(&widget, "RenderTopNav");
    ui_top_nav_background(bounds);

    if(nav.action_count > 0 && nav.actions != NULL)
        actions_w = nav.action_count * action_total +
                    (nav.action_count - 1) * action_gap + pad;

    if(nav.options != NULL && nav.option_count > 0 && nav.selected_index != NULL) {
        int dropdown_w = w - pad * 2 - actions_w;

        if(nav.dropdown_min_width > 0 && dropdown_w < nav.dropdown_min_width)
            dropdown_w = nav.dropdown_min_width;
        if(dropdown_x + dropdown_w > x + w - actions_w)
            dropdown_w = x + w - actions_w - dropdown_x;
        if(dropdown_w < 1)
            dropdown_w = 1;
        if(!nav.disabled)
            Combobox((ComboboxProps){.id = nav.id, .bounds = {dropdown_x, dropdown_y, dropdown_w, dropdown_h},
            .options = nav.options, .option_count = nav.option_count, .selected_index = nav.selected_index});
        if(*nav.selected_index != previous)
            result.selected_menu_item = *nav.selected_index;
    } else {
        ui_top_nav_title(nav.title, bounds, actions_w + pad);
    }

    for(int i = 0; i < nav.action_count && nav.actions != NULL; i++) {
        const TopNavAction *action = &nav.actions[i];
        int ax = x + w - pad - action_total -
                 i * (action_total + action_gap);
        int ay = y + (h - action_total) / 2;

        if(ay < y)
            ay = y;
        if(!nav.disabled && !action->disabled &&
           Button((ButtonProps){
               .bounds = {ax, ay, action_total, action_total},
               .icon = action->icon, .icon_only = true,
               .tone = ButtonToneNeutral, .emphasis = ButtonEmphasisSoft,
               .style = {.normal = {.fields = StyleIconSize,
                   .icon_size = (float)action_icon * 1000.0f / Scale(1000)}}
           }))
            result.clicked_action = i;
    }

    EndUIWidget(&widget);
    return result;
}
