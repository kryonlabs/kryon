#include "ui_internal.h"

/* Aero style: Windows Vista/7 glass rendered with plain primitives so every
 * backend (raylib, Android, web, software, null) draws the same picture.
 * Gloss comes from alpha-stepped white overlays, glass from translucent
 * fills, glow from stacked rounded outline rings. */

int
ui_aero_style(void)
{
    return GetEffectiveThemeStyle() == THEME_STYLE_AERO;
}

static int
ui_aero_luminance(Color color)
{
    return ((int)color.r * 299 + (int)color.g * 587 + (int)color.b * 114) / 1000;
}

Color
ui_aero_on_color(Color color)
{
    return ui_aero_luminance(color) < 128 ? RAYWHITE
                                          : (Color){0x1A, 0x1A, 0x1A, 0xFF};
}

UIAeroScheme
ui_aero_scheme(void)
{
    UIAeroScheme scheme;
    UIStyleTokens tokens = GetUIStyleTokens();
    Color base = c_button.a != 0 ? c_button : c_surface;
    Color surface = c_surface.a != 0 ? c_surface : c_bg;
    int dark = GetEffectiveThemeDarkMode();

    scheme.glass = surface;
    scheme.glass.a = tokens.panel_alpha;
    scheme.fill_top = dark ? LightenUIColor(base, 16) : LightenUIColor(base, 30);
    scheme.fill_bottom = dark ? DarkenUIColor(base, 8) : DarkenUIColor(base, 14);
    scheme.border = dark ? LightenUIColor(base, 28) : DarkenUIColor(base, 48);
    scheme.highlight = LightenUIColor(base, dark ? 40 : 70);
    scheme.glow = c_circle.a != 0 ? c_circle : c_link;
    scheme.focus_ring = scheme.glow;
    scheme.inset_fill = dark ? DarkenUIColor(surface, 6) : DarkenUIColor(surface, 10);
    scheme.inset_border = dark ? LightenUIColor(surface, 18)
                               : DarkenUIColor(surface, 34);
    scheme.text = c_text;
    return scheme;
}

UIAeroScheme
GetUIAeroScheme(void)
{
    return ui_aero_scheme();
}

/* Pixel radius of a rounded shape described by the normalized fraction
 * raylib's rounded-rect calls take. */
static float
ui_aero_radius_px(Rectangle bounds, float radius)
{
    float min_side = bounds.width < bounds.height ? bounds.width : bounds.height;

    if(radius <= 0.0f || min_side <= 0.0f)
        return 0.0f;
    return radius * min_side;
}

static float
ui_aero_radius_for(Rectangle bounds, float radius_px)
{
    float min_side = bounds.width < bounds.height ? bounds.width : bounds.height;

    if(radius_px <= 0.0f || min_side <= 0.0f)
        return 0.0f;
    if(radius_px > min_side * 0.5f)
        radius_px = min_side * 0.5f;
    return radius_px / min_side;
}

/* Soft layered drop shadow below a rounded shape. Level 0..3 controls how
 * far the shadow reaches; panels use higher levels than controls. */
static void
ui_aero_shadow(Rectangle bounds, float radius_px, int level)
{
    UIStyleTokens tokens = GetUIStyleTokens();
    Color shadow = BLACK;
    int step = ScaleUIPx(1 + level);

    if(tokens.shadow_alpha <= 0 || level <= 0)
        return;
    if(level > 3)
        level = 3;

    for(int i = 3; i >= 1; i--) {
        float offset = (float)(step * i);
        unsigned char alpha = (unsigned char)(tokens.shadow_alpha * (4 - i) / 4);

        if(alpha == 0)
            continue;
        shadow.a = alpha;
        DrawRectangleRounded((Rectangle){bounds.x, bounds.y + offset,
                                         bounds.width, bounds.height},
                             ui_aero_radius_for(bounds, radius_px), 12, shadow);
    }
}

/* White gloss over the top part of a rounded shape. The overlay rect is
 * drawn taller than the gloss band so its own bottom rounding is cut off
 * flat by the clip, while its top corners keep the shape's radius. */
static void
ui_aero_gloss(Rectangle bounds, float radius_px, float coverage,
              unsigned char alpha)
{
    Color gloss = WHITE;
    int band = (int)(bounds.height * coverage);
    float r = radius_px;

    if(alpha == 0 || band <= 0 || bounds.width <= 0 || bounds.height <= 0)
        return;
    if((float)band + r > bounds.height) {
        band = (int)(bounds.height - r);
        if(band <= 0)
            return;
    }

    gloss.a = alpha;
    BeginUIClip((int)bounds.x, (int)bounds.y, (int)bounds.width, band);
    DrawRectangleRounded((Rectangle){bounds.x, bounds.y,
                                     bounds.width, (float)band + r},
                         ui_aero_radius_for((Rectangle){bounds.x, bounds.y,
                                                        bounds.width,
                                                        (float)band + r},
                                            r),
                         12, gloss);
    EndUIClip();
}

/* Accent glow rings around a shape, brightest close to the edge. */
static void
ui_aero_glow(Rectangle bounds, float radius_px, Color tint)
{
    Color glow = tint;
    float r = radius_px;
    int e1 = ScaleUIPx(1);
    int e2 = ScaleUIPx(2);

    if(tint.a == 0)
        return;
    glow.a = 95;
    DrawRectangleRoundedLines((Rectangle){bounds.x - (float)e1, bounds.y - (float)e1,
                                          bounds.width + (float)(e1 * 2),
                                          bounds.height + (float)(e1 * 2)},
                              ui_aero_radius_for(bounds, r + (float)e1), 12, glow);
    glow.a = 45;
    DrawRectangleRoundedLines((Rectangle){bounds.x - (float)e2, bounds.y - (float)e2,
                                          bounds.width + (float)(e2 * 2),
                                          bounds.height + (float)(e2 * 2)},
                              ui_aero_radius_for(bounds, r + (float)e2), 12, glow);
}

void
ui_aero_paint_control(Rectangle bounds, Color base, Color border, float radius,
                      int hovered, int pressed, int focused)
{
    UIAeroScheme scheme = ui_aero_scheme();
    UIStyleTokens tokens = GetUIStyleTokens();
    float r = ui_aero_radius_px(bounds, radius);
    Color fill_bottom = scheme.fill_bottom;
    Color border_color = border.a != 0 ? border : scheme.border;
    Color highlight = scheme.highlight;

    if(bounds.width <= 0 || bounds.height <= 0)
        return;

    ui_aero_shadow(bounds, r, 1);
    if(hovered && !pressed)
        ui_aero_glow(bounds, r, scheme.glow);

    if(pressed) {
        Color flat = DarkenUIColor(base, 18);

        flat.a = tokens.control_alpha;
        DrawRectangleRounded(bounds, radius, 12, flat);
    } else {
        fill_bottom = base.a != 0 ? base : fill_bottom;
        fill_bottom.a = fill_bottom.a < tokens.control_alpha
                            ? fill_bottom.a : tokens.control_alpha;
        DrawRectangleRounded(bounds, radius, 12, fill_bottom);
        if(tokens.shine_alpha > 0) {
            ui_aero_gloss(bounds, r, 0.58f,
                          (unsigned char)(tokens.shine_alpha * 0.55f));
            ui_aero_gloss(bounds, r, 0.30f,
                          (unsigned char)(tokens.shine_alpha * 0.40f));
        }
    }

    /* 1px inner highlight sits just inside the border and sells the glass. */
    highlight.a = GetEffectiveThemeDarkMode() ? 60 : 110;
    if(bounds.width > ScaleUIPx(4) && bounds.height > ScaleUIPx(4))
        DrawRectangleRoundedLines((Rectangle){bounds.x + 1, bounds.y + 1,
                                              bounds.width - 2, bounds.height - 2},
                                  ui_aero_radius_for(bounds,
                                                     r > 1.0f ? r - 1.0f : r),
                                  12, highlight);

    if(border_color.a != 0) {
        if(border_color.a > tokens.border_alpha)
            border_color.a = tokens.border_alpha;
        DrawRectangleRoundedLines(bounds, radius, 12, border_color);
    }

    if(focused) {
        Color ring = scheme.focus_ring;

        ring.a = 200;
        DrawRectangleRoundedLines((Rectangle){bounds.x - 1, bounds.y - 1,
                                              bounds.width + 2, bounds.height + 2},
                                  ui_aero_radius_for(bounds, r + 1), 12, ring);
    }
}

void
ui_aero_paint_panel(Rectangle bounds, Color tint, float radius, int shadow_level)
{
    UIAeroScheme scheme = ui_aero_scheme();
    UIStyleTokens tokens = GetUIStyleTokens();
    float r = ui_aero_radius_px(bounds, radius);
    Color fill = tint.a != 0 ? tint : scheme.glass;
    Color highlight = scheme.highlight;
    Color border = GetEffectiveThemeDarkMode() ? LightenUIColor(fill, 22)
                                               : DarkenUIColor(fill, 40);

    if(bounds.width <= 0 || bounds.height <= 0)
        return;

    ui_aero_shadow(bounds, r, shadow_level);

    if(fill.a > tokens.panel_alpha)
        fill.a = tokens.panel_alpha;
    DrawRectangleRounded(bounds, radius, 12, fill);
    if(tokens.shine_alpha > 0)
        ui_aero_gloss(bounds, r, 0.45f,
                      (unsigned char)(tokens.shine_alpha * 0.35f));

    highlight.a = GetEffectiveThemeDarkMode() ? 50 : 95;
    if(bounds.width > ScaleUIPx(4) && bounds.height > ScaleUIPx(4))
        DrawRectangleRoundedLines((Rectangle){bounds.x + 1, bounds.y + 1,
                                              bounds.width - 2, bounds.height - 2},
                                  ui_aero_radius_for(bounds,
                                                     r > 1.0f ? r - 1.0f : r),
                                  12, highlight);

    if(border.a > tokens.border_alpha)
        border.a = tokens.border_alpha;
    DrawRectangleRoundedLines(bounds, radius, 12, border);
}

void
ui_aero_paint_inset(Rectangle bounds, Color base, float radius)
{
    UIAeroScheme scheme = ui_aero_scheme();
    Color fill = base.a != 0 ? base : scheme.inset_fill;
    Color shade = BLACK;
    Color light = WHITE;

    if(bounds.width <= 0 || bounds.height <= 0)
        return;

    DrawRectangleRounded(bounds, radius, 12, fill);

    /* Carved-in read: a dark line under the top edge, a light line over
     * the bottom edge. */
    if(bounds.width > ScaleUIPx(4) && bounds.height > ScaleUIPx(4)) {
        shade.a = 38;
        DrawRectangleRounded((Rectangle){bounds.x + 1, bounds.y + 1,
                                         bounds.width - 2, (float)ScaleUIPx(2)},
                             radius * 0.5f, 4, shade);
        light.a = 30;
        DrawRectangle((int)bounds.x + 1,
                      (int)(bounds.y + bounds.height - 2),
                      (int)bounds.width - 2, ScaleUIPx(1), light);
    }

    DrawRectangleRoundedLines(bounds, radius, 12, scheme.inset_border);
}

void
ui_aero_paint_track(Rectangle bounds, Color top, Color bottom, float radius,
                     int glossy)
{
    float r = ui_aero_radius_px(bounds, radius);
    Color border = GetEffectiveThemeDarkMode() ? LightenUIColor(bottom, 26)
                                               : DarkenUIColor(bottom, 42);

    if(bounds.width <= 0 || bounds.height <= 0)
        return;

    /* True gradient for the flat middle; the rounded border ring drawn on
     * top keeps the shape reading as rounded at the small radii tracks
     * use. */
    DrawRectangleGradientV((int)bounds.x, (int)bounds.y,
                           (int)bounds.width, (int)bounds.height,
                           top, bottom);
    if(glossy)
        ui_aero_gloss(bounds, r, 0.5f, 70);
    DrawRectangleRoundedLines(bounds, radius, 12, border);
}

Color
ui_aero_titlebar_top(void)
{
    return GetEffectiveThemeDarkMode() ? LightenUIColor(c_bg, 16)
                                       : LightenUIColor(c_bg, 26);
}

Color
ui_aero_titlebar_bottom(void)
{
    return GetEffectiveThemeDarkMode() ? DarkenUIColor(c_bg, 4)
                                       : DarkenUIColor(c_bg, 10);
}
