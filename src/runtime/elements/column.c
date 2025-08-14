/**
 * @file column.c
 * @brief Implementation of the Column layout element.
 *
 * Column elements arrange their children vertically with support for
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
static void column_render(KryonRuntime* runtime, KryonElement* element, KryonRenderCommand* commands, size_t* command_count, size_t max_commands);
static bool column_handle_event(KryonRuntime* runtime, KryonElement* element, const ElementEvent* event);
static void column_destroy(KryonRuntime* runtime, KryonElement* element);

// The VTable binds the generic element interface to our specific column functions.
static const ElementVTable g_column_vtable = {
    .render = column_render,
    .handle_event = column_handle_event,
    .destroy = column_destroy
};

// =============================================================================
//  Public Registration Function
// =============================================================================

/**
 * @brief Registers the Column element type with the element registry.
 */
bool register_column_element(void) {
    return element_register_type("Column", &g_column_vtable);
}

// =============================================================================
//  Element VTable Implementations
// =============================================================================

/**
 * @brief Calculates layout for the Column element by positioning its children vertically.
 * Complete implementation with all alignment modes including space distribution.
 */
// Legacy layout function removed - positioning now handled by calculate_all_element_positions pipeline

/**
 * @brief Renders the Column element (layout elements don't render themselves).
 */
static void column_render(KryonRuntime* runtime, KryonElement* element, KryonRenderCommand* commands, size_t* command_count, size_t max_commands) {
    // Column is a layout element - it doesn't render itself, only positions children during layout phase
    // The layout calculation should have already positioned all children correctly
    // This function is called during rendering and should not modify positions
    
    (void)runtime;
    (void)element;
    (void)commands;
    (void)command_count;
    (void)max_commands;
    
    // Column elements don't generate render commands themselves
    // Their children will be rendered separately by the rendering system
}

/**
 * @brief Handles events for the Column element.
 */
static bool column_handle_event(KryonRuntime* runtime, KryonElement* element, const ElementEvent* event) {
    // Use the generic script event handler for standard events
    return generic_script_event_handler(runtime, element, event);
}

/**
 * @brief Cleans up any custom data for a Column element.
 */
static void column_destroy(KryonRuntime* runtime, KryonElement* element) {
    // No custom data to free for a column element.
    (void)runtime;
    (void)element;
}