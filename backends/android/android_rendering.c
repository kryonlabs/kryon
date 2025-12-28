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

// WORKAROUND: Global variable to bypass corrupted parameter passing
static IRComponent* g_component_to_render = NULL;

// WORKAROUND: Inline wrapper to avoid ABI mismatch when calling from ir_android_renderer.c
void render_component_tree_inline(AndroidIRRenderer* ir_renderer) {
    if (!ir_renderer || !ir_renderer->last_root || !ir_renderer->renderer) {
        __android_log_print(ANDROID_LOG_WARN, "KryonBackend", "render_tree_inline: NULL params");
        return;
    }

    __android_log_print(ANDROID_LOG_DEBUG, "KryonBackend", "render_tree_inline: root=%p, child_count=%d",
                      ir_renderer->last_root, ir_renderer->last_root->child_count);

    // WORKAROUND: Use global variable to bypass parameter passing corruption
    g_component_to_render = ir_renderer->last_root;
    render_component_android(ir_renderer, NULL, 0.0f, 0.0f, 1.0f);
}

__attribute__((noinline))
void render_component_android(AndroidIRRenderer* ir_renderer,
                              IRComponent* component,
                              float parent_x, float parent_y,
                              float parent_opacity) {
    static int component_render_count = 0;
    component_render_count++;

    // WORKAROUND: If component is NULL, use global variable (root component)
    if (component_render_count % 60 == 0) {
        __android_log_print(ANDROID_LOG_DEBUG, "KryonBackend",
                          "Received params: component=%p, g_component_to_render=%p",
                          component, g_component_to_render);
    }

    if (component == NULL && g_component_to_render != NULL) {
        component = g_component_to_render;
        g_component_to_render = NULL;  // Clear after use
        __android_log_print(ANDROID_LOG_DEBUG, "KryonBackend",
                          "Using global component workaround: ptr=%p, child_count=%d",
                          component, component->child_count);
    } else if (g_component_to_render != NULL) {
        __android_log_print(ANDROID_LOG_ERROR, "KryonBackend",
                          "Global set but component not NULL! component=%p, global=%p",
                          component, g_component_to_render);
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
    float x, y;

    // Check if component has absolute positioning
    if (component->style && component->style->position_mode == IR_POSITION_ABSOLUTE) {
        // Use absolute positioning
        x = component->style->absolute_x;
        y = component->style->absolute_y;
    } else {
        // Use computed layout position
        x = parent_x + component->layout_state->computed.x;
        y = parent_y + component->layout_state->computed.y;
    }

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

    // DEBUG: Log component type every 60 frames
    if (component_render_count % 60 == 0) {
        __android_log_print(ANDROID_LOG_INFO, "KryonBackend",
            "Component type=%d at (%.1f,%.1f) size %.1fx%.1f",
            component->type, x, y, width, height);
    }

    // Render based on component type
    switch (component->type) {
        case IR_COMPONENT_CONTAINER:
        case IR_COMPONENT_ROW:
        case IR_COMPONENT_COLUMN:
            // TEMPORARY: Log ALL containers to debug
            {
                uint32_t bg_color = component->style ?
                    (component->style->background.data.a << 24) | (component->style->background.data.r << 16) |
                    (component->style->background.data.g << 8) | component->style->background.data.b : 0;
                if (component_render_count < 10 || component_render_count % 60 == 0) {
                    __android_log_print(ANDROID_LOG_INFO, "KryonBackend",
                        "Rendering container at (%.1f,%.1f) size %.1fx%.1f bg=0x%08x",
                        x, y, width, height, bg_color);
                }
            }
            render_box_shadow(renderer, component, x, y, width, height, opacity);
            render_background(renderer, component, x, y, width, height, opacity);
            render_border(renderer, component, x, y, width, height, opacity);
            break;

        case IR_COMPONENT_CENTER:
            // Center is just a layout container, no visual rendering
            break;

        case IR_COMPONENT_TEXT:
            if (component->text_content) {
                __android_log_print(ANDROID_LOG_WARN, "KryonBackend",
                    "RENDERING TEXT: '%s' at (%.1f,%.1f)", component->text_content, x, y);

                IRColor text_color = component->style ?
                    component->style->font.color :
                    IR_COLOR_RGBA(0, 0, 0, 255);

                int font_size = component->style && component->style->font.size > 0 ?
                    (int)component->style->font.size : 16;

                uint32_t color = ir_color_to_rgba(text_color, opacity);

                __android_log_print(ANDROID_LOG_WARN, "KryonBackend",
                    "Text color=0x%08x, font_size=%d", color, font_size);

                // Use Roboto as default font since it's registered on Android
                const char* font_name = (component->style && component->style->font.family) ?
                                       component->style->font.family : "Roboto";

                android_renderer_draw_text(renderer, component->text_content,
                                          x, y, font_name, font_size, color);
            } else {
                __android_log_print(ANDROID_LOG_WARN, "KryonBackend",
                    "TEXT component has no text_content!");
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
        // DEBUG: Log child rendering
        if (component_render_count < 10 || component_render_count % 60 == 0) {
            __android_log_print(ANDROID_LOG_INFO, "KryonBackend",
                "Component type=%d has %d children, rendering them now",
                component->type, component->child_count);
        }

        for (uint32_t i = 0; i < component->child_count; i++) {
            IRComponent* child = component->children[i];

            // DEBUG: Log each child
            if (component_render_count < 10 || component_render_count % 60 == 0) {
                __android_log_print(ANDROID_LOG_INFO, "KryonBackend",
                    "  Child[%d]: type=%d, has_layout=%d, layout_valid=%d",
                    i, child ? child->type : -1,
                    child && child->layout_state ? 1 : 0,
                    child && child->layout_state && child->layout_state->layout_valid ? 1 : 0);
            }

            // WORKAROUND: Use global variable for all calls to avoid parameter corruption
            g_component_to_render = child;
            render_component_android(ir_renderer, NULL, x, y, opacity);
        }
    } else {
        static int no_children_log = 0;
        if (no_children_log++ % 60 == 0) {
            __android_log_print(ANDROID_LOG_DEBUG, "KryonBackend", "Component has no children (child_count=%d)", component->child_count);
        }
    }
}
