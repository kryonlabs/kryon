#include "ui_internal.h"
#include "ui_style_internal.h"
#include "ui_paint_internal.h"
#include "theme.h"
#include "runtime/theme.h"
#include "runtime/style.h"
#include <math.h>

/* Execute the portable layer description generated from runtime/surface.kry. */

Rectangle
ui_draw_material(Rectangle bounds, Rectangle surface_bounds, Color background, Color border,
                          Color light, float radius, float border_width,
                          float hover, float press, int disabled,
                          Color focus, float focused, float opacity,
                          FillStates fill_states, MaterialKind material)
{
    MaterialPaint paint = {
        .bounds = bounds, .surface = surface_bounds,
        .value = {.background = ColorToInt(background), .border = ColorToInt(border),
                  .focus = ColorToInt(focus), .radius = radius,
                  .border_width = border_width, .opacity = opacity, .material = material},
        .light = ColorToInt(light), .ambient = ColorToInt(GetThemeSurface()),
        .hover = hover, .press = press, .focus = focused, .disabled = disabled,
        .fill = fill_states, .fill_valid = true, .scale = (float)Scale(1000) / 1000.0f
    };
    paint = PrepareMaterial(paint);
    for(int i = 0; i < MaterialLayerCount(material); i++)
        ui_draw_surface(PaintMaterialLayer(paint, i));
    return MaterialContentBounds(paint);
}

static ThemeMetrics g_ui_style_override;
static int g_ui_style_override_enabled = 0;

#if !defined(KRYON_BACKEND_TERMI)
typedef struct {
    unsigned int key;
    Vector2 origin;
    float age;
    int active;
    unsigned long frame_seen;
} UIDefaultRipple;

#define UI_DEFAULT_RIPPLE_MAX 64

static UIDefaultRipple g_default_ripples[UI_DEFAULT_RIPPLE_MAX];
#endif

ThemeMetrics
GetThemeMetricsForThemeStyle(ThemeStyle style)
{
    ThemeMetrics tokens;

    if(style == THEME_STYLE_SYSTEM)
        style = GetDefaultPlatformThemeStyle();

    memset(&tokens, 0, sizeof(tokens));
    Metrics defaults = DefaultMetrics();
    tokens.radius_small = defaults.radius_small;
    tokens.radius_medium = defaults.radius_medium;
    tokens.radius_large = defaults.radius_large;
    tokens.radius_pill = defaults.radius_pill;
    tokens.border_width = defaults.border_width;
    tokens.focus_width = defaults.focus_width;
    tokens.focus_gap = defaults.focus_gap;
    tokens.space_1 = defaults.space_1;
    tokens.space_2 = defaults.space_2;
    tokens.space_3 = defaults.space_3;
    tokens.space_4 = defaults.space_4;
    tokens.space_5 = defaults.space_5;
    tokens.space_6 = defaults.space_6;
    tokens.control_height_small = defaults.control_height_small;
    tokens.control_height_medium = defaults.control_height_medium;
    tokens.control_height_large = defaults.control_height_large;
    tokens.control_padding_small = defaults.control_padding_small;
    tokens.control_padding_medium = defaults.control_padding_medium;
    tokens.control_padding_large = defaults.control_padding_large;
    tokens.control_gap = defaults.control_gap;
    tokens.font_size_small = defaults.font_size_small;
    tokens.font_size_medium = defaults.font_size_medium;
    tokens.font_size_large = defaults.font_size_large;
    tokens.icon_size_small = defaults.icon_size_small;
    tokens.icon_size_medium = defaults.icon_size_medium;
    tokens.icon_size_large = defaults.icon_size_large;
    tokens.shadow_offset_y = defaults.shadow_offset_y;
    tokens.shadow_blur = defaults.shadow_blur;
    tokens.disabled_opacity = defaults.disabled_opacity;
    tokens.transition_fast_ms = defaults.transition_fast_ms;
    tokens.transition_normal_ms = defaults.transition_normal_ms;

    /* Field-wise assembly (rather than a designated compound literal) so
     * the same source builds with the strict native Plan 9 compiler. */
    tokens.control_radius = 2.0f;
    tokens.panel_radius = 0.0f;
    tokens.control_alpha = 255;
    tokens.panel_alpha = 255;
    tokens.title_bar_alpha = 255;
    tokens.border_alpha = 255;
    tokens.shadow_alpha = 0;
    tokens.shine_alpha = 0;
    tokens.bevel_enabled = 1;
    tokens.touch_target_min = 36;
    tokens.shadow_offset_y = 0;
    if(style != THEME_STYLE_CLASSIC) {
        tokens.control_radius = 4.0f;
        tokens.panel_radius = 8.0f;
        tokens.control_alpha = 222;
        tokens.panel_alpha = 235;
        tokens.title_bar_alpha = 222;
        tokens.border_alpha = 118;
        tokens.shadow_alpha = 28;
        tokens.shine_alpha = 0;
        tokens.bevel_enabled = 0;
        tokens.touch_target_min = 48;
        tokens.shadow_offset_y = 2;
    }
    return tokens;
}

ThemeMetrics
GetThemeMetrics(void)
{
    if(g_ui_style_override_enabled)
        return g_ui_style_override;
    return GetThemeMetricsForThemeStyle(GetEffectiveThemeStyle());
}

void
SetThemeMetrics(ThemeMetrics tokens)
{
    g_ui_style_override = tokens;
    g_ui_style_override_enabled = 1;
}

void
ClearThemeMetricsOverride(void)
{
    memset(&g_ui_style_override, 0, sizeof(g_ui_style_override));
    g_ui_style_override_enabled = 0;
}

int
ui_classic_style(void)
{
    return GetThemeMetrics().bevel_enabled != 0;
}

int
ui_modern_style(void)
{
    return !ui_classic_style();
}

static StyleData
pack_style(Style value)
{
    return (StyleData){
        .fields = value.fields,
        .material = value.material,
        .typeface = StringView(value.typeface, value.typeface ? strlen(value.typeface) : 0),
        .background = ColorToInt(value.background),
        .background_end = ColorToInt(value.background_end),
        .foreground = ColorToInt(value.foreground),
        .border = ColorToInt(value.border),
        .focus = ColorToInt(value.focus),
        .radius = value.radius,
        .border_width = value.border_width,
        .opacity = value.opacity,
        .padding_x = value.padding_x,
        .padding_y = value.padding_y,
        .gap = value.gap,
        .font_size = value.font_size,
        .icon_size = value.icon_size,
        .offset_x = value.content_offset.x,
        .offset_y = value.content_offset.y
    };
}

Style
ui_unpack_style(StyleData value)
{
    return (Style){
        .material = (MaterialKind)value.material,
        .typeface = value.typeface.data,
        .fields = value.fields,
        .background = GetColor(value.background),
        .background_end = GetColor(value.background_end),
        .foreground = GetColor(value.foreground),
        .border = GetColor(value.border),
        .focus = GetColor(value.focus),
        .radius = value.radius,
        .border_width = value.border_width,
        .opacity = value.opacity,
        .padding_x = value.padding_x,
        .padding_y = value.padding_y,
        .gap = value.gap,
        .font_size = value.font_size,
        .icon_size = value.icon_size,
        .content_offset = {value.offset_x, value.offset_y}
    };
}

Style
MergeStyle(Style base, Style overrides)
{
    return ui_unpack_style(MergeValues(pack_style(base), pack_style(overrides)));
}

Style
ui_style_transition(Style resolved, Style normal, Style hover,
                    Style press, Style focus, float h, float p, float f,
                    FillStates *fill)
{
    StyleFrame frame = TransitionFrame(pack_style(resolved), pack_style(normal),
        pack_style(hover), pack_style(press), pack_style(focus), h, p, f);
    *fill = frame.fill;
    return ui_unpack_style(frame.value);
}

FillStates
ui_style_fill(Style value)
{
    return FillState(value.fields, ColorToInt(value.background),
        ColorToInt(value.background_end));
}

StyleStates
ui_pack_style_states(ControlStyle control)
{
    StyleStates states = {
        .normal = pack_style(control.normal),
        .hover = pack_style(control.hover),
        .pressed = pack_style(control.pressed),
        .focused = pack_style(control.focused),
        .disabled = pack_style(control.disabled),
        .loading = pack_style(control.loading),
        .selected = pack_style(control.selected)
    };
    return states;
}

Style
ResolveControlStyle(Style base, ControlStyle control, ButtonState state)
{
    return ui_unpack_style(ResolveValues(pack_style(base), ui_pack_style_states(control), state));
}

float
ui_radius_px(Rectangle bounds, float radius_px)
{
    float min_side;
    float radius;

    if(radius_px <= 0.0f)
        return 0.0f;
    min_side = bounds.height < bounds.width ? bounds.height : bounds.width;
    if(min_side <= 0.0f)
        return 0.0f;
    radius = (float)Scale(radius_px) / min_side;
    return radius > 0.5f ? 0.5f : radius;
}

int
ui_control_bevel_enabled(void)
{
    return GetThemeMetrics().bevel_enabled != 0;
}

int
ui_touch_target_min(void)
{
    return Scale(GetThemeMetrics().touch_target_min);
}

Color
ui_alpha(Color color, unsigned char alpha)
{
    color.a = alpha;
    return color;
}

int
ui_default_style(void)
{
    return GetEffectiveThemeStyle() == THEME_STYLE_DEFAULT;
}

static int
ui_color_luminance(Color color)
{
    return ((int)color.r * 299 + (int)color.g * 587 + (int)color.b * 114) / 1000;
}

Color
ui_default_on_color(Color color)
{
    return ui_color_luminance(color) < 128 ? RAYWHITE : (Color){0x1D, 0x1B, 0x20, 0xFF};
}

static Color
ui_default_tone(Color base, int light_delta, int dark_delta)
{
    int delta = GetEffectiveThemeDarkMode() ? dark_delta : -light_delta;
    int r = (int)base.r + delta;
    int g = (int)base.g + delta;
    int b = (int)base.b + delta;

    if(r < 0)
        r = 0;
    if(r > 255)
        r = 255;
    if(g < 0)
        g = 0;
    if(g > 255)
        g = 255;
    if(b < 0)
        b = 0;
    if(b > 255)
        b = 255;

    return (Color){(unsigned char)r, (unsigned char)g, (unsigned char)b, base.a};
}

ThemeScheme
ui_default_scheme(void)
{
    /* Every themed widget asks for the scheme on every frame (the Go
     * bindings fetch it per draw call). The computation is pure given the
     * current theme colors and dark mode, so memoize it on those inputs
     * instead of re-deriving tones and dark-mode queries each time. */
    static ThemeScheme cache;
    static int cache_valid = 0;
    static Color key_bg, key_surface, key_text, key_circle, key_button;
    static int key_dark;
    Color input_surface = c_surface.a != 0 ? c_surface : c_bg;
    int dark = GetEffectiveThemeDarkMode();
    ThemeScheme scheme;
    Color disabled = c_text;

    if(cache_valid && dark == key_dark &&
       key_bg.r == c_bg.r && key_bg.g == c_bg.g && key_bg.b == c_bg.b &&
       key_bg.a == c_bg.a &&
       key_surface.r == input_surface.r && key_surface.g == input_surface.g &&
       key_surface.b == input_surface.b && key_surface.a == input_surface.a &&
       key_text.r == c_text.r && key_text.g == c_text.g &&
       key_text.b == c_text.b && key_text.a == c_text.a &&
       key_circle.r == c_circle.r && key_circle.g == c_circle.g &&
       key_circle.b == c_circle.b && key_circle.a == c_circle.a &&
       key_button.r == c_button.r && key_button.g == c_button.g &&
       key_button.b == c_button.b && key_button.a == c_button.a)
        return cache;

    scheme.primary = c_circle;
    scheme.on_primary = ui_default_on_color(scheme.primary);
    scheme.secondary = c_button;
    scheme.on_secondary = ui_default_on_color(scheme.secondary);
    scheme.surface = input_surface;
    scheme.on_surface = c_text;
    scheme.surface_container = ui_default_tone(c_bg, 4, 10);
    scheme.surface_variant = ui_default_tone(c_bg, 10, 18);
    scheme.on_surface_variant = ui_default_tone(c_text, 34, 28);
    scheme.outline = ui_default_tone(c_bg, 44, 42);
    scheme.error = dark
                       ? (Color){0xF2, 0xB8, 0xB5, 0xFF}
                       : (Color){0xBA, 0x1A, 0x1A, 0xFF};
    scheme.on_error = ui_default_on_color(scheme.error);
    scheme.disabled_container = ui_default_tone(c_bg, 14, 14);
    scheme.disabled_container.a = 96;
    disabled.a = 96;
    scheme.disabled_content = disabled;

    cache = scheme;
    cache_valid = 1;
    key_bg = c_bg;
    key_surface = input_surface;
    key_text = c_text;
    key_circle = c_circle;
    key_button = c_button;
    key_dark = dark;
    return scheme;
}

ThemeScheme
GetThemeScheme(void)
{
    return ui_default_scheme();
}

Color
ui_default_surface_container(void)
{
    return ui_default_scheme().surface_container;
}

Color
ui_default_surface_variant(void)
{
    return ui_default_scheme().surface_variant;
}

Color
ui_default_outline(void)
{
    return ui_default_scheme().outline;
}

void
ui_default_state_layer(Rectangle bounds, Color on_color,
                        int hovered, int focused, int pressed)
{
    Color layer = on_color;
    float radius = ui_radius_px(bounds, GetThemeMetrics().control_radius);

    if(pressed)
        layer.a = 31;
    else if(focused)
        layer.a = 31;
    else if(hovered)
        layer.a = 20;
    else
        return;
    DrawRectangleRounded(bounds, radius, 12, layer);
}

void
ui_default_focus(Rectangle bounds)
{
    Color outline = c_circle;
    Rectangle focus_bounds;
    float radius;

    outline.a = 220;
    focus_bounds = (Rectangle){bounds.x - Scale(2),
                               bounds.y - Scale(2),
                               bounds.width + Scale(4),
                               bounds.height + Scale(4)};
    radius = ui_radius_px(focus_bounds, GetThemeMetrics().control_radius + 2.0f);
    DrawRectangleRoundedLines(focus_bounds, radius, 12, outline);
}

void
ui_default_elevation(Rectangle bounds, float radius, int level)
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
    y1 = Scale(level);
    y2 = Scale(level * 2);
    DrawRectangleRounded((Rectangle){bounds.x, bounds.y + (float)y2,
                                     bounds.width, bounds.height},
                         radius, 12, shadow);
    shadow.a = (unsigned char)(10 + level * 4);
    DrawRectangleRounded((Rectangle){bounds.x, bounds.y + (float)y1,
                                     bounds.width, bounds.height},
                         radius, 12, shadow);
}

void
ui_default_ripple(Rectangle bounds, Color on_color, int key, int pressed)
{
#if defined(KRYON_BACKEND_TERMI)
    (void)bounds;
    (void)on_color;
    (void)key;
    (void)pressed;
    return;
#else
    UIDefaultRipple *ripple;
    Vector2 mouse;
    float dt;
    float max_radius;
    float radius;
    Color color = on_color;
    unsigned int hash = (unsigned int)key * 2654435761u;

    if(key == 0)
        return;
    ripple = &g_default_ripples[hash % UI_DEFAULT_RIPPLE_MAX];
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
    if(radius < Scale(8))
        radius = (float)Scale(8);
    if(radius > max_radius)
        radius = max_radius;
    color.a = pressed ? 28 : (unsigned char)(28.0f * (1.0f - ripple->age / 0.32f));
    DrawCircleV(ripple->origin, radius, color);
#endif
}

void
ui_draw_control_background(Rectangle bounds, Color background, Color border,
                           float classic_radius)
{
    ThemeMetrics tokens = GetThemeMetrics();
    float radius = tokens.bevel_enabled ? classic_radius
                                        : ui_radius_px(bounds, tokens.control_radius);

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

    if(ui_default_style()) {
        ui_default_elevation(bounds, radius, tokens.shadow_offset_y);
    } else if(tokens.shadow_alpha > 0 && tokens.shadow_offset_y > 0) {
        Color shadow = DarkenUIColor(c_bg, 35);
        shadow.a = tokens.shadow_alpha;
        DrawRectangleRounded((Rectangle){bounds.x,
                                         bounds.y + Scale(tokens.shadow_offset_y),
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
    if(tokens.shine_alpha > 0 && radius < 0.45f) {
        Color shine = WHITE;
        shine.a = tokens.shine_alpha;
        int inset = Scale(2);
        int shine_h = Scale(3);
        if(bounds.width > (float)(inset * 2) && bounds.height > (float)(shine_h + inset))
            DrawRectangleRounded((Rectangle){bounds.x + (float)inset,
                                             bounds.y + Scale(1),
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
        /* Text fields use the Default pixel radius. A normalized legacy
         * radius scales with height and turns large text areas into pills. */
        ui_draw_control_background(bounds, background, border, 0.0f);
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
