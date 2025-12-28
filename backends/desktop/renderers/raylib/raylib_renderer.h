/**
 * Raylib Renderer Backend Header
 */

#ifndef RAYLIB_RENDERER_H
#define RAYLIB_RENDERER_H

#include <raylib.h>
#include "../../desktop_internal.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// RAYLIB RENDERER DATA
// ============================================================================

typedef struct {
    int window_width;
    int window_height;
    Font default_font;
    char default_font_path[512];
    int default_font_size;
} RaylibRendererData;

static inline RaylibRendererData* raylib_get_data(DesktopIRRenderer* renderer) {
    if (!renderer || !renderer->ops) return NULL;
    return (RaylibRendererData*)renderer->ops->backend_data;
}

// ============================================================================
// RAYLIB FONT MANAGEMENT
// ============================================================================

Font raylib_load_cached_font(const char* path, int size);
bool raylib_resolve_font_path(const char* family, bool bold, bool italic,
                              char* out_path, size_t max_len);
float raylib_measure_text_width(Font font, const char* text, float font_size);
float raylib_get_font_height(Font font, float font_size);
float raylib_get_font_ascent(Font font, float font_size);
float raylib_get_font_descent(Font font, float font_size);
void raylib_fonts_cleanup();

// ============================================================================
// RAYLIB VISUAL EFFECTS (raylib_effects.c)
// ============================================================================

void raylib_render_linear_gradient(Rectangle rect, IRGradient* gradient);
void raylib_render_radial_gradient(Rectangle rect, IRGradient* gradient);
void raylib_render_conic_gradient(Rectangle rect, IRGradient* gradient);
void raylib_render_rounded_rect(Rectangle rect, float top_left, float top_right,
                                float bottom_right, float bottom_left, Color color);
void raylib_render_text_with_shadow(Font font, const char* text, Vector2 position,
                                    float font_size, Color text_color,
                                    float shadow_offset_x, float shadow_offset_y,
                                    Color shadow_color);
void raylib_set_blend_mode(int mode);
void raylib_reset_blend_mode();

// ============================================================================
// RAYLIB LAYOUT & TEXT MEASUREMENT (raylib_layout.c)
// ============================================================================

int raylib_wrap_text_into_lines(const char* text, float max_width, Font font,
                                 float font_size, char*** out_lines);
void raylib_text_measure_callback(const char* text, const char* font_family,
                                   float font_size, float max_width,
                                   float* out_width, float* out_height,
                                   void* user_data);

// ============================================================================
// RAYLIB OPERATIONS
// ============================================================================

DesktopRendererOps* desktop_raylib_get_ops(void);

#ifdef __cplusplus
}
#endif

#endif // RAYLIB_RENDERER_H
