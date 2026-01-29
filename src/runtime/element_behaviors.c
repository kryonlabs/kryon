/**

 * @file element_behaviors.c
 * @brief Implementation of the Element Behavior System
 * 
 * This file implements the core behavior system infrastructure including
 * behavior registration, element definition composition, and state management.
 * 
 * 0BSD License
 */
#include "lib9.h"


#include "element_behaviors.h"
#include "elements.h"
#include "runtime.h"
#include "memory.h"

// =============================================================================
// BEHAVIOR REGISTRY
// =============================================================================

#define MAX_BEHAVIORS 32
#define MAX_ELEMENT_DEFINITIONS 64

typedef struct {
    const ElementBehavior* behaviors[MAX_BEHAVIORS];
    size_t behavior_count;
    
    ElementDefinition definitions[MAX_ELEMENT_DEFINITIONS];
    size_t definition_count;
    
    bool initialized;
} BehaviorSystem;

static BehaviorSystem g_behavior_system = {0};

// =============================================================================
// COMPOSED VTABLE SYSTEM
// =============================================================================

/**
 * @brief Composed VTable that aggregates multiple behaviors
 */
typedef struct {
    ElementVTable base_vtable;
    const ElementBehavior** behaviors;
    size_t behavior_count;
    const ElementDefinition* definition;
} ComposedVTable;

// Storage for composed VTables (they need to persist)
static ComposedVTable g_composed_vtables[MAX_ELEMENT_DEFINITIONS];
static size_t g_composed_vtable_count = 0;

// =============================================================================
// BEHAVIOR SYSTEM INITIALIZATION
// =============================================================================

bool element_behavior_system_init(void) {
    if (g_behavior_system.initialized) {
        return true;
    }
    
    memset(&g_behavior_system, 0, sizeof(g_behavior_system));
    g_behavior_system.initialized = true;
    
    print("✓ Element Behavior System initialized\n");
    return true;
}

void element_behavior_system_cleanup(void) {
    // Free any dynamically allocated behavior arrays in composed VTables
    for (size_t i = 0; i < g_composed_vtable_count; i++) {
        if (g_composed_vtables[i].behaviors) {
            kryon_free((void*)g_composed_vtables[i].behaviors);
        }
    }
    
    memset(&g_behavior_system, 0, sizeof(g_behavior_system));
    g_composed_vtable_count = 0;
}

// =============================================================================
// BEHAVIOR REGISTRATION
// =============================================================================

bool element_behavior_register(const ElementBehavior* behavior) {
    if (!behavior || !behavior->name) {
        print("ERROR: Invalid behavior (NULL or no name)\n");
        return false;
    }
    
    if (g_behavior_system.behavior_count >= MAX_BEHAVIORS) {
        print("ERROR: Maximum number of behaviors (%d) exceeded\n", MAX_BEHAVIORS);
        return false;
    }
    
    // Check for duplicate behavior names
    for (size_t i = 0; i < g_behavior_system.behavior_count; i++) {
        if (strcmp(g_behavior_system.behaviors[i]->name, behavior->name) == 0) {
            print("ERROR: Behavior '%s' already registered\n", behavior->name);
            return false;
        }
    }
    
    g_behavior_system.behaviors[g_behavior_system.behavior_count++] = behavior;
    print("✓ Registered behavior: %s\n", behavior->name);
    return true;
}

const ElementBehavior* element_behavior_get(const char* name) {
    if (!name) return NULL;
    
    for (size_t i = 0; i < g_behavior_system.behavior_count; i++) {
        if (strcmp(g_behavior_system.behaviors[i]->name, name) == 0) {
            return g_behavior_system.behaviors[i];
        }
    }
    
    return NULL;
}

// =============================================================================
// UNIFIED STATE MANAGEMENT
// =============================================================================

ElementBehaviorState* element_get_behavior_state(struct KryonElement* element) {
    if (!element) return NULL;
    
    // If state doesn't exist, create it
    if (!element->user_data) {
        ElementBehaviorState* state = kryon_alloc(sizeof(ElementBehaviorState));
        if (!state) {
            print("ERROR: Failed to allocate element behavior state\n");
            return NULL;
        }
        
        memset(state, 0, sizeof(ElementBehaviorState));
        state->selected_index = -1;  // Default to no selection
        state->initialized = true;
        
        element->user_data = state;
    }
    
    return (ElementBehaviorState*)element->user_data;
}

void element_destroy_behavior_state(struct KryonElement* element) {
    if (!element || !element->user_data) return;
    
    ElementBehaviorState* state = (ElementBehaviorState*)element->user_data;
    
    // Call custom data destructor if present
    if (state->destroy_custom_data && state->custom_data) {
        state->destroy_custom_data(state->custom_data);
    }
    
    kryon_free(state);
    element->user_data = NULL;
}

// State manipulation functions
bool element_set_selected_index(struct KryonElement* element, int index) {
    ElementBehaviorState* state = element_get_behavior_state(element);
    if (!state) return false;
    
    if (state->selected_index != index) {
        state->selected_index = index;
        state->render_dirty = true;
        
        // Call state change callback if present
        if (state->on_state_changed) {
            state->on_state_changed(element);
        }
    }
    
    return true;
}

bool element_set_hovered(struct KryonElement* element, bool hovered) {
    ElementBehaviorState* state = element_get_behavior_state(element);
    if (!state) return false;
    
    if (state->hovered != hovered) {
        state->hovered = hovered;
        state->render_dirty = true;
        
        if (state->on_state_changed) {
            state->on_state_changed(element);
        }
    }
    
    return true;
}

bool element_set_clicked(struct KryonElement* element, bool clicked) {
    ElementBehaviorState* state = element_get_behavior_state(element);
    if (!state) return false;
    
    if (state->clicked != clicked) {
        state->clicked = clicked;
        state->render_dirty = true;
        
        if (state->on_state_changed) {
            state->on_state_changed(element);
        }
    }
    
    return true;
}

// =============================================================================
// COMPOSED VTABLE FUNCTIONS
// =============================================================================

/**
 * @brief Composed render function that calls all behavior render functions
 */
static void composed_render(struct KryonRuntime* runtime, 
                           struct KryonElement* element,
                           KryonRenderCommand* commands,
                           size_t* command_count,
                           size_t max_commands) {
    
    ComposedVTable* composed = (ComposedVTable*)element_get_vtable(element->type_name);
    if (!composed) return;
    
    // Call each behavior's render function
    for (size_t i = 0; i < composed->behavior_count; i++) {
        const ElementBehavior* behavior = composed->behaviors[i];
        if (behavior && behavior->render) {
            behavior->render(runtime, element, commands, command_count, max_commands);
            
            // Stop if we've reached the command limit
            if (*command_count >= max_commands) break;
        }
    }
    
    // Call custom render function if present
    if (composed->definition && composed->definition->custom_render) {
        composed->definition->custom_render(runtime, element, commands, command_count, max_commands);
    }
}

/**
 * @brief Composed event handler that calls all behavior event handlers
 */
static bool composed_handle_event(struct KryonRuntime* runtime,
                                 struct KryonElement* element,
                                 const ElementEvent* event) {
    
    ComposedVTable* composed = (ComposedVTable*)element_get_vtable(element->type_name);
    if (!composed) return false;
    
    // Call custom event handler first if present
    if (composed->definition && composed->definition->custom_handle_event) {
        if (composed->definition->custom_handle_event(runtime, element, event)) {
            return true;  // Event handled by custom handler
        }
    }
    
    // Call each behavior's event handler
    for (size_t i = 0; i < composed->behavior_count; i++) {
        const ElementBehavior* behavior = composed->behaviors[i];
        if (behavior && behavior->handle_event) {
            if (behavior->handle_event(runtime, element, event)) {
                return true;  // Event handled by this behavior
            }
        }
    }
    
    return false;  // Event not handled
}

/**
 * @brief Composed destroy function that calls all behavior destroy functions
 */
static void composed_destroy(struct KryonRuntime* runtime,
                            struct KryonElement* element) {
    
    ComposedVTable* composed = (ComposedVTable*)element_get_vtable(element->type_name);
    if (!composed) return;
    
    // Call custom destroy function first if present
    if (composed->definition && composed->definition->custom_destroy) {
        composed->definition->custom_destroy(element);
    }
    
    // Call each behavior's destroy function
    for (size_t i = 0; i < composed->behavior_count; i++) {
        const ElementBehavior* behavior = composed->behaviors[i];
        if (behavior && behavior->destroy) {
            behavior->destroy(element);
        }
    }
    
    // Destroy unified state
    element_destroy_behavior_state(element);
}

// =============================================================================
// ELEMENT DEFINITION REGISTRATION
// =============================================================================

bool element_definition_register(const ElementDefinition* definition) {
    if (!definition || !definition->type_name || !definition->behavior_names) {
        print("ERROR: Invalid element definition\n");
        return false;
    }
    
    if (g_behavior_system.definition_count >= MAX_ELEMENT_DEFINITIONS) {
        print("ERROR: Maximum element definitions (%d) exceeded\n", MAX_ELEMENT_DEFINITIONS);
        return false;
    }
    
    if (g_composed_vtable_count >= MAX_ELEMENT_DEFINITIONS) {
        print("ERROR: Maximum composed VTables (%d) exceeded\n", MAX_ELEMENT_DEFINITIONS);
        return false;
    }
    
    // Count behaviors and validate they exist
    size_t behavior_count = 0;
    for (const char** name = definition->behavior_names; *name; name++) {
        if (!element_behavior_get(*name)) {
            print("ERROR: Behavior '%s' not found for element '%s'\n", *name, definition->type_name);
            return false;
        }
        behavior_count++;
    }
    
    if (behavior_count == 0) {
        print("ERROR: Element '%s' has no behaviors\n", definition->type_name);
        return false;
    }
    
    // Create composed VTable
    ComposedVTable* composed = &g_composed_vtables[g_composed_vtable_count++];
    memset(composed, 0, sizeof(ComposedVTable));
    
    // Allocate behavior array (use malloc since this runs during constructor phase)
    composed->behaviors = malloc(sizeof(ElementBehavior*) * behavior_count);
    if (!composed->behaviors) {
        print("ERROR: Failed to allocate behavior array for element '%s'\n", definition->type_name);
        g_composed_vtable_count--;
        return false;
    }
    
    // Populate behavior array
    composed->behavior_count = behavior_count;
    composed->definition = definition;
    
    size_t idx = 0;
    for (const char** name = definition->behavior_names; *name; name++) {
        composed->behaviors[idx++] = element_behavior_get(*name);
    }
    
    // Set up base VTable
    composed->base_vtable.render = composed_render;
    composed->base_vtable.handle_event = composed_handle_event;
    composed->base_vtable.destroy = composed_destroy;
    
    // Store definition copy
    g_behavior_system.definitions[g_behavior_system.definition_count++] = *definition;
    
    // Register with element system
    if (!element_register_type(definition->type_name, &composed->base_vtable)) {
        print("ERROR: Failed to register element type '%s'\n", definition->type_name);
        free((void*)composed->behaviors);
        g_composed_vtable_count--;
        g_behavior_system.definition_count--;
        return false;
    }
    
    print("✓ Registered element: %s (", definition->type_name);
    for (size_t i = 0; i < behavior_count; i++) {
        print("%s", composed->behaviors[i]->name);
        if (i < behavior_count - 1) print(", ");
    }
    print(")\n");
    
    return true;
}

// =============================================================================
// BEHAVIOR INITIALIZATION SYSTEM
// =============================================================================

/**
 * @brief Initialize all behaviors for a newly created element
 * This should be called when an element is created
 */
bool element_initialize_behaviors(struct KryonElement* element) {
    if (!element || !element->type_name) return false;
    
    // Find the composed VTable for this element type
    ComposedVTable* composed = NULL;
    for (size_t i = 0; i < g_composed_vtable_count; i++) {
        if (g_composed_vtables[i].definition && 
            strcmp(g_composed_vtables[i].definition->type_name, element->type_name) == 0) {
            composed = &g_composed_vtables[i];
            break;
        }
    }
    
    if (!composed) return false;
    
    // Ensure behavior state exists
    ElementBehaviorState* state = element_get_behavior_state(element);
    if (!state) return false;
    
    // Call custom init if present
    if (composed->definition->custom_init) {
        if (!composed->definition->custom_init(element)) {
            print("ERROR: Custom init failed for element '%s'\n", element->type_name);
            return false;
        }
    }
    
    // Initialize all behaviors
    for (size_t i = 0; i < composed->behavior_count; i++) {
        const ElementBehavior* behavior = composed->behaviors[i];
        if (behavior && behavior->init) {
            if (!behavior->init(element)) {
                print("ERROR: Behavior '%s' init failed for element '%s'\n", 
                       behavior->name, element->type_name);
                return false;
            }
        }
    }
    
    return true;
}
