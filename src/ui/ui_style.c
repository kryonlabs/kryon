#include "ui_internal.h"
#include "theme.h"

static UIStyleTokens g_ui_style_override;
static int g_ui_style_override_enabled = 0;

UIStyleTokens
GetUIStyleTokensForThemeStyle(ThemeStyle style)
{
    if(style == THEME_STYLE_SYSTEM)
        style = GetDefaultPlatformThemeStyle();

    switch(style) {
    case THEME_STYLE_RETRO:
        return (UIStyleTokens){
            .control_radius = 0.06f,
            .panel_radius = 0.0f,
            .control_alpha = 255,
            .panel_alpha = 255,
            .border_alpha = 255,
            .shadow_alpha = 0,
            .shine_alpha = 0,
            .bevel_enabled = 1,
            .touch_target_min = 36,
            .shadow_offset_y = 0
        };
    case THEME_STYLE_MATERIAL:
        return (UIStyleTokens){
            .control_radius = 0.50f,
            .panel_radius = 0.30f,
            .control_alpha = 255,
            .panel_alpha = 255,
            .border_alpha = 255,
            .shadow_alpha = 36,
            .shine_alpha = 0,
            .bevel_enabled = 0,
            .touch_target_min = 48,
            .shadow_offset_y = 2
        };
    case THEME_STYLE_FLUENT:
        return (UIStyleTokens){
            .control_radius = 0.16f,
            .panel_radius = 0.16f,
            .control_alpha = 246,
            .panel_alpha = 248,
            .border_alpha = 220,
            .shadow_alpha = 32,
            .shine_alpha = 92,
            .bevel_enabled = 0,
            .touch_target_min = 36,
            .shadow_offset_y = 1
        };
    case THEME_STYLE_ADWAITA:
        return (UIStyleTokens){
            .control_radius = 0.10f,
            .panel_radius = 0.12f,
            .control_alpha = 255,
            .panel_alpha = 255,
            .border_alpha = 255,
            .shadow_alpha = 0,
            .shine_alpha = 0,
            .bevel_enabled = 0,
            .touch_target_min = 40,
            .shadow_offset_y = 0
        };
    case THEME_STYLE_LIQUID_GLASS:
        return (UIStyleTokens){
            .control_radius = 0.45f,
            .panel_radius = 0.36f,
            .control_alpha = 150,
            .panel_alpha = 188,
            .border_alpha = 190,
            .shadow_alpha = 56,
            .shine_alpha = 132,
            .bevel_enabled = 0,
            .touch_target_min = 40,
            .shadow_offset_y = 2
        };
    case THEME_STYLE_AERO:
    case THEME_STYLE_SYSTEM:
    default:
        return (UIStyleTokens){
            .control_radius = 0.20f,
            .panel_radius = 0.18f,
            .control_alpha = 188,
            .panel_alpha = 148,
            .border_alpha = 220,
            .shadow_alpha = 82,
            .shine_alpha = 174,
            .bevel_enabled = 0,
            .touch_target_min = 36,
            .shadow_offset_y = 4
        };
    }
}

UIStyleTokens
GetUIStyleTokens(void)
{
    if(g_ui_style_override_enabled)
        return g_ui_style_override;
    return GetUIStyleTokensForThemeStyle(GetEffectiveThemeStyle());
}

void
SetUIStyleTokens(UIStyleTokens tokens)
{
    g_ui_style_override = tokens;
    g_ui_style_override_enabled = 1;
}

void
ClearUIStyleTokensOverride(void)
{
    memset(&g_ui_style_override, 0, sizeof(g_ui_style_override));
    g_ui_style_override_enabled = 0;
}

int
ui_retro_style(void)
{
    return GetUIStyleTokens().bevel_enabled != 0;
}

int
ui_modern_style(void)
{
    return !ui_retro_style();
}

float
ui_control_radius(float classic_radius)
{
    UIStyleTokens tokens = GetUIStyleTokens();
    return tokens.bevel_enabled ? classic_radius : tokens.control_radius;
}

int
ui_control_bevel_enabled(void)
{
    return GetUIStyleTokens().bevel_enabled != 0;
}

int
ui_touch_target_min(void)
{
    return ScaleUIPx(GetUIStyleTokens().touch_target_min);
}

Color
ui_alpha(Color color, unsigned char alpha)
{
    color.a = alpha;
    return color;
}

int
ui_material_style(void)
{
    return GetEffectiveThemeStyle() == THEME_STYLE_MATERIAL;
}

static int
ui_aero_style(void)
{
    return GetEffectiveThemeStyle() == THEME_STYLE_AERO;
}

static int
ui_color_luminance(Color color)
{
    return ((int)color.r * 299 + (int)color.g * 587 + (int)color.b * 114) / 1000;
}

Color
ui_material_on_color(Color color)
{
    return ui_color_luminance(color) < 128 ? RAYWHITE : (Color){0x1D, 0x1B, 0x20, 0xFF};
}

Color
ui_material_surface_container(void)
{
    return GetEffectiveThemeDarkMode() ? LightenUIColor(c_bg, 10) : DarkenUIColor(c_bg, 4);
}

Color
ui_material_outline(void)
{
    return GetEffectiveThemeDarkMode() ? LightenUIColor(c_bg, 42) : DarkenUIColor(c_bg, 44);
}

void
ui_material_state_layer(Rectangle bounds, Color on_color,
                        int hovered, int focused, int pressed)
{
    Color layer = on_color;

    if(pressed)
        layer.a = 31;
    else if(focused)
        layer.a = 31;
    else if(hovered)
        layer.a = 20;
    else
        return;
    DrawRectangleRounded(bounds, 0.50f, 12, layer);
}

void
ui_material_focus(Rectangle bounds)
{
    Color outline = c_circle;

    outline.a = 220;
    DrawRectangleRoundedLines((Rectangle){bounds.x - ScaleUIPx(2),
                                          bounds.y - ScaleUIPx(2),
                                          bounds.width + ScaleUIPx(4),
                                          bounds.height + ScaleUIPx(4)},
                              0.50f, 12, outline);
}

void
ui_draw_control_background(Rectangle bounds, Color background, Color border,
                           float classic_radius)
{
    UIStyleTokens tokens = GetUIStyleTokens();
    float radius = tokens.bevel_enabled ? classic_radius : tokens.control_radius;

    if(tokens.bevel_enabled) {
        if(classic_radius <= 0.0f) {
            DrawRectangleRec(bounds, background);
            DrawRectangleLinesEx(bounds, 1, border);
        } else {
            DrawRectangleRounded(bounds, classic_radius, 8, background);
            DrawRectangleRoundedLines(bounds, classic_radius, 8, border);
        }
        return;
    }

    if(classic_radius > 0.0f)
        radius = classic_radius;

    if(ui_aero_style()) {
        Color shadow = BLACK;
        Color glass = LightenUIColor(background, GetEffectiveThemeDarkMode() ? 16 : 28);
        Color inner = c_circle;
        Color top = WHITE;
        Color bottom = GetEffectiveThemeDarkMode() ? LightenUIColor(c_bg, 30) :
                                                     DarkenUIColor(c_bg, 12);
        int inset = ScaleUIPx(2);
        int top_h = (int)(bounds.height * 0.48f);

        shadow.a = tokens.shadow_alpha;
        DrawRectangleRounded((Rectangle){bounds.x,
                                         bounds.y + ScaleUIPx(tokens.shadow_offset_y),
                                         bounds.width, bounds.height},
                             radius, 12, shadow);

        if(tokens.control_alpha < glass.a)
            glass.a = tokens.control_alpha;
        if(tokens.border_alpha < border.a)
            border.a = tokens.border_alpha;
        DrawRectangleRounded(bounds, radius, 12, glass);

        inner.a = 52;
        if(bounds.width > (float)(inset * 2) &&
           bounds.height > (float)(inset * 2))
            DrawRectangleRounded((Rectangle){bounds.x + (float)inset,
                                             bounds.y + (float)inset,
                                             bounds.width - (float)(inset * 2),
                                             bounds.height - (float)(inset * 2)},
                                 radius, 12, inner);

        top.a = tokens.shine_alpha;
        if(top_h > ScaleUIPx(3))
            DrawRectangleRounded((Rectangle){bounds.x + (float)inset,
                                             bounds.y + (float)ScaleUIPx(1),
                                             bounds.width - (float)(inset * 2),
                                             (float)top_h},
                                 radius, 12, top);

        bottom.a = 48;
        if(bounds.height > (float)ScaleUIPx(8))
            DrawRectangleRounded((Rectangle){bounds.x + (float)inset,
                                             bounds.y + bounds.height * 0.56f,
                                             bounds.width - (float)(inset * 2),
                                             bounds.height * 0.40f},
                                 radius, 12, bottom);

        if(border.a != 0)
            DrawRectangleRoundedLines(bounds, radius, 12, border);
        return;
    }

    if(tokens.shadow_alpha > 0 && tokens.shadow_offset_y > 0) {
        Color shadow = DarkenUIColor(c_bg, 35);
        shadow.a = tokens.shadow_alpha;
        DrawRectangleRounded((Rectangle){bounds.x,
                                         bounds.y + ScaleUIPx(tokens.shadow_offset_y),
                                         bounds.width, bounds.height},
                             radius, 12, shadow);
    }

    if(tokens.control_alpha < background.a)
        background.a = tokens.control_alpha;
    if(tokens.border_alpha < border.a)
        border.a = tokens.border_alpha;
    DrawRectangleRounded(bounds, radius, 12, background);
    if(border.a != 0)
        DrawRectangleRoundedLines(bounds, radius, 12, border);
    if(tokens.shine_alpha > 0) {
        Color shine = WHITE;
        shine.a = tokens.shine_alpha;
        int inset = ScaleUIPx(2);
        int shine_h = ScaleUIPx(3);
        if(bounds.width > (float)(inset * 2) && bounds.height > (float)(shine_h + inset))
            DrawRectangleRounded((Rectangle){bounds.x + (float)inset,
                                             bounds.y + ScaleUIPx(1),
                                             bounds.width - (float)(inset * 2),
                                             (float)shine_h},
                                 radius, 8, shine);
    }
}

void
ui_draw_box_background(Rectangle bounds, float radius, Color background,
                       Color border)
{
    if(ui_modern_style()) {
        ui_draw_control_background(bounds, background, border, radius);
        return;
    }
    if(radius <= 0.0f) {
        DrawRectangleRec(bounds, background);
        DrawRectangleLinesEx(bounds, 1, border);
    } else {
        DrawRectangleRounded(bounds, radius, 8, background);
        DrawRectangleRoundedLines(bounds, radius, 8, border);
    }
}
