/**
 * @file software_renderer.h
 * @brief Kryon Software Renderer - Basic pixel buffer rendering for development
 * 
 * CPU-based renderer for development and testing, providing basic drawing
 * primitives for UI elements without GPU dependencies.
 * 
 * @version 1.0.0
 * @author Kryon Labs
 */

#ifndef KRYON_INTERNAL_SOFTWARE_RENDERER_H
#define KRYON_INTERNAL_SOFTWARE_RENDERER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include "internal/runtime.h"

// =============================================================================
// FORWARD DECLARATIONS
// =============================================================================

typedef struct KryonSoftwareRenderer KryonSoftwareRenderer;
typedef struct KryonPixelBuffer KryonPixelBuffer;
typedef struct KryonFont KryonFont;
typedef struct KryonColor KryonColor;
typedef struct KryonRect KryonRect;

// =============================================================================
// DATA STRUCTURES
// =============================================================================

/**
 * @brief RGBA color structure
 */
struct KryonColor {
    uint8_t r;  // Red (0-255)
    uint8_t g;  // Green (0-255)
    uint8_t b;  // Blue (0-255)
    uint8_t a;  // Alpha (0-255)
};

/**
 * @brief Rectangle structure
 */
struct KryonRect {
    int x;      // X position
    int y;      // Y position
    int width;  // Width
    int height; // Height
};

/**
 * @brief Pixel buffer for rendering
 */
struct KryonPixelBuffer {
    uint32_t *pixels;    // RGBA pixels (native endian)
    int width;           // Buffer width
    int height;          // Buffer height
    int stride;          // Bytes per row
    bool owns_memory;    // Whether buffer owns the pixel memory
};

/**
 * @brief Basic font structure
 */
struct KryonFont {
    const uint8_t *data;     // Font bitmap data
    int char_width;          // Character width
    int char_height;         // Character height
    int first_char;          // First character in font
    int char_count;          // Number of characters
    int spacing;             // Character spacing
};

/**
 * @brief Software renderer state
 */
struct KryonSoftwareRenderer {
    // Render target
    KryonPixelBuffer *target;        // Current render target
    KryonPixelBuffer *backbuffer;    // Back buffer for double buffering
    
    // Render state
    KryonColor clear_color;          // Clear color
    KryonColor draw_color;           // Current drawing color
    KryonRect clip_rect;             // Clipping rectangle
    bool clipping_enabled;           // Whether clipping is enabled
    
    // Text rendering
    KryonFont *current_font;         // Current font
    KryonFont *default_font;         // Built-in default font
    
    // Transformation
    float scale_x;                   // X scale factor
    float scale_y;                   // Y scale factor
    int offset_x;                    // X offset
    int offset_y;                    // Y offset
    
    // Performance
    struct {
        uint64_t pixels_drawn;       // Total pixels drawn
        uint64_t primitives_drawn;   // Total primitives drawn
        double render_time;          // Total render time
    } stats;
};

// =============================================================================
// RENDERER API
// =============================================================================

/**
 * @brief Create software renderer
 * @param width Target width
 * @param height Target height
 * @return Renderer instance or NULL on failure
 */
KryonSoftwareRenderer *kryon_software_renderer_create(int width, int height);

/**
 * @brief Destroy software renderer
 * @param renderer Renderer to destroy
 */
void kryon_software_renderer_destroy(KryonSoftwareRenderer *renderer);

/**
 * @brief Set render target
 * @param renderer Renderer instance
 * @param buffer Pixel buffer to render to (NULL to use backbuffer)
 */
void kryon_software_renderer_set_target(KryonSoftwareRenderer *renderer, KryonPixelBuffer *buffer);

/**
 * @brief Clear render target
 * @param renderer Renderer instance
 */
void kryon_software_renderer_clear(KryonSoftwareRenderer *renderer);

/**
 * @brief Present backbuffer (swap buffers)
 * @param renderer Renderer instance
 * @return Pointer to presented buffer
 */
KryonPixelBuffer *kryon_software_renderer_present(KryonSoftwareRenderer *renderer);

// =============================================================================
// DRAWING PRIMITIVES
// =============================================================================

/**
 * @brief Set drawing color
 * @param renderer Renderer instance
 * @param color Drawing color
 */
void kryon_software_renderer_set_color(KryonSoftwareRenderer *renderer, KryonColor color);

/**
 * @brief Draw pixel
 * @param renderer Renderer instance
 * @param x X coordinate
 * @param y Y coordinate
 */
void kryon_software_renderer_draw_pixel(KryonSoftwareRenderer *renderer, int x, int y);

/**
 * @brief Draw line
 * @param renderer Renderer instance
 * @param x1 Start X
 * @param y1 Start Y
 * @param x2 End X
 * @param y2 End Y
 */
void kryon_software_renderer_draw_line(KryonSoftwareRenderer *renderer, 
                                      int x1, int y1, int x2, int y2);

/**
 * @brief Draw rectangle (outline)
 * @param renderer Renderer instance
 * @param rect Rectangle to draw
 */
void kryon_software_renderer_draw_rect(KryonSoftwareRenderer *renderer, KryonRect rect);

/**
 * @brief Fill rectangle
 * @param renderer Renderer instance
 * @param rect Rectangle to fill
 */
void kryon_software_renderer_fill_rect(KryonSoftwareRenderer *renderer, KryonRect rect);

/**
 * @brief Draw rounded rectangle (outline)
 * @param renderer Renderer instance
 * @param rect Rectangle
 * @param radius Corner radius
 */
void kryon_software_renderer_draw_rounded_rect(KryonSoftwareRenderer *renderer, 
                                              KryonRect rect, int radius);

/**
 * @brief Fill rounded rectangle
 * @param renderer Renderer instance
 * @param rect Rectangle
 * @param radius Corner radius
 */
void kryon_software_renderer_fill_rounded_rect(KryonSoftwareRenderer *renderer, 
                                              KryonRect rect, int radius);

// =============================================================================
// TEXT RENDERING
// =============================================================================

/**
 * @brief Set current font
 * @param renderer Renderer instance
 * @param font Font to use (NULL for default)
 */
void kryon_software_renderer_set_font(KryonSoftwareRenderer *renderer, KryonFont *font);

/**
 * @brief Draw text
 * @param renderer Renderer instance
 * @param text Text to draw
 * @param x X position
 * @param y Y position
 */
void kryon_software_renderer_draw_text(KryonSoftwareRenderer *renderer, 
                                      const char *text, int x, int y);

/**
 * @brief Measure text dimensions
 * @param renderer Renderer instance
 * @param text Text to measure
 * @param width Output for width
 * @param height Output for height
 */
void kryon_software_renderer_measure_text(KryonSoftwareRenderer *renderer,
                                         const char *text, int *width, int *height);

// =============================================================================
// CLIPPING
// =============================================================================

/**
 * @brief Set clipping rectangle
 * @param renderer Renderer instance
 * @param rect Clipping rectangle (NULL to disable)
 */
void kryon_software_renderer_set_clip(KryonSoftwareRenderer *renderer, const KryonRect *rect);

/**
 * @brief Push clipping rectangle onto stack
 * @param renderer Renderer instance
 * @param rect New clipping rectangle
 */
void kryon_software_renderer_push_clip(KryonSoftwareRenderer *renderer, KryonRect rect);

/**
 * @brief Pop clipping rectangle from stack
 * @param renderer Renderer instance
 */
void kryon_software_renderer_pop_clip(KryonSoftwareRenderer *renderer);

// =============================================================================
// ELEMENT RENDERING
// =============================================================================

/**
 * @brief Render element tree
 * @param renderer Renderer instance
 * @param runtime Runtime instance with element tree
 */
void kryon_software_renderer_render_elements(KryonSoftwareRenderer *renderer, 
                                            KryonRuntime *runtime);

/**
 * @brief Render single element
 * @param renderer Renderer instance
 * @param element Element to render
 */
void kryon_software_renderer_render_element(KryonSoftwareRenderer *renderer,
                                           const KryonElement *element);

// =============================================================================
// PIXEL BUFFER MANAGEMENT
// =============================================================================

/**
 * @brief Create pixel buffer
 * @param width Buffer width
 * @param height Buffer height
 * @return Pixel buffer or NULL on failure
 */
KryonPixelBuffer *kryon_pixel_buffer_create(int width, int height);

/**
 * @brief Create pixel buffer from existing memory
 * @param pixels Pixel data
 * @param width Buffer width
 * @param height Buffer height
 * @param stride Bytes per row
 * @return Pixel buffer or NULL on failure
 */
KryonPixelBuffer *kryon_pixel_buffer_create_from_memory(uint32_t *pixels, int width, 
                                                        int height, int stride);

/**
 * @brief Destroy pixel buffer
 * @param buffer Buffer to destroy
 */
void kryon_pixel_buffer_destroy(KryonPixelBuffer *buffer);

/**
 * @brief Copy pixel buffer
 * @param src Source buffer
 * @param dst Destination buffer
 * @param src_rect Source rectangle (NULL for entire buffer)
 * @param dst_x Destination X
 * @param dst_y Destination Y
 */
void kryon_pixel_buffer_copy(const KryonPixelBuffer *src, KryonPixelBuffer *dst,
                            const KryonRect *src_rect, int dst_x, int dst_y);

// =============================================================================
// COLOR UTILITIES
// =============================================================================

/**
 * @brief Create color from components
 * @param r Red (0-255)
 * @param g Green (0-255)
 * @param b Blue (0-255)
 * @param a Alpha (0-255)
 * @return Color structure
 */
static inline KryonColor kryon_color_rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    KryonColor color = {r, g, b, a};
    return color;
}

/**
 * @brief Create color from hex value
 * @param hex Hex color value (0xRRGGBBAA)
 * @return Color structure
 */
static inline KryonColor kryon_color_from_hex(uint32_t hex) {
    KryonColor color = {
        (hex >> 24) & 0xFF,  // R
        (hex >> 16) & 0xFF,  // G
        (hex >> 8) & 0xFF,   // B
        hex & 0xFF           // A
    };
    return color;
}

/**
 * @brief Convert color to 32-bit pixel
 * @param color Color structure
 * @return 32-bit RGBA pixel
 */
static inline uint32_t kryon_color_to_pixel(KryonColor color) {
    return (color.r << 24) | (color.g << 16) | (color.b << 8) | color.a;
}

/**
 * @brief Blend colors with alpha
 * @param fg Foreground color
 * @param bg Background color
 * @return Blended color
 */
KryonColor kryon_color_blend(KryonColor fg, KryonColor bg);

// =============================================================================
// PREDEFINED COLORS
// =============================================================================

extern const KryonColor KRYON_COLOR_BLACK;
extern const KryonColor KRYON_COLOR_WHITE;
extern const KryonColor KRYON_COLOR_RED;
extern const KryonColor KRYON_COLOR_GREEN;
extern const KryonColor KRYON_COLOR_BLUE;
extern const KryonColor KRYON_COLOR_TRANSPARENT;

#ifdef __cplusplus
}
#endif

#endif // KRYON_INTERNAL_SOFTWARE_RENDERER_H