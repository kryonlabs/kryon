#include "ui_widgets.h"

#include "kryon_compat.generated.h"  /* DrawRectangleRec, DrawRectangleLinesEx, DrawLine, BLANK, Rectangle */
#include "ui_tree.h"
#include "ui_internal.h"

void
WidgetText(const char *label, int x, int y, int font_size, Color color)
{
    UITextNode(label, x, y, font_size, color);
}

void
WidgetRect(int x, int y, int w, int h, Color fill, Color border)
{
    UIRectNode(x, y, w, h, fill, border);
}

void
WidgetLine(int x1, int y1, int x2, int y2, Color color)
{
    UILineNode(x1, y1, x2, y2, color);
}

void
WidgetBackground(Color color)
{
    UIBackground(color);
}

int
WidgetButton(int x, int y, int w, int h, const char *label,
             UIButtonStyle style)
{
    Color bg = GetThemeButton();
    Color hover = GetThemeButtonHover();

    if(style == UI_BUTTON_STYLE_SECONDARY) {
        bg = GetThemeSurface();
        hover = LightenUIColor(GetThemeSurface(), 12);
    } else if(style == UI_BUTTON_STYLE_DANGER) {
        bg = (Color){210, 58, 58, 255};
        hover = LightenUIColor(bg, 12);
    }

    return UIButtonNode((UIButton){{x, y, w, h}, label, GetUIFontSize(), 0, 0,
                                   bg, hover, GetThemeText(),
                                   DarkenUIColor(bg, 35), 0.06f});
}

void
WidgetSprite(const char *asset_path, int x, int y, int w, int h)
{
    WidgetSpriteEx((UISprite){
        asset_path,
        (Rectangle){x, y, w, h},
        (Rectangle){0, 0, 0, 0},
        (Vector2){0, 0},
        0.0f,
        WHITE,
        UI_SPRITE_FIT_CONTAIN
    });
}

void
WidgetSpriteEx(UISprite sprite)
{
    UISpriteNode(sprite);
}

int
WidgetTabBar(int x, int y, int w, int h, const UITab *tabs, int count,
             int *selected_index)
{
    int selected = selected_index != NULL ? *selected_index : 0;
    int clicked;

    if(h <= 0)
        h = ui_tab_bar_height();

    clicked = UITabBarNode((UITabBar){
        .bounds = {x, y, w, h},
        .tabs = tabs,
        .count = count,
        .selected_index = selected,
        .font = GetUIFontSize(),
        .focus_selected = 1
    });
    if(clicked >= 0 && selected_index != NULL)
        *selected_index = clicked;

    return clicked;
}
