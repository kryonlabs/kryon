/**
 * @file row.c
 * @brief Implementation of the Row layout element.
 *
 * Row elements arrange their children horizontally with support for
 * alignment, spacing, and responsive layout properties.
 *
 * 0BSD License
 */

#include "elements.h"
#include "runtime.h"
#include "memory.h"
#include <stdio.h>
#include <string.h>


// Forward declarations for the VTable functions
static void row_render(KryonRuntime* runtime, KryonElement* element, KryonRenderCommand* commands, size_t* command_count, size_t max_commands);
static bool row_handle_event(KryonRuntime* runtime, KryonElement* element, const ElementEvent* event);
static void row_destroy(KryonRuntime* runtime, KryonElement* element);

// The VTable binds the generic element interface to our specific row functions.
static const ElementVTable g_row_vtable = {
    .render = row_render,
    .handle_event = row_handle_event,
    .destroy = row_destroy
};

// =============================================================================
//  Public Registration Function
// =============================================================================

/**
 * @brief Registers the Row element type with the element registry.
 */
bool register_row_element(void) {
    return element_register_type("Row", &g_row_vtable);
}

// =============================================================================
//  Element VTable Implementations
// =============================================================================

/**
 * @brief Calculates layout for the Row element by positioning its children horizontally.
 * Complete implementation with all alignment modes including space distribution.
 */
// Legacy layout function removed - positioning now handled by calculate_all_element_positions pipeline

/**
 * @brief Renders the Row element (layout elements don't render themselves).
 */
static void row_render(KryonRuntime* runtime, KryonElement* element, KryonRenderCommand* commands, size_t* command_count, size_t max_commands) {
    // Row is a layout element - it doesn't render itself, only positions children during layout phase
    // The layout calculation should have already positioned all children correctly
    // This function is called during rendering and should not modify positions
    
    (void)runtime;
    (void)element;
    (void)commands;
    (void)command_count;
    (void)max_commands;
    
    // Row elements don't generate render commands themselves
    // Their children will be rendered separately by the rendering system
}

/**
 * @brief Handles events for the Row element.
 */
static bool row_handle_event(KryonRuntime* runtime, KryonElement* element, const ElementEvent* event) {
    // Use the generic script event handler for standard events
    return generic_script_event_handler(runtime, element, event);
}

/**
 * @brief Cleans up any custom data for a Row element.
 */
static void row_destroy(KryonRuntime* runtime, KryonElement* element) {
    // No custom data to free for a row element.
    (void)runtime;
    (void)element;
}