#include "ui_internal.h"
#include "ui_style_internal.h"
#include "ui_paint_internal.h"
#include "theme.h"
#include "runtime/focus.h"
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
        .light = ColorToInt(light), .ambient = ColorToInt(ui_app_style().background),
        .hover = hover, .press = press, .focus = focused, .disabled = disabled,
        .fill = fill_states, .fill_valid = true, .scale = (float)Scale(1000) / 1000.0f
    };
    paint = PrepareMaterial(paint);
    for(int i = 0; i < MaterialLayerCount(paint.value.material); i++)
        ui_draw_surface(PaintMaterialLayer(paint, i));
    return MaterialContentBounds(paint);
}

static ThemeMetrics g_ui_style_override;
static int g_ui_style_override_enabled = 0;
static int g_fancy_effects_enabled = 1;

void
SetFancyEffectsEnabled(int enabled)
{
    int next = enabled != 0;
    if(g_fancy_effects_enabled == next)
        return;
    g_fancy_effects_enabled = next;
    InvalidateTree(INVALIDATE_PAINT);
}

int
FancyEffectsEnabled(void)
{
    return g_fancy_effects_enabled != 0;
}

static ThemeMetrics
ui_apply_effects_metrics(ThemeMetrics tokens)
{
    if(g_fancy_effects_enabled)
        return tokens;
    tokens.shadow_blur = 0.0f;
    tokens.shadow_alpha = 0;
    tokens.shine_alpha = 0;
    tokens.shadow_offset_y = 0;
    return tokens;
}


#if !defined(KRYON_BACKEND_TERMI)
typedef struct {
    unsigned int key;
    Vector2 origin;
    float age;
    int active;
    unsigned long frame_seen;
} DefaultRipple;

#define DEFAULT_RIPPLE_MAX 64

static DefaultRipple g_default_ripples[DEFAULT_RIPPLE_MAX];
#endif

ThemeMetrics
GetDefaultThemeMetrics(void)
{
    ThemeMetrics tokens;

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
    tokens.bevel_enabled = 0;
    tokens.control_radius = 4.0f;
    tokens.panel_radius = 8.0f;
    tokens.control_alpha = 222;
    tokens.panel_alpha = 235;
    tokens.title_bar_alpha = 222;
    tokens.border_alpha = 118;
    tokens.shadow_alpha = 28;
    tokens.shine_alpha = 0;
    tokens.touch_target_min = 48;
    tokens.shadow_offset_y = 2;
    return tokens;
}

ThemeMetrics
GetThemeMetrics(void)
{
    if(g_ui_style_override_enabled)
        return ui_apply_effects_metrics(g_ui_style_override);
    return ui_apply_effects_metrics(GetDefaultThemeMetrics());
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

static StyleData
pack_style(Style value)
{
    return (StyleData){
        .fields = value.fields,
        .material = value.material,
        .typeface = StringView(value.typeface, value.typeface ? strlen(value.typeface) : 0),
        .letter_spacing = value.letter_spacing,
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

StyleData
ui_style_apply_effects_data(StyleData value)
{
    /* Optional glow must not replace the selected material or its colors. */
    return value;
}

StyleFrame
ui_style_apply_effects_frame(StyleFrame frame)
{
    frame.value = ui_style_apply_effects_data(frame.value);
    return frame;
}

FillStates
ui_style_apply_effects_fill(FillStates fill)
{
    return fill;
}

Style
ui_app_style(void)
{
    StyleData value = ResolveActiveStyle((StyleData){0},
                                         StyleDefaultFacts(StyleKindApp()),
                                         ButtonStateNormal);
    return ui_style_apply_effects(ui_unpack_style(value));
}

Style
ui_surface_style(void)
{
    StyleData value = ResolveActiveStyle((StyleData){0},
                                         StyleDefaultFacts(StyleKindSurface()),
                                         ButtonStateNormal);
    return ui_style_apply_effects(ui_unpack_style(value));
}

void
ui_runtime_theme_values(Palette *palette_out, Metrics *metrics_out)
{
    ThemeScheme scheme = ui_default_scheme();
    const Theme *theme = GetThemeRef();
    Palette defaults = DefaultPalette(GetEffectiveThemeDarkMode());
    Color surface = theme != NULL ? theme->colors.surface : scheme.surface;
    Color neutral = theme != NULL ? theme->colors.surface_raised : scheme.surface_variant;
    Color accent = theme != NULL ? theme->colors.accent : scheme.primary;
    Color accent_hover = theme != NULL ? theme->colors.accent_hover
                                        : accent;
    Color accent_pressed = theme != NULL ? theme->colors.accent_pressed
                                          : DarkenColor(accent, 14);
    Color danger = theme != NULL ? theme->colors.danger : GetColor(defaults.danger);
    Color success = theme != NULL ? theme->colors.success : GetColor(defaults.success);
    Color warning = theme != NULL ? theme->colors.warning : GetColor(defaults.warning);
    Color disabled_text = theme != NULL
        ? theme->colors.text_disabled
        : GetColor(defaults.text_disabled);

    Palette palette = {
        .background = ColorToInt(theme != NULL ? theme->colors.background : scheme.surface),
        .surface = ColorToInt(surface),
        .surface_raised = ColorToInt(neutral),
        .surface_sunken = ColorToInt(theme != NULL ? theme->colors.surface_sunken : scheme.surface_container),
        .overlay = ColorToInt(theme != NULL ? theme->colors.overlay : GetColor(defaults.overlay)),
        .accent = ColorToInt(accent),
        .accent_hover = ColorToInt(accent_hover),
        .accent_pressed = ColorToInt(accent_pressed),
        .on_accent = ColorToInt(theme != NULL ? theme->colors.on_accent : scheme.on_primary),
        .text = ColorToInt(theme != NULL ? theme->colors.text : scheme.on_surface),
        .text_muted = ColorToInt(theme != NULL ? theme->colors.text_muted : scheme.on_surface_variant),
        .text_disabled = ColorToInt(disabled_text),
        .icon = ColorToInt(theme != NULL ? theme->colors.icon : scheme.on_surface),
        .icon_muted = ColorToInt(theme != NULL ? theme->colors.icon_muted : scheme.on_surface_variant),
        .border = ColorToInt(theme != NULL ? theme->colors.border : scheme.outline),
        .border_strong = ColorToInt(theme != NULL ? theme->colors.border_strong : scheme.outline),
        .divider = ColorToInt(theme != NULL ? theme->colors.divider : scheme.outline),
        .danger = ColorToInt(danger),
        .on_danger = ColorToInt(theme != NULL ? theme->colors.on_danger : GetColor(defaults.on_danger)),
        .success = ColorToInt(success),
        .on_success = ColorToInt(theme != NULL ? theme->colors.on_success : GetColor(defaults.on_success)),
        .warning = ColorToInt(warning),
        .on_warning = ColorToInt(theme != NULL ? theme->colors.on_warning : GetColor(defaults.on_warning)),
        .link = ColorToInt(theme != NULL ? theme->colors.link : scheme.primary),
        .focus = ColorToInt(theme != NULL ? theme->colors.focus : scheme.primary),
        .shadow = ColorToInt(theme != NULL ? theme->colors.shadow : GetColor(defaults.shadow))
    };
    ThemeMetrics metrics = GetThemeMetrics();
    Metrics tokens = {
        .transition_normal_ms = metrics.transition_normal_ms,
        .transition_fast_ms = metrics.transition_fast_ms,
        .radius_medium = metrics.radius_medium,
        .radius_large = metrics.radius_large,
        .radius_pill = metrics.radius_pill,
        .border_width = metrics.border_width,
        .control_padding_small = metrics.control_padding_small,
        .control_padding_medium = metrics.control_padding_medium,
        .control_padding_large = metrics.control_padding_large,
        .control_gap = metrics.control_gap,
        .font_size_small = metrics.font_size_small,
        .font_size_medium = metrics.font_size_medium,
        .font_size_large = metrics.font_size_large,
        .icon_size_small = metrics.icon_size_small,
        .icon_size_medium = metrics.icon_size_medium,
        .icon_size_large = metrics.icon_size_large
    };

    *palette_out = palette;
    *metrics_out = tokens;
}

Style
ui_unpack_style(StyleData value)
{
    return (Style){
        .material = (MaterialKind)value.material,
        .typeface = value.typeface.data,
        .letter_spacing = value.letter_spacing,
        .fields = value.fields,
        .background = GetColor(value.background),
        .background_end = GetColor(value.background_end),
        .foreground = GetColor(value.foreground),
        .border = GetColor(value.border),
        .focus = GetColor(value.focus),
        .radius = value.radius,
        .border_width = value.border_width,
        .opacity = StyleOpacityValue(value.fields, value.opacity),
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
    frame = ui_style_apply_effects_frame(frame);
    *fill = frame.fill;
    return ui_unpack_style(frame.value);
}

FillStates
ui_style_fill(Style value)
{
    return ui_style_apply_effects_fill(FillState(value.fields, ColorToInt(value.background),
        ColorToInt(value.background_end)));
}

Style
ui_style_apply_effects(Style value)
{
    return ui_unpack_style(ui_style_apply_effects_data(pack_style(value)));
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
    Color input_surface = c_surface;
    int dark = GetEffectiveThemeDarkMode();
    ThemeScheme scheme;
    Scheme roles;

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

    roles = SchemeFor(ColorToInt(c_bg), ColorToInt(input_surface),
                      ColorToInt(c_text), ColorToInt(c_circle),
                      ColorToInt(c_button), dark != 0, StyleDisabledAlpha());
    scheme.primary = GetColor(roles.primary);
    scheme.on_primary = GetColor(roles.on_primary);
    scheme.secondary = GetColor(roles.secondary);
    scheme.on_secondary = GetColor(roles.on_secondary);
    scheme.surface = GetColor(roles.surface);
    scheme.on_surface = GetColor(roles.on_surface);
    scheme.surface_container = GetColor(roles.surface_container);
    scheme.surface_variant = GetColor(roles.surface_variant);
    scheme.on_surface_variant = GetColor(roles.on_surface_variant);
    scheme.outline = GetColor(roles.outline);
    scheme.error = GetColor(roles.error);
    scheme.on_error = GetColor(roles.on_error);
    scheme.disabled_container = GetColor(roles.disabled_container);
    scheme.disabled_content = GetColor(roles.disabled_content);

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

void
ui_default_elevation(Rectangle bounds, float radius, int level)
{
    Color shadow;
    StyleElevationPaint paint = StyleElevationPaintFor(level);

    if(!paint.visible)
        return;

    shadow = BLACK;
    shadow.a = (unsigned char)paint.far_alpha;
    DrawRectangleRounded((Rectangle){bounds.x,
                                     bounds.y + (float)Scale(paint.far_offset),
                                     bounds.width, bounds.height},
                         radius, 12, shadow);
    shadow.a = (unsigned char)paint.near_alpha;
    DrawRectangleRounded((Rectangle){bounds.x,
                                     bounds.y + (float)Scale(paint.near_offset),
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
    DefaultRipple *ripple;
    Vector2 mouse;
    float dt;
    float max_radius;
    StyleRipplePaint paint;
    Color color = on_color;
    unsigned int hash = (unsigned int)key * 2654435761u;

    if(key == 0 || !FancyEffectsEnabled())
        return;
    ripple = &g_default_ripples[hash % DEFAULT_RIPPLE_MAX];
    if(ripple->key != hash || g_ui_frame_serial - ripple->frame_seen > 20) {
        memset(ripple, 0, sizeof(*ripple));
        ripple->key = hash;
    }
    ripple->frame_seen = g_ui_frame_serial;

    mouse = ui_mouse_world();
    if(pressed && !ripple->active) {
        ripple->origin = CheckCollisionPointRec(mouse, bounds)
                             ? mouse
                             : StyleRippleFallbackOrigin(bounds);
        ripple->age = 0.0f;
        ripple->active = 1;
    }
    if(!ripple->active)
        return;

    dt = GetFrameTime();
    if(dt <= 0.0f || dt > 0.1f)
        dt = 1.0f / 60.0f;
    ripple->age += dt;

    max_radius = sqrtf(bounds.width * bounds.width + bounds.height * bounds.height);
    paint = StyleRipplePaintFor(ripple->age, pressed != 0, max_radius,
                                (float)Scale(1000) / 1000.0f);
    if(!paint.visible) {
        ripple->active = 0;
        return;
    }

    color.a = (unsigned char)paint.alpha;
    DrawCircleV(ripple->origin, paint.radius, color);
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

    ui_default_elevation(bounds, radius, tokens.shadow_offset_y);
    if(tokens.control_alpha < background.a)
        background.a = tokens.control_alpha;
    if(tokens.border_alpha < border.a)
        border.a = tokens.border_alpha;
    DrawRectangleRounded(bounds, radius, 12, background);
    if(border.a != 0)
        DrawRectangleRoundedLines(bounds, radius, 12, border);
    StyleShinePaint shine_paint =
        StyleShinePaintFor(bounds, radius, tokens.shine_alpha,
                           (float)Scale(1000) / 1000.0f);
    if(shine_paint.visible) {
        Color shine = WHITE;
        shine.a = tokens.shine_alpha;
        DrawRectangleRounded(shine_paint.bounds, radius, 8, shine);
    }
}
