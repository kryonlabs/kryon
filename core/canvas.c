#include "include/kryon.h"
#include "include/kryon_canvas.h"
#include <string.h>
#include <math.h>
#include <stdlib.h>

// ============================================================================
// Global Canvas State
// ============================================================================

static kryon_canvas_state_t* g_canvas = NULL;
static kryon_cmd_buf_t* g_command_buffer = NULL;

// ============================================================================
// Canvas Lifecycle and State Management
// ============================================================================

void kryon_canvas_init(uint16_t width, uint16_t height) {
    if (g_canvas != NULL) {
        kryon_canvas_shutdown();
    }

#if KRYON_NO_HEAP
    // For MCU, use static allocation
    static kryon_canvas_state_t static_canvas;
    static kryon_cmd_buf_t static_cmd_buf;
    g_canvas = &static_canvas;
    g_command_buffer = &static_cmd_buf;
#else
    // For desktop, use dynamic allocation
    g_canvas = (kryon_canvas_state_t*)malloc(sizeof(kryon_canvas_state_t));
    if (g_canvas == NULL) return;

    g_command_buffer = (kryon_cmd_buf_t*)malloc(sizeof(kryon_cmd_buf_t));
    if (g_command_buffer == NULL) {
        free(g_canvas);
        g_canvas = NULL;
        return;
    }
#endif

    // Initialize canvas state
    memset(g_canvas, 0, sizeof(kryon_canvas_state_t));
    memset(g_command_buffer, 0, sizeof(kryon_cmd_buf_t));

    // Initialize transform and clip stacks
    kryon_transform_stack_init(&g_canvas->transform_stack);
    kryon_clip_stack_init(&g_canvas->clip_stack, width, height);

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
    if (g_canvas == NULL) return;
    kryon_clip_stack_init(&g_canvas->clip_stack, width, height);
}

kryon_canvas_state_t* kryon_canvas_get_state(void) {
    return g_canvas;
}

void kryon_canvas_clear(void) {
    if (g_canvas == NULL) return;
    kryon_canvas_clear_color(g_canvas->background_color);
}

void kryon_canvas_clear_color(uint32_t color) {
    if (g_canvas == NULL || g_command_buffer == NULL) return;

    // Clear the entire canvas with the specified color
    // For now, we'll draw a rectangle covering the entire canvas area
    int16_t clip_x, clip_y;
    uint16_t clip_w, clip_h;
    kryon_clip_stack_get(&g_canvas->clip_stack, &clip_x, &clip_y, &clip_w, &clip_h);

    kryon_draw_rect(g_command_buffer, clip_x, clip_y, clip_w, clip_h, color);
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

void kryon_canvas_origin(void) {
    if (g_canvas == NULL) return;
    kryon_transform_stack_init(&g_canvas->transform_stack);
    g_canvas->dirty.transform = true;
}

void kryon_canvas_translate(kryon_fp_t x, kryon_fp_t y) {
    if (g_canvas == NULL) return;
    kryon_transform_stack_translate(&g_canvas->transform_stack, x, y);
    g_canvas->dirty.transform = true;
}

void kryon_canvas_rotate(kryon_fp_t angle) {
    if (g_canvas == NULL) return;
    kryon_transform_stack_rotate(&g_canvas->transform_stack, angle);
    g_canvas->dirty.transform = true;
}

void kryon_canvas_scale(kryon_fp_t sx, kryon_fp_t sy) {
    if (g_canvas == NULL) return;
    kryon_transform_stack_scale(&g_canvas->transform_stack, sx, sy);
    g_canvas->dirty.transform = true;
}

void kryon_canvas_shear(kryon_fp_t kx, kryon_fp_t ky) {
    if (g_canvas == NULL) return;

    // Shear matrix: [1, kx, ky, 1, 0, 0]
    kryon_fp_t shear_matrix[6] = {
        KRYON_FP_FROM_INT(1), kx,
        ky, KRYON_FP_FROM_INT(1),
        KRYON_FP_FROM_INT(0), KRYON_FP_FROM_INT(0)
    };

    kryon_transform_stack_multiply(&g_canvas->transform_stack, shear_matrix);
    g_canvas->dirty.transform = true;
}

void kryon_canvas_push(void) {
    if (g_canvas == NULL) return;
    kryon_transform_stack_push(&g_canvas->transform_stack);
}

void kryon_canvas_pop(void) {
    if (g_canvas == NULL) return;
    kryon_transform_stack_pop(&g_canvas->transform_stack);
    g_canvas->dirty.transform = true;
}

void kryon_canvas_get_transform(kryon_fp_t matrix[6]) {
    if (g_canvas == NULL || matrix == NULL) return;
    kryon_transform_stack_get(&g_canvas->transform_stack, matrix);
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
    kryon_fp_t avg_radius = KRYON_FP_DIV(KRYON_FP_ADD(rx, ry), KRYON_FP_FROM_INT(2));
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

    // Transform vertices through current transform stack
    kryon_fp_t* transformed_vertices;
#if KRYON_NO_HEAP
    static kryon_fp_t static_transformed[256]; // Maximum 128 vertices
    if (vertex_count * 2 > 256) return;
    transformed_vertices = static_transformed;
#else
    transformed_vertices = (kryon_fp_t*)malloc(vertex_count * 2 * sizeof(kryon_fp_t));
    if (transformed_vertices == NULL) return;
#endif

    // Apply transforms
    for (uint16_t i = 0; i < vertex_count; i++) {
        kryon_fp_t x = vertices[i * 2];
        kryon_fp_t y = vertices[i * 2 + 1];
        kryon_transform_stack_transform_point(&g_canvas->transform_stack, &x, &y);
        transformed_vertices[i * 2] = x;
        transformed_vertices[i * 2 + 1] = y;
    }

    // Create triangle fan from vertices (center + pairs of vertices)
    uint32_t color = g_canvas->color;

    // First vertex is the center
    kryon_fp_t center_x = transformed_vertices[0];
    kryon_fp_t center_y = transformed_vertices[1];

    // Draw triangles from center to each edge
    for (uint16_t i = 1; i < vertex_count - 1; i++) {
        kryon_fp_t x1 = transformed_vertices[i * 2];
        kryon_fp_t y1 = transformed_vertices[i * 2 + 1];
        kryon_fp_t x2 = transformed_vertices[(i + 1) * 2];
        kryon_fp_t y2 = transformed_vertices[(i + 1) * 2 + 1];

        // Draw triangle as three lines for now (would need triangle fill command)
        kryon_draw_line(buf,
                       KRYON_FP_TO_INT(center_x), KRYON_FP_TO_INT(center_y),
                       KRYON_FP_TO_INT(x1), KRYON_FP_TO_INT(y1), color);
        kryon_draw_line(buf,
                       KRYON_FP_TO_INT(x1), KRYON_FP_TO_INT(y1),
                       KRYON_FP_TO_INT(x2), KRYON_FP_TO_INT(y2), color);
        kryon_draw_line(buf,
                       KRYON_FP_TO_INT(x2), KRYON_FP_TO_INT(y2),
                       KRYON_FP_TO_INT(center_x), KRYON_FP_TO_INT(center_y), color);
    }

#if !KRYON_NO_HEAP
    free(transformed_vertices);
#endif
}

void kryon_canvas_line_loop(const kryon_fp_t* vertices, uint16_t vertex_count, kryon_cmd_buf_t* buf) {
    if (vertices == NULL || vertex_count < 2 || buf == NULL) return;

    uint32_t color = g_canvas->color;
    kryon_fp_t line_width = g_canvas->line_width;

    // Transform and draw line segments
    for (uint16_t i = 0; i < vertex_count; i++) {
        kryon_fp_t x1 = vertices[i * 2];
        kryon_fp_t y1 = vertices[i * 2 + 1];

        kryon_transform_stack_transform_point(&g_canvas->transform_stack, &x1, &y1);

        kryon_fp_t x2 = vertices[((i + 1) % vertex_count) * 2];
        kryon_fp_t y2 = vertices[((i + 1) % vertex_count) * 2 + 1];

        kryon_transform_stack_transform_point(&g_canvas->transform_stack, &x2, &y2);

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

        // Transform top-left corner
        transformed_x = x;
        transformed_y = y;
        kryon_transform_stack_transform_point(&g_canvas->transform_stack, &transformed_x, &transformed_y);

        // Transform bottom-right corner to calculate width/height
        transformed_w = x + width;
        transformed_h = y + height;
        kryon_transform_stack_transform_point(&g_canvas->transform_stack, &transformed_w, &transformed_h);

        transformed_w = KRYON_FP_SUB(transformed_w, transformed_x);
        transformed_h = KRYON_FP_SUB(transformed_h, transformed_y);

        kryon_draw_rect(g_command_buffer,
                       KRYON_FP_TO_INT(transformed_x),
                       KRYON_FP_TO_INT(transformed_y),
                       KRYON_FP_TO_UINT(transformed_w),
                       KRYON_FP_TO_UINT(transformed_h),
                       color);
    } else {
        // Draw outline using line loop
        kryon_canvas_line_loop(corners, 4, g_command_buffer);
    }
}

void kryon_canvas_circle(kryon_draw_mode_t mode, kryon_fp_t x, kryon_fp_t y, kryon_fp_t radius) {
    if (g_canvas == NULL || g_command_buffer == NULL || radius <= 0) return;

    kryon_fp_t* vertices;
    uint16_t vertex_count = kryon_canvas_tessellate_circle(radius, &vertices);
    if (vertex_count == 0) return;

    // Translate circle to center position
    for (uint16_t i = 0; i < vertex_count; i++) {
        vertices[i * 2] = KRYON_FP_ADD(vertices[i * 2], x);
        vertices[i * 2 + 1] = KRYON_FP_ADD(vertices[i * 2 + 1], y);
    }

    if (mode == KRYON_DRAW_FILL) {
        kryon_canvas_triangle_fan(vertices, vertex_count, g_command_buffer);
    } else {
        kryon_canvas_line_loop(vertices, vertex_count, g_command_buffer);
    }

#if !KRYON_NO_HEAP
    free(vertices);
#endif
}

void kryon_canvas_ellipse(kryon_draw_mode_t mode, kryon_fp_t x, kryon_fp_t y, kryon_fp_t rx, kryon_fp_t ry) {
    if (g_canvas == NULL || g_command_buffer == NULL || rx <= 0 || ry <= 0) return;

    kryon_fp_t* vertices;
    uint16_t vertex_count = kryon_canvas_tessellate_ellipse(rx, ry, &vertices);
    if (vertex_count == 0) return;

    // Translate ellipse to center position
    for (uint16_t i = 0; i < vertex_count; i++) {
        vertices[i * 2] = KRYON_FP_ADD(vertices[i * 2], x);
        vertices[i * 2 + 1] = KRYON_FP_ADD(vertices[i * 2 + 1], y);
    }

    if (mode == KRYON_DRAW_FILL) {
        kryon_canvas_triangle_fan(vertices, vertex_count, g_command_buffer);
    } else {
        kryon_canvas_line_loop(vertices, vertex_count, g_command_buffer);
    }

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

    // Transform line endpoints
    kryon_transform_stack_transform_point(&g_canvas->transform_stack, &x1, &y1);
    kryon_transform_stack_transform_point(&g_canvas->transform_stack, &x2, &y2);

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

    // Transform point
    kryon_transform_stack_transform_point(&g_canvas->transform_stack, &x, &y);

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

    // Use core arc command for now
    kryon_fp_t transformed_cx = cx;
    kryon_fp_t transformed_cy = cy;
    kryon_transform_stack_transform_point(&g_canvas->transform_stack, &transformed_cx, &transformed_cy);

    kryon_draw_arc(g_command_buffer,
                  KRYON_FP_TO_INT(transformed_cx),
                  KRYON_FP_TO_INT(transformed_cy),
                  KRYON_FP_TO_UINT(radius),
                  KRYON_FP_TO_INT(start_deg),
                  KRYON_FP_TO_INT(end_deg),
                  g_canvas->color);
}

// ============================================================================
// Text Rendering (Love2D Style)
// ============================================================================

void kryon_canvas_print(const char* text, kryon_fp_t x, kryon_fp_t y) {
    if (g_canvas == NULL || g_command_buffer == NULL || text == NULL) return;

    // Transform position
    kryon_transform_stack_transform_point(&g_canvas->transform_stack, &x, &y);

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