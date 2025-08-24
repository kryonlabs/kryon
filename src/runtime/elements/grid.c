/**
 * @file grid.c
 * @brief Implementation of the Grid element.
 *
 * This file contains the layout and rendering logic for Grid elements.
 * Grid elements arrange their children in a 2D grid layout with configurable
 * columns, rows, and spacing.
 *
 * 0BSD License
 */

#include "elements.h"
#include "runtime.h"
#include "memory.h"
#include "color_utils.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

// Forward declarations for the VTable functions
static void grid_render(KryonRuntime* runtime, KryonElement* element, KryonRenderCommand* commands, size_t* command_count, size_t max_commands);
static bool grid_handle_event(KryonRuntime* runtime, KryonElement* element, const ElementEvent* event);
static void grid_destroy(KryonRuntime* runtime, KryonElement* element);

// The VTable binds the generic element interface to our specific grid functions.
static const ElementVTable g_grid_vtable = {
    .render = grid_render,
    .handle_event = grid_handle_event,
    .destroy = grid_destroy
};

// =============================================================================
//  Public Registration Function
// =============================================================================

/**
 * @brief Registers the Grid element type with the element registry.
 */
bool register_grid_element(void) {
    return element_register_type("Grid", &g_grid_vtable);
}

// Grid layout calculation is now handled centrally in elements.c:position_grid_children()

// =============================================================================
//  Element VTable Implementations
// =============================================================================

/**
 * @brief Renders the Grid element background/border only (positioning handled by elements.c)
 */
static void grid_render(KryonRuntime* runtime, KryonElement* element, KryonRenderCommand* commands, size_t* command_count, size_t max_commands) {
    if (*command_count >= max_commands - 1) return;
    
    // Grid itself can have background and border styling
    uint32_t bg_color_val = get_element_property_color(element, "backgroundColor", 0x00000000); // Transparent default
    if (bg_color_val == 0x00000000) {
        bg_color_val = get_element_property_color(element, "background", 0x00000000);
    }
    uint32_t border_color_val = get_element_property_color(element, "borderColor", 0x000000FF);
    float border_width = get_element_property_float(element, "borderWidth", 0.0f);
    float border_radius = get_element_property_float(element, "borderRadius", 0.0f);
    int z_index = get_element_property_int(element, "zIndex", 0);
    
    // Render grid background if visible
    if (bg_color_val != 0x00000000 || border_width > 0.0f) {
        KryonVec2 position = { element->x, element->y };
        KryonVec2 size = { element->width, element->height };
        KryonColor bg_color = color_u32_to_f32(bg_color_val);
        KryonColor border_color = color_u32_to_f32(border_color_val);
        
        KryonRenderCommand cmd = kryon_cmd_draw_rect(
            position,
            size,
            bg_color,
            border_radius
        );
        
        if (border_width > 0.0f) {
            cmd.data.draw_rect.border_width = border_width;
            cmd.data.draw_rect.border_color = border_color;
        }
        
        cmd.z_index = z_index;
        commands[(*command_count)++] = cmd;
    }
}

/**
 * @brief Handles events for the Grid element
 */
static bool grid_handle_event(KryonRuntime* runtime, KryonElement* element, const ElementEvent* event) {
    // Use the generic script event handler for standard events
    return generic_script_event_handler(runtime, element, event);
}

/**
 * @brief Cleans up any custom data for a Grid element
 */
static void grid_destroy(KryonRuntime* runtime, KryonElement* element) {
    // No custom data to free for a basic grid element
    (void)runtime;
    (void)element;
}