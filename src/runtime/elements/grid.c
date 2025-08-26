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
#include "element_mixins.h"
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
    
    // Render grid background and border using mixin
    render_background_and_border(element, commands, command_count, max_commands);
}

/**
 * @brief Handles events for the Grid element
 */
static bool grid_handle_event(KryonRuntime* runtime, KryonElement* element, const ElementEvent* event) {
    // Use the generic script event handler for standard events
    return handle_script_events(runtime, element, event);
}

/**
 * @brief Cleans up any custom data for a Grid element
 */
static void grid_destroy(KryonRuntime* runtime, KryonElement* element) {
    // No custom data to free for a basic grid element
    (void)runtime;
    (void)element;
}