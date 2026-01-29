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
#include "lib9.h"


#include "elements.h"
#include "runtime.h"
#include "memory.h"
#include "color_utils.h"
#include "element_mixins.h"
#include <math.h>

// Forward declarations for TabGroup integration
extern int tabgroup_get_selected_index(KryonElement* tabgroup_element);

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

    // Render background and border using mixin
    render_background_and_border(element, commands, command_count, max_commands);

    // Calculate content area with padding
    float content_x = x + padding;
    float content_y = y + padding;
    float content_width = width - (2 * padding);
    float content_height = height - (2 * padding);

    // Ensure content area is valid
    if (content_width <= 0 || content_height <= 0) {
        return;
    }

    // Get the active index - check for TabGroup parent first (auto-mode)
    int active_index = 0;

    // Mode 1: Check if we have a TabGroup parent (auto-mode)
    if (element->parent && strcmp(element->parent->type_name, "TabGroup") == 0) {
        active_index = tabgroup_get_selected_index(element->parent);
    } else {
        // Mode 2: Use bound variable (standalone or ID-based mode)
        const char* active_index_str = get_element_property_string_with_runtime(runtime, element, "activeIndex");
        if (active_index_str) {
            active_index = atoi(active_index_str);
        }
    }

    // Find the Nth TabPanel child (skip @for templates and other non-TabPanel elements)
    int panel_index = 0;
    KryonElement* active_panel = NULL;
    for (size_t i = 0; i < element->child_count; i++) {
        KryonElement* child = element->children[i];
        if (child && strcmp(child->type_name, "TabPanel") == 0) {
            if (panel_index == active_index) {
                active_panel = child;
                break;
            }
            panel_index++;
        }
    }

    // Update active panel bounds
    if (active_panel) {
        active_panel->x = content_x;
        active_panel->y = content_y;
        active_panel->width = content_width;
        active_panel->height = content_height;

    }

    // Hide all non-active TabPanel children using visibility property
    panel_index = 0;
    for (size_t i = 0; i < element->child_count; i++) {
        KryonElement* child = element->children[i];
        if (child && strcmp(child->type_name, "TabPanel") == 0) {
            if (panel_index == active_index) {
                // Make active panel visible
                child->visible = true;
            } else {
                // Hide inactive panels using visibility property
                child->visible = false;
            }
            panel_index++;
        }
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
