#include "ui_internal.h"
#include "ui_style_internal.h"
#include "runtime/title_bar.h"

/* Screen header (title bar) widgets. These were split out of modal.c so that
 * modal.c holds only modal/dialog code. The public declarations live in
 * ui_modal.h, which kryon.h includes. */

static void
RenderTitleBarBackground(int height)
{
    StyleFrame bar_frame = ui_control_style_frame_role_kind(
        (ButtonProps){.tone = ButtonToneNeutral, .emphasis = ButtonEmphasisSoft,
                      .size = ControlSizeMedium},
        ButtonStateNormal, 0, 0, 0, 0, StyleKindTitleBar(), 1);
    Style bar = ui_unpack_style(bar_frame.value);
    Rectangle bounds = {0, 0, (float)ui_view_width, (float)height};

    ui_draw_material(bounds, (Rectangle){0}, bar.background, bar.border,
                     bar.border, bar.radius, bar.border_width, 0.0f, 0.0f,
                     0, bar.focus, 0.0f, bar.opacity, ui_style_fill(bar),
                     bar.material);
    DrawLine(0, height - 1, ui_view_width, height - 1, bar.border);
}

static int
RenderTitleBarReturnButton(Texture2D return_icon, Rectangle bounds,
                           TitleBarMetrics metrics)
{
    IconActionSpec button = {0};

    button.bounds = bounds;
    button.icon = return_icon;
    button.icon_size = metrics.leading_icon_size;
    button.icon_padding = metrics.leading_padding;
    Style normal = ui_unpack_style(ui_control_style_frame_role_kind(
        (ButtonProps){.tone = ButtonToneNeutral, .emphasis = ButtonEmphasisSoft,
                      .icon_only = true},
        ButtonStateNormal, 0, 0, 0, 0, StyleKindTitleBar(), 17).value);
    Style hover = ui_unpack_style(ui_control_style_frame_role_kind(
        (ButtonProps){.tone = ButtonToneNeutral, .emphasis = ButtonEmphasisSoft,
                      .icon_only = true},
        ButtonStateHover, 0, 0, 0, 0, StyleKindTitleBar(), 17).value);
    button.icon_color = normal.foreground;
    button.hover_background = hover.background;
    button.radius = 0.50f;
    return RenderIconAction(button);
}

static void
RenderTitleBarCenteredTitle(const char *title, int height,
                                       int side_reserved)
{
    int font;
    int title_w;
    TitleBarMetrics metrics = TitleBarMetricsFor((float)GetScale());
    TitleBarLayout layout = TitleBarLayoutFor(ui_view_width, height,
                                              side_reserved > Scale(12),
                                              false, 0, 0, metrics);
    int max_w = (int)layout.title_bounds.width;

    if(title == NULL)
        title = "";
    font = GetTitleFontSize(title, max_w);
    title_w = TextWidth(title, font);
    while(TitleBarShouldShrinkTitleFont(title_w, max_w, font, Text12)) {
        font--;
        title_w = TextWidth(title, font);
    }
    Style text = ui_unpack_style(ui_control_style_frame_role_kind(
        (ButtonProps){.tone = ButtonToneNeutral, .emphasis = ButtonEmphasisSoft,
                      .size = ControlSizeMedium},
        ButtonStateNormal, 0, 0, 0, 0, StyleKindTitleBar(), 16).value);
    RenderText(title, TitleBarTitleX(ui_view_width, title_w),
               GetUIControlTextY(title, 0, height, font),
               font, text.foreground);
}

int
ui_title_bar_height(void)
{
    return ui_tab_bar_height();
}

int
RenderTitleBar(TitleBarProps title_bar)
{
    int height = title_bar.height;
    int clicked = 0;
    TitleBarMetrics metrics = TitleBarMetricsFor((float)GetScale());
    TitleBarLayout layout;
    int side_reserved = metrics.side_margin;

    if(height <= 0)
        height = ui_title_bar_height();
    layout = TitleBarLayoutFor(ui_view_width, height,
                               title_bar.has_leading_action != 0,
                               title_bar.has_dropdown != 0,
                               title_bar.dropdown.height,
                               title_bar.dropdown.min_width,
                               metrics);
    RenderTitleBarBackground(height);
    if(title_bar.has_leading_action) {
        clicked = RenderTitleBarReturnButton(title_bar.leading_icon,
                                             layout.leading_bounds, metrics);
        side_reserved = layout.side_reserved;
    }
    if(title_bar.has_dropdown) {
        TitleBarDropdown dropdown = title_bar.dropdown;
        if(!dropdown.disabled)
            Dropdown((DropdownProps){.id = dropdown.id, .bounds = layout.dropdown_bounds,
                .options = dropdown.options, .option_count = dropdown.option_count, .selected_index = dropdown.selected_index});
        return clicked;
    }
    RenderTitleBarCenteredTitle(title_bar.title, height, side_reserved);
    return clicked;
}
