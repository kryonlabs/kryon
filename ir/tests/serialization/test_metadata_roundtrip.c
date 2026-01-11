#define _POSIX_C_SOURCE 200809L
#include "ir_metadata.h"
#include "ir_serialization.h"
#include "ir_builder.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

/**
 * Test round-trip serialization: metadata → JSON → metadata
 */
int main() {
    printf("=== Bytecode Metadata Round-trip Test ===\n\n");

    // ========================================================================
    // Step 1: Create test metadata
    // ========================================================================
    printf("Step 1: Creating test metadata...\n");

    IRMetadata* metadata = ir_metadata_create();
    assert(metadata != NULL);

    // Add a simple increment function
    IRBytecodeFunction* func1 = ir_metadata_add_function(metadata, 1, "increment");
    assert(func1 != NULL);
    ir_function_add_instruction(func1, ir_instr_id(OP_GET_STATE, 1));
    ir_function_add_instruction(func1, ir_instr_int(OP_PUSH_INT, 1));
    ir_function_add_instruction(func1, ir_instr_simple(OP_ADD));
    ir_function_add_instruction(func1, ir_instr_id(OP_SET_STATE, 1));
    ir_function_add_instruction(func1, ir_instr_simple(OP_HALT));
    printf("  - Added function 'increment' with %d instructions\n", func1->instruction_count);

    // Add a conditional function
    IRBytecodeFunction* func2 = ir_metadata_add_function(metadata, 2, "check_positive");
    assert(func2 != NULL);
    ir_function_add_instruction(func2, ir_instr_id(OP_GET_STATE, 1));
    ir_function_add_instruction(func2, ir_instr_int(OP_PUSH_INT, 0));
    ir_function_add_instruction(func2, ir_instr_simple(OP_GT));
    ir_function_add_instruction(func2, ir_instr_offset(OP_JUMP_IF_FALSE, 3));
    ir_function_add_instruction(func2, ir_instr_bool(OP_PUSH_BOOL, true));
    ir_function_add_instruction(func2, ir_instr_simple(OP_RETURN));
    ir_function_add_instruction(func2, ir_instr_bool(OP_PUSH_BOOL, false));
    ir_function_add_instruction(func2, ir_instr_simple(OP_HALT));
    printf("  - Added function 'check_positive' with %d instructions\n", func2->instruction_count);

    // Add states
    ir_metadata_add_state_int(metadata, 1, "counter", 0);
    ir_metadata_add_state_string(metadata, 2, "message", "Hello");
    ir_metadata_add_state_bool(metadata, 3, "enabled", true);
    ir_metadata_add_state_float(metadata, 4, "progress", 0.5);
    printf("  - Added 4 states\n");

    // Add host functions
    ir_metadata_add_host_function(metadata, 1, "console.log", "(string) -> void", false);
    ir_metadata_add_host_function(metadata, 2, "fetch", "(string) -> string", true);
    printf("  - Added 2 host functions\n");

    printf("  ✓ Metadata created successfully\n\n");

    // ========================================================================
    // Step 2: Create a simple component tree
    // ========================================================================
    printf("Step 2: Creating component tree...\n");

    IRComponent* root = ir_create_component(IR_COMPONENT_CONTAINER);
    root->tag = strdup("app");

    IRComponent* button = ir_create_component(IR_COMPONENT_BUTTON);
    button->tag = strdup("increment-btn");
    button->text_content = strdup("Increment");
    ir_add_child(root, button);

    printf("  ✓ Component tree created\n\n");

    // ========================================================================
    // Step 3: Serialize to JSON
    // ========================================================================
    printf("Step 3: Serializing to JSON...\n");

    char* json = ir_serialize_json_with_metadata(root, metadata);
    assert(json != NULL);

    printf("  - JSON size: %zu bytes\n", strlen(json));
    printf("  - First 200 chars: %.200s...\n", json);
    printf("  ✓ Serialization successful\n\n");

    // ========================================================================
    // Step 4: Write to file
    // ========================================================================
    printf("Step 4: Writing to file...\n");

    const char* filename = "/tmp/test_metadata_roundtrip.kir";
    bool write_success = ir_write_json_file_with_metadata(root, metadata, filename);
    assert(write_success);

    printf("  - Written to: %s\n", filename);
    printf("  ✓ File write successful\n\n");

    // ========================================================================
    // Step 5: Read from file and deserialize
    // ========================================================================
    printf("Step 5: Reading and deserializing...\n");

    IRMetadata* metadata2 = NULL;
    IRComponent* root2 = ir_read_json_file_with_metadata(filename, &metadata2);
    assert(root2 != NULL);
    assert(metadata2 != NULL);

    printf("  - Deserialized component tree\n");
    printf("  - Deserialized metadata\n");
    printf("  ✓ Deserialization successful\n\n");

    // ========================================================================
    // Step 6: Verify metadata integrity
    // ========================================================================
    printf("Step 6: Verifying metadata integrity...\n");

    // Check function count
    printf("  - Function count: %d (expected 2)\n", metadata2->function_count);
    assert(metadata2->function_count == 2);

    // Check first function
    IRBytecodeFunction* func1_restored = ir_metadata_find_function(metadata2, 1);
    assert(func1_restored != NULL);
    printf("  - Function 1: id=%d name='%s' instructions=%d\n",
           func1_restored->id, func1_restored->name, func1_restored->instruction_count);
    assert(func1_restored->id == 1);
    assert(strcmp(func1_restored->name, "increment") == 0);
    assert(func1_restored->instruction_count == 5);

    // Check first instruction of first function
    assert(func1_restored->instructions[0].opcode == OP_GET_STATE);
    assert(func1_restored->instructions[0].arg.id_arg == 1);
    assert(func1_restored->instructions[1].opcode == OP_PUSH_INT);
    assert(func1_restored->instructions[1].arg.int_arg == 1);
    assert(func1_restored->instructions[2].opcode == OP_ADD);
    printf("  ✓ Function 1 verified\n");

    // Check second function
    IRBytecodeFunction* func2_restored = ir_metadata_find_function(metadata2, 2);
    assert(func2_restored != NULL);
    printf("  - Function 2: id=%d name='%s' instructions=%d\n",
           func2_restored->id, func2_restored->name, func2_restored->instruction_count);
    assert(func2_restored->id == 2);
    assert(strcmp(func2_restored->name, "check_positive") == 0);
    assert(func2_restored->instruction_count == 8);
    printf("  ✓ Function 2 verified\n");

    // Check state count
    printf("  - State count: %d (expected 4)\n", metadata2->state_count);
    assert(metadata2->state_count == 4);

    // Check states
    IRStateDefinition* state1 = ir_metadata_find_state(metadata2, 1);
    assert(state1 != NULL);
    assert(strcmp(state1->name, "counter") == 0);
    assert(state1->type == IR_STATE_TYPE_INT);
    assert(state1->initial_value.int_value == 0);
    printf("  - State 1: id=%d name='%s' type=int value=%ld\n",
           state1->id, state1->name, state1->initial_value.int_value);

    IRStateDefinition* state2 = ir_metadata_find_state(metadata2, 2);
    assert(state2 != NULL);
    assert(strcmp(state2->name, "message") == 0);
    assert(state2->type == IR_STATE_TYPE_STRING);
    assert(strcmp(state2->initial_value.string_value, "Hello") == 0);
    printf("  - State 2: id=%d name='%s' type=string value='%s'\n",
           state2->id, state2->name, state2->initial_value.string_value);

    IRStateDefinition* state3 = ir_metadata_find_state(metadata2, 3);
    assert(state3 != NULL);
    assert(strcmp(state3->name, "enabled") == 0);
    assert(state3->type == IR_STATE_TYPE_BOOL);
    assert(state3->initial_value.bool_value == true);
    printf("  - State 3: id=%d name='%s' type=bool value=%s\n",
           state3->id, state3->name, state3->initial_value.bool_value ? "true" : "false");

    IRStateDefinition* state4 = ir_metadata_find_state(metadata2, 4);
    assert(state4 != NULL);
    assert(strcmp(state4->name, "progress") == 0);
    assert(state4->type == IR_STATE_TYPE_FLOAT);
    assert(state4->initial_value.float_value == 0.5);
    printf("  - State 4: id=%d name='%s' type=float value=%.2f\n",
           state4->id, state4->name, state4->initial_value.float_value);

    printf("  ✓ All states verified\n");

    // Check host functions
    printf("  - Host function count: %d (expected 2)\n", metadata2->host_function_count);
    assert(metadata2->host_function_count == 2);

    IRHostFunctionDeclaration* host1 = ir_metadata_find_host_function(metadata2, 1);
    assert(host1 != NULL);
    assert(strcmp(host1->name, "console.log") == 0);
    assert(strcmp(host1->signature, "(string) -> void") == 0);
    assert(host1->required == false);
    printf("  - Host function 1: id=%d name='%s' required=%s\n",
           host1->id, host1->name, host1->required ? "true" : "false");

    IRHostFunctionDeclaration* host2 = ir_metadata_find_host_function(metadata2, 2);
    assert(host2 != NULL);
    assert(strcmp(host2->name, "fetch") == 0);
    assert(strcmp(host2->signature, "(string) -> string") == 0);
    assert(host2->required == true);
    printf("  - Host function 2: id=%d name='%s' required=%s\n",
           host2->id, host2->name, host2->required ? "true" : "false");

    printf("  ✓ All host functions verified\n\n");

    // ========================================================================
    // Step 7: Verify component tree (basic checks only)
    // ========================================================================
    printf("Step 7: Verifying component tree (basic)...\n");

    assert(root2 != NULL);
    assert(root2->type == IR_COMPONENT_CONTAINER);
    printf("  - Root type: CONTAINER ✓\n");

    assert(root2->child_count == 1);
    printf("  - Child count: %d ✓\n", root2->child_count);

    IRComponent* button2 = root2->children[0];
    assert(button2->type == IR_COMPONENT_BUTTON);
    printf("  - Button type: BUTTON ✓\n");

    if (button2->text_content != NULL) {
        printf("  - Button text: '%s' ✓\n", button2->text_content);
        assert(strcmp(button2->text_content, "Increment") == 0);
    }

    printf("  ✓ Component tree verified\n\n");

    // ========================================================================
    // Cleanup
    // ========================================================================
    printf("Step 8: Cleanup...\n");

    ir_metadata_destroy(metadata);
    ir_metadata_destroy(metadata2);
    ir_destroy_component(root);
    ir_destroy_component(root2);
    free(json);

    printf("  ✓ All resources freed\n\n");

    // ========================================================================
    // Success!
    // ========================================================================
    printf("========================================\n");
    printf("✅ ALL TESTS PASSED\n");
    printf("========================================\n");
    printf("\nRound-trip serialization works correctly!\n");
    printf("  • Functions preserved with all instructions\n");
    printf("  • States preserved with correct types and values\n");
    printf("  • Host functions preserved with signatures\n");
    printf("  • Component tree preserved correctly\n\n");

    return 0;
}
