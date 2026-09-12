#include "ui_internal.h"
#include "runtime/title_bar.h"

/* Screen header (title bar) widgets. These were split out of modal.c so that
 * modal.c holds only modal/dialog code. The public declarations live in
 * ui_modal.h, which kryon.h includes. */

static void
RenderTitleBarBackground(int height)
{
    Color top = DarkenColor(c_bg, 8);
    Color bottom = c_bg;
    Color divider = GetThemeText();
    ThemeMetrics tokens = GetThemeMetrics();

    if(ui_default_style()) {
        top = ui_default_surface_container();
        bottom = c_bg;
    }
    if(tokens.title_bar_alpha < top.a)
        top.a = tokens.title_bar_alpha;
    if(tokens.title_bar_alpha < bottom.a)
        bottom.a = tokens.title_bar_alpha;
    DrawRectangleGradientV(0, 0, ui_view_width, height, top, bottom);
    if(ui_modern_style() && GetThemeMetrics().shine_alpha > 0) {
        Color shine = WHITE;
        shine.a = GetThemeMetrics().shine_alpha;
        DrawRectangle(0, 0, ui_view_width, Scale(1), shine);
    }
    divider.a = 34;
    DrawLine(0, height - 1, ui_view_width, height - 1, divider);
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
    button.icon_color = GetThemeText();
    button.hover_background = Fade(GetThemeText(), 0.12f);
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
    RenderText(title, TitleBarTitleX(ui_view_width, title_w),
                    GetUIControlTextY(title, 0, height, font),
                    font, c_text);
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
