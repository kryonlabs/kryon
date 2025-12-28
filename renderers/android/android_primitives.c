#include "android_renderer_internal.h"
#include <string.h>

// ============================================================================
// Primitive Rendering Functions
// ============================================================================

void android_renderer_draw_rect(AndroidRenderer* renderer,
                                 float x, float y,
                                 float width, float height,
                                 uint32_t color) {
    if (!renderer) return;

    // Check if we need to flush batch
    if (renderer->vertex_count + 4 > MAX_VERTICES ||
        renderer->index_count + 6 > MAX_INDICES) {
        android_renderer_flush_batch(renderer);
    }

    // Switch to color shader
    android_shader_use(renderer, SHADER_PROGRAM_COLOR);

    // Extract color components
    uint8_t a = (color >> 24) & 0xFF;
    uint8_t r = (color >> 16) & 0xFF;
    uint8_t g = (color >> 8) & 0xFF;
    uint8_t b = color & 0xFF;

    // Apply global opacity
    a = (uint8_t)(a * renderer->global_opacity);

    // Add 4 vertices for the rectangle
    uint32_t base_index = renderer->vertex_count;

    // Top-left
    renderer->vertices[renderer->vertex_count].x = x;
    renderer->vertices[renderer->vertex_count].y = y;
    renderer->vertices[renderer->vertex_count].u = 0.0f;
    renderer->vertices[renderer->vertex_count].v = 0.0f;
    renderer->vertices[renderer->vertex_count].r = r;
    renderer->vertices[renderer->vertex_count].g = g;
    renderer->vertices[renderer->vertex_count].b = b;
    renderer->vertices[renderer->vertex_count].a = a;
    renderer->vertex_count++;

    // Top-right
    renderer->vertices[renderer->vertex_count].x = x + width;
    renderer->vertices[renderer->vertex_count].y = y;
    renderer->vertices[renderer->vertex_count].u = 1.0f;
    renderer->vertices[renderer->vertex_count].v = 0.0f;
    renderer->vertices[renderer->vertex_count].r = r;
    renderer->vertices[renderer->vertex_count].g = g;
    renderer->vertices[renderer->vertex_count].b = b;
    renderer->vertices[renderer->vertex_count].a = a;
    renderer->vertex_count++;

    // Bottom-right
    renderer->vertices[renderer->vertex_count].x = x + width;
    renderer->vertices[renderer->vertex_count].y = y + height;
    renderer->vertices[renderer->vertex_count].u = 1.0f;
    renderer->vertices[renderer->vertex_count].v = 1.0f;
    renderer->vertices[renderer->vertex_count].r = r;
    renderer->vertices[renderer->vertex_count].g = g;
    renderer->vertices[renderer->vertex_count].b = b;
    renderer->vertices[renderer->vertex_count].a = a;
    renderer->vertex_count++;

    // Bottom-left
    renderer->vertices[renderer->vertex_count].x = x;
    renderer->vertices[renderer->vertex_count].y = y + height;
    renderer->vertices[renderer->vertex_count].u = 0.0f;
    renderer->vertices[renderer->vertex_count].v = 1.0f;
    renderer->vertices[renderer->vertex_count].r = r;
    renderer->vertices[renderer->vertex_count].g = g;
    renderer->vertices[renderer->vertex_count].b = b;
    renderer->vertices[renderer->vertex_count].a = a;
    renderer->vertex_count++;

    // Add indices for 2 triangles (6 indices)
    renderer->indices[renderer->index_count++] = base_index + 0;
    renderer->indices[renderer->index_count++] = base_index + 1;
    renderer->indices[renderer->index_count++] = base_index + 2;
    renderer->indices[renderer->index_count++] = base_index + 0;
    renderer->indices[renderer->index_count++] = base_index + 2;
    renderer->indices[renderer->index_count++] = base_index + 3;
}

void android_renderer_draw_rounded_rect(AndroidRenderer* renderer,
                                         float x, float y,
                                         float width, float height,
                                         float radius,
                                         uint32_t color) {
    if (!renderer) return;

    // Check if we need to flush batch
    if (renderer->vertex_count + 4 > MAX_VERTICES ||
        renderer->index_count + 6 > MAX_INDICES) {
        android_renderer_flush_batch(renderer);
    }

    // Switch to rounded rect shader
    android_shader_use(renderer, SHADER_PROGRAM_ROUNDED_RECT);

#ifdef __ANDROID__
    // Set uniforms for size and radius
    glUniform2f(renderer->shader_programs[SHADER_PROGRAM_ROUNDED_RECT].u_size,
                width, height);
    glUniform1f(renderer->shader_programs[SHADER_PROGRAM_ROUNDED_RECT].u_radius,
                radius);
#endif

    // Extract color components
    uint8_t a = (color >> 24) & 0xFF;
    uint8_t r = (color >> 16) & 0xFF;
    uint8_t g = (color >> 8) & 0xFF;
    uint8_t b = color & 0xFF;

    // Apply global opacity
    a = (uint8_t)(a * renderer->global_opacity);

    // Add 4 vertices for the rectangle
    // Note: Using u,v fields for local position (0,0 to width,height)
    uint32_t base_index = renderer->vertex_count;

    // Top-left
    renderer->vertices[renderer->vertex_count].x = x;
    renderer->vertices[renderer->vertex_count].y = y;
    renderer->vertices[renderer->vertex_count].u = 0.0f;
    renderer->vertices[renderer->vertex_count].v = 0.0f;
    renderer->vertices[renderer->vertex_count].r = r;
    renderer->vertices[renderer->vertex_count].g = g;
    renderer->vertices[renderer->vertex_count].b = b;
    renderer->vertices[renderer->vertex_count].a = a;
    renderer->vertex_count++;

    // Top-right
    renderer->vertices[renderer->vertex_count].x = x + width;
    renderer->vertices[renderer->vertex_count].y = y;
    renderer->vertices[renderer->vertex_count].u = width;
    renderer->vertices[renderer->vertex_count].v = 0.0f;
    renderer->vertices[renderer->vertex_count].r = r;
    renderer->vertices[renderer->vertex_count].g = g;
    renderer->vertices[renderer->vertex_count].b = b;
    renderer->vertices[renderer->vertex_count].a = a;
    renderer->vertex_count++;

    // Bottom-right
    renderer->vertices[renderer->vertex_count].x = x + width;
    renderer->vertices[renderer->vertex_count].y = y + height;
    renderer->vertices[renderer->vertex_count].u = width;
    renderer->vertices[renderer->vertex_count].v = height;
    renderer->vertices[renderer->vertex_count].r = r;
    renderer->vertices[renderer->vertex_count].g = g;
    renderer->vertices[renderer->vertex_count].b = b;
    renderer->vertices[renderer->vertex_count].a = a;
    renderer->vertex_count++;

    // Bottom-left
    renderer->vertices[renderer->vertex_count].x = x;
    renderer->vertices[renderer->vertex_count].y = y + height;
    renderer->vertices[renderer->vertex_count].u = 0.0f;
    renderer->vertices[renderer->vertex_count].v = height;
    renderer->vertices[renderer->vertex_count].r = r;
    renderer->vertices[renderer->vertex_count].g = g;
    renderer->vertices[renderer->vertex_count].b = b;
    renderer->vertices[renderer->vertex_count].a = a;
    renderer->vertex_count++;

    // Add indices for 2 triangles (6 indices)
    renderer->indices[renderer->index_count++] = base_index + 0;
    renderer->indices[renderer->index_count++] = base_index + 1;
    renderer->indices[renderer->index_count++] = base_index + 2;
    renderer->indices[renderer->index_count++] = base_index + 0;
    renderer->indices[renderer->index_count++] = base_index + 2;
    renderer->indices[renderer->index_count++] = base_index + 3;
}

void android_renderer_draw_rect_outline(AndroidRenderer* renderer,
                                         float x, float y,
                                         float width, float height,
                                         float border_width,
                                         uint32_t color) {
    if (!renderer) return;

    // Draw 4 rectangles for the outline
    // Top
    android_renderer_draw_rect(renderer, x, y, width, border_width, color);

    // Right
    android_renderer_draw_rect(renderer, x + width - border_width, y + border_width,
                                border_width, height - border_width * 2, color);

    // Bottom
    android_renderer_draw_rect(renderer, x, y + height - border_width,
                                width, border_width, color);

    // Left
    android_renderer_draw_rect(renderer, x, y + border_width,
                                border_width, height - border_width * 2, color);
}

// ============================================================================
// Clipping
// ============================================================================

void android_renderer_push_clip_rect(AndroidRenderer* renderer,
                                      float x, float y,
                                      float width, float height) {
    if (!renderer) return;

    if (renderer->clip_stack_depth >= MAX_CLIP_STACK_DEPTH) {
        return;  // Stack overflow
    }

    ClipRect* clip = &renderer->clip_stack[renderer->clip_stack_depth++];
    clip->x = x;
    clip->y = y;
    clip->width = width;
    clip->height = height;

    // Flush batch before changing scissor
    android_renderer_flush_batch(renderer);

#ifdef __ANDROID__
    glEnable(GL_SCISSOR_TEST);
    glScissor((GLint)x, (GLint)(renderer->window_height - y - height),
              (GLsizei)width, (GLsizei)height);
#endif
}

void android_renderer_pop_clip_rect(AndroidRenderer* renderer) {
    if (!renderer || renderer->clip_stack_depth == 0) {
        return;
    }

    // Flush batch before changing scissor
    android_renderer_flush_batch(renderer);

    renderer->clip_stack_depth--;

    if (renderer->clip_stack_depth == 0) {
#ifdef __ANDROID__
        glDisable(GL_SCISSOR_TEST);
#endif
    } else {
        // Restore previous clip rect
        ClipRect* clip = &renderer->clip_stack[renderer->clip_stack_depth - 1];
#ifdef __ANDROID__
        glScissor((GLint)clip->x,
                  (GLint)(renderer->window_height - clip->y - clip->height),
                  (GLsizei)clip->width,
                  (GLsizei)clip->height);
#endif
    }
}

// ============================================================================
// Opacity
// ============================================================================

void android_renderer_set_opacity(AndroidRenderer* renderer, float opacity) {
    if (!renderer) return;

    renderer->global_opacity = opacity;
}

void android_renderer_reset_opacity(AndroidRenderer* renderer) {
    if (!renderer) return;

    renderer->global_opacity = 1.0f;
}

// ============================================================================
// Gradient Rendering
// ============================================================================

void android_renderer_draw_gradient_rect(AndroidRenderer* renderer,
                                          float x, float y,
                                          float width, float height,
                                          uint32_t color_start,
                                          uint32_t color_end,
                                          bool vertical) {
    if (!renderer) return;

    // Check if we need to flush batch
    if (renderer->vertex_count + 4 > MAX_VERTICES ||
        renderer->index_count + 6 > MAX_INDICES) {
        android_renderer_flush_batch(renderer);
    }

    // Switch to gradient shader
    android_shader_use(renderer, SHADER_PROGRAM_GRADIENT);

    // Extract color components for start color
    uint8_t a1 = (color_start >> 24) & 0xFF;
    uint8_t r1 = (color_start >> 16) & 0xFF;
    uint8_t g1 = (color_start >> 8) & 0xFF;
    uint8_t b1 = color_start & 0xFF;

    // Extract color components for end color
    uint8_t a2 = (color_end >> 24) & 0xFF;
    uint8_t r2 = (color_end >> 16) & 0xFF;
    uint8_t g2 = (color_end >> 8) & 0xFF;
    uint8_t b2 = color_end & 0xFF;

    // Apply global opacity
    a1 = (uint8_t)(a1 * renderer->global_opacity);
    a2 = (uint8_t)(a2 * renderer->global_opacity);

#ifdef __ANDROID__
    // Set gradient color uniforms
    glUniform4f(renderer->shader_programs[SHADER_PROGRAM_GRADIENT].u_color_start,
                r1 / 255.0f, g1 / 255.0f, b1 / 255.0f, a1 / 255.0f);
    glUniform4f(renderer->shader_programs[SHADER_PROGRAM_GRADIENT].u_color_end,
                r2 / 255.0f, g2 / 255.0f, b2 / 255.0f, a2 / 255.0f);
#endif

    // Add 4 vertices for the rectangle
    // Note: Using u field for gradient t value (0.0 to 1.0)
    uint32_t base_index = renderer->vertex_count;

    if (vertical) {
        // Vertical gradient (top to bottom)
        // Top-left
        renderer->vertices[renderer->vertex_count].x = x;
        renderer->vertices[renderer->vertex_count].y = y;
        renderer->vertices[renderer->vertex_count].u = 0.0f;  // gradient t = 0
        renderer->vertices[renderer->vertex_count].v = 0.0f;
        renderer->vertices[renderer->vertex_count].r = 0;
        renderer->vertices[renderer->vertex_count].g = 0;
        renderer->vertices[renderer->vertex_count].b = 0;
        renderer->vertices[renderer->vertex_count].a = 0;
        renderer->vertex_count++;

        // Top-right
        renderer->vertices[renderer->vertex_count].x = x + width;
        renderer->vertices[renderer->vertex_count].y = y;
        renderer->vertices[renderer->vertex_count].u = 0.0f;  // gradient t = 0
        renderer->vertices[renderer->vertex_count].v = 0.0f;
        renderer->vertices[renderer->vertex_count].r = 0;
        renderer->vertices[renderer->vertex_count].g = 0;
        renderer->vertices[renderer->vertex_count].b = 0;
        renderer->vertices[renderer->vertex_count].a = 0;
        renderer->vertex_count++;

        // Bottom-right
        renderer->vertices[renderer->vertex_count].x = x + width;
        renderer->vertices[renderer->vertex_count].y = y + height;
        renderer->vertices[renderer->vertex_count].u = 1.0f;  // gradient t = 1
        renderer->vertices[renderer->vertex_count].v = 0.0f;
        renderer->vertices[renderer->vertex_count].r = 0;
        renderer->vertices[renderer->vertex_count].g = 0;
        renderer->vertices[renderer->vertex_count].b = 0;
        renderer->vertices[renderer->vertex_count].a = 0;
        renderer->vertex_count++;

        // Bottom-left
        renderer->vertices[renderer->vertex_count].x = x;
        renderer->vertices[renderer->vertex_count].y = y + height;
        renderer->vertices[renderer->vertex_count].u = 1.0f;  // gradient t = 1
        renderer->vertices[renderer->vertex_count].v = 0.0f;
        renderer->vertices[renderer->vertex_count].r = 0;
        renderer->vertices[renderer->vertex_count].g = 0;
        renderer->vertices[renderer->vertex_count].b = 0;
        renderer->vertices[renderer->vertex_count].a = 0;
        renderer->vertex_count++;
    } else {
        // Horizontal gradient (left to right)
        // Top-left
        renderer->vertices[renderer->vertex_count].x = x;
        renderer->vertices[renderer->vertex_count].y = y;
        renderer->vertices[renderer->vertex_count].u = 0.0f;  // gradient t = 0
        renderer->vertices[renderer->vertex_count].v = 0.0f;
        renderer->vertices[renderer->vertex_count].r = 0;
        renderer->vertices[renderer->vertex_count].g = 0;
        renderer->vertices[renderer->vertex_count].b = 0;
        renderer->vertices[renderer->vertex_count].a = 0;
        renderer->vertex_count++;

        // Top-right
        renderer->vertices[renderer->vertex_count].x = x + width;
        renderer->vertices[renderer->vertex_count].y = y;
        renderer->vertices[renderer->vertex_count].u = 1.0f;  // gradient t = 1
        renderer->vertices[renderer->vertex_count].v = 0.0f;
        renderer->vertices[renderer->vertex_count].r = 0;
        renderer->vertices[renderer->vertex_count].g = 0;
        renderer->vertices[renderer->vertex_count].b = 0;
        renderer->vertices[renderer->vertex_count].a = 0;
        renderer->vertex_count++;

        // Bottom-right
        renderer->vertices[renderer->vertex_count].x = x + width;
        renderer->vertices[renderer->vertex_count].y = y + height;
        renderer->vertices[renderer->vertex_count].u = 1.0f;  // gradient t = 1
        renderer->vertices[renderer->vertex_count].v = 0.0f;
        renderer->vertices[renderer->vertex_count].r = 0;
        renderer->vertices[renderer->vertex_count].g = 0;
        renderer->vertices[renderer->vertex_count].b = 0;
        renderer->vertices[renderer->vertex_count].a = 0;
        renderer->vertex_count++;

        // Bottom-left
        renderer->vertices[renderer->vertex_count].x = x;
        renderer->vertices[renderer->vertex_count].y = y + height;
        renderer->vertices[renderer->vertex_count].u = 0.0f;  // gradient t = 0
        renderer->vertices[renderer->vertex_count].v = 0.0f;
        renderer->vertices[renderer->vertex_count].r = 0;
        renderer->vertices[renderer->vertex_count].g = 0;
        renderer->vertices[renderer->vertex_count].b = 0;
        renderer->vertices[renderer->vertex_count].a = 0;
        renderer->vertex_count++;
    }

    // Add indices for 2 triangles (6 indices)
    renderer->indices[renderer->index_count++] = base_index + 0;
    renderer->indices[renderer->index_count++] = base_index + 1;
    renderer->indices[renderer->index_count++] = base_index + 2;
    renderer->indices[renderer->index_count++] = base_index + 0;
    renderer->indices[renderer->index_count++] = base_index + 2;
    renderer->indices[renderer->index_count++] = base_index + 3;
}

// ============================================================================
// Shadow Rendering
// ============================================================================

void android_renderer_draw_box_shadow(AndroidRenderer* renderer,
                                       float x, float y,
                                       float width, float height,
                                       float radius,
                                       float offset_x, float offset_y,
                                       float blur,
                                       uint32_t shadow_color) {
    if (!renderer) return;

    // Expand shadow bounds to account for blur
    float shadow_expand = blur * 2.0f;
    float shadow_x = x + offset_x - shadow_expand;
    float shadow_y = y + offset_y - shadow_expand;
    float shadow_w = width + shadow_expand * 2.0f;
    float shadow_h = height + shadow_expand * 2.0f;

    // Check if we need to flush batch
    if (renderer->vertex_count + 4 > MAX_VERTICES ||
        renderer->index_count + 6 > MAX_INDICES) {
        android_renderer_flush_batch(renderer);
    }

    // Switch to shadow shader
    android_shader_use(renderer, SHADER_PROGRAM_SHADOW);

    // Extract shadow color components
    uint8_t a = (shadow_color >> 24) & 0xFF;
    uint8_t r = (shadow_color >> 16) & 0xFF;
    uint8_t g = (shadow_color >> 8) & 0xFF;
    uint8_t b = shadow_color & 0xFF;

    // Apply global opacity
    a = (uint8_t)(a * renderer->global_opacity);

#ifdef __ANDROID__
    // Set shadow uniforms
    glUniform2f(renderer->shader_programs[SHADER_PROGRAM_SHADOW].u_shadow_offset,
                offset_x, offset_y);
    glUniform1f(renderer->shader_programs[SHADER_PROGRAM_SHADOW].u_shadow_blur,
                blur);
    glUniform4f(renderer->shader_programs[SHADER_PROGRAM_SHADOW].u_shadow_color,
                r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f);
    glUniform2f(renderer->shader_programs[SHADER_PROGRAM_SHADOW].u_box_size,
                width, height);
    glUniform1f(renderer->shader_programs[SHADER_PROGRAM_SHADOW].u_box_radius,
                radius);
#endif

    // Add 4 vertices for the shadow quad
    // Note: Using u,v fields for local position
    uint32_t base_index = renderer->vertex_count;

    // Top-left
    renderer->vertices[renderer->vertex_count].x = shadow_x;
    renderer->vertices[renderer->vertex_count].y = shadow_y;
    renderer->vertices[renderer->vertex_count].u = 0.0f;
    renderer->vertices[renderer->vertex_count].v = 0.0f;
    renderer->vertices[renderer->vertex_count].r = 0;
    renderer->vertices[renderer->vertex_count].g = 0;
    renderer->vertices[renderer->vertex_count].b = 0;
    renderer->vertices[renderer->vertex_count].a = 0;
    renderer->vertex_count++;

    // Top-right
    renderer->vertices[renderer->vertex_count].x = shadow_x + shadow_w;
    renderer->vertices[renderer->vertex_count].y = shadow_y;
    renderer->vertices[renderer->vertex_count].u = shadow_w;
    renderer->vertices[renderer->vertex_count].v = 0.0f;
    renderer->vertices[renderer->vertex_count].r = 0;
    renderer->vertices[renderer->vertex_count].g = 0;
    renderer->vertices[renderer->vertex_count].b = 0;
    renderer->vertices[renderer->vertex_count].a = 0;
    renderer->vertex_count++;

    // Bottom-right
    renderer->vertices[renderer->vertex_count].x = shadow_x + shadow_w;
    renderer->vertices[renderer->vertex_count].y = shadow_y + shadow_h;
    renderer->vertices[renderer->vertex_count].u = shadow_w;
    renderer->vertices[renderer->vertex_count].v = shadow_h;
    renderer->vertices[renderer->vertex_count].r = 0;
    renderer->vertices[renderer->vertex_count].g = 0;
    renderer->vertices[renderer->vertex_count].b = 0;
    renderer->vertices[renderer->vertex_count].a = 0;
    renderer->vertex_count++;

    // Bottom-left
    renderer->vertices[renderer->vertex_count].x = shadow_x;
    renderer->vertices[renderer->vertex_count].y = shadow_y + shadow_h;
    renderer->vertices[renderer->vertex_count].u = 0.0f;
    renderer->vertices[renderer->vertex_count].v = shadow_h;
    renderer->vertices[renderer->vertex_count].r = 0;
    renderer->vertices[renderer->vertex_count].g = 0;
    renderer->vertices[renderer->vertex_count].b = 0;
    renderer->vertices[renderer->vertex_count].a = 0;
    renderer->vertex_count++;

    // Add indices for 2 triangles (6 indices)
    renderer->indices[renderer->index_count++] = base_index + 0;
    renderer->indices[renderer->index_count++] = base_index + 1;
    renderer->indices[renderer->index_count++] = base_index + 2;
    renderer->indices[renderer->index_count++] = base_index + 0;
    renderer->indices[renderer->index_count++] = base_index + 2;
    renderer->indices[renderer->index_count++] = base_index + 3;
}
