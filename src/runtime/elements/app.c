/**
 * @file app.c
 * @brief Implementation of the App element.
 *
 * This file contains the rendering and layout logic for App elements.
 * App elements are the root container that also handles window-level
 * properties and inherits Container-style layout capabilities.
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
static void app_render(KryonRuntime* runtime, KryonElement* element, KryonRenderCommand* commands, size_t* command_count, size_t max_commands);
static bool app_handle_event(KryonRuntime* runtime, KryonElement* element, const ElementEvent* event);
static void app_destroy(KryonRuntime* runtime, KryonElement* element);

// The VTable binds the generic element interface to our specific app functions.
static const ElementVTable g_app_vtable = {
    .render = app_render,
    .handle_event = app_handle_event,
    .destroy = app_destroy
};

// =============================================================================
//  Public Registration Function
// =============================================================================

/**
 * @brief Registers the App element type with the element registry.
 */
bool register_app_element(void) {
    return element_register_type("App", &g_app_vtable);
}

// =============================================================================
//  Element VTable Implementations
// =============================================================================

/**
 * @brief Renders the App element with Container-like styling capabilities.
 * App elements can have background colors, borders, and handle window properties.
 */
static void app_render(KryonRuntime* runtime, KryonElement* element, KryonRenderCommand* commands, size_t* command_count, size_t max_commands) {
    if (*command_count >= max_commands - 1) return;
    
    // --- 1. Render App Background and Border (like Container) ---
    
    // Get app visual properties  
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
    
    // Render app background if visible
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
    
    // --- 2. Window Properties Handling ---
    
    // Window properties like windowTitle, windowWidth, windowHeight are handled
    // by the runtime system during window creation, not during rendering.
    // The App element's main responsibility during rendering is visual styling.
    
    // --- 3. App Layout Responsibility ---
    
    // NOTE: App elements do NOT position their children during rendering.
    // Child positioning is handled by the layout calculation phase in elements.c
    // through the new position_app_children() function.
    // App renders its own background/border and window-level styling.
}

/**
 * @brief Handles events for the App element.
 * App can handle script-based events like onClick, just like Container.
 */
static bool app_handle_event(KryonRuntime* runtime, KryonElement* element, const ElementEvent* event) {
    // Use the generic script event handler for standard events
    return generic_script_event_handler(runtime, element, event);
}

/**
 * @brief Cleans up any custom data for an App element.
 */
static void app_destroy(KryonRuntime* runtime, KryonElement* element) {
    // No custom data to free for a basic app element.
    (void)runtime;
    (void)element;
}