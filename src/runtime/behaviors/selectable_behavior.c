/**
 * @file selectable_behavior.c
 * @brief Selectable Behavior Implementation
 * 
 * Handles selection state and selection-related events for elements that can be
 * selected or have selectable children (like TabBars, Dropdowns, etc.).
 * 
 * 0BSD License
 */

#include "element_behaviors.h"
#include "elements.h"
#include "runtime.h"
#include "memory.h"
#include <stdio.h>
#include <string.h>

// =============================================================================
// SELECTABLE BEHAVIOR IMPLEMENTATION
// =============================================================================

static bool selectable_init(struct KryonElement* element) {
    ElementBehaviorState* state = element_get_behavior_state(element);
    if (!state) return false;
    
    // Initialize selection state from properties
    state->selected = get_element_property_bool(element, "selected", false);
    state->selected_index = get_element_property_int(element, "selectedIndex", -1);
    
    return true;
}

static void selectable_render(struct KryonRuntime* runtime, 
                             struct KryonElement* element,
                             KryonRenderCommand* commands,
                             size_t* command_count,
                             size_t max_commands) {
    
    // Selectable behavior can provide visual feedback for selection state
    // This could modify the appearance based on whether the element is selected
    
    ElementBehaviorState* state = element_get_behavior_state(element);
    if (!state) return;
    
    // The actual visual rendering is typically handled by custom render functions
    // or by other behaviors like Renderable, but we could add selection indicators here
    
    // For example, for TabBar elements, this could render selection indicators
    // For now, we leave this to element-specific custom render functions
}

static bool selectable_handle_event(struct KryonRuntime* runtime,
                                   struct KryonElement* element,
                                   const ElementEvent* event) {
    
    ElementBehaviorState* state = element_get_behavior_state(element);
    if (!state) return false;
    
    switch (event->type) {
        case ELEMENT_EVENT_CLICKED:
            // Handle selection changes on click
            if (strcmp(element->type_name, "TabBar") == 0) {
                // For TabBar, determine which tab was clicked based on mouse position
                float mouse_x = event->data.mousePos.x;
                int tab_count = element->child_count;
                
                if (tab_count > 0) {
                    // Calculate which tab was clicked
                    float tab_width = element->width / (float)tab_count;
                    int clicked_tab = (int)((mouse_x - element->x) / tab_width);
                    
                    if (clicked_tab >= 0 && clicked_tab < tab_count) {
                        element_set_selected_index(element, clicked_tab);
                        
                        // Generate selection changed event
                        ElementEvent selection_event = {0};
                        selection_event.type = ELEMENT_EVENT_SELECTION_CHANGED;
                        selection_event.timestamp = event->timestamp;
                        selection_event.data.selectionChanged.oldValue = state->selected_index;
                        selection_event.data.selectionChanged.newValue = clicked_tab;
                        
                        // Trigger script handler
                        generic_script_event_handler(runtime, element, &selection_event);
                        
                        return true; // Event handled
                    }
                }
            } else {
                // For other selectable elements, toggle selection state
                bool new_selected = !state->selected;
                state->selected = new_selected;
                state->render_dirty = true;
                
                // Generate selection changed event
                ElementEvent selection_event = {0};
                selection_event.type = ELEMENT_EVENT_SELECTION_CHANGED;
                selection_event.timestamp = event->timestamp;
                selection_event.data.selectionChanged.oldValue = state->selected ? 0 : 1;
                selection_event.data.selectionChanged.newValue = state->selected ? 1 : 0;
                
                generic_script_event_handler(runtime, element, &selection_event);
                
                return true; // Event handled
            }
            break;
            
        case ELEMENT_EVENT_SELECTION_CHANGED:
            // Handle programmatic selection changes
            state->selected_index = event->data.selectionChanged.newValue;
            state->render_dirty = true;
            
            // Trigger script handler
            return generic_script_event_handler(runtime, element, event);
            
        default:
            break;
    }
    
    return false;
}

static void selectable_destroy(struct KryonElement* element) {
    // No special cleanup needed for selectable behavior
    (void)element;
}

static void* selectable_get_state(struct KryonElement* element) {
    return element_get_behavior_state(element);
}

// =============================================================================
// SELECTABLE UTILITY FUNCTIONS
// =============================================================================

/**
 * @brief Get the selected index for a selectable element
 */
int selectable_get_selected_index(struct KryonElement* element) {
    ElementBehaviorState* state = element_get_behavior_state(element);
    return state ? state->selected_index : -1;
}

/**
 * @brief Set the selected index for a selectable element
 */
bool selectable_set_selected_index(struct KryonElement* element, int index) {
    ElementBehaviorState* state = element_get_behavior_state(element);
    if (!state) return false;
    
    if (state->selected_index != index) {
        int old_index = state->selected_index;
        state->selected_index = index;
        state->render_dirty = true;
        
        // Trigger state change callback if present
        if (state->on_state_changed) {
            state->on_state_changed(element);
        }
        
        // Generate selection changed event would happen here if we had runtime context
        // For now, just trigger the state change callback
        (void)old_index; // Suppress unused variable warning
    }
    
    return true;
}

/**
 * @brief Check if an element is selected
 */
bool selectable_is_selected(struct KryonElement* element) {
    ElementBehaviorState* state = element_get_behavior_state(element);
    return state ? state->selected : false;
}

/**
 * @brief Set the selected state for a selectable element
 */
bool selectable_set_selected(struct KryonElement* element, bool selected) {
    ElementBehaviorState* state = element_get_behavior_state(element);
    if (!state) return false;
    
    if (state->selected != selected) {
        state->selected = selected;
        state->render_dirty = true;
        
        if (state->on_state_changed) {
            state->on_state_changed(element);
        }
    }
    
    return true;
}

// =============================================================================
// BEHAVIOR DEFINITION
// =============================================================================

static const ElementBehavior g_selectable_behavior = {
    .name = "Selectable",
    .init = selectable_init,
    .render = selectable_render,
    .handle_event = selectable_handle_event,
    .destroy = selectable_destroy,
    .get_state = selectable_get_state
};

// =============================================================================
// REGISTRATION FUNCTION
// =============================================================================

__attribute__((constructor))
void register_selectable_behavior(void) {
    element_behavior_register(&g_selectable_behavior);
}