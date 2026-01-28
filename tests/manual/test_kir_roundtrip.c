/**
 * @file test_kir_roundtrip.c
 * @brief Manual test for KIR write → read round-trip
 *
 * Usage: ./test_kir_roundtrip
 */

#define _POSIX_C_SOURCE 200809L
#include "kir_format.h"
#include "parser.h"
#include "memory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * Create a simple test AST
 */
static KryonASTNode *create_test_ast(void) {
    // Create root node
    KryonASTNode *root = calloc(1, sizeof(KryonASTNode));
    root->type = KRYON_AST_ROOT;
    root->node_id = 1;
    root->location.line = 1;
    root->location.column = 1;
    root->location.filename = strdup("test.kry");

    // Create an element (Button)
    KryonASTNode *button = calloc(1, sizeof(KryonASTNode));
    button->type = KRYON_AST_ELEMENT;
    button->node_id = 2;
    button->data.element.element_type = strdup("Button");
    button->location.line = 2;
    button->location.column = 1;

    // Create a property (text: "Click Me")
    KryonASTNode *text_prop = calloc(1, sizeof(KryonASTNode));
    text_prop->type = KRYON_AST_PROPERTY;
    text_prop->node_id = 3;
    text_prop->data.property.name = strdup("text");

    // Create literal value
    KryonASTNode *text_value = calloc(1, sizeof(KryonASTNode));
    text_value->type = KRYON_AST_LITERAL;
    text_value->node_id = 4;
    text_value->data.literal.value.type = KRYON_VALUE_STRING;
    text_value->data.literal.value.data.string_value = strdup("Click Me");

    text_prop->data.property.value = text_value;

    // Add property to button
    button->data.element.properties = calloc(1, sizeof(KryonASTNode*));
    button->data.element.properties[0] = text_prop;
    button->data.element.property_count = 1;
    button->data.element.property_capacity = 1;

    // Add button to root
    root->data.element.children = calloc(1, sizeof(KryonASTNode*));
    root->data.element.children[0] = button;
    root->data.element.child_count = 1;
    root->data.element.child_capacity = 1;

    return root;
}

/**
 * Verify AST structure
 */
static bool verify_ast(const KryonASTNode *ast) {
    if (!ast) {
        printf("❌ AST is NULL\n");
        return false;
    }

    if (ast->type != KRYON_AST_ROOT) {
        printf("❌ Root node type mismatch\n");
        return false;
    }

    if (ast->data.element.child_count != 1) {
        printf("❌ Root should have 1 child, got %zu\n", ast->data.element.child_count);
        return false;
    }

    KryonASTNode *button = ast->data.element.children[0];
    if (button->type != KRYON_AST_ELEMENT) {
        printf("❌ Child is not an ELEMENT\n");
        return false;
    }

    if (!button->data.element.element_type || strcmp(button->data.element.element_type, "Button") != 0) {
        printf("❌ Element type should be 'Button', got '%s'\n",
               button->data.element.element_type ? button->data.element.element_type : "NULL");
        return false;
    }

    if (button->data.element.property_count != 1) {
        printf("❌ Button should have 1 property, got %zu\n", button->data.element.property_count);
        return false;
    }

    KryonASTNode *prop = button->data.element.properties[0];
    if (prop->type != KRYON_AST_PROPERTY) {
        printf("❌ Property node type mismatch\n");
        return false;
    }

    if (!prop->data.property.name || strcmp(prop->data.property.name, "text") != 0) {
        printf("❌ Property name should be 'text', got '%s'\n",
               prop->data.property.name ? prop->data.property.name : "NULL");
        return false;
    }

    if (!prop->data.property.value || prop->data.property.value->type != KRYON_AST_LITERAL) {
        printf("❌ Property value should be a LITERAL\n");
        return false;
    }

    if (prop->data.property.value->data.literal.value.type != KRYON_VALUE_STRING) {
        printf("❌ Literal value should be STRING\n");
        return false;
    }

    const char *str_value = prop->data.property.value->data.literal.value.data.string_value;
    if (!str_value || strcmp(str_value, "Click Me") != 0) {
        printf("❌ String value should be 'Click Me', got '%s'\n",
               str_value ? str_value : "NULL");
        return false;
    }

    printf("✅ AST structure verified successfully\n");
    return true;
}

int main(void) {
    printf("=== KIR Round-Trip Test ===\n\n");

    // Step 1: Create test AST
    printf("Step 1: Creating test AST...\n");
    KryonASTNode *original_ast = create_test_ast();
    if (!verify_ast(original_ast)) {
        printf("❌ Original AST verification failed\n");
        return 1;
    }

    // Step 2: Write AST to KIR
    printf("\nStep 2: Writing AST to KIR JSON...\n");
    KryonKIRWriter *writer = kryon_kir_writer_create(NULL);
    if (!writer) {
        printf("❌ Failed to create KIR writer\n");
        return 1;
    }

    char *json_string = NULL;
    if (!kryon_kir_write_string(writer, original_ast, &json_string)) {
        size_t error_count;
        const char **errors = kryon_kir_writer_get_errors(writer, &error_count);
        printf("❌ Failed to write KIR:\n");
        for (size_t i = 0; i < error_count; i++) {
            printf("  - %s\n", errors[i]);
        }
        kryon_kir_writer_destroy(writer);
        return 1;
    }

    printf("✅ KIR written successfully (%zu bytes)\n", strlen(json_string));
    printf("\nGenerated KIR:\n%s\n", json_string);

    // Also write to file for inspection
    if (kryon_kir_write_file(writer, original_ast, "/tmp/test.kir")) {
        printf("✅ KIR also written to /tmp/test.kir\n");
    }

    kryon_kir_writer_destroy(writer);

    // Step 3: Read KIR back to AST
    printf("\nStep 3: Reading KIR JSON back to AST...\n");
    KryonKIRReader *reader = kryon_kir_reader_create(NULL);
    if (!reader) {
        printf("❌ Failed to create KIR reader\n");
        free(json_string);
        return 1;
    }

    KryonASTNode *reconstructed_ast = NULL;
    if (!kryon_kir_read_string(reader, json_string, &reconstructed_ast)) {
        size_t error_count;
        const char **errors = kryon_kir_reader_get_errors(reader, &error_count);
        printf("❌ Failed to read KIR:\n");
        for (size_t i = 0; i < error_count; i++) {
            printf("  - %s\n", errors[i]);
        }
        kryon_kir_reader_destroy(reader);
        free(json_string);
        return 1;
    }

    printf("✅ KIR read successfully\n");
    kryon_kir_reader_destroy(reader);
    free(json_string);

    // Step 4: Verify reconstructed AST
    printf("\nStep 4: Verifying reconstructed AST...\n");
    if (!verify_ast(reconstructed_ast)) {
        printf("❌ Reconstructed AST verification failed\n");
        return 1;
    }

    printf("\n=== ✅ Round-Trip Test PASSED ===\n");
    printf("Summary:\n");
    printf("  - Original AST created successfully\n");
    printf("  - AST serialized to KIR JSON successfully\n");
    printf("  - KIR JSON parsed back to AST successfully\n");
    printf("  - Reconstructed AST matches original structure\n");
    printf("\n✅ KIR write → read round-trip works correctly!\n");

    return 0;
}
