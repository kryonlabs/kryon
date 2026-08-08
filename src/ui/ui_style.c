#include "ui_internal.h"
#include "theme.h"

static UIStyleTokens g_ui_style_override;
static int g_ui_style_override_enabled = 0;

typedef struct {
    unsigned int key;
    Vector2 origin;
    float age;
    int active;
    unsigned long frame_seen;
} UIMaterialRipple;

#define UI_MATERIAL_RIPPLE_MAX 64

static UIMaterialRipple g_material_ripples[UI_MATERIAL_RIPPLE_MAX];

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
    case THEME_STYLE_SYSTEM:
    default:
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
ui_color_luminance(Color color)
{
    return ((int)color.r * 299 + (int)color.g * 587 + (int)color.b * 114) / 1000;
}

Color
ui_material_on_color(Color color)
{
    return ui_color_luminance(color) < 128 ? RAYWHITE : (Color){0x1D, 0x1B, 0x20, 0xFF};
}

static Color
ui_material_tone(Color base, int light_delta, int dark_delta)
{
    return GetEffectiveThemeDarkMode() ? LightenUIColor(base, dark_delta) :
                                         DarkenUIColor(base, light_delta);
}

UIMaterialScheme
ui_material_scheme(void)
{
    UIMaterialScheme scheme;
    Color disabled = c_text;

    scheme.primary = c_circle;
    scheme.on_primary = ui_material_on_color(scheme.primary);
    scheme.secondary = c_button;
    scheme.on_secondary = ui_material_on_color(scheme.secondary);
    scheme.surface = c_surface.a != 0 ? c_surface : c_bg;
    scheme.on_surface = c_text;
    scheme.surface_container = ui_material_tone(c_bg, 4, 10);
    scheme.surface_variant = ui_material_tone(c_bg, 10, 18);
    scheme.on_surface_variant = ui_material_tone(c_text, 34, 28);
    scheme.outline = ui_material_tone(c_bg, 44, 42);
    scheme.error = GetEffectiveThemeDarkMode()
                       ? (Color){0xF2, 0xB8, 0xB5, 0xFF}
                       : (Color){0xBA, 0x1A, 0x1A, 0xFF};
    scheme.on_error = ui_material_on_color(scheme.error);
    scheme.disabled_container = ui_material_tone(c_bg, 14, 14);
    scheme.disabled_container.a = 96;
    disabled.a = 96;
    scheme.disabled_content = disabled;
    return scheme;
}

UIMaterialScheme
GetUIMaterialScheme(void)
{
    return ui_material_scheme();
}

Color
ui_material_surface_container(void)
{
    return ui_material_scheme().surface_container;
}

Color
ui_material_surface_variant(void)
{
    return ui_material_scheme().surface_variant;
}

Color
ui_material_outline(void)
{
    return ui_material_scheme().outline;
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
ui_material_elevation(Rectangle bounds, float radius, int level)
{
    Color shadow;
    int y1;
    int y2;

    if(level <= 0)
        return;
    if(level > 4)
        level = 4;

    shadow = BLACK;
    shadow.a = (unsigned char)(18 + level * 6);
    y1 = ScaleUIPx(level);
    y2 = ScaleUIPx(level * 2);
    DrawRectangleRounded((Rectangle){bounds.x, bounds.y + (float)y2,
                                     bounds.width, bounds.height},
                         radius, 12, shadow);
    shadow.a = (unsigned char)(10 + level * 4);
    DrawRectangleRounded((Rectangle){bounds.x, bounds.y + (float)y1,
                                     bounds.width, bounds.height},
                         radius, 12, shadow);
}

void
ui_material_ripple(Rectangle bounds, Color on_color, int key, int pressed)
{
    UIMaterialRipple *ripple;
    Vector2 mouse;
    float dt;
    float max_radius;
    float radius;
    Color color = on_color;
    unsigned int hash = (unsigned int)key * 2654435761u;

    if(key == 0)
        return;
    ripple = &g_material_ripples[hash % UI_MATERIAL_RIPPLE_MAX];
    if(ripple->key != hash || g_ui_frame_serial - ripple->frame_seen > 20) {
        memset(ripple, 0, sizeof(*ripple));
        ripple->key = hash;
    }
    ripple->frame_seen = g_ui_frame_serial;

    mouse = ui_mouse_world();
    if(pressed && !ripple->active) {
        ripple->origin = CheckCollisionPointRec(mouse, bounds)
                             ? mouse
                             : (Vector2){bounds.x + bounds.width * 0.5f,
                                         bounds.y + bounds.height * 0.5f};
        ripple->age = 0.0f;
        ripple->active = 1;
    }
    if(!ripple->active)
        return;

    dt = GetFrameTime();
    if(dt <= 0.0f || dt > 0.1f)
        dt = 1.0f / 60.0f;
    ripple->age += dt;
    if(!pressed && ripple->age > 0.32f) {
        ripple->active = 0;
        return;
    }

    max_radius = sqrtf(bounds.width * bounds.width + bounds.height * bounds.height);
    radius = max_radius * (ripple->age / 0.32f);
    if(radius < ScaleUIPx(8))
        radius = (float)ScaleUIPx(8);
    if(radius > max_radius)
        radius = max_radius;
    color.a = pressed ? 28 : (unsigned char)(28.0f * (1.0f - ripple->age / 0.32f));
    DrawCircleV(ripple->origin, radius, color);
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

    if(ui_material_style()) {
        ui_material_elevation(bounds, radius, tokens.shadow_offset_y);
    } else if(tokens.shadow_alpha > 0 && tokens.shadow_offset_y > 0) {
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
