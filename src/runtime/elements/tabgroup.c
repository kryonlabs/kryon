/**

 * @file tabgroup.c
 * @brief Implementation of the TabGroup element.
 *
 * This file contains the rendering, event handling, and lifecycle functions
 * for the TabGroup element, which manages TabBar and TabContent children
 * with automatic state scoping and linking.
 *
 * 0BSD License
 */
#include "lib9.h"


#include "elements.h"
#include "runtime.h"
#include "memory.h"
#include "color_utils.h"
#include "element_mixins.h"

// =============================================================================
// TABGROUP STATE (Element-Owned)
// =============================================================================

typedef struct {
    int selected_tab_index;        // Currently active tab (0-based) - SOURCE OF TRUTH
    int last_synced_value;         // Last value we synced to the bound variable
    bool initialized;              // Whether state has been initialized
} TabGroupState;

static TabGroupState* get_tabgroup_state(KryonElement* element) {
    return (TabGroupState*)element->user_data;
}

static TabGroupState* ensure_tabgroup_state(KryonElement* element) {
    TabGroupState* state = get_tabgroup_state(element);
    if (!state) {
        state = kryon_alloc(sizeof(TabGroupState));
        if (state) {
            memset(state, 0, sizeof(TabGroupState));

            // Initialize with default values
            state->selected_tab_index = get_element_property_int(element, "selectedIndex", 0);
            state->last_synced_value = state->selected_tab_index;  // Track initial value
            state->initialized = true;
            element->user_data = state;
        }
    }
    return state;
}

/**
 * @brief Public function to get the current selected index from TabGroup
 * This allows child TabBar/TabContent elements to query the parent's state
 */
int tabgroup_get_selected_index(KryonElement* tabgroup_element) {
    if (!tabgroup_element || strcmp(tabgroup_element->type_name, "TabGroup") != 0) return 0;

    TabGroupState* state = get_tabgroup_state(tabgroup_element);
    if (state && state->initialized) {
        return state->selected_tab_index;
    }
    return get_element_property_int(tabgroup_element, "selectedIndex", 0);
}

/**
 * @brief Public function to set the selected index in TabGroup
 * This allows child Tab elements to update the parent's state
 */
void tabgroup_set_selected_index(KryonRuntime* runtime, KryonElement* tabgroup_element, int index) {
    print("🔍 TABGROUP: tabgroup_set_selected_index called with index %d\n", index);

    if (!tabgroup_element || strcmp(tabgroup_element->type_name, "TabGroup") != 0) {
        print("🔍 TABGROUP: Invalid TabGroup element\n");
        return;
    }

    TabGroupState* state = ensure_tabgroup_state(tabgroup_element);
    if (state) {
        print("🔍 TABGROUP: Setting TabGroup state selected_tab_index to %d\n", index);
        state->selected_tab_index = index;

        // Update the bound variable if one exists
        if (runtime) {
            const char* selected_index_str = get_element_property_string(tabgroup_element, "selectedIndex");
            if (selected_index_str) {
                // If selectedIndex is bound to a variable, update it
                char index_str[32];
                snprint(index_str, sizeof(index_str), "%d", index);

                print("🔍 TABGROUP: Attempting to update bound variable '%s' to '%s'\n",
                       selected_index_str, index_str);
                bool success = kryon_runtime_set_variable(runtime, selected_index_str, index_str);
                print("🔍 TABGROUP: Variable update %s\n", success ? "succeeded" : "failed");

                // Track this value - we know we set it, so don't sync back from it
                if (success) {
                    state->last_synced_value = index;
                }
            }
        }
    } else {
        print("🔍 TABGROUP: Failed to get/create TabGroup state\n");
    }
}

static void destroy_tabgroup_state(TabGroupState* state) {
    if (state) {
        kryon_free(state);
    }
}

// Forward declarations for the VTable functions
static void tabgroup_render(KryonRuntime* runtime, KryonElement* element, KryonRenderCommand* commands, size_t* command_count, size_t max_commands);
static bool tabgroup_handle_event(KryonRuntime* runtime, KryonElement* element, const ElementEvent* event);
static void tabgroup_destroy(KryonRuntime* runtime, KryonElement* element);

// The VTable binds the generic element interface to our specific tabgroup functions.
static const ElementVTable g_tabgroup_vtable = {
    .render = tabgroup_render,
    .handle_event = tabgroup_handle_event,
    .destroy = tabgroup_destroy
};

// =============================================================================
//  Public Registration Function
// =============================================================================

/**
 * @brief Registers the TabGroup element type with the element registry.
 * This is called once at runtime startup.
 */
bool register_tabgroup_element(void) {
    return element_register_type("TabGroup", &g_tabgroup_vtable);
}

// =============================================================================
//  Element VTable Implementations
// =============================================================================

/**
 * @brief Renders the TabGroup element.
 * TabGroup is a layout container - positioning is handled by the global layout system.
 * TabGroup itself doesn't generate render commands - its children render themselves.
 */
static void tabgroup_render(KryonRuntime* runtime, KryonElement* element, KryonRenderCommand* commands, size_t* command_count, size_t max_commands) {
    if (*command_count >= max_commands - 1) return;

    // Ensure state is initialized
    TabGroupState* state = ensure_tabgroup_state(element);

    // Sync with selectedIndex property ONLY if it changed externally (e.g. from script updates)
    // We track last_synced_value to distinguish "we changed it" vs "external change"
    if (state && runtime) {
        const char* selected_index_str = get_element_property_string_with_runtime(runtime, element, "selectedIndex");
        if (selected_index_str) {
            int property_value = atoi(selected_index_str);

            // Only sync if property changed AND it's not the value we just set
            if (property_value != state->last_synced_value) {
                print("🔧 TABGROUP: External change detected! Property: %d, Last synced: %d, Current state: %d\n",
                       property_value, state->last_synced_value, state->selected_tab_index);
                print("🔧 TABGROUP: Syncing state from external property change: %d -> %d\n",
                       state->selected_tab_index, property_value);

                // External change - sync property -> state
                state->selected_tab_index = property_value;
                state->last_synced_value = property_value;
            }
            // else: property == last_synced_value means we set it ourselves, don't sync back
        }
    }

    // TabGroup is primarily a layout container with state management
    // Render background and border if specified
    render_background_and_border(element, commands, command_count, max_commands);

    // Children (TabBar, TabContent) render themselves and can query our state
}

/**
 * @brief Handles events for the TabGroup element.
 */
static bool tabgroup_handle_event(KryonRuntime* runtime, KryonElement* element, const ElementEvent* event) {
    if (!element || !event) return false;

    // TabGroup doesn't handle events directly - children handle their own events
    // Use script event handler for potential TabGroup script interactions
    return handle_script_events(runtime, element, event);
}

/**
 * @brief Destroys the TabGroup element and cleans up resources.
 */
static void tabgroup_destroy(KryonRuntime* runtime, KryonElement* element) {
    if (!element) return;

    // Clean up TabGroup state
    TabGroupState* state = get_tabgroup_state(element);
    if (state) {
        destroy_tabgroup_state(state);
        element->user_data = NULL;
    }

    // Child elements are automatically destroyed by the element system
    (void)runtime; // Unused parameter
}
