#include "ui_internal.h"
#include "runtime/toolbar.h"

ToolbarResult
RenderToolbar(ToolbarProps toolbar)
{
    ToolbarResult result = {-1, -1};
    ToolbarLayout layout = ToolbarLayoutFor((ToolbarSpec){
        .x = toolbar.x,
        .y = toolbar.y,
        .width = toolbar.width,
        .height = toolbar.height,
        .action_count = toolbar.action_count,
        .action_icon_size = toolbar.action_icon_size,
        .action_icon_padding = toolbar.action_icon_padding,
        .action_gap = toolbar.action_gap,
        .side_padding = toolbar.side_padding,
        .dropdown_min_width = toolbar.dropdown_min_width,
        .dropdown_max_width = toolbar.dropdown_max_width,
        .dropdown_height = toolbar.dropdown_height,
        .scale = (float)Scale(1000) / 1000.0f
    });

    Color bar = DarkenUIColor(c_bg, 14);
    if(ui_modern_style()) {
        ThemeMetrics tokens = GetThemeMetrics();
        if(tokens.panel_alpha < bar.a)
            bar.a = tokens.panel_alpha;
    }
    DrawRectangle(toolbar.x, toolbar.y, toolbar.width, toolbar.height, bar);
    if(ui_modern_style() && GetThemeMetrics().shine_alpha > 0) {
        Color shine = WHITE;
        shine.a = GetThemeMetrics().shine_alpha;
        DrawRectangle(toolbar.x, toolbar.y, toolbar.width, Scale(1), shine);
    }
    DrawLine(toolbar.x, toolbar.y + toolbar.height - 1,
             toolbar.x + toolbar.width, toolbar.y + toolbar.height - 1,
             DarkenUIColor(c_bg, 42));

    if(toolbar.actions != NULL && toolbar.action_count > 0) {
        for(int i = toolbar.action_count - 1; i >= 0; i--) {
            Rectangle action_bounds = ToolbarActionBoundsFor(layout, i,
                                                             toolbar.action_count);
            if(!toolbar.actions[i].disabled &&
               Button((ButtonProps){
                   .bounds = action_bounds,
                   .icon = toolbar.actions[i].icon, .icon_only = true,
                   .tone = ButtonToneNeutral, .emphasis = ButtonEmphasisSoft,
                   .style = {.normal = {.fields = StyleIconSize,
                       .icon_size = (float)layout.action_icon_size * 1000.0f / Scale(1000)}}
               }))
                result.clicked_action = i;
        }
    }

    if(toolbar.options != NULL && toolbar.option_count > 0 &&
       toolbar.selected_index != NULL) {
        if(Dropdown((DropdownProps){.id = toolbar.id, .bounds = layout.dropdown_bounds,
            .options = toolbar.options, .option_count = toolbar.option_count, .selected_index = toolbar.selected_index}))
            result.selected_menu_item = toolbar.selected_index != NULL
                                            ? *toolbar.selected_index
                                            : -1;
    }

    return result;
}
