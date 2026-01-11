/*
 * test_event_metadata.c
 *
 * Phase 2 Test: Event Metadata Serialization
 *
 * This test verifies that:
 * 1. Event metadata (bytecode_function_id) can be added to components
 * 2. Events with bytecode function IDs serialize correctly to binary format
 * 3. Events with bytecode function IDs serialize correctly to JSON
 * 4. Round-trip serialization preserves bytecode_function_id
 */

#define _POSIX_C_SOURCE 200809L

#include "ir_builder.h"
#include "ir_serialization.h"
#include "ir_metadata.h"
#include "ir_vm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

// Helper to print test results
static void print_test_result(const char* test_name, bool passed) {
    printf("[%s] %s\n", passed ? "PASS" : "FAIL", test_name);
}

// Test 1: Create component with bytecode event handler
bool test_create_event_with_bytecode() {
    // Create a button with a click handler
    IRComponent* button = ir_create_component(IR_COMPONENT_BUTTON);
    button->text_content = strdup("Click Me");

    // Create an event with bytecode function ID
    IREvent* clickEvent = (IREvent*)calloc(1, sizeof(IREvent));
    clickEvent->type = IR_EVENT_CLICK;
    clickEvent->bytecode_function_id = 123;  // Function ID from metadata
    clickEvent->logic_id = NULL;  // No legacy logic ID

    // Attach event to component
    button->events = clickEvent;

    // Verify
    bool result = (button->events != NULL &&
                   button->events->type == IR_EVENT_CLICK &&
                   button->events->bytecode_function_id == 123);

    ir_destroy_component(button);
    return result;
}

// Test 2: Binary serialization round-trip
bool test_binary_roundtrip() {
    // Create component with bytecode event
    IRComponent* original = ir_create_component(IR_COMPONENT_BUTTON);
    original->id = 100;
    original->text_content = strdup("Submit");

    IREvent* event = (IREvent*)calloc(1, sizeof(IREvent));
    event->type = IR_EVENT_CLICK;
    event->bytecode_function_id = 456;
    event->logic_id = strdup("legacy_handler");
    event->handler_data = strdup("param1");
    original->events = event;

    // Serialize to binary
    IRBuffer* buffer = ir_serialize_binary(original);
    if (!buffer) {
        printf("  Failed to serialize to binary\n");
        ir_destroy_component(original);
        return false;
    }

    // Deserialize from binary
    ir_buffer_seek(buffer, 0);  // Reset to start
    IRComponent* deserialized = ir_deserialize_binary(buffer);

    if (!deserialized) {
        printf("  Failed to deserialize from binary\n");
        ir_buffer_destroy(buffer);
        ir_destroy_component(original);
        return false;
    }

    // Verify event metadata is preserved
    bool result = (deserialized->events != NULL &&
                   deserialized->events->type == IR_EVENT_CLICK &&
                   deserialized->events->bytecode_function_id == 456 &&
                   strcmp(deserialized->events->logic_id, "legacy_handler") == 0 &&
                   strcmp(deserialized->events->handler_data, "param1") == 0);

    if (result) {
        printf("  ✓ Binary round-trip preserved bytecode_function_id: %u\n",
               deserialized->events->bytecode_function_id);
    }

    ir_buffer_destroy(buffer);
    ir_destroy_component(original);
    ir_destroy_component(deserialized);

    return result;
}

// Test 3: JSON v2 serialization round-trip
bool test_json_roundtrip() {
    // Create component with multiple events
    IRComponent* original = ir_create_component(IR_COMPONENT_BUTTON);
    original->id = 200;
    original->text_content = strdup("Multi Event Button");

    // Click event with bytecode
    IREvent* clickEvent = (IREvent*)calloc(1, sizeof(IREvent));
    clickEvent->type = IR_EVENT_CLICK;
    clickEvent->bytecode_function_id = 789;
    original->events = clickEvent;

    // Hover event with bytecode
    IREvent* hoverEvent = (IREvent*)calloc(1, sizeof(IREvent));
    hoverEvent->type = IR_EVENT_HOVER;
    hoverEvent->bytecode_function_id = 790;
    clickEvent->next = hoverEvent;

    // Serialize to JSON
    char* json = ir_serialize_json(original);
    if (!json) {
        printf("  Failed to serialize to JSON\n");
        ir_destroy_component(original);
        return false;
    }

    printf("  JSON output:\n%s\n", json);

    // Deserialize from JSON
    IRComponent* deserialized = ir_deserialize_json_v2(json);
    if (!deserialized) {
        printf("  Failed to deserialize from JSON\n");
        free(json);
        ir_destroy_component(original);
        return false;
    }

    // Verify first event
    bool result = (deserialized->events != NULL &&
                   deserialized->events->type == IR_EVENT_CLICK &&
                   deserialized->events->bytecode_function_id == 789);

    // Verify second event
    if (result && deserialized->events->next != NULL) {
        result = (deserialized->events->next->type == IR_EVENT_HOVER &&
                  deserialized->events->next->bytecode_function_id == 790);
    } else {
        result = false;
    }

    if (result) {
        printf("  ✓ JSON round-trip preserved both event function IDs: %u, %u\n",
               deserialized->events->bytecode_function_id,
               deserialized->events->next->bytecode_function_id);
    }

    free(json);
    ir_destroy_component(original);
    ir_destroy_component(deserialized);

    return result;
}

// Test 4: Full integration with metadata
bool test_full_integration() {
    // Create metadata with bytecode function
    IRMetadata* metadata = ir_metadata_create();

    // Add a simple function to metadata
    IRBytecodeFunction* func = ir_metadata_add_function(metadata, 999, "handle_click");
    ir_function_add_instruction(func, ir_instr_int(OP_PUSH_INT, 42));
    ir_function_add_instruction(func, ir_instr_simple(OP_HALT));

    // Add a state
    ir_metadata_add_state_int(metadata, 100, "counter", 0);

    // Create component that references the function
    IRComponent* root = ir_create_component(IR_COMPONENT_CONTAINER);
    root->id = 1;

    IRComponent* button = ir_create_component(IR_COMPONENT_BUTTON);
    button->id = 2;
    button->text_content = strdup("Increment");

    // Event that references function 999
    IREvent* event = (IREvent*)calloc(1, sizeof(IREvent));
    event->type = IR_EVENT_CLICK;
    event->bytecode_function_id = 999;  // References function in metadata
    button->events = event;

    ir_add_child(root, button);

    // Serialize to JSON with metadata
    char* json = ir_serialize_json_with_metadata(root, metadata);
    if (!json) {
        printf("  Failed to serialize with metadata\n");
        ir_metadata_destroy(metadata);
        ir_destroy_component(root);
        return false;
    }

    printf("  Full JSON with metadata:\n%s\n", json);

    // Deserialize with metadata
    IRMetadata* loaded_metadata = NULL;
    IRComponent* loaded_root = ir_deserialize_json_with_metadata(json, &loaded_metadata);

    if (!loaded_root || !loaded_metadata) {
        printf("  Failed to deserialize with metadata\n");
        free(json);
        ir_metadata_destroy(metadata);
        ir_destroy_component(root);
        return false;
    }

    // Verify button event
    IRComponent* loaded_button = loaded_root->children[0];
    bool result = (loaded_button->events != NULL &&
                   loaded_button->events->bytecode_function_id == 999);

    // Verify metadata function exists
    if (result) {
        IRBytecodeFunction* loaded_func = ir_metadata_find_function(loaded_metadata, 999);
        result = (loaded_func != NULL &&
                  strcmp(loaded_func->name, "handle_click") == 0);
    }

    if (result) {
        printf("  ✓ Full integration: Event function ID %u matches metadata function '%s'\n",
               loaded_button->events->bytecode_function_id,
               ir_metadata_find_function(loaded_metadata, 999)->name);
    }

    free(json);
    ir_metadata_destroy(metadata);
    ir_metadata_destroy(loaded_metadata);
    ir_destroy_component(root);
    ir_destroy_component(loaded_root);

    return result;
}

int main() {
    printf("=== Phase 2: Event Metadata Serialization Test ===\n\n");

    int passed = 0;
    int total = 0;

    #define RUN_TEST(test_func) do { \
        total++; \
        bool result = test_func(); \
        print_test_result(#test_func, result); \
        if (result) passed++; \
    } while(0)

    RUN_TEST(test_create_event_with_bytecode);
    RUN_TEST(test_binary_roundtrip);
    RUN_TEST(test_json_roundtrip);
    RUN_TEST(test_full_integration);

    printf("\n=== Results: %d/%d tests passed ===\n", passed, total);

    if (passed == total) {
        printf("✓ Phase 2 event metadata serialization working!\n");
        printf("\nNext steps:\n");
        printf("  - Phase 3: Backend integration (SDL3/terminal event dispatch)\n");
        printf("  - Phase 4: Auto-compilation from Nim procs\n");
        return 0;
    } else {
        printf("✗ Some tests failed\n");
        return 1;
    }
}
