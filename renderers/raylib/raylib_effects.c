/*
 * Raylib Effects Rendering Module
 *
 * Implements gradient rendering, shadows, and rounded rectangles for Raylib.
 * Uses Raylib's native drawing functions where possible.
 */

#include "raylib_effects.h"
#include <math.h>
#include <stdio.h>

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

Color raylib_unpack_color(uint32_t color) {
    return (Color){
        .r = (color >> 24) & 0xFF,
        .g = (color >> 16) & 0xFF,
        .b = (color >> 8) & 0xFF,
        .a = color & 0xFF
    };
}

uint32_t raylib_apply_opacity(uint32_t color, float opacity) {
    uint8_t a = color & 0xFF;
    a = (uint8_t)(a * opacity);
    return (color & 0xFFFFFF00) | a;
}

/* ============================================================================
 * Gradient Utilities
 * ============================================================================ */

static int find_gradient_stop_binary(Raylib_GradientStop* stops, uint8_t count, float t) {
    if (count < 2) return 0;

    int left = 0;
    int right = count - 2;

    while (left < right) {
        int mid = (left + right) / 2;
        if (t < stops[mid].position) {
            right = mid - 1;
        } else if (t > stops[mid + 1].position) {
            left = mid + 1;
        } else {
            return mid;
        }
    }

    if (left < 0) return 0;
    if (left >= count - 1) return count - 2;
    return left;
}

static void interpolate_color(
    Raylib_GradientStop* from,
    Raylib_GradientStop* to,
    float t,
    uint8_t* r, uint8_t* g, uint8_t* b, uint8_t* a
) {
    *r = (uint8_t)(from->r + (to->r - from->r) * t);
    *g = (uint8_t)(from->g + (to->g - from->g) * t);
    *b = (uint8_t)(from->b + (to->b - from->b) * t);
    *a = (uint8_t)(from->a + (to->a - from->a) * t);
}

/* ============================================================================
 * Linear Gradient
 * ============================================================================ */

void raylib_render_linear_gradient(
    Rectangle rect,
    int16_t angle,
    Raylib_GradientStop* stops,
    uint8_t stop_count,
    float opacity
) {
    if (!stops || stop_count < 2) return;

    /* Convert angle to radians */
    float angle_rad = angle * (3.14159265f / 180.0f);
    float cos_a = cosf(angle_rad);
    float sin_a = sinf(angle_rad);

    /* Calculate gradient vector */
    float dx = cos_a * rect.width;
    float dy = sin_a * rect.height;
    float length = sqrtf(dx * dx + dy * dy);

    /* Draw gradient using horizontal strips */
    int strips = (int)(length > 0 ? length : rect.height);
    if (strips < 1) strips = 1;
    if (strips > 256) strips = 256;

    for (int i = 0; i < strips; i++) {
        float t = (float)i / (float)strips;

        /* Find gradient stops */
        int stop_idx = find_gradient_stop_binary(stops, stop_count, t);

        /* Interpolate color */
        Raylib_GradientStop* from = &stops[stop_idx];
        Raylib_GradientStop* to = &stops[stop_idx + 1];
        float local_t = (t - from->position) / (to->position - from->position);
        if (local_t < 0.0f) local_t = 0.0f;
        if (local_t > 1.0f) local_t = 1.0f;

        uint8_t r, g, b, a;
        interpolate_color(from, to, local_t, &r, &g, &b, &a);

        /* Apply opacity */
        uint8_t final_a = (uint8_t)(a * opacity);

        /* Draw strip */
        Color strip_color = { r, g, b, final_a };
        float strip_y = rect.y + (t * rect.height);
        Rectangle strip_rect = {
            .x = rect.x,
            .y = strip_y,
            .width = rect.width,
            .height = rect.height / strips + 1.0f
        };
        DrawRectangleRec(strip_rect, strip_color);
    }
}

/* ============================================================================
 * Radial Gradient
 * ============================================================================ */

void raylib_render_radial_gradient(
    Rectangle rect,
    Raylib_GradientStop* stops,
    uint8_t stop_count,
    float opacity
) {
    if (!stops || stop_count < 2) return;

    Vector2 center = {
        .x = rect.x + rect.width * 0.5f,
        .y = rect.y + rect.height * 0.5f
    };
    float max_radius = sqrtf(rect.width * rect.width + rect.height * rect.height) / 2.0f;

    /* Fixed ring count */
    int rings = 30;

    for (int i = rings; i >= 0; i--) {
        float t = (float)i / (float)rings;

        /* Find gradient stops */
        int stop_idx = find_gradient_stop_binary(stops, stop_count, t);

        /* Interpolate color */
        Raylib_GradientStop* from = &stops[stop_idx];
        Raylib_GradientStop* to = &stops[stop_idx + 1];
        float local_t = (t - from->position) / (to->position - from->position);
        if (local_t < 0.0f) local_t = 0.0f;
        if (local_t > 1.0f) local_t = 1.0f;

        uint8_t r, g, b, a;
        interpolate_color(from, to, local_t, &r, &g, &b, &a);

        /* Apply opacity */
        uint8_t final_a = (uint8_t)(a * opacity);

        /* Draw filled circle */
        Color circle_color = { r, g, b, final_a };
        float radius = t * max_radius;

        DrawCircleV(center, radius, circle_color);
    }
}

/* ============================================================================
 * Conic Gradient
 * ============================================================================ */

void raylib_render_conic_gradient(
    Rectangle rect,
    Raylib_GradientStop* stops,
    uint8_t stop_count,
    float opacity
) {
    if (!stops || stop_count < 2) return;

    Vector2 center = {
        .x = rect.x + rect.width * 0.5f,
        .y = rect.y + rect.height * 0.5f
    };

    /* Fixed section count */
    int sections = 36;
    float max_dist = sqrtf(rect.width * rect.width + rect.height * rect.height);

    for (int deg = 0; deg < sections; deg++) {
        float t = (float)deg / (float)sections;

        /* Find gradient stops */
        int stop_idx = find_gradient_stop_binary(stops, stop_count, t);

        /* Interpolate color */
        Raylib_GradientStop* from = &stops[stop_idx];
        Raylib_GradientStop* to = &stops[stop_idx + 1];
        float local_t = (t - from->position) / (to->position - from->position);
        if (local_t < 0.0f) local_t = 0.0f;
        if (local_t > 1.0f) local_t = 1.0f;

        uint8_t r, g, b, a;
        interpolate_color(from, to, local_t, &r, &g, &b, &a);

        /* Apply opacity */
        uint8_t final_a = (uint8_t)(a * opacity);

        Color line_color = { r, g, b, final_a };

        /* Draw line from center outward */
        float angle = (float)deg * 3.14159265f / 18.0f;
        Vector2 end = {
            .x = center.x + cosf(angle) * max_dist,
            .y = center.y + sinf(angle) * max_dist
        };

        DrawLineV(center, end, line_color);
    }
}

/* ============================================================================
 * Gradient Dispatcher
 * ============================================================================ */

void raylib_render_gradient(
    Rectangle rect,
    Raylib_GradientType type,
    int16_t angle,
    Raylib_GradientStop* stops,
    uint8_t stop_count,
    float opacity
) {
    switch (type) {
        case RAYLIB_GRADIENT_LINEAR:
            raylib_render_linear_gradient(rect, angle, stops, stop_count, opacity);
            break;
        case RAYLIB_GRADIENT_RADIAL:
            raylib_render_radial_gradient(rect, stops, stop_count, opacity);
            break;
        case RAYLIB_GRADIENT_CONIC:
            raylib_render_conic_gradient(rect, stops, stop_count, opacity);
            break;
    }
}

/* ============================================================================
 * Rounded Rectangle
 * ============================================================================ */

void raylib_render_rounded_rect(
    Rectangle rect,
    uint16_t radius,
    uint32_t color
) {
    Color c = raylib_unpack_color(color);

    if (radius == 0) {
        /* No rounding - just fill rect */
        DrawRectangleRec(rect, c);
        return;
    }

    /* Clamp radius */
    float max_radius = (rect.width < rect.height ? rect.width : rect.height) / 2.0f;
    if (radius > max_radius) radius = (uint16_t)max_radius;

    /* Use Raylib's built-in rounded rectangle function */
    DrawRectangleRounded(rect, (float)radius / max_radius, 16, c);
}

/* ============================================================================
 * Shadow Rendering
 * ============================================================================ */

void raylib_render_shadow(
    Rectangle rect,
    int16_t offset_x,
    int16_t offset_y,
    uint16_t blur_radius,
    uint16_t spread_radius,
    uint32_t color,
    bool inset
) {
    /* TODO: Blur not implemented yet - just render solid shadow */
    (void)blur_radius;
    (void)inset;

    Color c = raylib_unpack_color(color);

    Rectangle shadow = {
        .x = rect.x + offset_x - spread_radius,
        .y = rect.y + offset_y - spread_radius,
        .width = rect.width + 2 * spread_radius,
        .height = rect.height + 2 * spread_radius
    };

    DrawRectangleRec(shadow, c);
}
