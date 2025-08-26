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
#include "element_mixins.h"
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
 * @brief Registers all container-based element types with the element registry.
 * This includes Container, Column, Row, and Center which all use the same vtable
 * but have different layout behaviors handled by the positioning system.
 */
bool register_container_element(void) {
    // Register all layout container types to use the same vtable
    // The positioning logic in elements.c handles the different layout behaviors
    bool success = true;
    
    success &= element_register_type("Container", &g_container_vtable);
    success &= element_register_type("Column", &g_container_vtable);
    success &= element_register_type("Row", &g_container_vtable);  
    success &= element_register_type("Center", &g_container_vtable);
    
    return success;
}

// =============================================================================
//  Element VTable Implementations
// =============================================================================

/**
 * @brief Renders container elements (Container, Column, Row, Center) with background and borders.
 * This function handles the visual rendering (background, borders) for all layout container types.
 * The actual positioning logic is handled separately by the layout system in elements.c.
 */
static void container_render(KryonRuntime* runtime, KryonElement* element, KryonRenderCommand* commands, size_t* command_count, size_t max_commands) {
    if (*command_count >= max_commands - 1) return;
    
    // --- 1. Render Container Background and Border (using shared mixin) ---
    render_background_and_border(element, commands, command_count, max_commands);
}

/**
 * @brief Handles events for the Container element.
 * Container can handle script-based events like onClick.
 */
static bool container_handle_event(KryonRuntime* runtime, KryonElement* element, const ElementEvent* event) {
    // Use the shared script event handler mixin
    return handle_script_events(runtime, element, event);
}

/**
 * @brief Cleans up any custom data for a Container element.
 */
static void container_destroy(KryonRuntime* runtime, KryonElement* element) {
    // No custom data to free for a basic container element.
    (void)runtime;
    (void)element;
}