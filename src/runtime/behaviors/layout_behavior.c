/**
 * @file layout_behavior.c
 * @brief Layout Behavior Implementation
 * 
 * Handles container-style layout functionality including Row, Column, Center, and Container.
 * This behavior doesn't render anything visually, but participates in the layout calculation system.
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
// LAYOUT BEHAVIOR IMPLEMENTATION
// =============================================================================

static bool layout_init(struct KryonElement* element) {
    // Ensure layout state is initialized
    ElementBehaviorState* state = element_get_behavior_state(element);
    if (!state) return false;
    
    state->layout_dirty = true;
    state->position_changed = false;
    
    return true;
}

static void layout_render(struct KryonRuntime* runtime, 
                         struct KryonElement* element,
                         KryonRenderCommand* commands,
                         size_t* command_count,
                         size_t max_commands) {
    
    // Layout behavior doesn't render anything visual
    // It only participates in the layout calculation system
    // Visual rendering is handled by the Renderable behavior
    
    // However, we can mark that this element needs layout recalculation if dirty
    ElementBehaviorState* state = element_get_behavior_state(element);
    if (state && state->layout_dirty) {
        // The layout system will handle this during position calculation
        // We just ensure the flag is properly set
    }
}

static bool layout_handle_event(struct KryonRuntime* runtime,
                               struct KryonElement* element,
                               const ElementEvent* event) {
    
    // Layout behavior can handle events that might affect layout
    // For example, window resize events that require recalculation
    
    ElementBehaviorState* state = element_get_behavior_state(element);
    if (!state) return false;
    
    switch (event->type) {
        case ELEMENT_EVENT_VALUE_CHANGED:
            // If any property changes, we might need to recalculate layout
            state->layout_dirty = true;
            break;
            
        default:
            break;
    }
    
    return false; // Layout behavior doesn't consume events
}

static void layout_destroy(struct KryonElement* element) {
    // No special cleanup needed for layout behavior
    (void)element;
}

static void* layout_get_state(struct KryonElement* element) {
    return element_get_behavior_state(element);
}

// =============================================================================
// LAYOUT UTILITY FUNCTIONS
// =============================================================================

/**
 * @brief Check if an element has layout behavior
 */
bool element_has_layout_behavior(struct KryonElement* element) {
    if (!element || !element->type_name) return false;
    
    // Check if this element type has layout behavior
    // This is used by the positioning system to know which elements are containers
    return (strcmp(element->type_name, "Container") == 0 ||
            strcmp(element->type_name, "Column") == 0 ||
            strcmp(element->type_name, "Row") == 0 ||
            strcmp(element->type_name, "Center") == 0 ||
            strcmp(element->type_name, "App") == 0 ||
            strcmp(element->type_name, "Grid") == 0 ||
            strcmp(element->type_name, "TabBar") == 0);
}

/**
 * @brief Get the layout type for an element
 */
const char* element_get_layout_type(struct KryonElement* element) {
    if (!element || !element->type_name) return "none";
    
    // Map element types to their layout behavior
    if (strcmp(element->type_name, "Row") == 0) return "row";
    if (strcmp(element->type_name, "Column") == 0) return "column";
    if (strcmp(element->type_name, "Center") == 0) return "center";
    if (strcmp(element->type_name, "Container") == 0) return "container";
    if (strcmp(element->type_name, "App") == 0) return "container";
    if (strcmp(element->type_name, "Grid") == 0) return "grid";
    if (strcmp(element->type_name, "TabBar") == 0) return "tabbar";
    
    return "none";
}

/**
 * @brief Mark an element's layout as dirty
 */
bool element_mark_layout_dirty(struct KryonElement* element) {
    ElementBehaviorState* state = element_get_behavior_state(element);
    if (!state) return false;
    
    state->layout_dirty = true;
    
    // Also mark parent layout as dirty since child changes affect parent
    if (element->parent) {
        element_mark_layout_dirty(element->parent);
    }
    
    return true;
}

/**
 * @brief Check if an element needs layout recalculation
 */
bool element_needs_layout_recalc(struct KryonElement* element) {
    ElementBehaviorState* state = element_get_behavior_state(element);
    return state ? state->layout_dirty : false;
}

/**
 * @brief Clear layout dirty flag
 */
void element_clear_layout_dirty(struct KryonElement* element) {
    ElementBehaviorState* state = element_get_behavior_state(element);
    if (state) {
        state->layout_dirty = false;
    }
}

// =============================================================================
// BEHAVIOR DEFINITION
// =============================================================================

static const ElementBehavior g_layout_behavior = {
    .name = "Layout",
    .init = layout_init,
    .render = layout_render,
    .handle_event = layout_handle_event,
    .destroy = layout_destroy,
    .get_state = layout_get_state
};

// =============================================================================
// REGISTRATION FUNCTION
// =============================================================================

__attribute__((constructor))
void register_layout_behavior(void) {
    element_behavior_register(&g_layout_behavior);
}