/**
 * Kryon Reactive Runtime Implementation
 * Provides runtime support for reactive state, computed properties, actions, and watchers
 */

#define _POSIX_C_SOURCE 200809L

#include "../include/ir_core.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// ============================================================================
// Reactive Runtime Context
// ============================================================================

typedef struct {
    IRReactiveManifest* manifest;
    bool is_batching;           // True if currently batching updates
    uint32_t batch_depth;       // Depth of nested batch operations
    uint32_t* dirty_vars;       // Variables modified during batch
    uint32_t dirty_count;       // Number of dirty variables
    uint32_t dirty_capacity;    // Capacity of dirty array
} ReactiveRuntimeContext;

// Global runtime context (one per application)
static ReactiveRuntimeContext* g_runtime_ctx = NULL;

// ============================================================================
// Runtime Context Management
// ============================================================================

ReactiveRuntimeContext* reactive_runtime_create(IRReactiveManifest* manifest) {
    if (!manifest) return NULL;

    ReactiveRuntimeContext* ctx = calloc(1, sizeof(ReactiveRuntimeContext));
    if (!ctx) {
        fprintf(stderr, "[ReactiveRuntime] Failed to allocate runtime context\n");
        return NULL;
    }

    ctx->manifest = manifest;
    ctx->is_batching = false;
    ctx->batch_depth = 0;
    ctx->dirty_capacity = 16;
    ctx->dirty_vars = calloc(ctx->dirty_capacity, sizeof(uint32_t));

    if (!ctx->dirty_vars) {
        fprintf(stderr, "[ReactiveRuntime] Failed to allocate dirty tracking array\n");
        free(ctx);
        return NULL;
    }

    return ctx;
}

void reactive_runtime_destroy(ReactiveRuntimeContext* ctx) {
    if (!ctx) return;
    free(ctx->dirty_vars);
    free(ctx);
}

// ============================================================================
// Reactive Variable Updates
// ============================================================================

void reactive_runtime_mark_dirty(ReactiveRuntimeContext* ctx, uint32_t var_id) {
    if (!ctx) return;

    // Check if already marked dirty
    for (uint32_t i = 0; i < ctx->dirty_count; i++) {
        if (ctx->dirty_vars[i] == var_id) {
            return;  // Already marked
        }
    }

    // Grow array if needed
    if (ctx->dirty_count >= ctx->dirty_capacity) {
        if (ctx->dirty_capacity >= 0x80000000) {
            fprintf(stderr, "[ReactiveRuntime] Cannot grow dirty array - would overflow\n");
            return;
        }
        ctx->dirty_capacity *= 2;
        uint32_t* new_dirty = realloc(ctx->dirty_vars, ctx->dirty_capacity * sizeof(uint32_t));
        if (!new_dirty) {
            fprintf(stderr, "[ReactiveRuntime] Failed to resize dirty array\n");
            return;
        }
        ctx->dirty_vars = new_dirty;
    }

    // Mark as dirty
    ctx->dirty_vars[ctx->dirty_count++] = var_id;

    // Invalidate computed properties that depend on this variable
    ir_reactive_manifest_invalidate_computed(ctx->manifest, var_id);
}

void reactive_runtime_flush_updates(ReactiveRuntimeContext* ctx) {
    if (!ctx || ctx->dirty_count == 0) return;

    printf("[ReactiveRuntime] Flushing %u dirty variable(s)\n", ctx->dirty_count);

    // Trigger watchers for dirty variables
    for (uint32_t i = 0; i < ctx->dirty_count; i++) {
        uint32_t var_id = ctx->dirty_vars[i];

        // Find watchers for this variable
        for (uint32_t w = 0; w < ctx->manifest->watcher_count; w++) {
            IRWatcher* watcher = &ctx->manifest->watchers[w];

            // Check if watcher depends on this variable
            for (uint32_t d = 0; d < watcher->watched_var_count; d++) {
                if (watcher->watched_var_ids[d] == var_id) {
                    printf("[ReactiveRuntime] Triggering watcher '%s' for var %u\n",
                           watcher->handler_function, var_id);
                    // TODO: Call the handler function
                    break;
                }
            }
        }
    }

    // Clear dirty array
    ctx->dirty_count = 0;
}

// ============================================================================
// Computed Property Evaluation
// ============================================================================

IRReactiveValue* reactive_runtime_evaluate_computed(ReactiveRuntimeContext* ctx,
                                                    uint32_t computed_id) {
    if (!ctx || !ctx->manifest) return NULL;

    IRComputedProperty* computed = ir_reactive_manifest_get_computed(ctx->manifest, computed_id);
    if (!computed) {
        fprintf(stderr, "[ReactiveRuntime] Computed property %u not found\n", computed_id);
        return NULL;
    }

    // Check if cached value is valid
    if (computed->state == IR_COMPUTED_STATE_VALID) {
        printf("[ReactiveRuntime] Returning cached value for computed '%s'\n", computed->name);
        return &computed->cached_value;
    }

    // Check for circular dependencies
    if (computed->state == IR_COMPUTED_STATE_COMPUTING) {
        fprintf(stderr, "[ReactiveRuntime] Circular dependency detected in computed '%s'\n", computed->name);
        return NULL;
    }

    printf("[ReactiveRuntime] Evaluating computed property '%s'\n", computed->name);

    // Mark as computing
    computed->state = IR_COMPUTED_STATE_COMPUTING;

    // TODO: Call the function to compute the value
    // For now, just return a dummy value
    computed->cached_value.as_int = 0;
    computed->state = IR_COMPUTED_STATE_VALID;

    return &computed->cached_value;
}

// ============================================================================
// Action Execution
// ============================================================================

void reactive_runtime_begin_batch(ReactiveRuntimeContext* ctx) {
    if (!ctx) return;

    ctx->batch_depth++;
    if (!ctx->is_batching) {
        ctx->is_batching = true;
        printf("[ReactiveRuntime] Beginning batch (depth=%u)\n", ctx->batch_depth);
    }
}

void reactive_runtime_end_batch(ReactiveRuntimeContext* ctx) {
    if (!ctx || ctx->batch_depth == 0) return;

    ctx->batch_depth--;
    if (ctx->batch_depth == 0) {
        printf("[ReactiveRuntime] Ending batch, flushing updates\n");
        ctx->is_batching = false;
        reactive_runtime_flush_updates(ctx);
    } else {
        printf("[ReactiveRuntime] Decremented batch depth to %u\n", ctx->batch_depth);
    }
}

void reactive_runtime_execute_action(ReactiveRuntimeContext* ctx,
                                     uint32_t action_id,
                                     void (*action_fn)(void)) {
    if (!ctx || !ctx->manifest) return;

    IRAction* action = ir_reactive_manifest_get_action(ctx->manifest, action_id);
    if (!action) {
        fprintf(stderr, "[ReactiveRuntime] Action %u not found\n", action_id);
        return;
    }

    printf("[ReactiveRuntime] Executing action '%s' (batched=%d, auto_save=%d)\n",
           action->name, action->is_batched, action->auto_save);

    // Begin batch if action is batched
    if (action->is_batched) {
        reactive_runtime_begin_batch(ctx);
    }

    // Execute the action function
    if (action_fn) {
        action_fn();
    } else {
        // TODO: Look up and call the function by name
        fprintf(stderr, "[ReactiveRuntime] Action function not implemented: %s\n",
                action->function_name);
    }

    // End batch if action is batched
    if (action->is_batched) {
        reactive_runtime_end_batch(ctx);
    }

    // Trigger auto-save watchers if configured
    if (action->auto_save && action->watch_path_count > 0) {
        printf("[ReactiveRuntime] Triggering auto-save for action '%s'\n", action->name);
        for (uint32_t i = 0; i < action->watch_path_count; i++) {
            // TODO: Trigger save for each watch path
            printf("[ReactiveRuntime]   Auto-saving: %s\n", action->watch_paths[i]);
        }
    }
}

// ============================================================================
// Watcher Registration
// ============================================================================

void reactive_runtime_register_watchers(ReactiveRuntimeContext* ctx) {
    if (!ctx || !ctx->manifest) return;

    printf("[ReactiveRuntime] Registering %u watcher(s)\n", ctx->manifest->watcher_count);

    // For each watcher, resolve the watched variables
    for (uint32_t i = 0; i < ctx->manifest->watcher_count; i++) {
        IRWatcher* watcher = &ctx->manifest->watchers[i];

        printf("[ReactiveRuntime] Registering watcher for '%s' -> %s\n",
               watcher->watched_path, watcher->handler_function);

        // Parse the watched path to extract variable IDs
        // For example: "state.habits" -> find var_id for "habits"
        // TODO: Implement path parsing and variable resolution

        // For now, just mark the watcher as registered
        if (watcher->immediate) {
            printf("[ReactiveRuntime] Executing immediate watcher '%s'\n",
                   watcher->handler_function);
            // TODO: Call the handler immediately
        }
    }
}

// ============================================================================
// Public API
// ============================================================================

void ir_reactive_runtime_init(IRReactiveManifest* manifest) {
    if (g_runtime_ctx) {
        fprintf(stderr, "[ReactiveRuntime] Runtime already initialized\n");
        return;
    }

    g_runtime_ctx = reactive_runtime_create(manifest);
    if (!g_runtime_ctx) {
        fprintf(stderr, "[ReactiveRuntime] Failed to initialize runtime\n");
        return;
    }

    printf("[ReactiveRuntime] Initialized reactive runtime\n");
    reactive_runtime_register_watchers(g_runtime_ctx);
}

void ir_reactive_runtime_shutdown(void) {
    if (!g_runtime_ctx) return;

    reactive_runtime_destroy(g_runtime_ctx);
    g_runtime_ctx = NULL;

    printf("[ReactiveRuntime] Shutdown reactive runtime\n");
}

void ir_reactive_runtime_notify_update(uint32_t var_id) {
    if (!g_runtime_ctx) {
        fprintf(stderr, "[ReactiveRuntime] Runtime not initialized\n");
        return;
    }

    reactive_runtime_mark_dirty(g_runtime_ctx, var_id);

    // If not batching, flush updates immediately
    if (!g_runtime_ctx->is_batching) {
        reactive_runtime_flush_updates(g_runtime_ctx);
    }
}

void ir_reactive_runtime_execute_action(uint32_t action_id, void (*action_fn)(void)) {
    if (!g_runtime_ctx) {
        fprintf(stderr, "[ReactiveRuntime] Runtime not initialized\n");
        return;
    }

    reactive_runtime_execute_action(g_runtime_ctx, action_id, action_fn);
}

IRReactiveValue* ir_reactive_runtime_get_computed(uint32_t computed_id) {
    if (!g_runtime_ctx) {
        fprintf(stderr, "[ReactiveRuntime] Runtime not initialized\n");
        return NULL;
    }

    return reactive_runtime_evaluate_computed(g_runtime_ctx, computed_id);
}

void ir_reactive_runtime_begin_batch(void) {
    if (!g_runtime_ctx) {
        fprintf(stderr, "[ReactiveRuntime] Runtime not initialized\n");
        return;
    }

    reactive_runtime_begin_batch(g_runtime_ctx);
}

void ir_reactive_runtime_end_batch(void) {
    if (!g_runtime_ctx) {
        fprintf(stderr, "[ReactiveRuntime] Runtime not initialized\n");
        return;
    }

    reactive_runtime_end_batch(g_runtime_ctx);
}
