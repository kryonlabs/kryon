/**
 * Unit Tests for IR State Manager
 *
 * Tests:
 * 1. Queue management
 * 2. Variable updates
 * 3. Dirty flag management
 * 4. Handler execution
 * 5. Flush processing
 * 6. Callback registration
 */

#include "../ir_state_manager.h"
#include "../ir_executor.h"
#include "../ir_builder.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

// ============================================================================
// Test Helpers
// ============================================================================

static uint32_t callback_count = 0;
static uint32_t last_callback_component_id = 0;
static IRDirtyFlags last_callback_flags = 0;

static void test_callback(uint32_t component_id, IRDirtyFlags flags, void* user_data) {
    callback_count++;
    last_callback_component_id = component_id;
    last_callback_flags = flags;
}

// ============================================================================
// Test Cases
// ============================================================================

void test_state_manager_create_destroy(void) {
    printf("Test: create/destroy state manager... ");

    IRStateManager* mgr = ir_state_manager_create(NULL);
    assert(mgr != NULL);
    assert(ir_state_manager_get_queue_size(mgr) == 0);
    assert(ir_state_manager_get_generation(mgr) == 1);

    ir_state_manager_destroy(mgr);

    printf("PASS\n");
}

void test_state_manager_custom_config(void) {
    printf("Test: custom configuration... ");

    IRStateManagerConfig config = {
        .max_queued_updates = 500,
        .flush_timeout_ms = 32,
        .enable_profiling = true,
        .enable_validation = true
    };

    IRStateManager* mgr = ir_state_manager_create(&config);
    assert(mgr != NULL);

    ir_state_manager_destroy(mgr);

    printf("PASS\n");
}

void test_state_manager_queue_set_var(void) {
    printf("Test: queue set variable... ");

    IRStateManager* mgr = ir_state_manager_create(NULL);
    assert(mgr != NULL);

    // Create executor
    IRExecutorContext* executor = ir_executor_create();
    ir_state_manager_set_executor(mgr, executor);

    // Queue a variable update
    IRValue val = { .type = VAR_TYPE_INT, .int_val = 42 };
    bool queued = ir_state_queue_set_var(mgr, "test_var", val, NULL);
    assert(queued);
    assert(ir_state_manager_get_queue_size(mgr) == 1);

    ir_state_manager_destroy(mgr);

    printf("PASS\n");
}

void test_state_manager_flush_empty(void) {
    printf("Test: flush with empty queue... ");

    IRStateManager* mgr = ir_state_manager_create(NULL);
    assert(mgr != NULL);

    // Create executor
    IRExecutorContext* executor = ir_executor_create();
    ir_state_manager_set_executor(mgr, executor);

    // Flush empty queue
    IRStateFlushResult result = {0};
    bool flushed = ir_state_flush(mgr, &result);
    assert(flushed);
    assert(result.updates_processed == 0);

    ir_state_manager_destroy(mgr);

    printf("PASS\n");
}

void test_state_manager_flush_with_var_update(void) {
    printf("Test: flush with variable update... ");

    IRStateManager* mgr = ir_state_manager_create(NULL);
    IRExecutorContext* executor = ir_executor_create();
    ir_state_manager_set_executor(mgr, executor);

    // Queue and flush a variable update
    IRValue val = { .type = VAR_TYPE_INT, .int_val = 123 };
    ir_state_queue_set_var(mgr, "counter", val, NULL);

    IRStateFlushResult result = {0};
    bool flushed = ir_state_flush(mgr, &result);
    assert(flushed);
    assert(ir_state_manager_get_queue_size(mgr) == 0);

    // Verify variable was set
    IRValue retrieved = ir_state_get_var(mgr, "counter", 0);
    assert(retrieved.type == VAR_TYPE_INT);
    assert(retrieved.int_val == 123);

    ir_state_manager_destroy(mgr);

    printf("PASS\n");
}

void test_state_manager_queue_mark_dirty(void) {
    printf("Test: queue mark dirty... ");

    IRStateManager* mgr = ir_state_manager_create(NULL);
    IRExecutorContext* executor = ir_executor_create();
    ir_state_manager_set_executor(mgr, executor);

    // Create a simple component tree
    IRComponent* root = ir_create_component(IR_COMPONENT_CONTAINER);
    root->id = 1;
    ir_state_manager_set_root(mgr, root);

    // Queue dirty mark
    bool queued = ir_state_queue_mark_dirty(mgr, 1, IR_DIRTY_LAYOUT, true);
    assert(queued);
    assert(ir_state_manager_get_queue_size(mgr) == 1);

    // Flush
    ir_state_flush(mgr, NULL);
    assert(ir_state_manager_get_queue_size(mgr) == 0);

    ir_state_manager_destroy(mgr);
    // Component will be freed when state manager is destroyed (if pooled)

    printf("PASS\n");
}

void test_state_manager_set_var_convenience(void) {
    printf("Test: set_var convenience wrapper... ");

    IRStateManager* mgr = ir_state_manager_create(NULL);
    IRExecutorContext* executor = ir_executor_create();
    ir_state_manager_set_executor(mgr, executor);

    // Use convenience wrapper
    IRValue val = { .type = VAR_TYPE_INT, .int_val = 999 };
    bool success = ir_state_set_var(mgr, "magic", val, NULL);
    assert(success);

    // Verify variable was set
    IRValue retrieved = ir_state_get_var(mgr, "magic", 0);
    assert(retrieved.type == VAR_TYPE_INT);
    assert(retrieved.int_val == 999);

    ir_state_manager_destroy(mgr);

    printf("PASS\n");
}

void test_state_manager_callback_registration(void) {
    printf("Test: callback registration... ");

    IRStateManager* mgr = ir_state_manager_create(NULL);
    IRExecutorContext* executor = ir_executor_create();
    ir_state_manager_set_executor(mgr, executor);

    // Register callback
    uint32_t callback_id = ir_state_register_callback(mgr, test_callback, NULL);
    assert(callback_id != UINT32_MAX);

    // Unregister
    ir_state_unregister_callback(mgr, callback_id);

    ir_state_manager_destroy(mgr);

    printf("PASS\n");
}

void test_state_manager_multiple_updates(void) {
    printf("Test: multiple queued updates... ");

    IRStateManager* mgr = ir_state_manager_create(NULL);
    IRExecutorContext* executor = ir_executor_create();
    ir_state_manager_set_executor(mgr, executor);

    // Queue multiple updates
    for (int i = 0; i < 10; i++) {
        IRValue val = { .type = VAR_TYPE_INT, .int_val = i };
        ir_state_queue_set_var(mgr, "var", val, NULL);
    }

    assert(ir_state_manager_get_queue_size(mgr) == 10);

    // Flush all
    IRStateFlushResult result = {0};
    ir_state_flush(mgr, &result);

    assert(ir_state_manager_get_queue_size(mgr) == 0);

    ir_state_manager_destroy(mgr);

    printf("PASS\n");
}

void test_state_manager_stats(void) {
    printf("Test: statistics tracking... ");

    IRStateManager* mgr = ir_state_manager_create(NULL);
    IRExecutorContext* executor = ir_executor_create();
    ir_state_manager_set_executor(mgr, executor);

    // Reset stats
    ir_state_manager_reset_stats(mgr);

    // Perform some operations
    IRValue val = { .type = VAR_TYPE_INT, .int_val = 1 };
    ir_state_queue_set_var(mgr, "stat", val, NULL);
    ir_state_flush(mgr, NULL);

    // Check stats
    uint64_t flushes = 0, updates = 0;
    double time_ms = 0;
    ir_state_manager_get_stats(mgr, &flushes, &updates, &time_ms);

    assert(flushes == 1);

    ir_state_manager_destroy(mgr);

    printf("PASS\n");
}

void test_state_manager_generation_counter(void) {
    printf("Test: generation counter... ");

    IRStateManager* mgr = ir_state_manager_create(NULL);
    IRExecutorContext* executor = ir_executor_create();
    ir_state_manager_set_executor(mgr, executor);

    uint32_t gen1 = ir_state_manager_get_generation(mgr);
    assert(gen1 == 1);

    // Flush should increment generation
    IRValue val = { .type = VAR_TYPE_INT, .int_val = 1 };
    ir_state_queue_set_var(mgr, "g", val, NULL);
    ir_state_flush(mgr, NULL);

    uint32_t gen2 = ir_state_manager_get_generation(mgr);
    assert(gen2 == 2);

    ir_state_manager_destroy(mgr);

    printf("PASS\n");
}

// ============================================================================
// Test Runner
// ============================================================================

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    printf("=== IR State Manager Unit Tests ===\n\n");

    test_state_manager_create_destroy();
    test_state_manager_custom_config();
    test_state_manager_queue_set_var();
    test_state_manager_flush_empty();
    test_state_manager_flush_with_var_update();
    test_state_manager_queue_mark_dirty();
    test_state_manager_set_var_convenience();
    test_state_manager_callback_registration();
    test_state_manager_multiple_updates();
    test_state_manager_stats();
    test_state_manager_generation_counter();

    printf("\n=== All tests passed ===\n");
    return 0;
}
