/**
 * Phase 3: Backend Integration Test
 *
 * Tests the integration of bytecode VM with the desktop backend:
 * 1. VM lifecycle in desktop renderer
 * 2. Bytecode event dispatch
 * 3. State change callbacks
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "ir_vm.h"
#include "ir_metadata.h"
#include "ir_builder.h"
#include "ir_core.h"

// Test state
static bool state_changed = false;
static uint32_t changed_state_id = 0;
static int64_t changed_value = 0;

// State change callback for testing
void test_state_callback(IRVM* vm, uint32_t state_id, IRValue new_value, void* user_data) {
    (void)vm;
    (void)user_data;

    printf("[Callback] State %u changed to: ", state_id);
    switch (new_value.type) {
        case VAL_INT:
            printf("%lld (int)\n", (long long)new_value.as.i);
            changed_value = new_value.as.i;
            break;
        case VAL_FLOAT:
            printf("%f (float)\n", new_value.as.f);
            break;
        case VAL_STRING:
            printf("\"%s\" (string)\n", new_value.as.s ? new_value.as.s : "(null)");
            break;
        case VAL_BOOL:
            printf("%s (bool)\n", new_value.as.b ? "true" : "false");
            break;
    }

    state_changed = true;
    changed_state_id = state_id;
}

// Test 1: VM lifecycle in isolation
bool test_vm_lifecycle() {
    printf("\n=== Test 1: VM Lifecycle ===\n");

    IRVM* vm = ir_vm_create();
    if (!vm) {
        printf("FAIL: VM creation failed\n");
        return false;
    }

    // Verify initial state
    if (vm->state_change_callback != NULL) {
        printf("FAIL: Callback should be NULL initially\n");
        ir_vm_destroy(vm);
        return false;
    }

    // Register callback
    ir_vm_register_state_callback(vm, test_state_callback, NULL);

    if (vm->state_change_callback != test_state_callback) {
        printf("FAIL: Callback not registered\n");
        ir_vm_destroy(vm);
        return false;
    }

    ir_vm_destroy(vm);
    printf("PASS: VM lifecycle works correctly\n");
    return true;
}

// Test 2: Event with bytecode function
bool test_event_bytecode() {
    printf("\n=== Test 2: Event with Bytecode Function ===\n");

    // Create a button component
    IRComponent* button = ir_create_component(IR_COMPONENT_BUTTON);
    button->id = 100;

    // Create a click event with bytecode function
    IREvent* click_event = (IREvent*)calloc(1, sizeof(IREvent));
    click_event->type = IR_EVENT_CLICK;
    click_event->bytecode_function_id = 42;  // Function ID 42
    click_event->logic_id = NULL;  // No legacy handler

    // Attach event to button
    button->events = click_event;

    // Verify event setup
    IREvent* found = ir_find_event(button, IR_EVENT_CLICK);
    if (!found) {
        printf("FAIL: Event not found\n");
        ir_destroy_component(button);
        return false;
    }

    if (found->bytecode_function_id != 42) {
        printf("FAIL: Bytecode function ID mismatch (expected 42, got %u)\n",
               found->bytecode_function_id);
        ir_destroy_component(button);
        return false;
    }

    ir_destroy_component(button);
    printf("PASS: Event bytecode function works\n");
    return true;
}

// Test 3: VM execution with state change callback
bool test_vm_state_callback() {
    printf("\n=== Test 3: VM State Change Callback ===\n");

    // Reset test state
    state_changed = false;
    changed_state_id = 0;
    changed_value = 0;

    // Create VM and register callback
    IRVM* vm = ir_vm_create();
    ir_vm_register_state_callback(vm, test_state_callback, NULL);

    // Create a simple bytecode function that increments a counter
    // counter = 0 (initial state)
    // counter = counter + 1
    IRBytecodeFunction func = {
        .id = 1,
        .name = "increment_counter",
        .instructions = (IRBytecodeInstruction[]){
            {.opcode = OP_GET_STATE, .has_arg = true, .arg = {.id_arg = 100}},  // Get state 100
            {.opcode = OP_PUSH_INT, .has_arg = true, .arg = {.int_arg = 1}},    // Push 1
            {.opcode = OP_ADD, .has_arg = false},                                // Add
            {.opcode = OP_SET_STATE, .has_arg = true, .arg = {.id_arg = 100}},  // Set state 100
            {.opcode = OP_HALT, .has_arg = false}
        },
        .instruction_count = 5
    };

    // Initialize state
    IRStateDefinition state_def = {
        .id = 100,
        .name = "counter",
        .type = IR_STATE_TYPE_INT,
        .initial_value = {.int_value = 0}
    };

    // Load state and function
    if (!ir_vm_init_state_from_def(vm, &state_def)) {
        printf("FAIL: State initialization failed\n");
        ir_vm_destroy(vm);
        return false;
    }

    if (!ir_vm_load_function(vm, &func)) {
        printf("FAIL: Function loading failed\n");
        ir_vm_destroy(vm);
        return false;
    }

    // Execute function
    if (!ir_vm_call_function(vm, 1)) {
        printf("FAIL: Function execution failed: %s\n", vm->error);
        ir_vm_destroy(vm);
        return false;
    }

    // Verify callback was called
    if (!state_changed) {
        printf("FAIL: State change callback was not called\n");
        ir_vm_destroy(vm);
        return false;
    }

    if (changed_state_id != 100) {
        printf("FAIL: Wrong state ID (expected 100, got %u)\n", changed_state_id);
        ir_vm_destroy(vm);
        return false;
    }

    if (changed_value != 1) {
        printf("FAIL: Wrong value (expected 1, got %lld)\n", (long long)changed_value);
        ir_vm_destroy(vm);
        return false;
    }

    ir_vm_destroy(vm);
    printf("PASS: State change callback works correctly\n");
    return true;
}

// Test 4: Full integration (component + event + VM)
bool test_full_integration() {
    printf("\n=== Test 4: Full Integration ===\n");

    // Reset test state
    state_changed = false;
    changed_state_id = 0;
    changed_value = 0;

    // Create VM
    IRVM* vm = ir_vm_create();
    ir_vm_register_state_callback(vm, test_state_callback, NULL);

    // Create metadata container
    IRMetadata* metadata = ir_metadata_create();

    // Add state definition
    IRStateDefinition* state = ir_metadata_add_state_int(metadata, 200, "click_count", 0);

    // Add bytecode function
    IRBytecodeFunction* func = ir_metadata_add_function(metadata, 10, "handle_click");

    // Function: click_count = click_count + 1
    ir_function_add_instruction(func, (IRBytecodeInstruction){
        .opcode = OP_GET_STATE, .has_arg = true, .arg = {.id_arg = 200}
    });
    ir_function_add_instruction(func, (IRBytecodeInstruction){
        .opcode = OP_PUSH_INT, .has_arg = true, .arg = {.int_arg = 1}
    });
    ir_function_add_instruction(func, (IRBytecodeInstruction){
        .opcode = OP_ADD, .has_arg = false
    });
    ir_function_add_instruction(func, (IRBytecodeInstruction){
        .opcode = OP_SET_STATE, .has_arg = true, .arg = {.id_arg = 200}
    });
    ir_function_add_instruction(func, (IRBytecodeInstruction){
        .opcode = OP_HALT, .has_arg = false
    });

    // Load metadata into VM
    if (!ir_vm_init_state_from_def(vm, state)) {
        printf("FAIL: State initialization failed\n");
        ir_metadata_destroy(metadata);
        ir_vm_destroy(vm);
        return false;
    }

    if (!ir_vm_load_function(vm, func)) {
        printf("FAIL: Function loading failed\n");
        ir_metadata_destroy(metadata);
        ir_vm_destroy(vm);
        return false;
    }

    // Create button component with event
    IRComponent* button = ir_create_component(IR_COMPONENT_BUTTON);
    button->id = 300;

    IREvent* click = (IREvent*)calloc(1, sizeof(IREvent));
    click->type = IR_EVENT_CLICK;
    click->bytecode_function_id = 10;  // References function 10
    button->events = click;

    // Simulate click event handling (like the backend would do)
    IREvent* event = ir_find_event(button, IR_EVENT_CLICK);
    if (event && event->bytecode_function_id != 0) {
        printf("[Test] Executing bytecode function %u for click on component %u\n",
               event->bytecode_function_id, button->id);

        if (!ir_vm_call_function(vm, event->bytecode_function_id)) {
            printf("FAIL: Function execution failed: %s\n", vm->error);
            ir_destroy_component(button);
            ir_metadata_destroy(metadata);
            ir_vm_destroy(vm);
            return false;
        }
    }

    // Verify state changed
    if (!state_changed || changed_state_id != 200 || changed_value != 1) {
        printf("FAIL: State not updated correctly\n");
        printf("  state_changed=%d, changed_state_id=%u, changed_value=%lld\n",
               state_changed, changed_state_id, (long long)changed_value);
        ir_destroy_component(button);
        ir_metadata_destroy(metadata);
        ir_vm_destroy(vm);
        return false;
    }

    // Cleanup
    ir_destroy_component(button);
    ir_metadata_destroy(metadata);
    ir_vm_destroy(vm);

    printf("PASS: Full integration works correctly\n");
    return true;
}

int main() {
    printf("===========================================\n");
    printf("Phase 3: Backend Integration Test Suite\n");
    printf("===========================================\n");

    int passed = 0;
    int total = 4;

    if (test_vm_lifecycle()) passed++;
    if (test_event_bytecode()) passed++;
    if (test_vm_state_callback()) passed++;
    if (test_full_integration()) passed++;

    printf("\n===========================================\n");
    printf("Results: %d/%d tests passed\n", passed, total);
    printf("===========================================\n");

    return (passed == total) ? 0 : 1;
}
