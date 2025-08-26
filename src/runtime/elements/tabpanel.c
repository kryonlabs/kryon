/**
 * @file tabpanel.c
 * @brief Implementation of the TabPanel element.
 *
 * This file contains the rendering, event handling, and lifecycle functions
 * for the TabPanel element, which represents content associated with a single tab.
 * 
 * TabPanel extends Container functionality but participates in the tab indexing
 * system. It has an optional 'index' property used at compile-time for reordering,
 * but at runtime it behaves like a standard container.
 *
 * 0BSD License
 */

#include "elements.h"
#include "runtime.h"
#include "memory.h"
#include "element_mixins.h"
#include <stdio.h>
#include <string.h>

// Forward declarations for the VTable functions
static void tabpanel_render(KryonRuntime* runtime, KryonElement* element, KryonRenderCommand* commands, size_t* command_count, size_t max_commands);
static bool tabpanel_handle_event(KryonRuntime* runtime, KryonElement* element, const ElementEvent* event);
static void tabpanel_destroy(KryonRuntime* runtime, KryonElement* element);

// The VTable binds the generic element interface to our specific tabpanel functions.
static const ElementVTable g_tabpanel_vtable = {
    .render = tabpanel_render,
    .handle_event = tabpanel_handle_event,
    .destroy = tabpanel_destroy
};

// =============================================================================
//  Public Registration Function
// =============================================================================

/**
 * @brief Registers the TabPanel element type with the element registry.
 * This is called once at runtime startup.
 */
bool register_tabpanel_element(void) {
    return element_register_type("TabPanel", &g_tabpanel_vtable);
}

// =============================================================================
//  Element VTable Implementations
// =============================================================================

/**
 * @brief Renders the TabPanel element.
 * TabPanel behaves like a Container - it renders background/border and 
 * lets the unified layout system handle child positioning.
 */
static void tabpanel_render(KryonRuntime* runtime, KryonElement* element, KryonRenderCommand* commands, size_t* command_count, size_t max_commands) {
    if (!element || !commands || !command_count) return;

    // Render background and border using shared mixin (like Container)
    render_background_and_border(element, commands, command_count, max_commands);

    // TabPanel acts as a container - child rendering is handled by the main element system
    // The unified layout engine in elements.c handles child positioning automatically
}

/**
 * @brief Handles events for the TabPanel element.
 * TabPanel can handle script-based events but doesn't need special tab logic.
 */
static bool tabpanel_handle_event(KryonRuntime* runtime, KryonElement* element, const ElementEvent* event) {
    if (!element || !event) return false;

    // TabPanel can have script event handlers (onClick, onMount, etc.)
    return handle_script_events(runtime, element, event);
}

/**
 * @brief Destroys the TabPanel element and cleans up resources.
 */
static void tabpanel_destroy(KryonRuntime* runtime, KryonElement* element) {
    if (!element) return;
    
    // TabPanel has no special state to clean up
    // Child elements are automatically destroyed by the element system
    (void)runtime; // Unused parameter
}