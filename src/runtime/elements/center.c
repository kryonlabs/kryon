/**
 * @file center.c
 * @brief Implementation of the Center layout element.
 *
 * Center elements center their children both horizontally and vertically
 * within their available space.
 *
 * 0BSD License
 */

#include "elements.h"
#include "runtime.h"
#include "memory.h"
#include <stdio.h>
#include <string.h>

// Forward declarations for the VTable functions
static void center_render(KryonRuntime* runtime, KryonElement* element, KryonRenderCommand* commands, size_t* command_count, size_t max_commands);
static bool center_handle_event(KryonRuntime* runtime, KryonElement* element, const ElementEvent* event);
static void center_destroy(KryonRuntime* runtime, KryonElement* element);

// The VTable binds the generic element interface to our specific center functions.
static const ElementVTable g_center_vtable = {
    .render = center_render,
    .handle_event = center_handle_event,
    .destroy = center_destroy
};

// =============================================================================
//  Public Registration Function
// =============================================================================

/**
 * @brief Registers the Center element type with the element registry.
 */
bool register_center_element(void) {
    return element_register_type("Center", &g_center_vtable);
}

// =============================================================================
//  Element VTable Implementations
// =============================================================================

/**
 * @brief Calculates layout for the Center element by centering its children.
 */

/**
 * @brief Renders the Center element by centering its children.
 */
static void center_render(KryonRuntime* runtime, KryonElement* element, KryonRenderCommand* commands, size_t* command_count, size_t max_commands) {
    // Center is a layout element - it doesn't render itself, only positions children during layout phase
    // The layout calculation should have already positioned all children correctly
    // This function is called during rendering and should not modify positions
    
    (void)runtime;
    (void)element;
    (void)commands;
    (void)command_count;
    (void)max_commands;
    
    // Center elements don't generate render commands themselves
    // Their children will be rendered separately by the rendering system
}

/**
 * @brief Handles events for the Center element.
 */
static bool center_handle_event(KryonRuntime* runtime, KryonElement* element, const ElementEvent* event) {
    // Use the generic script event handler for standard events
    return generic_script_event_handler(runtime, element, event);
}

/**
 * @brief Cleans up any custom data for a Center element.
 */
static void center_destroy(KryonRuntime* runtime, KryonElement* element) {
    // No custom data to free for a center element.
    (void)runtime;
    (void)element;
}