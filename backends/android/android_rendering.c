/*
 * Android Component Rendering - IR to AndroidRenderer
 * Renders IR component tree using computed layout from ir_layout_compute_tree()
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "android_internal.h"

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

// Helper to convert IRColor to RGBA uint32
static uint32_t ir_color_to_rgba(IRColor color, float opacity) {
    uint8_t a = (uint8_t)(color.data.a * opacity);
    uint8_t r = color.data.r;
    uint8_t g = color.data.g;
    uint8_t b = color.data.b;
    return (a << 24) | (r << 16) | (g << 8) | b;
}

// Render box shadow
static void render_box_shadow(AndroidRenderer* renderer,
                              IRComponent* component,
                              float x, float y,
                              float width, float height,
                              float opacity) {
    if (!component->style) return;

    IRBoxShadow* shadow = &component->style->box_shadow;
    if (shadow->blur_radius <= 0) return;

    // Draw shadow as a blurred rect offset from the component
    float shadow_x = x + shadow->offset_x;
    float shadow_y = y + shadow->offset_y;

    uint32_t shadow_color = ir_color_to_rgba(shadow->color, opacity);

    // Simple shadow approximation: draw multiple rects with increasing transparency
    int blur_steps = (int)fminf(shadow->blur_radius, 10);
    for (int i = 0; i < blur_steps; i++) {
        float offset = (float)i;
        float alpha_factor = 1.0f - ((float)i / blur_steps);

        uint8_t shadow_a = (uint8_t)((shadow_color >> 24) * alpha_factor);
        uint32_t blurred_color = (shadow_a << 24) | (shadow_color & 0x00FFFFFF);

        android_renderer_draw_rect(renderer,
                                   shadow_x - offset, shadow_y - offset,
                                   width + offset * 2, height + offset * 2,
                                   blurred_color);
    }
}

// Render background with rounded corners
static void render_background(AndroidRenderer* renderer,
                              IRComponent* component,
                              float x, float y,
                              float width, float height,
                              float opacity) {
    if (!component->style) return;

    IRColor bg_color = component->style->background;

    // Skip if fully transparent
    if (bg_color.data.a == 0) return;

    uint32_t color = ir_color_to_rgba(bg_color, opacity);
    float radius = component->style->border.radius;

    if (radius > 0) {
        android_renderer_draw_rounded_rect(renderer, x, y, width, height, radius, color);
    } else {
        android_renderer_draw_rect(renderer, x, y, width, height, color);
    }
}

// Render border outline
static void render_border(AndroidRenderer* renderer,
                         IRComponent* component,
                         float x, float y,
                         float width, float height,
                         float opacity) {
    if (!component->style) return;

    IRBorder* border = &component->style->border;
    if (border->width <= 0 || border->color.data.a == 0) return;

    uint32_t border_color = ir_color_to_rgba(border->color, opacity);
    float bw = border->width;
    float radius = border->radius;

    if (radius > 0) {
        // Draw 4 rounded rect outlines for the border
        // Top
        android_renderer_draw_rounded_rect(renderer, x, y, width, bw, radius, border_color);
        // Bottom
        android_renderer_draw_rounded_rect(renderer, x, y + height - bw, width, bw, radius, border_color);
        // Left
        android_renderer_draw_rounded_rect(renderer, x, y + bw, bw, height - 2 * bw, radius, border_color);
        // Right
        android_renderer_draw_rounded_rect(renderer, x + width - bw, y + bw, bw, height - 2 * bw, radius, border_color);
    } else {
        // Draw 4 rects for the border
        // Top
        android_renderer_draw_rect(renderer, x, y, width, bw, border_color);
        // Bottom
        android_renderer_draw_rect(renderer, x, y + height - bw, width, bw, border_color);
        // Left
        android_renderer_draw_rect(renderer, x, y + bw, bw, height - 2 * bw, border_color);
        // Right
        android_renderer_draw_rect(renderer, x + width - bw, y + bw, bw, height - 2 * bw, border_color);
    }
}

// ============================================================================
// MAIN RENDERING FUNCTION
// ============================================================================

void render_component_android(AndroidIRRenderer* ir_renderer,
                              IRComponent* component,
                              float parent_x, float parent_y,
                              float parent_opacity) {
    static int component_render_count = 0;
    component_render_count++;

    // WORKAROUND: If this is the root component (parent_x/y = 0) and has wrong pointer, use last_root instead
    if (ir_renderer && ir_renderer->last_root &&
        component != ir_renderer->last_root &&
        parent_x == 0.0f && parent_y == 0.0f) {
        if (component_render_count % 60 == 0) {
            __android_log_print(ANDROID_LOG_WARN, "KryonBackend", "POINTER CORRUPTION DETECTED! Passed=%p, Expected=%p, using last_root",
                              component, ir_renderer->last_root);
        }
        component = ir_renderer->last_root;
    }

    if (component_render_count % 60 == 0) {
        __android_log_print(ANDROID_LOG_DEBUG, "KryonBackend", "render_component called, count=%d, component ptr=%p, child_count=%d, children ptr=%p",
                          component_render_count, component, component ? component->child_count : -999,
                          component ? component->children : NULL);
    }

    if (!ir_renderer || !component || !ir_renderer->renderer) {
        __android_log_print(ANDROID_LOG_WARN, "KryonBackend", "render_component: NULL params");
        return;
    }

    // Skip if layout is not valid
    if (!component->layout_state) {
        if (component_render_count % 60 == 0) {
            __android_log_print(ANDROID_LOG_WARN, "KryonBackend", "Component has no layout_state!");
        }
        return;
    }

    if (!component->layout_state->layout_valid) {
        if (component_render_count % 60 == 0) {
            __android_log_print(ANDROID_LOG_WARN, "KryonBackend", "Component layout not valid!");
        }
        return;
    }

    AndroidRenderer* renderer = ir_renderer->renderer;

    // Get computed layout bounds (NEW API)
    float x = parent_x + component->layout_state->computed.x;
    float y = parent_y + component->layout_state->computed.y;
    float width = component->layout_state->computed.width;
    float height = component->layout_state->computed.height;

    // Apply opacity cascading
    float opacity = parent_opacity;
    if (component->style) {
        opacity *= component->style->opacity;
    }

    // Skip if fully transparent
    if (opacity <= 0.01f) return;

    // Set opacity for renderer
    android_renderer_set_opacity(renderer, opacity);

    // Render based on component type
    switch (component->type) {
        case IR_COMPONENT_CONTAINER:
        case IR_COMPONENT_ROW:
        case IR_COMPONENT_COLUMN:
            render_box_shadow(renderer, component, x, y, width, height, opacity);
            render_background(renderer, component, x, y, width, height, opacity);
            render_border(renderer, component, x, y, width, height, opacity);
            break;

        case IR_COMPONENT_CENTER:
            // Center is just a layout container, no visual rendering
            break;

        case IR_COMPONENT_TEXT:
            if (component->text_content) {
                IRColor text_color = component->style ?
                    component->style->font.color :
                    IR_COLOR_RGBA(0, 0, 0, 255);

                int font_size = component->style && component->style->font.size > 0 ?
                    (int)component->style->font.size : 16;

                uint32_t color = ir_color_to_rgba(text_color, opacity);

                android_renderer_draw_text(renderer, component->text_content,
                                          x, y, NULL, font_size, color);
            }
            break;

        case IR_COMPONENT_BUTTON:
            render_box_shadow(renderer, component, x, y, width, height, opacity);
            render_background(renderer, component, x, y, width, height, opacity);
            render_border(renderer, component, x, y, width, height, opacity);

            // Render button text centered
            if (component->text_content) {
                IRColor text_color = component->style ?
                    component->style->font.color :
                    IR_COLOR_RGBA(255, 255, 255, 255);

                int font_size = component->style && component->style->font.size > 0 ?
                    (int)component->style->font.size : 16;

                uint32_t color = ir_color_to_rgba(text_color, opacity);

                float text_width, text_height;
                android_renderer_measure_text(renderer, component->text_content,
                                             NULL, font_size,
                                             &text_width, &text_height);

                float text_x = x + (width - text_width) / 2.0f;
                float text_y = y + (height - text_height) / 2.0f;

                android_renderer_draw_text(renderer, component->text_content,
                                          text_x, text_y,
                                          NULL, font_size, color);
            }
            break;

        case IR_COMPONENT_INPUT:
            render_background(renderer, component, x, y, width, height, opacity);
            render_border(renderer, component, x, y, width, height, opacity);

            // Render input text with padding
            if (component->text_content && component->text_content[0]) {
                IRColor text_color = component->style ?
                    component->style->font.color :
                    IR_COLOR_RGBA(0, 0, 0, 255);

                int font_size = component->style && component->style->font.size > 0 ?
                    (int)component->style->font.size : 16;

                uint32_t color = ir_color_to_rgba(text_color, opacity);

                android_renderer_draw_text(renderer, component->text_content,
                                          x + 8, y + 8,
                                          NULL, font_size, color);
            }
            break;

        case IR_COMPONENT_CHECKBOX:
            render_background(renderer, component, x, y, width, height, opacity);
            render_border(renderer, component, x, y, width, height, opacity);

            // TODO: Render checkmark if checked
            break;

        case IR_COMPONENT_IMAGE:
            // TODO: Implement image rendering with texture loading
            render_background(renderer, component, x, y, width, height, opacity);
            render_border(renderer, component, x, y, width, height, opacity);
            break;

        default:
            break;
    }

    // Recursively render children
    if (component->children) {
        for (uint32_t i = 0; i < component->child_count; i++) {
            render_component_android(ir_renderer, component->children[i],
                                    x, y, opacity);
        }
    } else {
        static int no_children_log = 0;
        if (no_children_log++ % 60 == 0) {
            __android_log_print(ANDROID_LOG_DEBUG, "KryonBackend", "Component has no children (child_count=%d)", component->child_count);
        }
    }
}
