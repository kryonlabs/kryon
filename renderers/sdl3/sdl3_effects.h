#ifndef SDL3_EFFECTS_H
#define SDL3_EFFECTS_H

#include <SDL3/SDL.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Gradient stop structure */
typedef struct {
    float position;      // 0.0 to 1.0
    uint8_t r, g, b, a;
} SDL3_GradientStop;

/* Gradient types */
typedef enum {
    SDL3_GRADIENT_LINEAR = 0,
    SDL3_GRADIENT_RADIAL = 1,
    SDL3_GRADIENT_CONIC = 2
} SDL3_GradientType;

/* ============================================================================
 * Gradient Rendering
 * ============================================================================ */

/**
 * Render a linear gradient.
 */
void sdl3_render_linear_gradient(
    SDL_Renderer* renderer,
    SDL_FRect* rect,
    int16_t angle,
    SDL3_GradientStop* stops,
    uint8_t stop_count,
    float opacity
);

/**
 * Render a radial gradient.
 */
void sdl3_render_radial_gradient(
    SDL_Renderer* renderer,
    SDL_FRect* rect,
    SDL3_GradientStop* stops,
    uint8_t stop_count,
    float opacity
);

/**
 * Render a conic gradient.
 */
void sdl3_render_conic_gradient(
    SDL_Renderer* renderer,
    SDL_FRect* rect,
    SDL3_GradientStop* stops,
    uint8_t stop_count,
    float opacity
);

/**
 * Render a gradient (dispatches to specific type).
 */
void sdl3_render_gradient(
    SDL_Renderer* renderer,
    SDL_FRect* rect,
    SDL3_GradientType type,
    int16_t angle,
    SDL3_GradientStop* stops,
    uint8_t stop_count,
    float opacity
);

/* ============================================================================
 * Shape Rendering
 * ============================================================================ */

/**
 * Render a rounded rectangle.
 */
void sdl3_render_rounded_rect(
    SDL_Renderer* renderer,
    SDL_FRect* rect,
    uint16_t radius,
    uint32_t color
);

/**
 * Render a box shadow.
 */
void sdl3_render_shadow(
    SDL_Renderer* renderer,
    SDL_FRect* rect,
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
 * Ensure blend mode is set (cached for performance).
 */
void sdl3_ensure_blend_mode(SDL_Renderer* renderer);

/**
 * Convert packed RGBA color to SDL_Color.
 */
SDL_Color sdl3_unpack_color(uint32_t color);

/**
 * Apply opacity to a packed color.
 */
uint32_t sdl3_apply_opacity(uint32_t color, float opacity);

#ifdef __cplusplus
}
#endif

#endif /* SDL3_EFFECTS_H */
