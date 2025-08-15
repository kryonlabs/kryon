/**
 * @file tab_content.c
 * @brief Implementation of the TabContent element.
 *
 * This file contains the rendering, event handling, and lifecycle functions
 * for the TabContent element, which represents the content area of tabs
 * and can contain any child elements.
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
static void tab_content_render(KryonRuntime* runtime, KryonElement* element, KryonRenderCommand* commands, size_t* command_count, size_t max_commands);
static bool tab_content_handle_event(KryonRuntime* runtime, KryonElement* element, const ElementEvent* event);
static void tab_content_destroy(KryonRuntime* runtime, KryonElement* element);

// The VTable binds the generic element interface to our specific tab content functions.
static const ElementVTable g_tab_content_vtable = {
    .render = tab_content_render,
    .handle_event = tab_content_handle_event,
    .destroy = tab_content_destroy
};

// =============================================================================
//  Public Registration Function
// =============================================================================

/**
 * @brief Registers the TabContent element type with the element registry.
 * This is called once at runtime startup.
 */
bool register_tab_content_element(void) {
    return element_register_type("TabContent", &g_tab_content_vtable);
}

// =============================================================================
//  Helper Functions
// =============================================================================

/**
 * @brief Gets tab content padding, defaults to 20
 */
static int get_content_padding(KryonElement* element) {
    return get_element_property_int(element, "padding", 20);
}

/**
 * @brief Gets tab content border radius, defaults to 0
 */
static int get_content_border_radius(KryonElement* element) {
    return get_element_property_int(element, "borderRadius", 0);
}

// =============================================================================
//  Element VTable Implementations
// =============================================================================

/**
 * @brief Renders the TabContent element by generating render commands.
 */
static void tab_content_render(KryonRuntime* runtime, KryonElement* element, KryonRenderCommand* commands, size_t* command_count, size_t max_commands) {
    if (!element || !commands || !command_count) return;

    int padding = get_content_padding(element);
    int border_radius = get_content_border_radius(element);
    
    // Get element bounds
    float x = element->x;
    float y = element->y;
    float width = element->width;
    float height = element->height;

    // Get colors
    uint32_t bg_color_val = get_element_property_color(element, "backgroundColor", 0xFFFFFFFF);
    uint32_t border_color_val = get_element_property_color(element, "borderColor", 0x00000000);
    int border_width = get_element_property_int(element, "borderWidth", 0);
    
    KryonColor bg_color = color_u32_to_f32(bg_color_val);
    KryonColor border_color = color_u32_to_f32(border_color_val);

    // Render background
    if (*command_count < max_commands) {
        KryonVec2 position = { x, y };
        KryonVec2 size = { width, height };
        commands[*command_count] = kryon_cmd_draw_rect(position, size, bg_color, (float)border_radius);
        (*command_count)++;
    }

    // Render border if specified
    if (border_width > 0 && border_color_val != 0x00000000 && *command_count < max_commands) {
        KryonVec2 position = { x, y };
        KryonVec2 size = { width, height };
        KryonRenderCommand cmd = kryon_cmd_draw_rect(position, size, bg_color, (float)border_radius);
        cmd.data.draw_rect.border_width = (float)border_width;
        cmd.data.draw_rect.border_color = border_color;
        commands[*command_count] = cmd;
        (*command_count)++;
    }

    // Calculate content area with padding
    float content_x = x + padding;
    float content_y = y + padding;
    float content_width = width - (2 * padding);
    float content_height = height - (2 * padding);

    // Ensure content area is valid
    if (content_width <= 0 || content_height <= 0) {
        return;
    }

    // Update child element bounds to fit within content area
    // Child rendering is handled by the main element rendering pipeline
    for (size_t i = 0; i < element->child_count; i++) {
        KryonElement* child = element->children[i];
        if (!child) continue;

        // Update child bounds to fit within content area
        // This is a simplified layout - in a real implementation you might want
        // to support different layout modes (column, row, grid, etc.)
        
        // For now, we'll use a simple stacking approach where each child
        // takes the full available width and height
        child->x = content_x;
        child->y = content_y;
        child->width = content_width;
        child->height = content_height;

        // Child elements will be rendered automatically by the main element system
    }
}

/**
 * @brief Handles events for the TabContent element.
 */
static bool tab_content_handle_event(KryonRuntime* runtime, KryonElement* element, const ElementEvent* event) {
    if (!element || !event) return false;

    // TabContent doesn't need to forward events to children
    // The main element event system handles child event forwarding automatically
    
    // For TabContent-specific behavior, use generic script handler
    return generic_script_event_handler(runtime, element, event);

    return false;
}

/**
 * @brief Destroys the TabContent element and cleans up resources.
 */
static void tab_content_destroy(KryonRuntime* runtime, KryonElement* element) {
    if (!element) return;
    
    // Child elements are automatically destroyed by the element system
    // No additional cleanup needed for TabContent
}