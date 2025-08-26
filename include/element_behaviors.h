/**
 * @file element_behaviors.h
 * @brief Element Behavior System - Composable UI Element Architecture
 * 
 * This system enables creation of UI elements through behavior composition instead
 * of inheritance. Elements are built by combining reusable behavior components
 * like Renderable, Clickable, Layout, etc.
 * 
 * Benefits:
 * - Zero code duplication between similar elements
 * - Trivial creation of complex elements through composition
 * - Automatic registration and VTable generation
 * - Unified state management across all behaviors
 * - Performance optimized (composition happens at registration time)
 * 
 * 0BSD License
 */

#ifndef KRYON_ELEMENT_BEHAVIORS_H
#define KRYON_ELEMENT_BEHAVIORS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "elements.h"
#include "runtime.h"

// Forward declarations
struct KryonRuntime;
struct KryonElement;

// =============================================================================
// UNIFIED ELEMENT STATE
// =============================================================================

/**
 * @brief Unified state structure that all behaviors can access and modify
 * This eliminates the need for each element type to manage its own state
 */
typedef struct {
    // Interaction state (used by Clickable, Hoverable behaviors)
    bool hovered;
    bool clicked;
    bool pressed;
    bool focused;
    
    // Selection state (used by Selectable behavior)
    bool selected;
    int selected_index;
    
    // Layout state (used by Layout behavior)
    bool layout_dirty;
    bool position_changed;
    
    // Render state (used by Renderable behavior)
    bool render_dirty;
    
    // Custom state data for complex elements
    void* custom_data;
    void (*destroy_custom_data)(void* data);
    
    // State change callbacks
    void (*on_state_changed)(struct KryonElement* element);
    
    // Initialization flag
    bool initialized;
} ElementBehaviorState;

// =============================================================================
// BEHAVIOR INTERFACE
// =============================================================================

/**
 * @brief Individual behavior component that can be mixed into elements
 * Each behavior provides specific functionality like rendering, event handling, etc.
 */
typedef struct ElementBehavior {
    const char* name;
    
    /**
     * @brief Initialize behavior state for an element
     * @param element Element to initialize behavior for
     * @return true on success, false on failure
     */
    bool (*init)(struct KryonElement* element);
    
    /**
     * @brief Render behavior's visual components
     * @param runtime Runtime context
     * @param element Element to render
     * @param commands Render command buffer
     * @param command_count Current command count (input/output)
     * @param max_commands Maximum commands allowed
     */
    void (*render)(struct KryonRuntime* runtime, 
                   struct KryonElement* element,
                   KryonRenderCommand* commands,
                   size_t* command_count,
                   size_t max_commands);
    
    /**
     * @brief Handle events for this behavior
     * @param runtime Runtime context
     * @param element Element receiving the event
     * @param event Event to handle
     * @return true if event was handled, false otherwise
     */
    bool (*handle_event)(struct KryonRuntime* runtime,
                         struct KryonElement* element,
                         const ElementEvent* event);
    
    /**
     * @brief Cleanup behavior-specific resources
     * @param element Element being destroyed
     */
    void (*destroy)(struct KryonElement* element);
    
    /**
     * @brief Get behavior-specific state data
     * @param element Element to get state for
     * @return Pointer to behavior state, or NULL if not applicable
     */
    void* (*get_state)(struct KryonElement* element);
    
} ElementBehavior;

// =============================================================================
// ELEMENT DEFINITION SYSTEM
// =============================================================================

/**
 * @brief Definition of an element type through behavior composition
 * Elements are defined by listing the behaviors they should have
 */
typedef struct ElementDefinition {
    const char* type_name;
    const char** behavior_names;  // NULL-terminated array of behavior names
    
    /**
     * @brief Optional custom render function for element-specific rendering
     * This is called after all behavior render functions
     */
    void (*custom_render)(struct KryonRuntime* runtime,
                          struct KryonElement* element,
                          KryonRenderCommand* commands,
                          size_t* command_count,
                          size_t max_commands);
    
    /**
     * @brief Optional custom event handler for element-specific logic
     * This is called before behavior event handlers
     */
    bool (*custom_handle_event)(struct KryonRuntime* runtime,
                                struct KryonElement* element,
                                const ElementEvent* event);
    
    /**
     * @brief Optional custom initialization for element-specific setup
     */
    bool (*custom_init)(struct KryonElement* element);
    
    /**
     * @brief Optional custom destruction for element-specific cleanup
     */
    void (*custom_destroy)(struct KryonElement* element);
    
} ElementDefinition;

// =============================================================================
// BEHAVIOR SYSTEM API
// =============================================================================

/**
 * @brief Initialize the behavior system
 * @return true on success, false on failure
 */
bool element_behavior_system_init(void);

/**
 * @brief Cleanup the behavior system
 */
void element_behavior_system_cleanup(void);

/**
 * @brief Register a behavior with the system
 * @param behavior Behavior to register
 * @return true on success, false on failure
 */
bool element_behavior_register(const ElementBehavior* behavior);

/**
 * @brief Register an element definition with the system
 * This composes the behaviors into a VTable and registers with element system
 * @param definition Element definition to register
 * @return true on success, false on failure
 */
bool element_definition_register(const ElementDefinition* definition);

/**
 * @brief Get behavior by name
 * @param name Name of the behavior
 * @return Pointer to behavior, or NULL if not found
 */
const ElementBehavior* element_behavior_get(const char* name);

/**
 * @brief Get or create unified state for an element
 * @param element Element to get state for
 * @return Pointer to element's behavior state
 */
ElementBehaviorState* element_get_behavior_state(struct KryonElement* element);

/**
 * @brief Destroy unified state for an element
 * @param element Element to destroy state for
 */
void element_destroy_behavior_state(struct KryonElement* element);

// =============================================================================
// CONVENIENCE MACROS
// =============================================================================

/**
 * @brief Declare an element type with automatic registration
 * This macro creates an element definition and registers it automatically
 * 
 * Example usage:
 *   DECLARE_ELEMENT(Button, "Renderable", "Clickable", "Text")
 *   DECLARE_ELEMENT_WITH_CUSTOM(TabBar, "Renderable", "Layout", "Selectable", tabbar_custom_render)
 */
#define DECLARE_ELEMENT(name, ...) \
    static const char* name##_behaviors[] = {__VA_ARGS__, NULL}; \
    static ElementDefinition name##_def = { \
        .type_name = #name, \
        .behavior_names = name##_behaviors, \
        .custom_render = NULL, \
        .custom_handle_event = NULL, \
        .custom_init = NULL, \
        .custom_destroy = NULL \
    }; \
    __attribute__((constructor)) void register_##name##_element(void) { \
        element_definition_register(&name##_def); \
    }

#define DECLARE_ELEMENT_WITH_CUSTOM(name, custom_render_func, ...) \
    static const char* name##_behaviors[] = {__VA_ARGS__, NULL}; \
    static ElementDefinition name##_def = { \
        .type_name = #name, \
        .behavior_names = name##_behaviors, \
        .custom_render = custom_render_func, \
        .custom_handle_event = NULL, \
        .custom_init = NULL, \
        .custom_destroy = NULL \
    }; \
    __attribute__((constructor)) void register_##name##_element(void) { \
        element_definition_register(&name##_def); \
    }

#define DECLARE_ELEMENT_FULL(name, custom_render, custom_event, custom_init, custom_destroy, ...) \
    static const char* name##_behaviors[] = {__VA_ARGS__, NULL}; \
    static ElementDefinition name##_def = { \
        .type_name = #name, \
        .behavior_names = name##_behaviors, \
        .custom_render = custom_render, \
        .custom_handle_event = custom_event, \
        .custom_init = custom_init, \
        .custom_destroy = custom_destroy \
    }; \
    __attribute__((constructor)) void register_##name##_element(void) { \
        element_definition_register(&name##_def); \
    }

// =============================================================================
// BEHAVIOR STATE ACCESS HELPERS
// =============================================================================

/**
 * @brief Check if element is hovered
 */
static inline bool element_is_hovered(struct KryonElement* element) {
    ElementBehaviorState* state = element_get_behavior_state(element);
    return state ? state->hovered : false;
}

/**
 * @brief Check if element is clicked
 */
static inline bool element_is_clicked(struct KryonElement* element) {
    ElementBehaviorState* state = element_get_behavior_state(element);
    return state ? state->clicked : false;
}

/**
 * @brief Check if element is selected
 */
static inline bool element_is_selected(struct KryonElement* element) {
    ElementBehaviorState* state = element_get_behavior_state(element);
    return state ? state->selected : false;
}

/**
 * @brief Get selected index for selectable elements
 */
static inline int element_get_selected_index(struct KryonElement* element) {
    ElementBehaviorState* state = element_get_behavior_state(element);
    return state ? state->selected_index : -1;
}

/**
 * @brief Set selected index for selectable elements
 */
bool element_set_selected_index(struct KryonElement* element, int index);

/**
 * @brief Set hover state
 */
bool element_set_hovered(struct KryonElement* element, bool hovered);

/**
 * @brief Set clicked state
 */
bool element_set_clicked(struct KryonElement* element, bool clicked);

/**
 * @brief Initialize all behaviors for a newly created element
 * This should be called when an element is created
 */
bool element_initialize_behaviors(struct KryonElement* element);

#ifdef __cplusplus
}
#endif

#endif // KRYON_ELEMENT_BEHAVIORS_H