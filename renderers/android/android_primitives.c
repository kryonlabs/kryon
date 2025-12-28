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

    // TODO: Implement rectangle rendering
    // 1. Switch to color shader
    // 2. Add vertices to batch
    // 3. Add indices for 2 triangles

    (void)x;
    (void)y;
    (void)width;
    (void)height;
    (void)color;
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

    // TODO: Implement rectangle outline rendering
    // Draw 4 rectangles (top, right, bottom, left)

    (void)x;
    (void)y;
    (void)width;
    (void)height;
    (void)border_width;
    (void)color;
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
