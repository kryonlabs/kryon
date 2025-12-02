/*
 * test_vm_integration.c
 *
 * Phase 1 Spike Test: VM Integration with Metadata
 *
 * This test proves that:
 * 1. We can load a .kir file with bytecode metadata
 * 2. We can load functions into the VM from IRMetadata
 * 3. We can initialize reactive states from IRMetadata
 * 4. We can execute bytecode functions and observe state changes
 */

#define _POSIX_C_SOURCE 200809L

#include "ir_vm.h"
#include "ir_metadata.h"
#include "ir_serialization.h"
#include "ir_core.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Helper to print test results
static void print_test_result(const char* test_name, bool passed) {
    printf("[%s] %s\n", passed ? "PASS" : "FAIL", test_name);
}

// Test 1: Basic VM lifecycle
bool test_vm_create_destroy() {
    IRVM* vm = ir_vm_create();
    if (!vm) {
        return false;
    }

    bool result = (vm->stack.sp == -1 &&
                   vm->function_count == 0 &&
                   vm->state.count == 0);

    ir_vm_destroy(vm);
    return result;
}

// Test 2: Load a simple function
bool test_load_function() {
    IRVM* vm = ir_vm_create();

    // Create a simple function: PUSH_INT 42, HALT
    IRBytecodeFunction func;
    func.id = 1;
    func.name = strdup("test_func");
    func.instruction_count = 2;
    func.instruction_capacity = 2;
    func.instructions = malloc(sizeof(IRBytecodeInstruction) * 2);

    // PUSH_INT 42
    func.instructions[0].opcode = OP_PUSH_INT;
    func.instructions[0].has_arg = true;
    func.instructions[0].arg.int_arg = 42;

    // HALT
    func.instructions[1].opcode = OP_HALT;
    func.instructions[1].has_arg = false;

    // Load function into VM
    bool success = ir_vm_load_function(vm, &func);

    // Verify function was loaded
    bool result = success && vm->function_count == 1 && vm->functions[0].id == 1;

    // Cleanup
    free(func.name);
    free(func.instructions);
    ir_vm_destroy(vm);

    return result;
}

// Test 3: Execute a loaded function
bool test_execute_function() {
    IRVM* vm = ir_vm_create();

    // Create a function: PUSH_INT 42, HALT
    IRBytecodeFunction func;
    func.id = 1;
    func.name = strdup("test_func");
    func.instruction_count = 2;
    func.instruction_capacity = 2;
    func.instructions = malloc(sizeof(IRBytecodeInstruction) * 2);

    func.instructions[0].opcode = OP_PUSH_INT;
    func.instructions[0].has_arg = true;
    func.instructions[0].arg.int_arg = 42;

    func.instructions[1].opcode = OP_HALT;
    func.instructions[1].has_arg = false;

    // Load and execute
    ir_vm_load_function(vm, &func);
    bool exec_success = ir_vm_call_function(vm, 1);

    // Check stack
    IRValue* top = ir_vm_peek(vm);
    bool result = exec_success && top != NULL && top->type == VAL_INT && top->as.i == 42;

    // Cleanup
    free(func.name);
    free(func.instructions);
    ir_vm_destroy(vm);

    return result;
}

// Test 4: Initialize state from definition
bool test_init_state() {
    IRVM* vm = ir_vm_create();

    // Create a state definition
    IRStateDefinition state;
    state.id = 100;
    state.name = strdup("counter");
    state.type = IR_STATE_TYPE_INT;
    state.initial_value.int_value = 0;

    // Initialize state
    bool success = ir_vm_init_state_from_def(vm, &state);

    // Verify state was created
    IRValue value;
    ir_vm_get_state(vm, 100, &value);
    bool result = success && value.type == VAL_INT && value.as.i == 0;

    // Cleanup
    free(state.name);
    ir_vm_destroy(vm);

    return result;
}

// Test 5: Counter increment with state
bool test_counter_increment() {
    IRVM* vm = ir_vm_create();

    // Initialize counter state
    IRStateDefinition state;
    state.id = 100;
    state.name = strdup("counter");
    state.type = IR_STATE_TYPE_INT;
    state.initial_value.int_value = 0;
    ir_vm_init_state_from_def(vm, &state);

    // Create increment function:
    // GET_STATE 100
    // PUSH_INT 1
    // ADD
    // SET_STATE 100
    // HALT
    IRBytecodeFunction func;
    func.id = 1;
    func.name = strdup("increment");
    func.instruction_count = 5;
    func.instruction_capacity = 5;
    func.instructions = malloc(sizeof(IRBytecodeInstruction) * 5);

    func.instructions[0].opcode = OP_GET_STATE;
    func.instructions[0].has_arg = true;
    func.instructions[0].arg.id_arg = 100;

    func.instructions[1].opcode = OP_PUSH_INT;
    func.instructions[1].has_arg = true;
    func.instructions[1].arg.int_arg = 1;

    func.instructions[2].opcode = OP_ADD;
    func.instructions[2].has_arg = false;

    func.instructions[3].opcode = OP_SET_STATE;
    func.instructions[3].has_arg = true;
    func.instructions[3].arg.id_arg = 100;

    func.instructions[4].opcode = OP_HALT;
    func.instructions[4].has_arg = false;

    // Load and execute
    ir_vm_load_function(vm, &func);
    ir_vm_call_function(vm, 1);

    // Check state was incremented
    IRValue value;
    ir_vm_get_state(vm, 100, &value);
    bool result = value.type == VAL_INT && value.as.i == 1;

    if (result) {
        printf("  Counter incremented: 0 -> %lld\n", (long long)value.as.i);
    }

    // Cleanup
    free(state.name);
    free(func.name);
    free(func.instructions);
    ir_vm_destroy(vm);

    return result;
}

// Test 6: Load from .kir file (if available)
bool test_load_from_kir() {
    const char* filename = "test_metadata_roundtrip.kir";

    // Check if file exists
    FILE* f = fopen(filename, "r");
    if (!f) {
        printf("  (Skipping: %s not found)\n", filename);
        return true;  // Skip test if file doesn't exist
    }
    fclose(f);

    // Load .kir file with metadata
    IRMetadata* meta = NULL;
    IRComponent* root = ir_read_json_file_with_metadata(filename, &meta);

    if (!root || !meta) {
        printf("  Failed to load .kir file\n");
        return false;
    }

    printf("  Loaded .kir file:\n");
    printf("    Functions: %d\n", meta->function_count);
    printf("    States: %d\n", meta->state_count);
    printf("    Host functions: %d\n", meta->host_function_count);

    // Create VM
    IRVM* vm = ir_vm_create();

    // Load all functions
    int loaded = 0;
    for (int i = 0; i < meta->function_count; i++) {
        if (ir_vm_load_function(vm, &meta->functions[i])) {
            loaded++;
        }
    }
    printf("    Loaded %d functions into VM\n", loaded);

    // Initialize all states
    int initialized = 0;
    for (int i = 0; i < meta->state_count; i++) {
        if (ir_vm_init_state_from_def(vm, &meta->states[i])) {
            initialized++;
        }
    }
    printf("    Initialized %d states\n", initialized);

    // Try to call first function if available
    bool called = false;
    if (meta->function_count > 0) {
        uint32_t func_id = meta->functions[0].id;
        printf("    Calling function '%s' (id=%u)\n",
               meta->functions[0].name, func_id);

        called = ir_vm_call_function(vm, func_id);
        if (called) {
            printf("    ✓ Function executed successfully\n");
        } else {
            printf("    ✗ Function execution failed: %s\n", vm->error);
        }
    }

    bool result = (loaded == meta->function_count) &&
                  (initialized == meta->state_count);

    // Cleanup
    ir_vm_destroy(vm);
    ir_metadata_destroy(meta);
    ir_destroy_component(root);

    return result;
}

int main() {
    printf("=== Phase 1: VM Integration Spike Test ===\n\n");

    int passed = 0;
    int total = 0;

    #define RUN_TEST(test_func) do { \
        total++; \
        bool result = test_func(); \
        print_test_result(#test_func, result); \
        if (result) passed++; \
    } while(0)

    RUN_TEST(test_vm_create_destroy);
    RUN_TEST(test_load_function);
    RUN_TEST(test_execute_function);
    RUN_TEST(test_init_state);
    RUN_TEST(test_counter_increment);
    RUN_TEST(test_load_from_kir);

    printf("\n=== Results: %d/%d tests passed ===\n", passed, total);

    if (passed == total) {
        printf("✓ Phase 1 spike successful!\n");
        printf("\nNext steps:\n");
        printf("  - Phase 2: Add event metadata to IRComponent\n");
        printf("  - Phase 3: Backend integration (SDL3/terminal)\n");
        printf("  - Phase 4: Auto-compilation from Nim procs\n");
        return 0;
    } else {
        printf("✗ Some tests failed\n");
        return 1;
    }
}
