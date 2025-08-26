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
#include "element_mixins.h"
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
    
    // --- 1. Render App Background and Border (using shared mixin) ---
    render_background_and_border(element, commands, command_count, max_commands);
    
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
    // Use the shared script event handler mixin
    return handle_script_events(runtime, element, event);
}

/**
 * @brief Cleans up any custom data for an App element.
 */
static void app_destroy(KryonRuntime* runtime, KryonElement* element) {
    // No custom data to free for a basic app element.
    (void)runtime;
    (void)element;
}