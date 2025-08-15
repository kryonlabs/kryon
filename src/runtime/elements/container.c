/**
 * @file container.c
 * @brief Implementation of the Container element.
 *
 * This file contains the rendering and layout logic for Container elements.
 * Container elements can hold child elements and support contentAlignment 
 * properties for centering and positioning children.
 *
 * 0BSD License
 */

#include "elements.h"
#include "runtime.h"
#include "memory.h"
#include "color_utils.h"
#include <stdio.h>
#include <string.h>

// Forward declarations for the VTable functions
static void container_render(KryonRuntime* runtime, KryonElement* element, KryonRenderCommand* commands, size_t* command_count, size_t max_commands);
static bool container_handle_event(KryonRuntime* runtime, KryonElement* element, const ElementEvent* event);
static void container_destroy(KryonRuntime* runtime, KryonElement* element);

// The VTable binds the generic element interface to our specific container functions.
static const ElementVTable g_container_vtable = {
    .render = container_render,
    .handle_event = container_handle_event,
    .destroy = container_destroy
};

// =============================================================================
//  Public Registration Function
// =============================================================================

/**
 * @brief Registers the Container element type with the element registry.
 */
bool register_container_element(void) {
    return element_register_type("Container", &g_container_vtable);
}

// =============================================================================
//  Element VTable Implementations
// =============================================================================

/**
 * @brief Renders the Container element and positions its children according to contentAlignment.
 */
static void container_render(KryonRuntime* runtime, KryonElement* element, KryonRenderCommand* commands, size_t* command_count, size_t max_commands) {
    if (*command_count >= max_commands - 1) return;
    
    // --- 1. Render Container Background and Border ---
    
    // Get container visual properties  
    uint32_t bg_color_val = get_element_property_color(element, "backgroundColor", 0x00000000); // Transparent default
    // Fallback to "background" property for compatibility
    if (bg_color_val == 0x00000000) {
        bg_color_val = get_element_property_color(element, "background", 0x00000000);
    }
    uint32_t border_color_val = get_element_property_color(element, "borderColor", 0x000000FF); // Black default
    float border_width = get_element_property_float(element, "borderWidth", 0.0f);
    float border_radius = get_element_property_float(element, "borderRadius", 0.0f);
    int z_index = get_element_property_int(element, "zIndex", 0);
    
    // Use element position from layout engine
    KryonVec2 position = { element->x, element->y };
    KryonVec2 size = { element->width, element->height };
    
    // Convert color values to KryonColor format
    KryonColor bg_color = color_u32_to_f32(bg_color_val);
    KryonColor border_color = color_u32_to_f32(border_color_val);
    
    // Container background will be rendered if visible
    
    // Render container background if visible
    if (bg_color.a > 0.0f || border_width > 0.0f) {
        KryonRenderCommand cmd = kryon_cmd_draw_rect(
            position,
            size,
            bg_color,
            border_radius
        );
        
        // Set border properties
        if (border_width > 0.0f) {
            cmd.data.draw_rect.border_width = border_width;
            cmd.data.draw_rect.border_color = border_color;
        }
        
        cmd.z_index = z_index;
        commands[(*command_count)++] = cmd;
    }
    
    // --- 2. Container Layout Responsibility ---
    
    // NOTE: Container elements do NOT position their children during rendering.
    // Child positioning is handled by layout elements (Column, Row, Center) during
    // the layout calculation phase. Container only draws its own background/border.
    //
    // Container DOES support contentAlignment for positioning children - this is
    // handled in the position_container_children() function in elements.c
}

// Legacy layout function removed - positioning now handled by calculate_all_element_positions pipeline

/**
 * @brief Handles events for the Container element.
 * Container can handle script-based events like onClick.
 */
static bool container_handle_event(KryonRuntime* runtime, KryonElement* element, const ElementEvent* event) {
    // Use the generic script event handler for standard events
    return generic_script_event_handler(runtime, element, event);
}

/**
 * @brief Cleans up any custom data for a Container element.
 */
static void container_destroy(KryonRuntime* runtime, KryonElement* element) {
    // No custom data to free for a basic container element.
    (void)runtime;
    (void)element;
}