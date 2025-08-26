/**
 * @file tabbar.c
 * @brief Implementation of the TabBar element.
 *
 * This file contains the rendering, event handling, and lifecycle functions
 * for the TabBar element, which manages tab headers and content switching.
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

// =============================================================================
// TABBAR STATE (Element-Owned)
// =============================================================================

typedef struct {
    int selected_tab_index;        // Currently active tab (0-based)
    int hovered_tab_index;         // Which tab is mouse-hovered (-1 = none)
    bool initialized;              // Whether state has been initialized
} TabBarState;

static TabBarState* get_tabbar_state(KryonElement* element) {
    return (TabBarState*)element->user_data;
}

static TabBarState* ensure_tabbar_state(KryonElement* element) {
    TabBarState* state = get_tabbar_state(element);
    if (!state) {
        state = kryon_alloc(sizeof(TabBarState));
        if (state) {
            memset(state, 0, sizeof(TabBarState));
            
            // Initialize with default values
            state->selected_tab_index = get_element_property_int(element, "selectedIndex", 0);
            state->hovered_tab_index = -1;
            state->initialized = true;
            element->user_data = state;
        }
    }
    return state;
}

/**
 * @brief Helper function for Tab elements to set the selected index
 * This allows Tab elements to communicate with their parent TabBar
 */
void tabbar_set_selected_index(KryonRuntime* runtime, KryonElement* tabbar_element, int index) {
    printf("🔍 TABBAR: tabbar_set_selected_index called with index %d\n", index);
    
    if (!tabbar_element || strcmp(tabbar_element->type_name, "TabBar") != 0) {
        printf("🔍 TABBAR: Invalid TabBar element\n");
        return;
    }
    
    TabBarState* state = ensure_tabbar_state(tabbar_element);
    if (state) {
        printf("🔍 TABBAR: Setting TabBar state selected_tab_index to %d\n", index);
        state->selected_tab_index = index;
        
        // Also update the bound variable so TabContent can see the change
        if (runtime) {
            // Convert index to string since variables are strings
            char index_str[32];
            snprintf(index_str, sizeof(index_str), "%d", index);
            
            printf("🔍 TABBAR: Setting selectedTab variable to '%s'\n", index_str);
            // Update the selectedTab variable (hardcoded for now, but could be made dynamic)
            bool success = kryon_runtime_set_variable(runtime, "selectedTab", index_str);
            printf("🔍 TABBAR: Variable update %s\n", success ? "succeeded" : "failed");
        }
    } else {
        printf("🔍 TABBAR: Failed to get/create TabBar state\n");
    }
}

static void destroy_tabbar_state(TabBarState* state) {
    if (state) {
        kryon_free(state);
    }
}

// Forward declarations for the VTable functions
static void tabbar_render(KryonRuntime* runtime, KryonElement* element, KryonRenderCommand* commands, size_t* command_count, size_t max_commands);
static bool tabbar_handle_event(KryonRuntime* runtime, KryonElement* element, const ElementEvent* event);
static void tabbar_destroy(KryonRuntime* runtime, KryonElement* element);

// The VTable binds the generic element interface to our specific tabbar functions.
static const ElementVTable g_tabbar_vtable = {
    .render = tabbar_render,
    .handle_event = tabbar_handle_event,
    .destroy = tabbar_destroy
};

// =============================================================================
//  Public Registration Function
// =============================================================================

/**
 * @brief Registers the TabBar element type with the element registry.
 * This is called once at runtime startup.
 */
bool register_tabbar_element(void) {
    return element_register_type("TabBar", &g_tabbar_vtable);
}

// =============================================================================
//  Helper Functions
// =============================================================================

/**
 * @brief Gets the position property as a string, defaults to "top"
 */
static const char* get_tabbar_position(KryonElement* element) {
    const char* position = get_element_property_string(element, "position");
    return position ? position : "top";
}

/**
 * @brief Gets the selected tab index from state or property
 */
static int get_selected_index(KryonElement* element) {
    TabBarState* state = get_tabbar_state(element);
    if (state && state->initialized) {
        return state->selected_tab_index;
    }
    return get_element_property_int(element, "selectedIndex", 0);
}

/**
 * @brief Public function to get the current selected index (for Tab elements)
 * This allows Tab elements to check the TabBar's current state
 */
int tabbar_get_selected_index(KryonElement* tabbar_element) {
    if (!tabbar_element || strcmp(tabbar_element->type_name, "TabBar") != 0) return 0;
    return get_selected_index(tabbar_element);
}

/**
 * @brief Public function to check if a specific tab is active
 * This allows other systems to query tab state
 */
bool tabbar_is_tab_active(KryonElement* tabbar_element, int tab_index) {
    if (!tabbar_element || strcmp(tabbar_element->type_name, "TabBar") != 0) return false;
    return get_selected_index(tabbar_element) == tab_index;
}

/**
 * @brief Gets tab spacing, defaults to 5
 */
static int get_tab_spacing(KryonElement* element) {
    return get_element_property_int(element, "tabSpacing", 5);
}

/**
 * @brief Gets indicator thickness, defaults to 2
 */
static int get_indicator_thickness(KryonElement* element) {
    return get_element_property_int(element, "indicatorThickness", 2);
}

/**
 * @brief Counts child Tab elements
 */
static int count_tab_children(KryonElement* element) {
    int count = 0;
    for (size_t i = 0; i < element->child_count; i++) {
        if (strcmp(element->children[i]->type_name, "Tab") == 0) {
            count++;
        }
    }
    return count;
}

/**
 * @brief Counts child TabContent elements
 */
static int count_tabcontent_children(KryonElement* element) {
    int count = 0;
    for (size_t i = 0; i < element->child_count; i++) {
        if (strcmp(element->children[i]->type_name, "TabContent") == 0) {
            count++;
        }
    }
    return count;
}

/**
 * @brief Gets the nth Tab child element
 */
static KryonElement* get_tab_child(KryonElement* element, int index) {
    int count = 0;
    for (size_t i = 0; i < element->child_count; i++) {
        if (strcmp(element->children[i]->type_name, "Tab") == 0) {
            if (count == index) {
                return element->children[i];
            }
            count++;
        }
    }
    return NULL;
}

/**
 * @brief Gets the nth TabContent child element
 */
static KryonElement* get_tabcontent_child(KryonElement* element, int index) {
    int count = 0;
    for (size_t i = 0; i < element->child_count; i++) {
        if (strcmp(element->children[i]->type_name, "TabContent") == 0) {
            if (count == index) {
                return element->children[i];
            }
            count++;
        }
    }
    return NULL;
}

// =============================================================================
//  Element VTable Implementations
// =============================================================================


/**
 * @brief Renders the TabBar element.
 * TabBar is a layout container - positioning is handled by the global layout system.
 * TabBar itself doesn't generate render commands - its children render themselves.
 */
static void tabbar_render(KryonRuntime* runtime, KryonElement* element, KryonRenderCommand* commands, size_t* command_count, size_t max_commands) {
    // TabBar is purely a layout container - no rendering needed
    // The global positioning system handles child positioning
    // Child Tab and TabContent elements render themselves
}

/**
 * @brief Handles events for the TabBar element.
 */
static bool tabbar_handle_event(KryonRuntime* runtime, KryonElement* element, const ElementEvent* event) {
    if (!element || !event) return false;

    // TabBar doesn't need special event handling anymore
    // Individual Tab elements handle their own clicks through the centralized event system
    // The global positioning system ensures Tab positions are correct before events fire

    // Handle mouse movement for hover effects (optional)
    if (event->type == ELEMENT_EVENT_MOUSE_MOVED) {
        TabBarState* state = ensure_tabbar_state(element);
        if (!state) return false;
        
        // Hover tracking could be implemented here if needed for visual effects
        // For now, let individual Tab elements handle their own hover states
    }

    // Use script event handler for potential TabBar script interactions
    return handle_script_events(runtime, element, event);
}

/**
 * @brief Destroys the TabBar element and cleans up resources.
 */
static void tabbar_destroy(KryonRuntime* runtime, KryonElement* element) {
    if (!element) return;
    
    // Clean up TabBar state
    TabBarState* state = get_tabbar_state(element);
    if (state) {
        destroy_tabbar_state(state);
        element->user_data = NULL;
    }
    
    // Child elements are automatically destroyed by the element system
    (void)runtime; // Unused parameter
}