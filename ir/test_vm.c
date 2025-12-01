#include "ir_vm.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

// Helper to write little-endian integers to bytecode
static void write_i64(uint8_t* buf, int64_t value) {
    memcpy(buf, &value, 8);
}

static void write_u32(uint8_t* buf, uint32_t value) {
    memcpy(buf, &value, 4);
}

// Test 1: Simple arithmetic (PUSH 5, PUSH 3, ADD)
void test_simple_add(void) {
    printf("Test 1: Simple addition (5 + 3)...\n");

    IRVM* vm = ir_vm_create();
    assert(vm != NULL);

    // Bytecode: PUSH_INT 5, PUSH_INT 3, ADD, HALT
    uint8_t code[256];
    int pos = 0;

    code[pos++] = OP_PUSH_INT;
    write_i64(&code[pos], 5);
    pos += 8;

    code[pos++] = OP_PUSH_INT;
    write_i64(&code[pos], 3);
    pos += 8;

    code[pos++] = OP_ADD;
    code[pos++] = OP_HALT;

    bool success = ir_vm_execute(vm, code, pos);
    assert(success);

    IRValue result;
    bool popped = ir_vm_pop(vm, &result);
    assert(popped);
    assert(result.type == VAL_INT);
    assert(result.as.i == 8);

    printf("  ✓ Result: %lld (expected 8)\n", (long long)result.as.i);

    ir_vm_destroy(vm);
}

// Test 2: State operations (GET_STATE, PUSH 1, ADD, SET_STATE)
// Simulates: counter.value += 1
void test_counter_increment(void) {
    printf("\nTest 2: Counter increment (state += 1)...\n");

    IRVM* vm = ir_vm_create();
    assert(vm != NULL);

    // Initialize counter state to 0
    IRValue initial_value;
    initial_value.type = VAL_INT;
    initial_value.as.i = 0;
    ir_vm_set_state(vm, 1, initial_value);

    // Bytecode: GET_STATE 1, PUSH_INT 1, ADD, SET_STATE 1, HALT
    uint8_t code[256];
    int pos = 0;

    code[pos++] = OP_GET_STATE;
    write_u32(&code[pos], 1);  // State ID = 1
    pos += 4;

    code[pos++] = OP_PUSH_INT;
    write_i64(&code[pos], 1);
    pos += 8;

    code[pos++] = OP_ADD;

    code[pos++] = OP_SET_STATE;
    write_u32(&code[pos], 1);  // State ID = 1
    pos += 4;

    code[pos++] = OP_HALT;

    bool success = ir_vm_execute(vm, code, pos);
    assert(success);

    // Verify state was updated
    IRValue final_value;
    ir_vm_get_state(vm, 1, &final_value);
    assert(final_value.type == VAL_INT);
    assert(final_value.as.i == 1);

    printf("  ✓ Counter incremented: 0 → %lld\n", (long long)final_value.as.i);

    ir_vm_destroy(vm);
}

// Test 3: Multiple increments (simulate 3 button clicks)
void test_multiple_increments(void) {
    printf("\nTest 3: Multiple increments (3 clicks)...\n");

    IRVM* vm = ir_vm_create();
    assert(vm != NULL);

    // Initialize counter state to 10
    IRValue initial_value;
    initial_value.type = VAL_INT;
    initial_value.as.i = 10;
    ir_vm_set_state(vm, 42, initial_value);

    // Bytecode for: counter += 5
    uint8_t code[256];
    int pos = 0;

    code[pos++] = OP_GET_STATE;
    write_u32(&code[pos], 42);
    pos += 4;

    code[pos++] = OP_PUSH_INT;
    write_i64(&code[pos], 5);
    pos += 8;

    code[pos++] = OP_ADD;

    code[pos++] = OP_SET_STATE;
    write_u32(&code[pos], 42);
    pos += 4;

    code[pos++] = OP_HALT;

    // Execute 3 times
    for (int i = 0; i < 3; i++) {
        vm->pc = 0;
        vm->halted = false;
        bool success = ir_vm_execute(vm, code, pos);
        assert(success);
    }

    // Verify final state: 10 + 5 + 5 + 5 = 25
    IRValue final_value;
    ir_vm_get_state(vm, 42, &final_value);
    assert(final_value.type == VAL_INT);
    assert(final_value.as.i == 25);

    printf("  ✓ Counter after 3 increments: 10 → %lld (expected 25)\n", (long long)final_value.as.i);

    ir_vm_destroy(vm);
}

// Test 4: Error handling (stack underflow)
void test_error_handling(void) {
    printf("\nTest 4: Error handling (stack underflow)...\n");

    IRVM* vm = ir_vm_create();
    assert(vm != NULL);

    // Bytecode: ADD (without pushing values first)
    uint8_t code[] = { OP_ADD, OP_HALT };

    bool success = ir_vm_execute(vm, code, sizeof(code));
    assert(!success);  // Should fail
    assert(vm->halted);
    assert(strlen(vm->error) > 0);

    printf("  ✓ Error caught: %s\n", vm->error);

    ir_vm_destroy(vm);
}

int main(void) {
    printf("========================================\n");
    printf("Kryon Bytecode VM - Prototype Tests\n");
    printf("========================================\n\n");

    test_simple_add();
    test_counter_increment();
    test_multiple_increments();
    test_error_handling();

    printf("\n========================================\n");
    printf("✓ All VM tests passed!\n");
    printf("========================================\n");

    return 0;
}
