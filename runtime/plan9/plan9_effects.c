/**
 * Plan 9 Backend - Visual Effects
 *
 * Implements visual effects like gradients, shadows, and opacity
 * Note: Plan 9's libdraw has limited alpha blending support
 */

#include "plan9_internal.h"
#include "../desktop/abstract/desktop_effects.h"
#include "../../ir/include/ir_log.h"

/* Helper: interpolate between two color components */
static uint8_t
lerp_color(uint8_t a, uint8_t b, float t)
{
    return (uint8_t)(a + t * (b - a));
}

/* Draw gradient (approximated with multiple rectangles) */
static void
plan9_draw_gradient(void* backend_data, float x, float y, float w, float h,
                   uint32_t color1, uint32_t color2, int vertical)
{
    Plan9RendererData* data;
    int steps;
    int i;
    float step_size;
    float t;
    Rectangle r;
    Image* color_img;
    ulong rgba;
    uint8_t r1, g1, b1, a1;
    uint8_t r2, g2, b2, a2;
    uint8_t r, g, b, a;

    data = (Plan9RendererData*)backend_data;
    if (!data || !data->screen)
        return;

    /* Extract color components (assuming RGBA format) */
    r1 = (color1 >> 24) & 0xFF;
    g1 = (color1 >> 16) & 0xFF;
    b1 = (color1 >> 8) & 0xFF;
    a1 = color1 & 0xFF;

    r2 = (color2 >> 24) & 0xFF;
    g2 = (color2 >> 16) & 0xFF;
    b2 = (color2 >> 8) & 0xFF;
    a2 = color2 & 0xFF;

    /* Use 20 steps for smooth gradient */
    steps = 20;
    step_size = vertical ? (h / steps) : (w / steps);

    /* Draw gradient as series of rectangles */
    for (i = 0; i < steps; i++) {
        t = (float)i / (steps - 1);

        /* Linear interpolation of color components */
        r = lerp_color(r1, r2, t);
        g = lerp_color(g1, g2, t);
        b = lerp_color(b1, b2, t);
        a = lerp_color(a1, a2, t);

        /* Pack into Plan 9 color format */
        rgba = (r << 24) | (g << 16) | (b << 8) | a;
        color_img = plan9_get_color_image(data, rgba);

        /* Calculate rectangle for this step */
        if (vertical) {
            r = Rect((int)x, (int)(y + i * step_size),
                    (int)(x + w), (int)(y + (i + 1) * step_size));
        } else {
            r = Rect((int)(x + i * step_size), (int)y,
                    (int)(x + (i + 1) * step_size), (int)(y + h));
        }

        /* Draw rectangle */
        draw(data->screen, r, color_img, nil, ZP);
    }
}

/* Draw shadow (simple offset with semi-transparent color) */
static void
plan9_draw_shadow(void* backend_data, float x, float y, float w, float h,
                 float blur, uint32_t color)
{
    Plan9RendererData* data;
    Rectangle r;
    Image* shadow_img;
    ulong shadow_color;

    data = (Plan9RendererData*)backend_data;
    if (!data || !data->screen)
        return;

    /* Create semi-transparent shadow color */
    shadow_color = (color & 0xFFFFFF00) | 0x80;  /* 50% alpha */
    shadow_img = plan9_get_color_image(data, shadow_color);

    /* Offset shadow slightly based on blur amount */
    r = Rect((int)(x + blur/2), (int)(y + blur/2),
            (int)(x + w + blur/2), (int)(y + h + blur/2));

    /* Draw with alpha compositing (SoverD operator) */
    draw(data->screen, r, shadow_img, nil, ZP);
}

/* Set global opacity (limited support on Plan 9) */
static void
plan9_set_opacity(void* backend_data, float opacity)
{
    /* Plan 9 doesn't have global opacity state */
    /* Opacity must be baked into each color's alpha channel */
    /* This is a no-op - opacity is handled per-command */
    (void)backend_data;
    (void)opacity;
}

/* Apply blur effect (not supported on Plan 9) */
static void
plan9_apply_blur(void* backend_data, float x, float y, float w, float h,
                float radius)
{
    /* Blur effects require pixel manipulation not available in libdraw */
    /* This is a no-op on Plan 9 */
    (void)backend_data;
    (void)x;
    (void)y;
    (void)w;
    (void)h;
    (void)radius;

    IR_LOG_WARN("PLAN9", "Blur effects not supported on Plan 9");
}

/* Effects operations table */
DesktopEffectsOps g_plan9_effects_ops = {
    plan9_draw_gradient,
    plan9_draw_shadow,
    plan9_set_opacity,
    plan9_apply_blur
};
