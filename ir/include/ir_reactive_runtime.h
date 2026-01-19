/**
 * Kryon Reactive Runtime API
 * Provides runtime support for reactive state, computed properties, actions, and watchers
 */

#ifndef IR_REACTIVE_RUNTIME_H
#define IR_REACTIVE_RUNTIME_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Forward declaration
typedef struct IRReactiveManifest IRReactiveManifest;

// ============================================================================
// Public API
// ============================================================================

/**
 * Initialize the reactive runtime with a manifest
 * Must be called before any other reactive runtime functions
 */
void ir_reactive_runtime_init(IRReactiveManifest* manifest);

/**
 * Shutdown the reactive runtime and free resources
 */
void ir_reactive_runtime_shutdown(void);

/**
 * Notify the runtime that a reactive variable has been updated
 * This will:
 * - Mark the variable as dirty
 * - Invalidate dependent computed properties
 * - Trigger watchers (if not batching)
 * - Flush updates (if not batching)
 *
 * @param var_id The ID of the variable that was updated
 */
void ir_reactive_runtime_notify_update(uint32_t var_id);

/**
 * Execute an action function
 * This will:
 * - Begin batching (if action is batched)
 * - Execute the action function
 * - End batching and flush updates (if action is batched)
 * - Trigger auto-save watchers (if configured)
 *
 * @param action_id The ID of the action to execute
 * @param action_fn The function to execute (NULL to look up by name)
 */
void ir_reactive_runtime_execute_action(uint32_t action_id, void (*action_fn)(void));

/**
 * Get the value of a computed property
 * This will:
 * - Return cached value if valid
 * - Recompute if dirty
 * - Detect circular dependencies
 *
 * @param computed_id The ID of the computed property
 * @return The computed value, or NULL if error
 */
IRReactiveValue* ir_reactive_runtime_get_computed(uint32_t computed_id);

// Forward declaration for return type
typedef struct {
    int64_t as_int;
    double as_float;
    char* as_string;
    bool as_bool;
} IRReactiveValue;

/**
 * Begin batching updates
 * All updates between begin and end will be batched
 * and flushed together when end_batch is called
 */
void ir_reactive_runtime_begin_batch(void);

/**
 * End batching and flush all pending updates
 */
void ir_reactive_runtime_end_batch(void);

#ifdef __cplusplus
}
#endif

#endif // IR_REACTIVE_RUNTIME_H
