/**
 * Desktop Effects - Platform-Agnostic Visual Effects
 *
 * This header defines interfaces for visual effects like gradients,
 * rounded rectangles, shadows, etc.
 */

#ifndef DESKTOP_EFFECTS_H
#define DESKTOP_EFFECTS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Forward declarations from IR
typedef struct IRGradient IRGradient;

/**
 * Visual effects operations table - implemented by each renderer
 */
typedef struct {
    /**
     * Render a gradient fill
     * @param renderer_ctx Renderer-specific context (SDL_Renderer*, etc.)
     * @param gradient Gradient definition from IR
     * @param x X position in pixels
     * @param y Y position in pixels
     * @param w Width in pixels
     * @param h Height in pixels
     * @param opacity Opacity multiplier (0.0 - 1.0)
     */
    void (*render_gradient)(
        void* renderer_ctx,
        IRGradient* gradient,
        float x, float y,
        float w, float h,
        float opacity
    );

    /**
     * Render a rounded rectangle
     * @param renderer_ctx Renderer-specific context
     * @param x X position in pixels
     * @param y Y position in pixels
     * @param w Width in pixels
     * @param h Height in pixels
     * @param radius Corner radius in pixels
     * @param color Color in RGBA format (0xRRGGBBAA)
     */
    void (*render_rounded_rect)(
        void* renderer_ctx,
        float x, float y,
        float w, float h,
        float radius,
        uint32_t color
    );

    /**
     * Render a text shadow effect
     * @param renderer_ctx Renderer-specific context
     * @param text Text to render
     * @param x X position in pixels
     * @param y Y position in pixels
     * @param color Shadow color in RGBA format
     * @param blur Blur radius in pixels
     */
    void (*render_text_shadow)(
        void* renderer_ctx,
        const char* text,
        float x, float y,
        uint32_t color,
        float blur
    );

    /**
     * Render a box shadow effect
     * @param renderer_ctx Renderer-specific context
     * @param x X position in pixels
     * @param y Y position in pixels
     * @param w Width in pixels
     * @param h Height in pixels
     * @param offset_x Shadow offset X in pixels
     * @param offset_y Shadow offset Y in pixels
     * @param blur Blur radius in pixels
     * @param color Shadow color in RGBA format
     */
    void (*render_box_shadow)(
        void* renderer_ctx,
        float x, float y,
        float w, float h,
        float offset_x, float offset_y,
        float blur,
        uint32_t color
    );
} DesktopEffectsOps;

#ifdef __cplusplus
}
#endif

#endif // DESKTOP_EFFECTS_H
