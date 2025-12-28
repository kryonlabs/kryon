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

    // TODO: Implement rounded rectangle rendering
    // 1. Switch to rounded rect shader
    // 2. Set uniforms (size, radius)
    // 3. Add vertices to batch

    (void)x;
    (void)y;
    (void)width;
    (void)height;
    (void)radius;
    (void)color;
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
