#ifndef IR_STATE_MANAGER_H
#define IR_STATE_MANAGER_H

/**
 * IR State Manager
 *
 * Centralized state management for the Kryon IR framework.
 * Replaces scattered state access patterns (executor globals, direct dirty flags,
 * reactive manifest sync) with a unified queuing and flushing system.
 *
 * DESIGN:
 * - All state changes are queued (non-blocking)
 * - Changes are applied in batches during flush()
 * - Single source of truth for all UI state
 * - Thread-safe potential via message queue pattern
 */

#include "ir_core.h"
#include "ir_executor.h"
#include <stdbool.h>
#include <stdint.h>

// ============================================================================
// Configuration
// ============================================================================

typedef struct {
    uint32_t max_queued_updates;     // Auto-flush when queue exceeds this (default: 1000)
    uint32_t flush_timeout_ms;        // Auto-flush timeout (default: 16ms = ~60fps)
    bool enable_profiling;            // Track update timing (debug only)
    bool enable_validation;           // Validate state consistency (debug only)
} IRStateManagerConfig;

// Default configuration
#define IR_STATE_MANAGER_DEFAULT_CONFIG { \
    .max_queued_updates = 1000, \
    .flush_timeout_ms = 16, \
    .enable_profiling = false, \
    .enable_validation = false \
}

// ============================================================================
// Update Types
// ============================================================================

typedef enum {
    IR_STATE_UPDATE_SET_VAR,          // Set variable value
    IR_STATE_UPDATE_MARK_DIRTY,       // Mark component dirty
    IR_STATE_UPDATE_CALL_HANDLER,     // Execute event handler
    IR_STATE_UPDATE_SYNC_INPUT,       // Sync Input component to variable
    IR_STATE_UPDATE_EVAL_EXPRESSION,  // Evaluate and assign expression
    IR_STATE_UPDATE_RENDER_LOOP,      // For-loop re-render
    IR_STATE_UPDATE_CONDITIONAL,      // Conditional re-evaluation
} IRStateUpdateType;

// Forward declaration
typedef struct IRStateUpdate IRStateUpdate;

// ============================================================================
// State Update
// ============================================================================

struct IRStateUpdate {
    IRStateUpdateType type;

    union {
        // IR_STATE_UPDATE_SET_VAR
        struct {
            char* var_name;
            char* scope;              // Optional scope (NULL = global)
            IRValue value;
        } set_var;

        // IR_STATE_UPDATE_MARK_DIRTY
        struct {
            uint32_t component_id;
            IRDirtyFlags flags;
            bool recursive;           // Mark subtree dirty
        } mark_dirty;

        // IR_STATE_UPDATE_CALL_HANDLER
        struct {
            uint32_t component_id;
            char* handler_name;
            uint32_t instance_id;
        } call_handler;

        // IR_STATE_UPDATE_SYNC_INPUT
        struct {
            IRComponent* input_comp;
        } sync_input;

        // IR_STATE_UPDATE_EVAL_EXPRESSION
        struct {
            char* expression;
            char* target_var;
            char* scope;
        } eval_expression;

        // IR_STATE_UPDATE_RENDER_LOOP
        struct {
            uint32_t for_loop_index;
        } render_loop;

        // IR_STATE_UPDATE_CONDITIONAL
        struct {
            uint32_t component_id;
            bool show;
        } conditional;
    };

    IRStateUpdate* next;              // Linked list for queue
};

// ============================================================================
// Change Notification Callbacks
// ============================================================================

typedef void (*IRStateChangeCallback)(
    uint32_t component_id,
    IRDirtyFlags changed_flags,
    void* user_data
);

// ============================================================================
// Flush Result
// ============================================================================

typedef struct {
    uint32_t updates_processed;
    uint32_t components_affected;
    uint32_t layout_recomputations;
    uint32_t expression_evaluations;
    double elapsed_ms;
    bool had_errors;
} IRStateFlushResult;

// ============================================================================
// State Manager
// ============================================================================

typedef struct IRStateManager {
    // Configuration
    IRStateManagerConfig config;

    // State (owned by state manager now)
    IRExecutorContext* executor;      // Owning reference
    IRReactiveManifest* manifest;     // Owning reference
    IRComponent* root_component;      // Non-owning reference

    // Update queue
    IRStateUpdate* update_queue_head;
    IRStateUpdate* update_queue_tail;
    uint32_t queue_count;

    // Change tracking
    uint32_t global_generation;       // Incremented on flush

    // Callbacks for change notification
    struct {
        IRStateChangeCallback* callbacks;
        void** user_data;
        uint32_t count;
        uint32_t capacity;
    } callbacks;

    // Validation state
    bool is_flushing;
    bool needs_flush;
    uint64_t last_flush_time;

    // Profiling data (when enabled)
    struct {
        uint64_t total_flushes;
        uint64_t total_updates_processed;
        double total_flush_time_ms;
    } stats;
} IRStateManager;

// ============================================================================
// Lifecycle
// ============================================================================

/**
 * Create a new state manager
 * @param config Configuration (NULL for defaults)
 * @return New state manager, or NULL on allocation failure
 */
IRStateManager* ir_state_manager_create(const IRStateManagerConfig* config);

/**
 * Destroy state manager and all owned resources
 * @param mgr State manager to destroy (can be NULL)
 */
void ir_state_manager_destroy(IRStateManager* mgr);

// ============================================================================
// Configuration
// ============================================================================

/**
 * Set or replace the executor context
 * Takes ownership of any existing executor and the new one
 * @param mgr State manager
 * @param executor Executor context (takes ownership)
 */
void ir_state_manager_set_executor(IRStateManager* mgr, IRExecutorContext* executor);

/**
 * Get the executor context (non-owning)
 * @param mgr State manager
 * @return Executor context, or NULL if not set
 */
IRExecutorContext* ir_state_manager_get_executor(IRStateManager* mgr);

/**
 * Set or replace the reactive manifest
 * Takes ownership of any existing manifest and the new one
 * @param mgr State manager
 * @param manifest Manifest (takes ownership)
 */
void ir_state_manager_set_manifest(IRStateManager* mgr, IRReactiveManifest* manifest);

/**
 * Get the reactive manifest (non-owning)
 * @param mgr State manager
 * @return Manifest, or NULL if not set
 */
IRReactiveManifest* ir_state_manager_get_manifest(IRStateManager* mgr);

/**
 * Set the root component
 * Does NOT take ownership (component tree is managed externally)
 * @param mgr State manager
 * @param root Root component (non-owning reference)
 */
void ir_state_manager_set_root(IRStateManager* mgr, IRComponent* root);

// ============================================================================
// Update Queueing (non-blocking)
// ============================================================================

/**
 * Queue a variable set operation
 * @param mgr State manager
 * @param name Variable name
 * @param value Value to set (takes ownership)
 * @param scope Scope string (NULL for global)
 * @return true if queued, false on allocation failure
 */
bool ir_state_queue_set_var(IRStateManager* mgr, const char* name, IRValue value, const char* scope);

/**
 * Queue a component dirty mark
 * @param mgr State manager
 * @param component_id Component ID
 * @param flags Dirty flags to set
 * @param recursive Whether to mark entire subtree dirty
 * @return true if queued, false on allocation failure
 */
bool ir_state_queue_mark_dirty(IRStateManager* mgr, uint32_t component_id, IRDirtyFlags flags, bool recursive);

/**
 * Queue a handler call
 * @param mgr State manager
 * @param component_id Component ID
 * @param handler_name Handler name to call
 * @param instance_id Instance ID for scoping
 * @return true if queued, false on allocation failure
 */
bool ir_state_queue_call_handler(IRStateManager* mgr, uint32_t component_id, const char* handler_name, uint32_t instance_id);

/**
 * Queue an Input component sync to variable
 * @param mgr State manager
 * @param input_comp Input component
 * @return true if queued, false on failure
 */
bool ir_state_queue_sync_input(IRStateManager* mgr, IRComponent* input_comp);

/**
 * Queue an expression evaluation
 * @param mgr State manager
 * @param expression Expression to evaluate
 * @param target_var Variable name to store result
 * @param scope Scope for evaluation
 * @return true if queued, false on allocation failure
 */
bool ir_state_queue_eval_expression(IRStateManager* mgr, const char* expression, const char* target_var, const char* scope);

/**
 * Queue a for-loop re-render
 * @param mgr State manager
 * @param for_loop_index Index of the for-loop in manifest
 * @return true if queued, false on allocation failure
 */
bool ir_state_queue_render_loop(IRStateManager* mgr, uint32_t for_loop_index);

/**
 * Queue a conditional visibility update
 * @param mgr State manager
 * @param component_id Component ID
 * @param show Whether to show the component
 * @return true if queued, false on allocation failure
 */
bool ir_state_queue_conditional(IRStateManager* mgr, uint32_t component_id, bool show);

// ============================================================================
// Flushing
// ============================================================================

/**
 * Check if a flush is needed
 * @param mgr State manager
 * @return true if updates are pending or timeout elapsed
 */
bool ir_state_flush_needed(IRStateManager* mgr);

/**
 * Process all queued updates
 * @param mgr State manager
 * @param out_result Optional result output (can be NULL)
 * @return true if flush succeeded, false on error
 */
bool ir_state_flush(IRStateManager* mgr, IRStateFlushResult* out_result);

// ============================================================================
// Callbacks
// ============================================================================

/**
 * Register a change notification callback
 * @param mgr State manager
 * @param callback Function to call on state changes
 * @param user_data User data passed to callback
 * @return Callback ID (for unregistering), or UINT32_MAX on error
 */
uint32_t ir_state_register_callback(IRStateManager* mgr, IRStateChangeCallback callback, void* user_data);

/**
 * Unregister a callback
 * @param mgr State manager
 * @param callback_id Callback ID from registration
 */
void ir_state_unregister_callback(IRStateManager* mgr, uint32_t callback_id);

// ============================================================================
// Inspection (debug)
// ============================================================================

/**
 * Get current queue size
 * @param mgr State manager
 * @return Number of pending updates
 */
uint32_t ir_state_manager_get_queue_size(IRStateManager* mgr);

/**
 * Get global generation counter
 * @param mgr State manager
 * @return Current generation (increments each flush)
 */
uint32_t ir_state_manager_get_generation(IRStateManager* mgr);

/**
 * Print queue contents to stderr (for debugging)
 * @param mgr State manager
 */
void ir_state_manager_print_queue(IRStateManager* mgr);

/**
 * Reset profiling statistics
 * @param mgr State manager
 */
void ir_state_manager_reset_stats(IRStateManager* mgr);

/**
 * Get profiling statistics
 * @param mgr State manager
 * @param out_flushes Output: total flushes performed
 * @param out_updates Output: total updates processed
 * @param out_time_ms Output: total flush time in ms
 */
void ir_state_manager_get_stats(IRStateManager* mgr, uint64_t* out_flushes, uint64_t* out_updates, double* out_time_ms);

// ============================================================================
// Convenience Wrappers (replace old APIs)
// ============================================================================

/**
 * Set a variable (convenience wrapper, queues and flushes)
 * Equivalent to: ir_state_queue_set_var() + ir_state_flush()
 * @param mgr State manager
 * @param name Variable name
 * @param value Value to set
 * @param scope Scope (NULL for global)
 * @return true on success
 */
bool ir_state_set_var(IRStateManager* mgr, const char* name, IRValue value, const char* scope);

/**
 * Get a variable value (delegates to executor)
 * @param mgr State manager
 * @param name Variable name
 * @param instance_id Instance ID (0 for global)
 * @return Variable value (caller should free string/array contents if needed)
 */
IRValue ir_state_get_var(IRStateManager* mgr, const char* name, uint32_t instance_id);

/**
 * Evaluate a text expression
 * @param mgr State manager
 * @param expression Expression to evaluate
 * @param scope Scope for variable lookup
 * @return Allocated string with result (caller must free), or NULL
 */
char* ir_state_eval_expression(IRStateManager* mgr, const char* expression, const char* scope);

#endif // IR_STATE_MANAGER_H
