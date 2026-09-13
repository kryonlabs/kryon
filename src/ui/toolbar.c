#include "ui_internal.h"
#include "ui_style_internal.h"
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

    StyleFrame bar_frame = ui_control_style_frame_role_kind(
        (ButtonProps){.tone = ButtonToneNeutral, .emphasis = ButtonEmphasisSoft,
                      .size = ControlSizeMedium,
                      .class_name = toolbar.class_name},
        ButtonStateNormal, 0, 0, 0, 0, StyleKindToolbar(), 1);
    StyleFrame divider_frame = ui_control_style_frame_role_kind(
        (ButtonProps){.tone = ButtonToneNeutral, .emphasis = ButtonEmphasisSoft,
                      .size = ControlSizeMedium,
                      .class_name = toolbar.class_name},
        ButtonStateNormal, 0, 0, 0, 0, StyleKindToolbar(), 18);
    Style bar = ui_unpack_style(bar_frame.value);
    Style divider = ui_unpack_style(divider_frame.value);
    Rectangle bar_bounds = {(float)toolbar.x, (float)toolbar.y,
                            (float)toolbar.width, (float)toolbar.height};
    ui_draw_material(bar_bounds, (Rectangle){0}, bar.background, bar.border,
                     bar.border, bar.radius, bar.border_width, 0.0f, 0.0f,
                     0, bar.focus, 0.0f, bar.opacity, ui_style_fill(bar),
                     bar.material);
    DrawLine(toolbar.x, toolbar.y + toolbar.height - 1,
             toolbar.x + toolbar.width, toolbar.y + toolbar.height - 1,
             divider.border);

    if(toolbar.actions != NULL && toolbar.action_count > 0) {
        for(int i = toolbar.action_count - 1; i >= 0; i--) {
            Rectangle action_bounds = ToolbarActionBoundsFor(layout, i,
                                                             toolbar.action_count);
            ButtonSpec button = {0};
            Style action = ui_unpack_style(ui_control_style_frame_role_kind(
                (ButtonProps){.tone = ButtonToneNeutral,
                              .emphasis = ButtonEmphasisSoft,
                              .size = ControlSizeMedium,
                              .icon_only = true,
                              .class_name = toolbar.class_name},
                ButtonStateNormal, 0, 0, 0, 0, StyleKindToolbar(), 17).value);
            Style action_hover = ui_unpack_style(ui_control_style_frame_role_kind(
                (ButtonProps){.tone = ButtonToneNeutral,
                              .emphasis = ButtonEmphasisSoft,
                              .size = ControlSizeMedium,
                              .icon_only = true,
                              .class_name = toolbar.class_name},
                ButtonStateHover, 0, 0, 0, 0, StyleKindToolbar(), 17).value);
            button.props.bounds = action_bounds;
            button.props.icon = toolbar.actions[i].icon;
            button.props.icon_type = toolbar.actions[i].icon_type;
            button.props.icon_only = 1;
            button.props.disabled = toolbar.actions[i].disabled;
            button.props.tone = ButtonToneNeutral;
            button.props.emphasis = ButtonEmphasisSoft;
            button.props.class_name = toolbar.class_name;
            button.props.style.normal = action;
            button.props.style.hover = action_hover;
            button.props.style.normal.fields |= StyleIconSize;
            button.props.style.normal.icon_size =
                (float)layout.action_icon_size * 1000.0f / Scale(1000);
            button.style_kind = StyleKindToolbar();
            button.style_resolved = 1;
            if(!toolbar.actions[i].disabled && ui_button_render(button))
                result.clicked_action = i;
        }
    }

    if(toolbar.options != NULL && toolbar.option_count > 0 &&
       toolbar.selected_index != NULL) {
        if(Dropdown((DropdownProps){.id = toolbar.id, .bounds = layout.dropdown_bounds,
            .class_name = toolbar.class_name, .options = toolbar.options,
            .option_count = toolbar.option_count, .selected_index = toolbar.selected_index}))
            result.selected_menu_item = toolbar.selected_index != NULL
                                            ? *toolbar.selected_index
                                            : -1;
    }

    return result;
}
