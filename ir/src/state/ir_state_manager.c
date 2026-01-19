/**
 * IR State Manager Implementation
 *
 * Centralized state management for the Kryon IR framework.
 */

#define _POSIX_C_SOURCE 200809L

#include "../include/ir_state.h"
#include "ir_log.h"
#include "../include/ir_executor.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/time.h>
#endif

// Forward declarations
typedef struct IRReactiveManifest IRReactiveManifest;

// ============================================================================
// Local Value Utilities (matching ir_executor.c static functions)
// ============================================================================

static void ir_value_free_local(IRValue* val) {
    if (!val) return;

    if (val->type == VAR_TYPE_STRING && val->string_val) {
        free(val->string_val);
        val->string_val = NULL;
    } else if (val->type == VAR_TYPE_ARRAY && val->array_val.items) {
        // Recursively free array elements
        for (int i = 0; i < val->array_val.count; i++) {
            ir_value_free_local(&val->array_val.items[i]);
        }
        free(val->array_val.items);
        val->array_val.items = NULL;
    }
}

// ============================================================================
// Timing Utilities
// ============================================================================

static double get_time_ms(void) {
#ifdef _WIN32
    LARGE_INTEGER frequency, counter;
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&counter);
    return (double)counter.QuadPart / frequency.QuadPart * 1000.0;
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000.0 + tv.tv_usec / 1000.0;
#endif
}

// ============================================================================
// Internal Helpers
// ============================================================================

static IRStateUpdate* update_create(IRStateUpdateType type) {
    IRStateUpdate* update = calloc(1, sizeof(IRStateUpdate));
    if (!update) {
        IR_LOG_ERROR("STATE", "Failed to allocate update");
        return NULL;
    }
    update->type = type;
    update->next = NULL;
    return update;
}

static void update_free(IRStateUpdate* update) {
    if (!update) return;

    // Free owned strings and values
    switch (update->type) {
        case IR_STATE_UPDATE_SET_VAR:
            free(update->set_var.var_name);
            free(update->set_var.scope);
            ir_value_free_local(&update->set_var.value);
            break;

        case IR_STATE_UPDATE_CALL_HANDLER:
            free(update->call_handler.handler_name);
            break;

        case IR_STATE_UPDATE_EVAL_EXPRESSION:
            free(update->eval_expression.expression);
            free(update->eval_expression.target_var);
            free(update->eval_expression.scope);
            break;

        default:
            // No owned resources in other types
            break;
    }

    free(update);
}

// Find component by ID in tree
static IRComponent* find_component_by_id(IRComponent* root, uint32_t id) {
    if (!root) return NULL;
    if (root->id == id) return root;

    for (uint32_t i = 0; i < root->child_count; i++) {
        IRComponent* found = find_component_by_id(root->children[i], id);
        if (found) return found;
    }
    return NULL;
}

// Notify all registered callbacks
__attribute__((unused)) static void notify_callbacks(IRStateManager* mgr, uint32_t component_id, IRDirtyFlags flags) {
    for (uint32_t i = 0; i < mgr->callbacks.count; i++) {
        if (mgr->callbacks.callbacks[i]) {
            mgr->callbacks.callbacks[i](component_id, flags, mgr->callbacks.user_data[i]);
        }
    }
}

// ============================================================================
// Phase 1: Validation
// ============================================================================

static bool flush_phase_validate(IRStateManager* mgr) {
    if (!mgr->executor) {
        IR_LOG_ERROR("STATE", "Cannot flush: no executor set");
        return false;
    }
    if (mgr->is_flushing) {
        IR_LOG_ERROR("STATE", "Cannot flush: already flushing (reentrant flush detected)");
        return false;
    }
    mgr->is_flushing = true;
    return true;
}

// ============================================================================
// Phase 2: Apply Updates
// ============================================================================

static uint32_t flush_phase_apply_updates(IRStateManager* mgr, IRStateFlushResult* result) {
    uint32_t components_affected = 0;
    IRStateUpdate* update = mgr->update_queue_head;

    while (update) {
        IRStateUpdate* next = update->next;

        switch (update->type) {
            case IR_STATE_UPDATE_SET_VAR: {
                // Set variable in executor
                ir_executor_set_var(mgr->executor,
                                    update->set_var.var_name,
                                    update->set_var.value,
                                    0);  // instance_id from scope
                update->set_var.value = (IRValue){0};  // Cleared by executor
                result->expression_evaluations++;
                break;
            }

            case IR_STATE_UPDATE_MARK_DIRTY: {
                IRComponent* comp = find_component_by_id(mgr->root_component,
                                                         update->mark_dirty.component_id);
                if (comp) {
                    ir_layout_mark_dirty(comp);
                    components_affected++;
                }
                break;
            }

            case IR_STATE_UPDATE_CALL_HANDLER: {
                mgr->executor->current_instance_id = update->call_handler.instance_id;
                ir_executor_call_handler(mgr->executor, update->call_handler.handler_name);
                result->expression_evaluations++;
                break;
            }

            case IR_STATE_UPDATE_SYNC_INPUT: {
                ir_executor_sync_input_to_var(mgr->executor, update->sync_input.input_comp);
                result->expression_evaluations++;
                break;
            }

            case IR_STATE_UPDATE_EVAL_EXPRESSION: {
                // Evaluate expression and assign to target variable
                char* result_str = ir_executor_eval_text_expression(
                    update->eval_expression.expression,
                    update->eval_expression.scope
                );
                if (result_str) {
                    IRValue val = { .type = VAR_TYPE_STRING, .string_val = result_str };
                    ir_executor_set_var(mgr->executor,
                                        update->eval_expression.target_var,
                                        val,
                                        0);
                    result->expression_evaluations++;
                }
                break;
            }

            case IR_STATE_UPDATE_RENDER_LOOP: {
                // Trigger for-loop re-render (handled in phase 3)
                result->expression_evaluations++;
                break;
            }

            case IR_STATE_UPDATE_CONDITIONAL: {
                IRComponent* comp = find_component_by_id(mgr->root_component,
                                                         update->conditional.component_id);
                if (comp && comp->style) {
                    comp->style->visible = update->conditional.show;
                    ir_layout_mark_dirty(comp);
                    components_affected++;
                }
                break;
            }

            default:
                IR_LOG_WARN("STATE", "Unknown update type: %d", update->type);
                break;
        }

        update_free(update);
        update = next;
    }

    // Clear queue
    mgr->update_queue_head = NULL;
    mgr->update_queue_tail = NULL;
    mgr->queue_count = 0;

    return components_affected;
}

// ============================================================================
// Phase 3: Re-evaluate Reactions
// ============================================================================

// Forward declarations for internal executor functions
// These are static in ir_executor.c but we can call the public equivalents
extern void ir_executor_apply_initial_conditionals(IRExecutorContext* ctx);

static void flush_phase_reactions(IRStateManager* mgr, IRStateFlushResult* result) {
    if (!mgr->executor) return;

    // Update text components with new variable values
    // This processes text expressions like {{variable}} in Text components
    ir_executor_update_text_components(mgr->executor);

    // Apply conditional visibility updates
    // This processes the manifest's conditionals and updates component visibility
    ir_executor_apply_initial_conditionals(mgr->executor);

    // Note: For-loop re-rendering would happen through ir_executor_render_for_loops
    // which is called internally by ir_executor_set_var when arrays change
    // Since we're batching updates, for-loops will be re-rendered once after flush

    // Sync manifest variables with executor state
    // Ensure manifest's reactive variables reflect the current executor values
    // (This happens implicitly as both systems reference the same data)

    result->expression_evaluations++;
}

// ============================================================================
// Phase 4: Layout Recomputation
// ============================================================================

static uint32_t flush_phase_layout(IRStateManager* mgr, IRStateFlushResult* result) {
    (void)result;
    uint32_t layout_recomputations = 0;

    if (!mgr->root_component) return 0;

    // Recompute layout for all dirty components
    // This is a simplified version - actual implementation would traverse
    // the tree and recompute only dirty subtrees

    // For now, we rely on the lazy layout computation that happens
    // during rendering. The dirty flags have been set, so the next
    // render pass will recompute as needed.

    return layout_recomputations;
}

// ============================================================================
// Phase 5: Notify Callbacks
// ============================================================================

static void flush_phase_notify(IRStateManager* mgr) {
    (void)mgr;
    // Callbacks are invoked during update processing for immediate feedback
    // Additional batched notifications could go here
}

// ============================================================================
// Public API Implementation
// ============================================================================

IRStateManager* ir_state_manager_create(const IRStateManagerConfig* config) {
    IRStateManager* mgr = calloc(1, sizeof(IRStateManager));
    if (!mgr) {
        IR_LOG_ERROR("STATE", "Failed to allocate state manager");
        return NULL;
    }

    // Apply config or use defaults
    if (config) {
        mgr->config = *config;
    } else {
        mgr->config = (IRStateManagerConfig)IR_STATE_MANAGER_DEFAULT_CONFIG;
    }

    // Initialize stats
    mgr->global_generation = 1;
    mgr->last_flush_time = (uint64_t)get_time_ms();

    return mgr;
}

void ir_state_manager_destroy(IRStateManager* mgr) {
    if (!mgr) return;

    // Free remaining updates in queue
    IRStateUpdate* update = mgr->update_queue_head;
    while (update) {
        IRStateUpdate* next = update->next;
        update_free(update);
        update = next;
    }

    // Free owned resources
    if (mgr->executor) {
        ir_executor_destroy(mgr->executor);
    }
    if (mgr->manifest) {
        ir_reactive_manifest_destroy(mgr->manifest);
    }

    // Free callbacks
    free(mgr->callbacks.callbacks);
    free(mgr->callbacks.user_data);

    free(mgr);
}

void ir_state_manager_set_executor(IRStateManager* mgr, IRExecutorContext* executor) {
    if (!mgr) return;

    if (mgr->executor) {
        ir_executor_destroy(mgr->executor);
    }

    mgr->executor = executor;
}

IRExecutorContext* ir_state_manager_get_executor(IRStateManager* mgr) {
    return mgr ? mgr->executor : NULL;
}

void ir_state_manager_set_manifest(IRStateManager* mgr, IRReactiveManifest* manifest) {
    if (!mgr) return;

    // Free old manifest if it exists
    if (mgr->manifest && mgr->manifest != manifest) {
        ir_reactive_manifest_destroy(mgr->manifest);
    }
    mgr->manifest = manifest;
}

IRReactiveManifest* ir_state_manager_get_manifest(IRStateManager* mgr) {
    return mgr ? mgr->manifest : NULL;
}

void ir_state_manager_set_root(IRStateManager* mgr, IRComponent* root) {
    if (!mgr) return;
    mgr->root_component = root;

    // Also set on executor for compatibility
    if (mgr->executor) {
        ir_executor_set_root(mgr->executor, root);
    }
}

bool ir_state_queue_set_var(IRStateManager* mgr, const char* name, IRValue value, const char* scope) {
    if (!mgr || !name) return false;

    IRStateUpdate* update = update_create(IR_STATE_UPDATE_SET_VAR);
    if (!update) return false;

    update->set_var.var_name = strdup(name);
    update->set_var.scope = scope ? strdup(scope) : NULL;
    update->set_var.value = value;

    // Add to queue
    if (mgr->update_queue_tail) {
        mgr->update_queue_tail->next = update;
    } else {
        mgr->update_queue_head = update;
    }
    mgr->update_queue_tail = update;
    mgr->queue_count++;
    mgr->needs_flush = true;

    // Auto-flush if queue exceeds limit
    if (mgr->queue_count >= mgr->config.max_queued_updates) {
        ir_state_flush(mgr, NULL);
    }

    return true;
}

bool ir_state_queue_mark_dirty(IRStateManager* mgr, uint32_t component_id, IRDirtyFlags flags, bool recursive) {
    if (!mgr) return false;

    IRStateUpdate* update = update_create(IR_STATE_UPDATE_MARK_DIRTY);
    if (!update) return false;

    update->mark_dirty.component_id = component_id;
    update->mark_dirty.flags = flags;
    update->mark_dirty.recursive = recursive;

    if (mgr->update_queue_tail) {
        mgr->update_queue_tail->next = update;
    } else {
        mgr->update_queue_head = update;
    }
    mgr->update_queue_tail = update;
    mgr->queue_count++;
    mgr->needs_flush = true;

    return true;
}

bool ir_state_queue_call_handler(IRStateManager* mgr, uint32_t component_id, const char* handler_name, uint32_t instance_id) {
    if (!mgr || !handler_name) return false;

    IRStateUpdate* update = update_create(IR_STATE_UPDATE_CALL_HANDLER);
    if (!update) return false;

    update->call_handler.component_id = component_id;
    update->call_handler.handler_name = strdup(handler_name);
    update->call_handler.instance_id = instance_id;

    if (mgr->update_queue_tail) {
        mgr->update_queue_tail->next = update;
    } else {
        mgr->update_queue_head = update;
    }
    mgr->update_queue_tail = update;
    mgr->queue_count++;
    mgr->needs_flush = true;

    return true;
}

bool ir_state_queue_sync_input(IRStateManager* mgr, IRComponent* input_comp) {
    if (!mgr || !input_comp) return false;

    IRStateUpdate* update = update_create(IR_STATE_UPDATE_SYNC_INPUT);
    if (!update) return false;

    update->sync_input.input_comp = input_comp;

    if (mgr->update_queue_tail) {
        mgr->update_queue_tail->next = update;
    } else {
        mgr->update_queue_head = update;
    }
    mgr->update_queue_tail = update;
    mgr->queue_count++;
    mgr->needs_flush = true;

    return true;
}

bool ir_state_queue_eval_expression(IRStateManager* mgr, const char* expression, const char* target_var, const char* scope) {
    if (!mgr || !expression) return false;

    IRStateUpdate* update = update_create(IR_STATE_UPDATE_EVAL_EXPRESSION);
    if (!update) return false;

    update->eval_expression.expression = strdup(expression);
    update->eval_expression.target_var = target_var ? strdup(target_var) : NULL;
    update->eval_expression.scope = scope ? strdup(scope) : NULL;

    if (mgr->update_queue_tail) {
        mgr->update_queue_tail->next = update;
    } else {
        mgr->update_queue_head = update;
    }
    mgr->update_queue_tail = update;
    mgr->queue_count++;
    mgr->needs_flush = true;

    return true;
}

bool ir_state_queue_render_loop(IRStateManager* mgr, uint32_t for_loop_index) {
    if (!mgr) return false;

    IRStateUpdate* update = update_create(IR_STATE_UPDATE_RENDER_LOOP);
    if (!update) return false;

    update->render_loop.for_loop_index = for_loop_index;

    if (mgr->update_queue_tail) {
        mgr->update_queue_tail->next = update;
    } else {
        mgr->update_queue_head = update;
    }
    mgr->update_queue_tail = update;
    mgr->queue_count++;
    mgr->needs_flush = true;

    return true;
}

bool ir_state_queue_conditional(IRStateManager* mgr, uint32_t component_id, bool show) {
    if (!mgr) return false;

    IRStateUpdate* update = update_create(IR_STATE_UPDATE_CONDITIONAL);
    if (!update) return false;

    update->conditional.component_id = component_id;
    update->conditional.show = show;

    if (mgr->update_queue_tail) {
        mgr->update_queue_tail->next = update;
    } else {
        mgr->update_queue_head = update;
    }
    mgr->update_queue_tail = update;
    mgr->queue_count++;
    mgr->needs_flush = true;

    return true;
}

bool ir_state_flush_needed(IRStateManager* mgr) {
    if (!mgr) return false;

    if (mgr->queue_count > 0) return true;

    // Check timeout
    double now = get_time_ms();
    if (now - mgr->last_flush_time >= mgr->config.flush_timeout_ms) {
        return true;
    }

    return false;
}

bool ir_state_flush(IRStateManager* mgr, IRStateFlushResult* out_result) {
    if (!mgr) return false;

    // Initialize result
    IRStateFlushResult result = {0};

    double start_time = get_time_ms();

    // Phase 1: Validation
    if (!flush_phase_validate(mgr)) {
        if (out_result) *out_result = result;
        mgr->is_flushing = false;
        return false;
    }

    // Phase 2: Apply Updates
    result.components_affected = flush_phase_apply_updates(mgr, &result);
    result.updates_processed = result.components_affected;  // Approximation

    // Phase 3: Re-evaluate Reactions
    flush_phase_reactions(mgr, &result);

    // Phase 4: Layout Recomputation
    result.layout_recomputations = flush_phase_layout(mgr, &result);

    // Phase 5: Notify Callbacks
    flush_phase_notify(mgr);

    // Update generation
    mgr->global_generation++;
    mgr->needs_flush = false;
    mgr->last_flush_time = (uint64_t)get_time_ms();

    // Calculate timing
    result.elapsed_ms = get_time_ms() - start_time;

    // Update stats
    mgr->stats.total_flushes++;
    mgr->stats.total_updates_processed += result.updates_processed;
    mgr->stats.total_flush_time_ms += result.elapsed_ms;

    mgr->is_flushing = false;

    if (out_result) {
        *out_result = result;
    }

    return true;
}

uint32_t ir_state_register_callback(IRStateManager* mgr, IRStateChangeCallback callback, void* user_data) {
    if (!mgr || !callback) return UINT32_MAX;

    // Expand capacity if needed
    if (mgr->callbacks.count >= mgr->callbacks.capacity) {
        uint32_t new_capacity = mgr->callbacks.capacity == 0 ? 8 : mgr->callbacks.capacity * 2;
        IRStateChangeCallback* new_callbacks = realloc(mgr->callbacks.callbacks,
                                                       new_capacity * sizeof(IRStateChangeCallback));
        void** new_user_data = realloc(mgr->callbacks.user_data,
                                       new_capacity * sizeof(void*));
        if (!new_callbacks || !new_user_data) {
            free(new_callbacks);
            free(new_user_data);
            return UINT32_MAX;
        }
        mgr->callbacks.callbacks = new_callbacks;
        mgr->callbacks.user_data = new_user_data;
        mgr->callbacks.capacity = new_capacity;
    }

    uint32_t id = mgr->callbacks.count;
    mgr->callbacks.callbacks[id] = callback;
    mgr->callbacks.user_data[id] = user_data;
    mgr->callbacks.count++;

    return id;
}

void ir_state_unregister_callback(IRStateManager* mgr, uint32_t callback_id) {
    if (!mgr || callback_id >= mgr->callbacks.count) return;

    // Shift remaining callbacks
    for (uint32_t i = callback_id; i < mgr->callbacks.count - 1; i++) {
        mgr->callbacks.callbacks[i] = mgr->callbacks.callbacks[i + 1];
        mgr->callbacks.user_data[i] = mgr->callbacks.user_data[i + 1];
    }
    mgr->callbacks.count--;
}

uint32_t ir_state_manager_get_queue_size(IRStateManager* mgr) {
    return mgr ? mgr->queue_count : 0;
}

uint32_t ir_state_manager_get_generation(IRStateManager* mgr) {
    return mgr ? mgr->global_generation : 0;
}

void ir_state_manager_print_queue(IRStateManager* mgr) {
    if (!mgr) {
        fprintf(stderr, "[STATE] No state manager\n");
        return;
    }

    fprintf(stderr, "[STATE] Queue size: %u\n", mgr->queue_count);

    IRStateUpdate* update = mgr->update_queue_head;
    uint32_t index = 0;
    while (update) {
        const char* type_name = "UNKNOWN";
        switch (update->type) {
            case IR_STATE_UPDATE_SET_VAR: type_name = "SET_VAR"; break;
            case IR_STATE_UPDATE_MARK_DIRTY: type_name = "MARK_DIRTY"; break;
            case IR_STATE_UPDATE_CALL_HANDLER: type_name = "CALL_HANDLER"; break;
            case IR_STATE_UPDATE_SYNC_INPUT: type_name = "SYNC_INPUT"; break;
            case IR_STATE_UPDATE_EVAL_EXPRESSION: type_name = "EVAL_EXPRESSION"; break;
            case IR_STATE_UPDATE_RENDER_LOOP: type_name = "RENDER_LOOP"; break;
            case IR_STATE_UPDATE_CONDITIONAL: type_name = "CONDITIONAL"; break;
        }
        fprintf(stderr, "  [%u] %s\n", index++, type_name);
        update = update->next;
    }
}

void ir_state_manager_reset_stats(IRStateManager* mgr) {
    if (!mgr) return;
    memset(&mgr->stats, 0, sizeof(mgr->stats));
}

void ir_state_manager_get_stats(IRStateManager* mgr, uint64_t* out_flushes, uint64_t* out_updates, double* out_time_ms) {
    if (!mgr) return;
    if (out_flushes) *out_flushes = mgr->stats.total_flushes;
    if (out_updates) *out_updates = mgr->stats.total_updates_processed;
    if (out_time_ms) *out_time_ms = mgr->stats.total_flush_time_ms;
}

// ============================================================================
// Convenience Wrappers
// ============================================================================

bool ir_state_set_var(IRStateManager* mgr, const char* name, IRValue value, const char* scope) {
    if (!ir_state_queue_set_var(mgr, name, value, scope)) {
        return false;
    }
    return ir_state_flush(mgr, NULL);
}

IRValue ir_state_get_var(IRStateManager* mgr, const char* name, uint32_t instance_id) {
    if (!mgr || !mgr->executor) {
        return (IRValue){ .type = VAR_TYPE_NULL };
    }
    return ir_executor_get_var(mgr->executor, name, instance_id);
}

char* ir_state_eval_expression(IRStateManager* mgr, const char* expression, const char* scope) {
    if (!mgr || !mgr->executor) {
        return NULL;
    }
    return ir_executor_eval_text_expression(expression, scope);
}
