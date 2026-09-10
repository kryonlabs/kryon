#include "ui_internal.h"

/* Screen header (title bar) widgets. These were split out of modal.c so that
 * modal.c holds only modal/dialog code. The public declarations live in
 * ui_modal.h, which kryon.h includes. */

static void
RenderTitleBarBackground(int height)
{
    Color top = DarkenUIColor(c_bg, 8);
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
RenderTitleBarReturnButton(Texture2D return_icon, int height)
{
    int icon_size = Scale(22);
    int padding = Scale(13);
    int button_size = icon_size + padding * 2;
    int x = Scale(12);
    int y = (height - button_size) / 2;
    IconButtonProps button = {0};

    if(y < 0)
        y = 0;
    button.bounds = (Rectangle){(float)x, (float)y,
                                (float)button_size, (float)button_size};
    button.icon = return_icon;
    button.icon_size = icon_size;
    button.icon_padding = padding;
    button.icon_color = GetThemeText();
    button.hover_background = Fade(GetThemeText(), 0.12f);
    button.radius = 0.50f;
    return RenderIconButton(button);
}

static void
RenderTitleBarCenteredTitle(const char *title, int height,
                                       int side_reserved)
{
    int font;
    int title_w;
    int max_w = ui_view_width - side_reserved * 2;

    if(title == NULL)
        title = "";
    if(max_w < Scale(48))
        max_w = ui_view_width - Scale(16);
    font = GetTitleFontSize(title, max_w);
    title_w = TextWidth(title, font);
    while(font > Text12 && title_w > max_w) {
        font--;
        title_w = TextWidth(title, font);
    }
    RenderText(title, (ui_view_width - title_w) / 2,
                    GetUIControlTextY(title, 0, height, font),
                    font, c_text);
}

int
ui_title_bar_height(void)
{
    return ui_tab_bar_height();
}

void
RenderTitleBar(const char *title, int height)
{
    RenderTitleBarBackground(height);
    RenderTitleBarCenteredTitle(title, height, Scale(12));
}

int
RenderReturnTitleBar(Texture2D return_icon, const char *title,
                          int height)
{
    int clicked;

    RenderTitleBarBackground(height);
    clicked = RenderTitleBarReturnButton(return_icon, height);
    RenderTitleBarCenteredTitle(title, height, Scale(68));
    return clicked;
}

int
RenderReturnDropdownTitleBar(Texture2D return_icon,
                                   UITitleBarDropdown dropdown,
                                   int height)
{
    int gap = Scale(4);
    int dropdown_x = Scale(12) + Scale(48) + gap;
    int dropdown_h = dropdown.height > 0 ? dropdown.height : Scale(32);
    int dropdown_y = (height - dropdown_h) / 2;
    int dropdown_w = ui_view_width - dropdown_x - Scale(12);
    int clicked;

    if(dropdown_y < 0)
        dropdown_y = 0;
    if(dropdown.min_width > 0 && dropdown_w < dropdown.min_width)
        dropdown_w = ui_view_width - dropdown_x;
    if(dropdown_w < 1)
        dropdown_w = 1;

    RenderTitleBarBackground(height);
    clicked = RenderTitleBarReturnButton(return_icon, height);
    if(!dropdown.disabled)
        Combobox((ComboboxProps){.id = dropdown.id, .bounds = {dropdown_x, dropdown_y, dropdown_w, dropdown_h},
            .options = dropdown.options, .option_count = dropdown.option_count, .selected_index = dropdown.selected_index});
    return clicked;
}
