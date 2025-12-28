#ifndef RAYLIB_EFFECTS_H
#define RAYLIB_EFFECTS_H

#include <raylib.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Gradient stop structure */
typedef struct {
    float position;      // 0.0 to 1.0
    uint8_t r, g, b, a;
} Raylib_GradientStop;

/* Gradient types */
typedef enum {
    RAYLIB_GRADIENT_LINEAR = 0,
    RAYLIB_GRADIENT_RADIAL = 1,
    RAYLIB_GRADIENT_CONIC = 2
} Raylib_GradientType;

/* ============================================================================
 * Gradient Rendering
 * ============================================================================ */

/**
 * Render a linear gradient.
 */
void raylib_render_linear_gradient(
    Rectangle rect,
    int16_t angle,
    Raylib_GradientStop* stops,
    uint8_t stop_count,
    float opacity
);

/**
 * Render a radial gradient.
 */
void raylib_render_radial_gradient(
    Rectangle rect,
    Raylib_GradientStop* stops,
    uint8_t stop_count,
    float opacity
);

/**
 * Render a conic gradient.
 */
void raylib_render_conic_gradient(
    Rectangle rect,
    Raylib_GradientStop* stops,
    uint8_t stop_count,
    float opacity
);

/**
 * Render a gradient (dispatches to specific type).
 */
void raylib_render_gradient(
    Rectangle rect,
    Raylib_GradientType type,
    int16_t angle,
    Raylib_GradientStop* stops,
    uint8_t stop_count,
    float opacity
);

/* ============================================================================
 * Shape Rendering
 * ============================================================================ */

/**
 * Render a rounded rectangle.
 */
void raylib_render_rounded_rect(
    Rectangle rect,
    uint16_t radius,
    uint32_t color
);

/**
 * Render a box shadow.
 */
void raylib_render_shadow(
    Rectangle rect,
    int16_t offset_x,
    int16_t offset_y,
    uint16_t blur_radius,
    uint16_t spread_radius,
    uint32_t color,
    bool inset
);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * Convert packed RGBA color to Raylib Color.
 */
Color raylib_unpack_color(uint32_t color);

/**
 * Apply opacity to a packed color.
 */
uint32_t raylib_apply_opacity(uint32_t color, float opacity);

#ifdef __cplusplus
}
#endif

#endif /* RAYLIB_EFFECTS_H */
