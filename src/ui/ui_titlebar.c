#include "ui_internal.h"

/* Screen header (title bar) widgets. These were split out of modal.c so that
 * modal.c holds only modal/dialog code. The public declarations live in
 * ui_modal.h, which kryon.h includes. */

static void
UIRenderTitleBarBackground(int height)
{
    Color bar = DarkenUIColor(c_bg, 14);
    if(ui_modern_style()) {
        UIStyleTokens tokens = GetUIStyleTokens();
        if(tokens.panel_alpha < bar.a)
            bar.a = tokens.panel_alpha;
    }
    DrawRectangle(0, 0, ui_view_width, height, bar);
    if(ui_modern_style() && GetUIStyleTokens().shine_alpha > 0) {
        Color shine = WHITE;
        shine.a = GetUIStyleTokens().shine_alpha;
        DrawRectangle(0, 0, ui_view_width, ScaleUIPx(1), shine);
    }
    DrawLine(0, height - 1, ui_view_width, height - 1,
             DarkenUIColor(c_bg, 42));
}

static int
UIRenderTitleBarReturnButton(Texture2D return_icon, int height)
{
    int icon_size = ScaleUIPx(18);
    int padding = ScaleUIPx(5);
    int button_size = icon_size + padding * 2;
    int x = ScaleUIPx(4);
    int y = (height - button_size) / 2;
    int hover = 0;

    if(y < 0)
        y = 0;
    return UIRenderPaddedIconBtn(x, y, icon_size, padding, return_icon, &hover);
}

static void
UIRenderTitleBarCenteredTitle(const char *title, int height,
                                       int side_reserved)
{
    int font = GetUIFontSize();
    int title_w;
    int max_w = ui_view_width - side_reserved * 2;

    if(title == NULL)
        title = "";
    if(max_w < ScaleUIPx(48))
        max_w = ui_view_width - ScaleUIPx(16);
    title_w = MeasureUIText(title, font);
    while(font > ScaleUIPx(12) && title_w > max_w) {
        font--;
        title_w = MeasureUIText(title, font);
    }
    UIRenderText(title, (ui_view_width - title_w) / 2,
                    GetUIControlTextY(title, 0, height, font),
                    font, c_text);
}

int
GetUITitleBarHeight(void)
{
    return GetUITabBarHeight();
}

void
UIRenderTitleBar(const char *title, int height)
{
    UIRenderTitleBarBackground(height);
    UIRenderTitleBarCenteredTitle(title, height, ScaleUIPx(12));
}

int
UIRenderReturnTitleBar(Texture2D return_icon, const char *title,
                          int height)
{
    int clicked;

    UIRenderTitleBarBackground(height);
    clicked = UIRenderTitleBarReturnButton(return_icon, height);
    UIRenderTitleBarCenteredTitle(title, height, ScaleUIPx(56));
    return clicked;
}

int
UIRenderReturnDropdownTitleBar(Texture2D return_icon,
                                   UITitleBarDropdown dropdown,
                                   int height)
{
    int icon_size = ScaleUIPx(18);
    int icon_padding = ScaleUIPx(5);
    int back_w = icon_size + icon_padding * 2;
    int gap = ScaleUIPx(4);
    int dropdown_x = ScaleUIPx(4) + back_w + gap;
    int dropdown_h = dropdown.height > 0 ? dropdown.height : ScaleUIPx(32);
    int dropdown_y = (height - dropdown_h) / 2;
    int dropdown_w = ui_view_width - dropdown_x - ScaleUIPx(4);
    int clicked;

    if(dropdown_y < 0)
        dropdown_y = 0;
    if(dropdown.min_width > 0 && dropdown_w < dropdown.min_width)
        dropdown_w = ui_view_width - dropdown_x;
    if(dropdown_w < 1)
        dropdown_w = 1;

    UIRenderTitleBarBackground(height);
    clicked = UIRenderTitleBarReturnButton(return_icon, height);
    if(!dropdown.disabled)
        UIRenderDropdown(dropdown.id, dropdown_x, dropdown_y,
                       dropdown_w, dropdown_h,
                       dropdown.options, dropdown.option_count,
                       dropdown.selected_index);
    return clicked;
}
