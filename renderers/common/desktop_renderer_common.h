// desktop_renderer_common.h
//
// Common desktop renderer infrastructure
// Provides shared command execution logic for SDL3, Raylib, and other desktop backends
//

#ifndef DESKTOP_RENDERER_COMMON_H
#define DESKTOP_RENDERER_COMMON_H

#include "../../core/include/kryon.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Backend Drawing Interface
// ============================================================================

/**
 * Function pointers for backend-specific drawing operations
 * Each desktop backend (SDL3, Raylib, etc.) implements these.
 */
typedef struct DesktopBackendDrawFunctions {
    // Basic shapes
    void (*draw_rect)(float x, float y, float w, float h, uint32_t color);
    void (*draw_text)(const char* text, float x, float y, float font_size, uint32_t color);
    void (*draw_line)(float x1, float y1, float x2, float y2, uint32_t color);
    void (*draw_circle)(float cx, float cy, float radius, uint32_t color);
    void (*draw_rounded_rect)(float x, float y, float w, float h, float radius, uint32_t color);
    void (*draw_polygon)(const float* vertices, uint16_t vertex_count, uint32_t color, bool filled);

    // Clipping
    void (*set_clip)(float x, float y, float w, float h);
    void (*push_clip)(void);
    void (*pop_clip)(void);

    // Image rendering
    void (*draw_image)(void* image_data, float x, float y, float w, float h, uint32_t tint);

} DesktopBackendDrawFunctions;

// ============================================================================
// Common Command Execution
// ============================================================================

/**
 * Execute command buffer using backend-specific drawing functions
 *
 * This consolidated function handles all command types (rect, text, line, circle, etc.)
 * and delegates to the appropriate backend-specific drawing function.
 *
 * @param renderer   Kryon renderer instance
 * @param buf        Command buffer to execute
 * @param draw_funcs Backend-specific drawing functions
 * @param user_data  Optional backend-specific user data (e.g., clip stack state)
 */
void desktop_execute_commands_common(kryon_renderer_t* renderer,
                                     kryon_cmd_buf_t* buf,
                                     const DesktopBackendDrawFunctions* draw_funcs,
                                     void* user_data);

// ============================================================================
// Clip Stack Management
// ============================================================================

/**
 * Shared clip stack state for backends that need nested clipping support
 * (SDL3 has this built-in, Raylib doesn't)
 */
typedef struct {
    float x, y, w, h;
} DesktopClipRect;

typedef struct {
    DesktopClipRect clips[8];      // Stack of clip rectangles
    int depth;                    // Current stack depth
    DesktopClipRect current;       // Pending clip rectangle
} DesktopClipStack;

/**
 * Initialize clip stack
 */
void desktop_clip_stack_init(DesktopClipStack* stack);

/**
 * Push clip onto stack
 */
void desktop_clip_stack_push(DesktopClipStack* stack, float x, float y, float w, float h);

/**
 * Pop clip from stack
 */
void desktop_clip_stack_pop(DesktopClipStack* stack);

/**
 * Get current clip (top of stack or default)
 */
bool desktop_clip_stack_get_current(const DesktopClipStack* stack, DesktopClipRect* out);

// ============================================================================
// Color Conversion Helpers
// ============================================================================

/**
 * Convert RGBA color to float array [r, g, b, a] (0.0-1.0)
 */
void desktop_color_to_floats(uint32_t rgba, float* out);

/**
 * Extract color components
 */
static inline uint8_t desktop_color_r(uint32_t rgba) { return (rgba >> 24) & 0xFF; }
static inline uint8_t desktop_color_g(uint32_t rgba) { return (rgba >> 16) & 0xFF; }
static inline uint8_t desktop_color_b(uint32_t rgba) { return (rgba >> 8) & 0xFF; }
static inline uint8_t desktop_color_a(uint32_t rgba) { return rgba & 0xFF; }

#ifdef __cplusplus
}
#endif

#endif // DESKTOP_RENDERER_COMMON_H
