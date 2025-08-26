/**
 * @file renderable_behavior.c
 * @brief Renderable Behavior Implementation
 * 
 * Handles basic visual rendering for elements including background colors,
 * borders, and other common visual properties.
 * 
 * 0BSD License
 */

#include "element_behaviors.h"
#include "elements.h"
#include "runtime.h"
#include "memory.h"
#include "color_utils.h"
#include <stdio.h>
#include <string.h>

// =============================================================================
// RENDERABLE BEHAVIOR IMPLEMENTATION
// =============================================================================

static bool renderable_init(struct KryonElement* element) {
    // No special initialization needed for basic rendering
    return true;
}

static void renderable_render(struct KryonRuntime* runtime, 
                             struct KryonElement* element,
                             KryonRenderCommand* commands,
                             size_t* command_count,
                             size_t max_commands) {
    
    if (*command_count >= max_commands - 1) return;
    
    // Get visual properties  
    uint32_t bg_color_val = get_element_property_color(element, "backgroundColor", 0x00000000); // Transparent default
    
    // Fallback to "background" property for compatibility
    if (bg_color_val == 0x00000000) {
        bg_color_val = get_element_property_color(element, "background", 0x00000000);
    }
    
    uint32_t border_color_val = get_element_property_color(element, "borderColor", 0x000000FF); // Black default
    float border_width = get_element_property_float(element, "borderWidth", 0.0f);
    
    // Smart border width: if borderColor is set but borderWidth is 0, default to 1.0
    if (border_width == 0.0f && border_color_val != 0x000000FF) {
        border_width = 1.0f;
    }
    float border_radius = get_element_property_float(element, "borderRadius", 0.0f);
    int z_index = get_element_property_int(element, "zIndex", 0);
    
    // Use element position from layout engine
    KryonVec2 position = { element->x, element->y };
    KryonVec2 size = { element->width, element->height };
    
    // Convert color values to KryonColor format
    KryonColor bg_color = color_u32_to_f32(bg_color_val);
    KryonColor border_color = color_u32_to_f32(border_color_val);
    
    // Only render if there's something visible
    if (bg_color.a > 0.0f || border_width > 0.0f) {
        KryonRenderCommand cmd = kryon_cmd_draw_rect(
            position,
            size,
            bg_color,
            border_radius
        );
        
        // Set border properties if needed
        if (border_width > 0.0f) {
            cmd.data.draw_rect.border_width = border_width;
            cmd.data.draw_rect.border_color = border_color;
        }
        
        cmd.z_index = z_index;
        commands[(*command_count)++] = cmd;
    }
}

static bool renderable_handle_event(struct KryonRuntime* runtime,
                                   struct KryonElement* element,
                                   const ElementEvent* event) {
    // Renderable behavior doesn't handle events directly
    return false;
}

static void renderable_destroy(struct KryonElement* element) {
    // No special cleanup needed for basic rendering
    (void)element;
}

static void* renderable_get_state(struct KryonElement* element) {
    // Return the unified behavior state
    return element_get_behavior_state(element);
}

// =============================================================================
// BEHAVIOR DEFINITION
// =============================================================================

static const ElementBehavior g_renderable_behavior = {
    .name = "Renderable",
    .init = renderable_init,
    .render = renderable_render,
    .handle_event = renderable_handle_event,
    .destroy = renderable_destroy,
    .get_state = renderable_get_state
};

// =============================================================================
// REGISTRATION FUNCTION
// =============================================================================

__attribute__((constructor))
void register_renderable_behavior(void) {
    element_behavior_register(&g_renderable_behavior);
}