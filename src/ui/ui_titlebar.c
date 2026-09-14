#include "ui_internal.h"
#include "ui_style_internal.h"
#include "runtime/style.h"
#include "runtime/title_bar.h"

/* Screen header (title bar) widgets. These were split out of modal.c so that
 * modal.c holds only modal/dialog code. The public declarations live in
 * ui_modal.h, which kryon.h includes. */

static void
RenderTitleBarBackground(int height, int class_name)
{
    TitleBarPaint paint = TitleBarPaintFor(ui_view_width, height);
    StyleFrame bar_frame = ui_control_style_frame_role_kind(
        (ButtonProps){.tone = ButtonToneNeutral, .emphasis = ButtonEmphasisSoft,
                      .size = ControlSizeMedium, .class_name = class_name},
        ButtonStateNormal, 0, 0, 0, 0, StyleKindTitleBar(),
        TitleBarBarRole());
    Style bar = ui_unpack_style(bar_frame.value);

    ui_draw_material(paint.bounds, (Rectangle){0}, bar.background, bar.border,
                     bar.border, bar.radius, bar.border_width, 0.0f, 0.0f,
                     0, bar.focus, 0.0f, bar.opacity, ui_style_fill(bar),
                     bar.material);
    DrawLine((int)paint.divider.x, (int)paint.divider.y,
             (int)(paint.divider.x + paint.divider.width),
             (int)paint.divider.y, bar.border);
}

static TitleBarMetrics
ui_title_bar_metrics(int class_name)
{
    StyleFrame bar = ui_control_style_frame_role_kind(
        (ButtonProps){.tone = ButtonToneNeutral, .emphasis = ButtonEmphasisSoft,
                      .size = ControlSizeMedium, .class_name = class_name},
        ButtonStateNormal, 0, 0, 0, 0, StyleKindTitleBar(),
        TitleBarBarRole());
    StyleFrame title = ui_control_style_frame_role_kind(
        (ButtonProps){.tone = ButtonToneNeutral, .emphasis = ButtonEmphasisSoft,
                      .size = ControlSizeMedium, .class_name = class_name},
        ButtonStateNormal, 0, 0, 0, 0, StyleKindTitleBar(),
        TitleBarTitleRole());
    StyleFrame action = ui_control_style_frame_role_kind(
        (ButtonProps){.tone = ButtonToneNeutral, .emphasis = ButtonEmphasisSoft,
                      .icon_only = true, .class_name = class_name},
        ButtonStateNormal, 0, 0, 0, 0, StyleKindTitleBar(),
        TitleBarActionRole());

    return TitleBarMetricsFor((float)GetScale(), bar, title, action);
}

static int
RenderTitleBarReturnButton(Texture2D return_icon, Rectangle bounds,
                           TitleBarMetrics metrics, int class_name)
{
    IconActionSpec button = {0};

    button.bounds = bounds;
    button.icon = return_icon;
    button.icon_size = metrics.leading_icon_size;
    button.icon_padding = metrics.leading_padding;
    Style normal = ui_unpack_style(ui_control_style_frame_role_kind(
        (ButtonProps){.tone = ButtonToneNeutral, .emphasis = ButtonEmphasisSoft,
                      .icon_only = true, .class_name = class_name},
        ButtonStateNormal, 0, 0, 0, 0, StyleKindTitleBar(),
        TitleBarActionRole()).value);
    Style hover = ui_unpack_style(ui_control_style_frame_role_kind(
        (ButtonProps){.tone = ButtonToneNeutral, .emphasis = ButtonEmphasisSoft,
                      .icon_only = true, .class_name = class_name},
        ButtonStateHover, 0, 0, 0, 0, StyleKindTitleBar(),
        TitleBarActionRole()).value);
    button.icon_color = normal.foreground;
    button.hover_background = hover.background;
    button.radius = 0.50f;
    return RenderIconAction(button);
}

static void
RenderTitleBarCenteredTitle(const char *title, int height,
                            int side_reserved, int class_name)
{
    int font;
    int title_w;
    TitleBarTitlePaint paint;
    TitleBarMetrics metrics = ui_title_bar_metrics(class_name);
    TitleBarLayout layout = TitleBarLayoutFor(ui_view_width, height,
                                              TitleBarReservedHasLeading(
                                                  side_reserved, metrics),
                                              false, 0, 0, metrics);
    int max_w = (int)layout.title_bounds.width;
    Style text = ui_unpack_style(ui_control_style_frame_role_kind(
        (ButtonProps){.tone = ButtonToneNeutral, .emphasis = ButtonEmphasisSoft,
                      .size = ControlSizeMedium, .class_name = class_name},
        ButtonStateNormal, 0, 0, 0, 0, StyleKindTitleBar(),
        TitleBarTitleRole()).value);

    if(title == NULL)
        title = "";
    font = ResolveFont(0, StyleFontValue(text.fields, text.font_size),
                       GetTitleFontSize(title, max_w));
    title_w = TextWidth(title, font);
    while(TitleBarShouldShrinkTitleFont(title_w, max_w, font, Text12)) {
        font = TitleBarShrinkTitleFontStep(title_w, max_w, font, Text12);
        title_w = TextWidth(title, font);
    }
    paint = TitleBarTitlePaintFor(layout, title_w, TextLineHeight(font));
    RenderText(title, paint.x, paint.y, font,
               Fade(text.foreground, text.opacity));
}

int
ui_title_bar_height(void)
{
    StyleFrame bar = ui_control_style_frame_role_kind(
        (ButtonProps){.tone = ButtonToneNeutral, .emphasis = ButtonEmphasisSoft,
                      .size = ControlSizeMedium},
        ButtonStateNormal, 0, 0, 0, 0, StyleKindTitleBar(),
        TitleBarBarRole());
    return TitleBarDefaultHeightFor((float)GetScale(), bar);
}

int
RenderTitleBar(TitleBarProps title_bar)
{
    int clicked = 0;
    TitleBarState state = TitleBarStateFor(
        title_bar.height, ui_title_bar_height(),
        title_bar.has_leading_action != 0, title_bar.has_dropdown != 0);
    int height = state.height;
    TitleBarMetrics metrics = ui_title_bar_metrics(title_bar.class_name);
    TitleBarLayout layout;
    int side_reserved = metrics.side_margin;

    layout = TitleBarLayoutFor(ui_view_width, height, state.has_leading,
                               state.has_dropdown,
                               title_bar.dropdown.height,
                               title_bar.dropdown.min_width,
                               metrics);
    RenderTitleBarBackground(height, title_bar.class_name);
    if(state.has_leading) {
        clicked = RenderTitleBarReturnButton(title_bar.leading_icon,
                                             layout.leading_bounds, metrics,
                                             title_bar.class_name);
        side_reserved = layout.side_reserved;
    }
    if(state.has_dropdown) {
        TitleBarDropdown dropdown = title_bar.dropdown;
        if(!dropdown.disabled)
            Dropdown((DropdownProps){.id = dropdown.id, .bounds = layout.dropdown_bounds,
                .options = dropdown.options, .option_count = dropdown.option_count, .selected_index = dropdown.selected_index});
        return clicked;
    }
    RenderTitleBarCenteredTitle(title_bar.title, height, side_reserved,
                                title_bar.class_name);
    return clicked;
}
