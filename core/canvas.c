#include "include/kryon.h"
#include "include/kryon_canvas.h"
#include <string.h>
#include <math.h>
#include <stdlib.h>

// ============================================================================
// Global Canvas State
// ============================================================================

kryon_canvas_draw_state_t* g_canvas = NULL;  // Exposed for external access
static kryon_cmd_buf_t* g_command_buffer = NULL;

// ============================================================================
// Canvas Lifecycle and State Management
// ============================================================================

void kryon_canvas_init(uint16_t width, uint16_t height) {
    (void)width;   // unused for now
    (void)height;  // unused for now

    if (g_canvas != NULL) {
        kryon_canvas_shutdown();
    }

#if KRYON_NO_HEAP
    // For MCU, use static allocation
    static kryon_canvas_draw_state_t static_canvas;
    static kryon_cmd_buf_t static_cmd_buf;
    g_canvas = &static_canvas;
    g_command_buffer = &static_cmd_buf;
#else
    // For desktop, use dynamic allocation
    g_canvas = (kryon_canvas_draw_state_t*)malloc(sizeof(kryon_canvas_draw_state_t));
    if (g_canvas == NULL) return;

    g_command_buffer = (kryon_cmd_buf_t*)malloc(sizeof(kryon_cmd_buf_t));
    if (g_command_buffer == NULL) {
        free(g_canvas);
        g_canvas = NULL;
        return;
    }
#endif

    // Initialize canvas state
    memset(g_canvas, 0, sizeof(kryon_canvas_draw_state_t));
    memset(g_command_buffer, 0, sizeof(kryon_cmd_buf_t));

    // Set default state
    g_canvas->color = KRYON_COLOR_WHITE;
    g_canvas->background_color = KRYON_COLOR_BLACK;
    g_canvas->line_width = KRYON_FP_FROM_INT(1);
    g_canvas->line_style = KRYON_LINE_SOLID;
    g_canvas->line_join = KRYON_LINE_JOIN_MITER;
    g_canvas->font_id = 0;
    g_canvas->font_size = KRYON_FP_FROM_INT(16);
    g_canvas->blend_mode = KRYON_BLEND_ALPHA;

    // Initialize command buffer
    kryon_cmd_buf_init(g_command_buffer);
}

void kryon_canvas_shutdown(void) {
    if (g_canvas == NULL) return;

    if (g_command_buffer) {
        kryon_cmd_buf_clear(g_command_buffer);
#if !KRYON_NO_HEAP
        free(g_command_buffer);
#endif
        g_command_buffer = NULL;
    }

#if !KRYON_NO_HEAP
    free(g_canvas);
#endif
    g_canvas = NULL;
}

void kryon_canvas_resize(uint16_t width, uint16_t height) {
    (void)width;   // unused for now
    (void)height;  // unused for now
    // TODO: Implement clip stack when available
}

kryon_canvas_draw_state_t* kryon_canvas_get_state(void) {
    return g_canvas;
}

void kryon_canvas_clear(void) {
    if (g_canvas == NULL) return;
    kryon_canvas_clear_color(g_canvas->background_color);
}

void kryon_canvas_clear_color(uint32_t color) {
    if (g_canvas == NULL || g_command_buffer == NULL) return;

    // Clear the entire canvas with the specified color
    // TODO: Use actual canvas dimensions when available
    // For now, draw a large rectangle
    kryon_draw_rect(g_command_buffer, 0, 0, 800, 600, color);
}

// ============================================================================
// Drawing State Management (Love2D Style)
// ============================================================================

void kryon_canvas_set_color(uint32_t color) {
    if (g_canvas == NULL) return;
    g_canvas->color = color;
    g_canvas->dirty.color = true;
}

void kryon_canvas_set_color_rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    kryon_canvas_set_color(kryon_color_rgba(r, g, b, a));
}

void kryon_canvas_set_color_rgb(uint8_t r, uint8_t g, uint8_t b) {
    kryon_canvas_set_color(kryon_color_rgb(r, g, b));
}

uint32_t kryon_canvas_get_color(void) {
    return g_canvas ? g_canvas->color : 0;
}

void kryon_canvas_set_background_color(uint32_t color) {
    if (g_canvas == NULL) return;
    g_canvas->background_color = color;
}

void kryon_canvas_set_background_color_rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    kryon_canvas_set_background_color(kryon_color_rgba(r, g, b, a));
}

void kryon_canvas_set_line_width(kryon_fp_t width) {
    if (g_canvas == NULL) return;
    g_canvas->line_width = width;
    g_canvas->dirty.line_width = true;
}

kryon_fp_t kryon_canvas_get_line_width(void) {
    return g_canvas ? g_canvas->line_width : KRYON_FP_FROM_INT(1);
}

void kryon_canvas_set_line_style(kryon_line_style_t style) {
    if (g_canvas == NULL) return;
    g_canvas->line_style = style;
}

kryon_line_style_t kryon_canvas_get_line_style(void) {
    return g_canvas ? g_canvas->line_style : KRYON_LINE_SOLID;
}

void kryon_canvas_set_line_join(kryon_line_join_t join) {
    if (g_canvas == NULL) return;
    g_canvas->line_join = join;
}

kryon_line_join_t kryon_canvas_get_line_join(void) {
    return g_canvas ? g_canvas->line_join : KRYON_LINE_JOIN_MITER;
}

void kryon_canvas_set_font(uint16_t font_id) {
    if (g_canvas == NULL) return;
    g_canvas->font_id = font_id;
    g_canvas->dirty.font = true;
}

uint16_t kryon_canvas_get_font(void) {
    return g_canvas ? g_canvas->font_id : 0;
}

void kryon_canvas_set_blend_mode(kryon_blend_mode_t mode) {
    if (g_canvas == NULL) return;
    g_canvas->blend_mode = mode;
    g_canvas->dirty.blend_mode = true;
}

kryon_blend_mode_t kryon_canvas_get_blend_mode(void) {
    return g_canvas ? g_canvas->blend_mode : KRYON_BLEND_ALPHA;
}

// ============================================================================
// Transform System (Love2D Style)
// ============================================================================
// TODO: Implement transform stack system

void kryon_canvas_origin(void) {
    if (g_canvas == NULL) return;
    // TODO: Reset transform stack
}

void kryon_canvas_translate(kryon_fp_t x, kryon_fp_t y) {
    (void)x; (void)y;
    if (g_canvas == NULL) return;
    // TODO: Implement translate
}

void kryon_canvas_rotate(kryon_fp_t angle) {
    (void)angle;
    if (g_canvas == NULL) return;
    // TODO: Implement rotate
}

void kryon_canvas_scale(kryon_fp_t sx, kryon_fp_t sy) {
    (void)sx; (void)sy;
    if (g_canvas == NULL) return;
    // TODO: Implement scale
}

void kryon_canvas_shear(kryon_fp_t kx, kryon_fp_t ky) {
    (void)kx; (void)ky;
    if (g_canvas == NULL) return;
    // TODO: Implement shear
}

void kryon_canvas_push(void) {
    if (g_canvas == NULL) return;
    // TODO: Push transform stack
}

void kryon_canvas_pop(void) {
    if (g_canvas == NULL) return;
    // TODO: Pop transform stack
}

void kryon_canvas_get_transform(kryon_fp_t matrix[6]) {
    if (g_canvas == NULL || matrix == NULL) return;
    // TODO: Get current transform
    // Return identity matrix for now
    matrix[0] = KRYON_FP_FROM_INT(1);
    matrix[1] = KRYON_FP_FROM_INT(0);
    matrix[2] = KRYON_FP_FROM_INT(0);
    matrix[3] = KRYON_FP_FROM_INT(1);
    matrix[4] = KRYON_FP_FROM_INT(0);
    matrix[5] = KRYON_FP_FROM_INT(0);
}

// ============================================================================
// Geometry Utilities for Canvas
// ============================================================================

uint16_t kryon_canvas_tessellate_circle(kryon_fp_t radius, kryon_fp_t** vertices) {
    if (radius <= 0 || vertices == NULL) return 0;

    // Calculate number of segments based on radius
    uint16_t segments = KRYON_DEFAULT_CIRCLE_SEGMENTS;
    if (radius < KRYON_FP_FROM_INT(10)) {
        segments = 16;
    } else if (radius > KRYON_FP_FROM_INT(100)) {
        segments = 64;
    }

    // Allocate vertex array (x, y pairs)
#if KRYON_NO_HEAP
    static kryon_fp_t static_vertices[128]; // Maximum segments
    if (segments * 2 > 128) segments = 64;
    *vertices = static_vertices;
#else
    *vertices = (kryon_fp_t*)malloc(segments * 2 * sizeof(kryon_fp_t));
    if (*vertices == NULL) return 0;
#endif

    // Generate circle vertices
    for (uint16_t i = 0; i < segments; i++) {
        kryon_fp_t angle = KRYON_FP_FROM_INT(i * 360) / KRYON_FP_FROM_INT(segments);

#if !KRYON_NO_FLOAT
        float angle_rad = kryon_fp_to_float(angle) * KRYON_PI / 180.0f;
        float cos_a = cosf(angle_rad);
        float sin_a = sinf(angle_rad);

        (*vertices)[i * 2] = kryon_fp_from_float(cos_a * kryon_fp_to_float(radius));
        (*vertices)[i * 2 + 1] = kryon_fp_from_float(sin_a * kryon_fp_to_float(radius));
#else
        // For MCU, use integer approximation or lookup
        // Simplified - just create a regular polygon
        int32_t angle_int = KRYON_FP_TO_INT(angle);
        kryon_fp_t cos_val = KRYON_FP_FROM_INT(1); // Placeholder
        kryon_fp_t sin_val = KRYON_FP_FROM_INT(0); // Placeholder

        // Handle 90-degree increments
        if (angle_int % 90 == 0) {
            int32_t quadrant = (angle_int / 90) % 4;
            switch (quadrant) {
                case 0: cos_val = KRYON_FP_FROM_INT(1); sin_val = KRYON_FP_FROM_INT(0); break;
                case 1: cos_val = KRYON_FP_FROM_INT(0); sin_val = KRYON_FP_FROM_INT(1); break;
                case 2: cos_val = KRYON_FP_FROM_INT(-1); sin_val = KRYON_FP_FROM_INT(0); break;
                case 3: cos_val = KRYON_FP_FROM_INT(0); sin_val = KRYON_FP_FROM_INT(-1); break;
            }
        }

        (*vertices)[i * 2] = KRYON_FP_MUL(cos_val, radius);
        (*vertices)[i * 2 + 1] = KRYON_FP_MUL(sin_val, radius);
#endif
    }

    return segments;
}

uint16_t kryon_canvas_tessellate_ellipse(kryon_fp_t rx, kryon_fp_t ry, kryon_fp_t** vertices) {
    if (rx <= 0 || ry <= 0 || vertices == NULL) return 0;

    // Calculate number of segments based on average radius
    kryon_fp_t avg_radius = KRYON_FP_DIV((rx + ry), KRYON_FP_FROM_INT(2));
    uint16_t segments = KRYON_DEFAULT_CIRCLE_SEGMENTS;
    if (avg_radius < KRYON_FP_FROM_INT(10)) {
        segments = 16;
    } else if (avg_radius > KRYON_FP_FROM_INT(100)) {
        segments = 64;
    }

    // Allocate vertex array
#if KRYON_NO_HEAP
    static kryon_fp_t static_vertices[128];
    if (segments * 2 > 128) segments = 64;
    *vertices = static_vertices;
#else
    *vertices = (kryon_fp_t*)malloc(segments * 2 * sizeof(kryon_fp_t));
    if (*vertices == NULL) return 0;
#endif

    // Generate ellipse vertices
    for (uint16_t i = 0; i < segments; i++) {
        kryon_fp_t angle = KRYON_FP_FROM_INT(i * 360) / KRYON_FP_FROM_INT(segments);

#if !KRYON_NO_FLOAT
        float angle_rad = kryon_fp_to_float(angle) * KRYON_PI / 180.0f;
        float cos_a = cosf(angle_rad);
        float sin_a = sinf(angle_rad);

        (*vertices)[i * 2] = kryon_fp_from_float(cos_a * kryon_fp_to_float(rx));
        (*vertices)[i * 2 + 1] = kryon_fp_from_float(sin_a * kryon_fp_to_float(ry));
#else
        // Simplified ellipse for MCU
        int32_t angle_int = KRYON_FP_TO_INT(angle);
        kryon_fp_t cos_val = KRYON_FP_FROM_INT(1);
        kryon_fp_t sin_val = KRYON_FP_FROM_INT(0);

        // Handle main points
        if (angle_int % 90 == 0) {
            int32_t quadrant = (angle_int / 90) % 4;
            switch (quadrant) {
                case 0: cos_val = KRYON_FP_FROM_INT(1); sin_val = KRYON_FP_FROM_INT(0); break;
                case 1: cos_val = KRYON_FP_FROM_INT(0); sin_val = KRYON_FP_FROM_INT(1); break;
                case 2: cos_val = KRYON_FP_FROM_INT(-1); sin_val = KRYON_FP_FROM_INT(0); break;
                case 3: cos_val = KRYON_FP_FROM_INT(0); sin_val = KRYON_FP_FROM_INT(-1); break;
            }
        }

        (*vertices)[i * 2] = KRYON_FP_MUL(cos_val, rx);
        (*vertices)[i * 2 + 1] = KRYON_FP_MUL(sin_val, ry);
#endif
    }

    return segments;
}

void kryon_canvas_triangle_fan(const kryon_fp_t* vertices, uint16_t vertex_count, kryon_cmd_buf_t* buf) {
    if (vertices == NULL || vertex_count < 3 || buf == NULL) return;

    // NOTE: Triangle fan rendering for filled shapes is not yet implemented
    // The renderer doesn't support filled triangles/polygons natively
    // For now, we approximate filled circles with line loops (outline only)
    // TODO: Add proper filled polygon rendering to the renderer

    // Just draw the outline as a line loop for now
    kryon_canvas_line_loop(vertices, vertex_count, buf);
}

void kryon_canvas_line_loop(const kryon_fp_t* vertices, uint16_t vertex_count, kryon_cmd_buf_t* buf) {
    if (vertices == NULL || vertex_count < 2 || buf == NULL) return;

    uint32_t color = g_canvas->color;
    kryon_fp_t line_width = g_canvas->line_width;

    // Draw line segments (transforms not yet implemented)
    for (uint16_t i = 0; i < vertex_count; i++) {
        kryon_fp_t x1 = vertices[i * 2];
        kryon_fp_t y1 = vertices[i * 2 + 1];

        kryon_fp_t x2 = vertices[((i + 1) % vertex_count) * 2];
        kryon_fp_t y2 = vertices[((i + 1) % vertex_count) * 2 + 1];

        // Draw line (multiple times for thickness if needed)
        int32_t width_int = KRYON_FP_TO_INT(line_width);
        if (width_int <= 1) {
            kryon_draw_line(buf,
                           KRYON_FP_TO_INT(x1), KRYON_FP_TO_INT(y1),
                           KRYON_FP_TO_INT(x2), KRYON_FP_TO_INT(y2), color);
        } else {
            // Draw thick lines by drawing multiple offset lines
            for (int32_t offset = -(width_int / 2); offset <= (width_int / 2); offset++) {
                kryon_draw_line(buf,
                               KRYON_FP_TO_INT(x1), KRYON_FP_TO_INT(y1 + offset),
                               KRYON_FP_TO_INT(x2), KRYON_FP_TO_INT(y2 + offset), color);
            }
        }
    }
}

// ============================================================================
// Basic Drawing Primitives (Love2D Style)
// ============================================================================

void kryon_canvas_rectangle(kryon_draw_mode_t mode, kryon_fp_t x, kryon_fp_t y, kryon_fp_t width, kryon_fp_t height) {
    if (g_canvas == NULL || g_command_buffer == NULL) return;

    uint32_t color = g_canvas->color;

    // Transform rectangle corners
    kryon_fp_t corners[8] = {x, y, x + width, y, x + width, y + height, x, y + height};

    if (mode == KRYON_DRAW_FILL) {
        // Draw filled rectangle using a single rect command
        kryon_fp_t transformed_x, transformed_y;
        kryon_fp_t transformed_w, transformed_h;

        // Use coordinates directly (transforms not yet implemented)
        transformed_x = x;
        transformed_y = y;
        transformed_w = width;
        transformed_h = height;

        // Width and height are already set correctly above

        kryon_draw_rect(g_command_buffer,
                       KRYON_FP_TO_INT(transformed_x),
                       KRYON_FP_TO_INT(transformed_y),
                       (uint16_t)KRYON_FP_TO_INT(transformed_w),
                       (uint16_t)KRYON_FP_TO_INT(transformed_h),
                       color);
    } else {
        // Draw outline using line loop
        kryon_canvas_line_loop(corners, 4, g_command_buffer);
    }
}

void kryon_canvas_circle(kryon_draw_mode_t mode, kryon_fp_t x, kryon_fp_t y, kryon_fp_t radius) {
    if (g_canvas == NULL || g_command_buffer == NULL || radius <= 0) return;

    (void)mode;  // Mode ignored for now - renderer doesn't support filled circles yet

    // Use the arc command to draw a full circle (0-360 degrees)
    // This is much more efficient than tessellating into 32+ line segments
    kryon_draw_arc(g_command_buffer,
                  KRYON_FP_TO_INT(x),
                  KRYON_FP_TO_INT(y),
                  (uint16_t)KRYON_FP_TO_INT(radius),
                  0,    // start angle (0 degrees)
                  360,  // end angle (360 degrees = full circle)
                  g_canvas->color);
}

void kryon_canvas_ellipse(kryon_draw_mode_t mode, kryon_fp_t x, kryon_fp_t y, kryon_fp_t rx, kryon_fp_t ry) {
    if (g_canvas == NULL || g_command_buffer == NULL || rx <= 0 || ry <= 0) return;

    // If it's a circle (rx == ry), use the optimized circle command
    if (rx == ry) {
        kryon_canvas_circle(mode, x, y, rx);
        return;
    }

    // Otherwise, tessellate the ellipse and draw as line loop
    // NOTE: True filled ellipses are not yet supported by the renderer
    kryon_fp_t* vertices;
    uint16_t vertex_count = kryon_canvas_tessellate_ellipse(rx, ry, &vertices);
    if (vertex_count == 0) return;

    // Translate ellipse to center position
    for (uint16_t i = 0; i < vertex_count; i++) {
        vertices[i * 2] = vertices[i * 2] + x;
        vertices[i * 2 + 1] = vertices[i * 2 + 1] + y;
    }

    // Draw as line loop (outline) - same for both fill and line mode for now
    kryon_canvas_line_loop(vertices, vertex_count, g_command_buffer);

#if !KRYON_NO_HEAP
    free(vertices);
#endif
}

void kryon_canvas_polygon(kryon_draw_mode_t mode, const kryon_fp_t* vertices, uint16_t vertex_count) {
    if (g_canvas == NULL || g_command_buffer == NULL || vertices == NULL || vertex_count < 3) return;

    if (mode == KRYON_DRAW_FILL) {
        kryon_canvas_triangle_fan(vertices, vertex_count, g_command_buffer);
    } else {
        kryon_canvas_line_loop(vertices, vertex_count, g_command_buffer);
    }
}

void kryon_canvas_line(kryon_fp_t x1, kryon_fp_t y1, kryon_fp_t x2, kryon_fp_t y2) {
    if (g_canvas == NULL || g_command_buffer == NULL) return;

    // Use coordinates directly (transforms not yet implemented)

    uint32_t color = g_canvas->color;
    kryon_fp_t line_width = g_canvas->line_width;

    // Draw line (multiple times for thickness if needed)
    int32_t width_int = KRYON_FP_TO_INT(line_width);
    if (width_int <= 1) {
        kryon_draw_line(g_command_buffer,
                       KRYON_FP_TO_INT(x1), KRYON_FP_TO_INT(y1),
                       KRYON_FP_TO_INT(x2), KRYON_FP_TO_INT(y2), color);
    } else {
        // Draw thick lines by drawing multiple offset lines
        for (int32_t offset = -(width_int / 2); offset <= (width_int / 2); offset++) {
            kryon_draw_line(g_command_buffer,
                           KRYON_FP_TO_INT(x1), KRYON_FP_TO_INT(y1 + offset),
                           KRYON_FP_TO_INT(x2), KRYON_FP_TO_INT(y2 + offset), color);
        }
    }
}

void kryon_canvas_line_points(const kryon_fp_t* points, uint16_t point_count) {
    if (points == NULL || point_count < 2) return;

    for (uint16_t i = 0; i < point_count - 1; i++) {
        kryon_canvas_line(points[i * 2], points[i * 2 + 1],
                         points[(i + 1) * 2], points[(i + 1) * 2 + 1]);
    }
}

void kryon_canvas_point(kryon_fp_t x, kryon_fp_t y) {
    if (g_canvas == NULL || g_command_buffer == NULL) return;

    // Use coordinates directly (transforms not yet implemented)

    uint32_t color = g_canvas->color;

    // Draw point as a 1x1 rectangle
    kryon_draw_rect(g_command_buffer,
                   KRYON_FP_TO_INT(x), KRYON_FP_TO_INT(y),
                   1, 1, color);
}

void kryon_canvas_points(const kryon_fp_t* points, uint16_t point_count) {
    if (points == NULL || point_count == 0) return;

    for (uint16_t i = 0; i < point_count; i++) {
        kryon_canvas_point(points[i * 2], points[i * 2 + 1]);
    }
}

void kryon_canvas_arc(kryon_draw_mode_t mode, kryon_fp_t cx, kryon_fp_t cy, kryon_fp_t radius,
                     kryon_fp_t start_angle, kryon_fp_t end_angle) {
    if (g_canvas == NULL || g_command_buffer == NULL || radius <= 0) return;

    // Convert angles from radians to degrees if needed
    kryon_fp_t start_deg = start_angle;
    kryon_fp_t end_deg = end_angle;

#if !KRYON_NO_FLOAT
    // Assume input is in radians, convert to degrees
    start_deg = kryon_fp_from_float(kryon_fp_to_float(start_angle) * 180.0f / KRYON_PI);
    end_deg = kryon_fp_from_float(kryon_fp_to_float(end_angle) * 180.0f / KRYON_PI);
#endif

    // Use coordinates directly (transforms not yet implemented)
    kryon_fp_t transformed_cx = cx;
    kryon_fp_t transformed_cy = cy;

    kryon_draw_arc(g_command_buffer,
                  KRYON_FP_TO_INT(transformed_cx),
                  KRYON_FP_TO_INT(transformed_cy),
                  (uint16_t)KRYON_FP_TO_INT(radius),
                  KRYON_FP_TO_INT(start_deg),
                  KRYON_FP_TO_INT(end_deg),
                  g_canvas->color);
}

// ============================================================================
// Text Rendering (Love2D Style)
// ============================================================================

void kryon_canvas_print(const char* text, kryon_fp_t x, kryon_fp_t y) {
    if (g_canvas == NULL || g_command_buffer == NULL || text == NULL) return;

    // Use coordinates directly (transforms not yet implemented)

    kryon_draw_text(g_command_buffer, text,
                   KRYON_FP_TO_INT(x), KRYON_FP_TO_INT(y),
                   g_canvas->font_id, g_canvas->color);
}

void kryon_canvas_printf(const char* text, kryon_fp_t x, kryon_fp_t y, kryon_fp_t wrap_limit, int32_t align) {
    // For now, just call print (word wrapping and alignment would be implemented later)
    kryon_canvas_print(text, x, y);
}

kryon_fp_t kryon_canvas_get_text_width(const char* text) {
    // Placeholder - would need font metrics from renderer
    if (text == NULL) return KRYON_FP_FROM_INT(0);
    return KRYON_FP_FROM_INT(strlen(text) * 8); // Assume 8 pixels per character
}

kryon_fp_t kryon_canvas_get_text_height(void) {
    return g_canvas ? g_canvas->font_size : KRYON_FP_FROM_INT(16);
}

kryon_fp_t kryon_canvas_get_font_size(void) {
    return g_canvas ? g_canvas->font_size : KRYON_FP_FROM_INT(16);
}

// ============================================================================
// Canvas Command Buffer Access
// ============================================================================

kryon_cmd_buf_t* kryon_canvas_get_command_buffer(void) {
    return g_command_buffer;
}