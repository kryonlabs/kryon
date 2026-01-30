/**
 * @file runtime_internal.h
 * @brief Internal Runtime Module API
 *
 * This header defines the internal API used by runtime submodules to communicate
 * with each other. It should only be included by files in src/runtime/core/.
 *
 * Module Organization:
 * - runtime.c              - Main public API and coordination
 * - runtime_lifecycle.c    - Initialization, shutdown, configuration
 * - runtime_tree.c         - Element tree operations
 * - runtime_events.c       - Event dispatch and handling
 * - runtime_directives.c   - @for/@if directive expansion
 * - runtime_layout.c       - Layout calculation
 * - runtime_updates.c      - Update loop and rendering
 */

#ifndef KRYON_RUNTIME_INTERNAL_H
#define KRYON_RUNTIME_INTERNAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "runtime.h"
#include "elements.h"
#include "events.h"
#include "memory.h"

// =============================================================================
// LIFECYCLE MODULE (runtime_lifecycle.c)
// =============================================================================

/**
 * Initialize a runtime instance with configuration
 * @param runtime Runtime instance to initialize
 * @param config Configuration settings
 * @return true on success, false on failure
 */
bool kryon_runtime_lifecycle_init(KryonRuntime *runtime, const KryonRuntimeConfig *config);

/**
 * Shutdown and cleanup a runtime instance
 * @param runtime Runtime instance to shutdown
 */
void kryon_runtime_lifecycle_shutdown(KryonRuntime *runtime);

/**
 * Validate runtime configuration
 * @param config Configuration to validate
 * @return true if valid, false otherwise
 */
bool kryon_runtime_lifecycle_validate_config(const KryonRuntimeConfig *config);

/**
 * Get default runtime configuration
 * @return Default configuration
 */
KryonRuntimeConfig kryon_runtime_default_config(void);

// =============================================================================
// TREE MODULE (runtime_tree.c)
// =============================================================================

/**
 * Create a new element in the tree
 * @param runtime Runtime context
 * @param type Element type ID
 * @param parent Parent element (can be NULL for root)
 * @return New element or NULL on failure
 */
KryonElement* kryon_runtime_tree_create_element(
    KryonRuntime *runtime,
    uint16_t type,
    KryonElement *parent
);

/**
 * Destroy an element and all its children
 * @param runtime Runtime context
 * @param element Element to destroy
 */
void kryon_runtime_tree_destroy_element(KryonRuntime *runtime, KryonElement *element);

/**
 * Add a child element to a parent
 * @param runtime Runtime context
 * @param parent Parent element
 * @param child Child element to add
 */
void kryon_runtime_tree_add_child(
    KryonRuntime *runtime,
    KryonElement *parent,
    KryonElement *child
);

/**
 * Insert child element at specific position
 * @param runtime Runtime context
 * @param parent Parent element
 * @param child Child element to insert
 * @param position Position index
 */
void kryon_runtime_tree_insert_child_at(
    KryonRuntime *runtime,
    KryonElement *parent,
    KryonElement *child,
    size_t position
);

/**
 * Remove a child element from parent
 * @param runtime Runtime context
 * @param parent Parent element
 * @param child Child element to remove
 */
void kryon_runtime_tree_remove_child(
    KryonRuntime *runtime,
    KryonElement *parent,
    KryonElement *child
);

/**
 * Find element by ID in tree
 * @param runtime Runtime context
 * @param root Root element to search from
 * @param element_id Element ID to find
 * @return Element or NULL if not found
 */
KryonElement* kryon_runtime_tree_find_by_id(
    KryonRuntime *runtime,
    KryonElement *root,
    const char *element_id
);

/**
 * Clone element deeply (with all children)
 * @param runtime Runtime context
 * @param element Element to clone
 * @return Cloned element or NULL on failure
 */
KryonElement* kryon_runtime_tree_clone_deep(
    KryonRuntime *runtime,
    KryonElement *element
);

/**
 * Update element tree recursively
 * @param runtime Runtime context
 * @param element Root element to update
 * @param delta_time Time delta in seconds
 */
void kryon_runtime_tree_update_recursive(
    KryonRuntime *runtime,
    KryonElement *element,
    double delta_time
);

// =============================================================================
// EVENT MODULE (runtime_events.c)
// =============================================================================

/**
 * Dispatch event to element and handlers
 * @param runtime Runtime context
 * @param element Target element
 * @param event Event to dispatch
 * @return true if event was handled
 */
bool kryon_runtime_events_dispatch(
    KryonRuntime *runtime,
    KryonElement *element,
    const KryonEvent *event
);

/**
 * Process event queue
 * @param runtime Runtime context
 */
void kryon_runtime_events_process_queue(KryonRuntime *runtime);

/**
 * Runtime event handler callback
 * @param event Event to handle
 * @param user_data Runtime context
 * @return true if event was handled
 */
bool kryon_runtime_events_handler(const KryonEvent *event, void *user_data);

/**
 * Register event handler on element
 * @param element Target element
 * @param event_type Type of event
 * @param handler Handler function
 * @param user_data User data for handler
 * @param capture Use capture phase
 * @return true on success
 */
bool kryon_runtime_events_register_handler(
    KryonElement *element,
    KryonEventType event_type,
    KryonEventHandler handler,
    void *user_data,
    bool capture
);

// =============================================================================
// DIRECTIVE MODULE (runtime_directives.c)
// =============================================================================

/**
 * Expand @for directive template
 * @param runtime Runtime context
 * @param for_element Element with @for directive
 */
void kryon_runtime_directives_expand_for(
    KryonRuntime *runtime,
    KryonElement *for_element
);

/**
 * Expand @if directive template
 * @param runtime Runtime context
 * @param if_element Element with @if directive
 */
void kryon_runtime_directives_expand_if(
    KryonRuntime *runtime,
    KryonElement *if_element
);

/**
 * Clear expanded children from directive
 * @param runtime Runtime context
 * @param directive_element Directive element
 */
void kryon_runtime_directives_clear_expanded(
    KryonRuntime *runtime,
    KryonElement *directive_element
);

/**
 * Clone element with variable substitution
 * @param runtime Runtime context
 * @param template_element Template to clone
 * @param var_name Variable name
 * @param var_value Variable value
 * @param index Iteration index
 * @return Cloned element with substitutions
 */
KryonElement* kryon_runtime_directives_clone_with_substitution(
    KryonRuntime *runtime,
    KryonElement *template_element,
    const char *var_name,
    const char *var_value,
    size_t index
);

/**
 * Set loop context recursively on element tree
 * @param element Root element
 * @param index_var_name Index variable name
 * @param index_var_value Index variable value
 * @param var_name Item variable name
 * @param var_value Item variable value
 */
void kryon_runtime_directives_set_loop_context(
    KryonElement *element,
    const char *index_var_name,
    const char *index_var_value,
    const char *var_name,
    const char *var_value
);

/**
 * Evaluate runtime condition expression
 * @param runtime Runtime context
 * @param condition_expr Condition expression string
 * @return true if condition is true
 */
bool kryon_runtime_directives_evaluate_condition(
    KryonRuntime *runtime,
    const char *condition_expr
);

// =============================================================================
// LAYOUT MODULE (runtime_layout.c)
// =============================================================================

/**
 * Calculate layout for element tree
 * @param runtime Runtime context
 * @param root Root element
 */
void kryon_runtime_layout_calculate(
    KryonRuntime *runtime,
    KryonElement *root
);

/**
 * Mark element layout as dirty
 * @param element Element to mark
 */
void kryon_runtime_layout_mark_dirty(KryonElement *element);

/**
 * Clear layout flags recursively
 * @param element Root element
 */
void kryon_runtime_layout_clear_flags(KryonElement *element);

/**
 * Calculate element height
 * @param element Element to calculate
 * @return Calculated height
 */
float kryon_runtime_layout_calculate_height(KryonElement *element);

/**
 * Process layout for element
 * @param element Element to process
 * @param commands Render command buffer
 * @param command_count Current command count
 * @param max_commands Maximum commands
 */
void kryon_runtime_layout_process(
    KryonElement *element,
    KryonRenderCommand *commands,
    size_t *command_count,
    size_t max_commands
);

// =============================================================================
// UPDATE MODULE (runtime_updates.c)
// =============================================================================

/**
 * Update runtime frame
 * @param runtime Runtime context
 * @param delta_time Time delta in seconds
 * @return true on success
 */
bool kryon_runtime_updates_update_frame(
    KryonRuntime *runtime,
    double delta_time
);

/**
 * Render runtime frame
 * @param runtime Runtime context
 * @return true on success
 */
bool kryon_runtime_updates_render_frame(KryonRuntime *runtime);

/**
 * Convert element tree to render commands
 * @param runtime Runtime context
 * @param element Root element
 * @param commands Command buffer
 * @param command_count Current command count
 * @param max_commands Maximum commands
 */
void kryon_runtime_updates_element_to_commands(
    KryonRuntime *runtime,
    KryonElement *element,
    KryonRenderCommand *commands,
    size_t *command_count,
    size_t max_commands
);

// =============================================================================
// UTILITY FUNCTIONS (shared across modules)
// =============================================================================

/**
 * Runtime error reporting
 * @param runtime Runtime context
 * @param format Printf-style format string
 */
void kryon_runtime_error(KryonRuntime *runtime, const char *format, ...);

/**
 * Resolve variable reference in string
 * @param runtime Runtime context
 * @param element Element context
 * @param value String value with possible variable references
 * @return Resolved string value
 */
const char* kryon_runtime_resolve_variable(
    KryonRuntime *runtime,
    KryonElement *element,
    const char *value
);

/**
 * Resolve component state variable
 * @param runtime Runtime context
 * @param element Element context
 * @param var_name Variable name
 * @return Variable value or NULL
 */
const char* kryon_runtime_resolve_component_state(
    KryonRuntime *runtime,
    KryonElement *element,
    const char *var_name
);

// =============================================================================
// GLOBAL STATE (to be refactored later)
// =============================================================================

/**
 * Set current global runtime
 * @param runtime Runtime to set as current
 */
void kryon_runtime_set_current(KryonRuntime *runtime);

/**
 * Get current global runtime
 * @return Current runtime or NULL
 */
KryonRuntime* kryon_runtime_get_current(void);

#ifdef __cplusplus
}
#endif

#endif // KRYON_RUNTIME_INTERNAL_H
