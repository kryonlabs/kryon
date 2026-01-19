/**
 * Raylib Visual Effects
 *
 * Provides visual effects for Raylib backend:
 * - Linear, radial, and conic gradients
 * - Rounded rectangles
 * - Text shadows
 * - Blend mode management
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include <raylib.h>

#include "../../desktop_internal.h"
#include "raylib_renderer.h"

// ============================================================================
// COLOR UTILITIES
// ============================================================================

/**
 * Convert IRColor to Raylib Color
 */
static Color ir_color_to_raylib(IRColor c) {
    return (Color){
        .r = c.data.r,
        .g = c.data.g,
        .b = c.data.b,
        .a = c.data.a
    };
}

/**
 * Convert IRGradientStop to Raylib Color
 */
static Color gradient_stop_to_color(IRGradientStop* stop) {
    return (Color){
        .r = stop->r,
        .g = stop->g,
        .b = stop->b,
        .a = stop->a
    };
}

/**
 * Lerp between two colors
 */
static Color lerp_color(Color a, Color b, float t) {
    return (Color){
        .r = (unsigned char)(a.r + (b.r - a.r) * t),
        .g = (unsigned char)(a.g + (b.g - a.g) * t),
        .b = (unsigned char)(a.b + (b.b - a.b) * t),
        .a = (unsigned char)(a.a + (b.a - a.a) * t)
    };
}

// ============================================================================
// GRADIENT RENDERING
// ============================================================================

/**
/**
 * Render linear gradient using angle
 */
void raylib_render_linear_gradient(Rectangle rect, IRGradient* gradient) {
    if (!gradient || gradient->stop_count < 2) return;

    // Convert angle to radians and calculate direction
    float angle_rad = gradient->angle * (PI / 180.0f);
    float cos_a = cosf(angle_rad);
    float sin_a = sinf(angle_rad);

    // Simple scanline rendering (slower but works for any angle)
    int start_x = (int)rect.x;
    int end_x = (int)(rect.x + rect.width);
    int start_y = (int)rect.y;
    int end_y = (int)(rect.y + rect.height);

    for (int y = start_y; y < end_y; y++) {
        for (int x = start_x; x < end_x; x++) {
            // Calculate position along gradient (0.0 to 1.0)
            float rel_x = (x - rect.x) / rect.width;
            float rel_y = (y - rect.y) / rect.height;
            float t = rel_x * cos_a + rel_y * sin_a;

            // Clamp to [0, 1]
            if (t < 0.0f) t = 0.0f;
            if (t > 1.0f) t = 1.0f;

            // Find color at position t
            Color color = gradient_stop_to_color(&gradient->stops[0]);
            for (int i = 0; i < gradient->stop_count - 1; i++) {
                if (t >= gradient->stops[i].position && t <= gradient->stops[i + 1].position) {
                    float local_t = (t - gradient->stops[i].position) /
                                   (gradient->stops[i + 1].position - gradient->stops[i].position);
                    Color c1 = gradient_stop_to_color(&gradient->stops[i]);
                    Color c2 = gradient_stop_to_color(&gradient->stops[i + 1]);
                    color = lerp_color(c1, c2, local_t);
                    break;
                }
            }
            DrawPixel(x, y, color);
        }
    }
}

/**
 * Render radial gradient using center point
 */
void raylib_render_radial_gradient(Rectangle rect, IRGradient* gradient) {
    if (!gradient || gradient->stop_count < 2) return;

    // Center is in normalized coordinates (0.0 to 1.0)
    float cx = rect.x + rect.width * gradient->center_x;
    float cy = rect.y + rect.height * gradient->center_y;
    float max_radius = sqrtf(rect.width * rect.width + rect.height * rect.height) / 2.0f;

    int start_x = (int)rect.x;
    int end_x = (int)(rect.x + rect.width);
    int start_y = (int)rect.y;
    int end_y = (int)(rect.y + rect.height);

    for (int y = start_y; y < end_y; y++) {
        for (int x = start_x; x < end_x; x++) {
            float dx = x - cx;
            float dy = y - cy;
            float dist = sqrtf(dx * dx + dy * dy);
            float t = dist / max_radius;

            if (t < 0.0f) t = 0.0f;
            if (t > 1.0f) t = 1.0f;

            Color color = gradient_stop_to_color(&gradient->stops[0]);
            for (int i = 0; i < gradient->stop_count - 1; i++) {
                if (t >= gradient->stops[i].position && t <= gradient->stops[i + 1].position) {
                    float local_t = (t - gradient->stops[i].position) /
                                   (gradient->stops[i + 1].position - gradient->stops[i].position);
                    Color c1 = gradient_stop_to_color(&gradient->stops[i]);
                    Color c2 = gradient_stop_to_color(&gradient->stops[i + 1]);
                    color = lerp_color(c1, c2, local_t);
                    break;
                }
            }
            DrawPixel(x, y, color);
        }
    }
}

/**
 * Render conic gradient (sweep gradient)
 */
void raylib_render_conic_gradient(Rectangle rect, IRGradient* gradient) {
    if (!gradient || gradient->stop_count < 2) return;

    float cx = rect.x + rect.width * gradient->center_x;
    float cy = rect.y + rect.height * gradient->center_y;

    int start_x = (int)rect.x;
    int end_x = (int)(rect.x + rect.width);
    int start_y = (int)rect.y;
    int end_y = (int)(rect.y + rect.height);

    for (int y = start_y; y < end_y; y++) {
        for (int x = start_x; x < end_x; x++) {
            float dx = x - cx;
            float dy = y - cy;
            float angle = atan2f(dy, dx);
            float t = (angle + PI) / (2.0f * PI);

            Color color = gradient_stop_to_color(&gradient->stops[0]);
            for (int i = 0; i < gradient->stop_count - 1; i++) {
                if (t >= gradient->stops[i].position && t <= gradient->stops[i + 1].position) {
                    float local_t = (t - gradient->stops[i].position) /
                                   (gradient->stops[i + 1].position - gradient->stops[i].position);
                    Color c1 = gradient_stop_to_color(&gradient->stops[i]);
                    Color c2 = gradient_stop_to_color(&gradient->stops[i + 1]);
                    color = lerp_color(c1, c2, local_t);
                    break;
                }
            }
            DrawPixel(x, y, color);
        }
    }
}

// ============================================================================
// ROUNDED RECTANGLES
// ============================================================================

/**
 * Render rounded rectangle
 * Raylib has built-in support for this!
 */
void raylib_render_rounded_rect(Rectangle rect, float top_left, float top_right,
                                float bottom_right, float bottom_left, Color color) {
    // Raylib's DrawRectangleRounded uses uniform radius, so we'll use average
    float avg_radius = (top_left + top_right + bottom_right + bottom_left) / 4.0f;

    // Calculate relative roundness (0.0 to 1.0)
    float min_dimension = rect.width < rect.height ? rect.width : rect.height;
    float roundness = (avg_radius / (min_dimension / 2.0f));

    // Clamp roundness to [0, 1]
    if (roundness < 0.0f) roundness = 0.0f;
    if (roundness > 1.0f) roundness = 1.0f;

    // Draw rounded rectangle (segments = 0 means auto)
    DrawRectangleRounded(rect, roundness, 0, color);
}

// ============================================================================
// TEXT SHADOWS
// ============================================================================

/**
 * Render text with shadow
 * Draws shadow first, then text on top
 */
void raylib_render_text_with_shadow(Font font, const char* text, Vector2 position,
                                    float font_size, Color text_color,
                                    float shadow_offset_x, float shadow_offset_y,
                                    Color shadow_color) {
    // Draw shadow
    Vector2 shadow_pos = {position.x + shadow_offset_x, position.y + shadow_offset_y};
    DrawTextEx(font, text, shadow_pos, font_size, 0, shadow_color);

    // Draw text on top
    DrawTextEx(font, text, position, font_size, 0, text_color);
}

// ============================================================================
// BLEND MODES
// ============================================================================

/**
 * Set blend mode for rendering
 * Raylib supports several blend modes natively
 */
void raylib_set_blend_mode(int mode) {
    // Map to Raylib blend modes
    BlendMode raylib_mode;

    switch (mode) {
        case 0: // Normal/Alpha
            raylib_mode = BLEND_ALPHA;
            break;
        case 1: // Additive
            raylib_mode = BLEND_ADDITIVE;
            break;
        case 2: // Multiply
            raylib_mode = BLEND_MULTIPLIED;
            break;
        default:
            raylib_mode = BLEND_ALPHA;
            break;
    }

    BeginBlendMode(raylib_mode);
}

/**
 * Restore default blend mode
 */
void raylib_reset_blend_mode() {
    EndBlendMode();
}
