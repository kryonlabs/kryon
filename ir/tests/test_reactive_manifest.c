/*
 * Test reactive manifest serialization and deserialization
 */

#include "../ir_core.h"
#include "../ir_builder.h"
#include "../ir_serialization.h"
#include "test_framework.h"
#include <stdio.h>
#include <string.h>

// Test reactive manifest creation and basic operations
TEST(test_manifest_creation) {
    IRReactiveManifest* manifest = ir_reactive_manifest_create();
    ASSERT_NONNULL(manifest);
    ASSERT_EQ(manifest->variable_count, 0);
    ASSERT_EQ(manifest->binding_count, 0);
    ir_reactive_manifest_destroy(manifest);
}

// Test adding variables
TEST(test_add_variables) {
    IRReactiveManifest* manifest = ir_reactive_manifest_create();

    // Add int variable
    IRReactiveValue int_val = { .as_int = 42 };
    uint32_t id1 = ir_reactive_manifest_add_var(manifest, "counter", IR_REACTIVE_TYPE_INT, int_val);
    ASSERT_GT(id1, 0);
    ASSERT_EQ(manifest->variable_count, 1);

    // Add float variable
    IRReactiveValue float_val = { .as_float = 3.14 };
    uint32_t id2 = ir_reactive_manifest_add_var(manifest, "pi", IR_REACTIVE_TYPE_FLOAT, float_val);
    ASSERT_GT(id2, id1);
    ASSERT_EQ(manifest->variable_count, 2);

    // Add string variable
    IRReactiveValue str_val = { .as_string = "Hello" };
    ir_reactive_manifest_add_var(manifest, "greeting", IR_REACTIVE_TYPE_STRING, str_val);
    ASSERT_EQ(manifest->variable_count, 3);

    // Add bool variable
    IRReactiveValue bool_val = { .as_bool = true };
    ir_reactive_manifest_add_var(manifest, "enabled", IR_REACTIVE_TYPE_BOOL, bool_val);
    ASSERT_EQ(manifest->variable_count, 4);

    ir_reactive_manifest_destroy(manifest);
}

// Test finding variables
TEST(test_find_variables) {
    IRReactiveManifest* manifest = ir_reactive_manifest_create();

    IRReactiveValue val = { .as_int = 100 };
    uint32_t id = ir_reactive_manifest_add_var(manifest, "test_var", IR_REACTIVE_TYPE_INT, val);

    // Find by name
    IRReactiveVarDescriptor* var = ir_reactive_manifest_find_var(manifest, "test_var");
    ASSERT_NONNULL(var);
    ASSERT_EQ(var->id, id);
    ASSERT_STR_EQ(var->name, "test_var");

    // Find by ID
    var = ir_reactive_manifest_get_var(manifest, id);
    ASSERT_NONNULL(var);
    ASSERT_EQ(var->value.as_int, 100);

    ir_reactive_manifest_destroy(manifest);
}

// Test serialization round-trip
TEST(test_serialization_roundtrip) {
    // Create a manifest with various data
    IRReactiveManifest* original = ir_reactive_manifest_create();

    // Add variables
    IRReactiveValue int_val = { .as_int = 42 };
    uint32_t var_id1 = ir_reactive_manifest_add_var(original, "counter", IR_REACTIVE_TYPE_INT, int_val);

    IRReactiveValue float_val = { .as_float = 3.14159 };
    uint32_t var_id2 = ir_reactive_manifest_add_var(original, "pi", IR_REACTIVE_TYPE_FLOAT, float_val);

    IRReactiveValue str_val = { .as_string = "Hello World" };
    ir_reactive_manifest_add_var(original, "message", IR_REACTIVE_TYPE_STRING, str_val);

    IRReactiveValue bool_val = { .as_bool = true };
    uint32_t var_id4 = ir_reactive_manifest_add_var(original, "enabled", IR_REACTIVE_TYPE_BOOL, bool_val);

    // Add bindings
    ir_reactive_manifest_add_binding(original, 100, var_id1, IR_BINDING_TEXT, "counter");
    ir_reactive_manifest_add_binding(original, 101, var_id2, IR_BINDING_ATTRIBUTE, "pi");

    // Add conditional
    uint32_t dep_vars[] = { var_id4 };
    ir_reactive_manifest_add_conditional(original, 200, "enabled", dep_vars, 1);

    // Add for-loop
    ir_reactive_manifest_add_for_loop(original, 300, "items", var_id1);

    // Create a simple component tree for testing
    IRComponent* root = ir_create_component(IR_COMPONENT_CONTAINER);
    root->id = 1;

    // Serialize with manifest
    IRBuffer* buffer = ir_serialize_binary_with_manifest(root, original);
    ASSERT_NONNULL(buffer);
    ASSERT_GT(buffer->size, 0);

    // Deserialize
    IRReactiveManifest* restored = NULL;
    ir_buffer_seek(buffer, 0);  // Reset buffer position
    buffer->data = buffer->base;  // Reset data pointer
    IRComponent* restored_root = ir_deserialize_binary_with_manifest(buffer, &restored);

    ASSERT_NONNULL(restored_root);
    ASSERT_NONNULL(restored);

    // Verify manifest contents
    ASSERT_EQ(restored->variable_count, 4);
    ASSERT_EQ(restored->binding_count, 2);
    ASSERT_EQ(restored->conditional_count, 1);
    ASSERT_EQ(restored->for_loop_count, 1);

    // Verify variable values
    IRReactiveVarDescriptor* var1 = ir_reactive_manifest_find_var(restored, "counter");
    ASSERT_NONNULL(var1);
    ASSERT_EQ(var1->value.as_int, 42);

    IRReactiveVarDescriptor* var2 = ir_reactive_manifest_find_var(restored, "pi");
    ASSERT_NONNULL(var2);
    ASSERT_FLOAT_EQ(var2->value.as_float, 3.14159, 0.00001);

    IRReactiveVarDescriptor* var3 = ir_reactive_manifest_find_var(restored, "message");
    ASSERT_NONNULL(var3);
    ASSERT_STR_EQ(var3->value.as_string, "Hello World");

    IRReactiveVarDescriptor* var4 = ir_reactive_manifest_find_var(restored, "enabled");
    ASSERT_NONNULL(var4);
    ASSERT_TRUE(var4->value.as_bool);

    // Cleanup
    ir_buffer_destroy(buffer);
    ir_destroy_component(root);
    ir_destroy_component(restored_root);
    ir_reactive_manifest_destroy(original);
    ir_reactive_manifest_destroy(restored);
}

// Test file write/read
TEST(test_file_roundtrip) {
    const char* filename = "/tmp/test_reactive_manifest.kir";

    // Create manifest
    IRReactiveManifest* original = ir_reactive_manifest_create();
    IRReactiveValue val = { .as_int = 123 };
    ir_reactive_manifest_add_var(original, "test", IR_REACTIVE_TYPE_INT, val);

    // Create component
    IRComponent* root = ir_create_component(IR_COMPONENT_CONTAINER);
    root->id = 1;

    // Write to file
    bool success = ir_write_binary_file_with_manifest(root, original, filename);
    ASSERT_TRUE(success);

    // Read from file
    IRReactiveManifest* restored = NULL;
    IRComponent* restored_root = ir_read_binary_file_with_manifest(filename, &restored);

    ASSERT_NONNULL(restored_root);
    ASSERT_NONNULL(restored);
    ASSERT_EQ(restored->variable_count, 1);

    IRReactiveVarDescriptor* var = ir_reactive_manifest_find_var(restored, "test");
    ASSERT_NONNULL(var);
    ASSERT_EQ(var->value.as_int, 123);

    // Cleanup
    ir_destroy_component(root);
    ir_destroy_component(restored_root);
    ir_reactive_manifest_destroy(original);
    ir_reactive_manifest_destroy(restored);
}

// Test empty manifest
TEST(test_empty_manifest) {
    IRReactiveManifest* empty = ir_reactive_manifest_create();
    IRComponent* root = ir_create_component(IR_COMPONENT_CONTAINER);
    root->id = 1;

    // Serialize empty manifest
    IRBuffer* buffer = ir_serialize_binary_with_manifest(root, empty);
    ASSERT_NONNULL(buffer);

    // Deserialize
    IRReactiveManifest* restored = NULL;
    ir_buffer_seek(buffer, 0);
    buffer->data = buffer->base;
    IRComponent* restored_root = ir_deserialize_binary_with_manifest(buffer, &restored);

    ASSERT_NONNULL(restored_root);
    ASSERT_NONNULL(restored);
    ASSERT_EQ(restored->variable_count, 0);
    ASSERT_EQ(restored->binding_count, 0);

    // Cleanup
    ir_buffer_destroy(buffer);
    ir_destroy_component(root);
    ir_destroy_component(restored_root);
    ir_reactive_manifest_destroy(empty);
    ir_reactive_manifest_destroy(restored);
}

// Main test suite
void run_reactive_manifest_tests(void) {
    BEGIN_SUITE("Reactive Manifest Serialization");

    RUN_TEST(test_manifest_creation);
    RUN_TEST(test_add_variables);
    RUN_TEST(test_find_variables);
    RUN_TEST(test_serialization_roundtrip);
    RUN_TEST(test_file_roundtrip);
    RUN_TEST(test_empty_manifest);

    END_SUITE();
}

RUN_TEST_SUITE(run_reactive_manifest_tests);
