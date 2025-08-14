/**
 * @file test_codegen.c
 * @brief Unit tests for Kryon code generator
 */

#include "codegen.h"
#include "parser.h"
#include "lexer.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

// Simple test framework
static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) \
    static void test_##name(void); \
    static void run_test_##name(void) { \
        printf("Running test: " #name "... "); \
        tests_run++; \
        test_##name(); \
        tests_passed++; \
        printf("PASSED\n"); \
    } \
    static void test_##name(void)

#define ASSERT(condition) \
    do { \
        if (!(condition)) { \
            printf("FAILED\n  Assertion failed: " #condition "\n"); \
            printf("  File: %s, Line: %d\n", __FILE__, __LINE__); \
            exit(1); \
        } \
    } while(0)

#define ASSERT_STR_EQ(actual, expected) \
    do { \
        if (strcmp((actual), (expected)) != 0) { \
            printf("FAILED\n  String mismatch:\n"); \
            printf("    Expected: \"%s\"\n", (expected)); \
            printf("    Actual:   \"%s\"\n", (actual)); \
            printf("  File: %s, Line: %d\n", __FILE__, __LINE__); \
            exit(1); \
        } \
    } while(0)

// Helper function to create a simple AST for testing
static KryonASTNode *create_test_ast(void) {
    // This is a simplified version - in practice we'd use the parser
    // For now, just return NULL to test error handling
    return NULL;
}

// Test code generator creation and destruction
TEST(codegen_lifecycle) {
    KryonCodeGenerator *codegen = kryon_codegen_create(NULL);
    ASSERT(codegen != NULL);
    
    kryon_codegen_destroy(codegen);
    
    // Test with custom config
    KryonCodeGenConfig config = kryon_codegen_default_config();
    config.optimization = KRYON_OPTIMIZE_SIZE;
    
    codegen = kryon_codegen_create(&config);
    ASSERT(codegen != NULL);
    
    kryon_codegen_destroy(codegen);
}

// Test configuration functions
TEST(codegen_configurations) {
    KryonCodeGenConfig default_config = kryon_codegen_default_config();
    ASSERT(default_config.optimization == KRYON_OPTIMIZE_BALANCED);
    ASSERT(default_config.enable_compression == true);
    ASSERT(default_config.validate_output == true);
    
    KryonCodeGenConfig size_config = kryon_codegen_size_config();
    ASSERT(size_config.optimization == KRYON_OPTIMIZE_SIZE);
    ASSERT(size_config.enable_compression == true);
    
    KryonCodeGenConfig speed_config = kryon_codegen_speed_config();
    ASSERT(speed_config.optimization == KRYON_OPTIMIZE_SPEED);
    ASSERT(speed_config.enable_compression == false);
    
    KryonCodeGenConfig debug_config = kryon_codegen_debug_config();
    ASSERT(debug_config.optimization == KRYON_OPTIMIZE_NONE);
    ASSERT(debug_config.preserve_debug_info == true);
}

// Test element and property mappings
TEST(element_property_mappings) {
    // Test known element types
    ASSERT(kryon_codegen_get_element_hex("Button") != 0);
    ASSERT(kryon_codegen_get_element_hex("Text") != 0);
    ASSERT(kryon_codegen_get_element_hex("Container") != 0);
    
    // Test unknown element type
    ASSERT(kryon_codegen_get_element_hex("UnknownElement") == 0);
    ASSERT(kryon_codegen_get_element_hex(NULL) == 0);
    
    // Test known property types
    ASSERT(kryon_codegen_get_property_hex("text") != 0);
    ASSERT(kryon_codegen_get_property_hex("backgroundColor") != 0);
    ASSERT(kryon_codegen_get_property_hex("posX") != 0);
    
    // Test unknown property type
    ASSERT(kryon_codegen_get_property_hex("unknownProperty") == 0);
    ASSERT(kryon_codegen_get_property_hex(NULL) == 0);
}

// Test basic header generation
TEST(header_generation) {
    KryonCodeGenerator *codegen = kryon_codegen_create(NULL);
    ASSERT(codegen != NULL);
    
    // Test header writing
    ASSERT(kryon_write_header(codegen));
    
    size_t binary_size;
    const uint8_t *binary = kryon_codegen_get_binary(codegen, &binary_size);
    ASSERT(binary != NULL);
    ASSERT(binary_size > 0);
    
    // Check magic number
    uint32_t magic = *(uint32_t*)binary;
    ASSERT(magic == 0x4B524200); // "KRB\0"
    
    kryon_codegen_destroy(codegen);
}

// Test generation with NULL AST (should fail gracefully)
TEST(generation_with_null_ast) {
    KryonCodeGenerator *codegen = kryon_codegen_create(NULL);
    ASSERT(codegen != NULL);
    
    // Should fail with NULL AST
    ASSERT(!kryon_codegen_generate(codegen, NULL));
    
    kryon_codegen_destroy(codegen);
}

// Test end-to-end compilation from source to binary
TEST(end_to_end_compilation) {
    const char *source = "Button { text: \"Hello World\" }";
    
    // Tokenize
    KryonLexer *lexer = kryon_lexer_create(source, 0, "test.kry", NULL);
    ASSERT(lexer != NULL);
    ASSERT(kryon_lexer_tokenize(lexer));
    
    // Parse
    KryonParser *parser = kryon_parser_create(lexer, NULL);
    ASSERT(parser != NULL);
    ASSERT(kryon_parser_parse(parser));
    
    const KryonASTNode *ast = kryon_parser_get_root(parser);
    ASSERT(ast != NULL);
    
    // Generate
    KryonCodeGenerator *codegen = kryon_codegen_create(NULL);
    ASSERT(codegen != NULL);
    
    bool success = kryon_codegen_generate(codegen, ast);
    if (!success) {
        size_t error_count;
        const char **errors = kryon_codegen_get_errors(codegen, &error_count);
        printf("\nCodegen errors (%zu):\n", error_count);
        for (size_t i = 0; i < error_count; i++) {
            printf("  %s\n", errors[i]);
        }
    }
    ASSERT(success);
    
    // Check binary output
    size_t binary_size;
    const uint8_t *binary = kryon_codegen_get_binary(codegen, &binary_size);
    ASSERT(binary != NULL);
    ASSERT(binary_size > 0);
    
    // Validate binary
    ASSERT(kryon_codegen_validate_binary(codegen));
    
    // Cleanup
    kryon_codegen_destroy(codegen);
    kryon_parser_destroy(parser);
    kryon_lexer_destroy(lexer);
}

// Test binary file writing
TEST(binary_file_writing) {
    KryonCodeGenerator *codegen = kryon_codegen_create(NULL);
    ASSERT(codegen != NULL);
    
    // Generate a simple header
    ASSERT(kryon_write_header(codegen));
    
    // Write to file
    const char *test_file = "/tmp/test_output.krb";
    ASSERT(kryon_write_file(codegen, test_file));
    
    // Verify file exists and has content
    FILE *file = fopen(test_file, "rb");
    ASSERT(file != NULL);
    
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    ASSERT(file_size > 0);
    
    fclose(file);
    
    // Clean up
    remove(test_file);
    kryon_codegen_destroy(codegen);
}

// Test error handling
TEST(error_handling) {
    KryonCodeGenerator *codegen = kryon_codegen_create(NULL);
    ASSERT(codegen != NULL);
    
    // Initially should have no errors
    size_t error_count;
    const char **errors = kryon_codegen_get_errors(codegen, &error_count);
    ASSERT(error_count == 0);
    
    kryon_codegen_destroy(codegen);
}

// Test statistics
TEST(statistics_tracking) {
    KryonCodeGenerator *codegen = kryon_codegen_create(NULL);
    ASSERT(codegen != NULL);
    
    const KryonCodeGenStats *stats = kryon_codegen_get_stats(codegen);
    ASSERT(stats != NULL);
    
    // Initially should be zero
    ASSERT(stats->input_ast_nodes == 0);
    ASSERT(stats->output_binary_size == 0);
    ASSERT(stats->generation_time == 0.0);
    
    kryon_codegen_destroy(codegen);
}

// Main test runner
int main(void) {
    printf("=== Kryon Code Generator Unit Tests ===\n\n");
    
    run_test_codegen_lifecycle();
    run_test_codegen_configurations();
    run_test_element_property_mappings();
    run_test_header_generation();
    run_test_generation_with_null_ast();
    run_test_end_to_end_compilation();
    run_test_binary_file_writing();
    run_test_error_handling();
    run_test_statistics_tracking();
    
    printf("\n=== Test Results ===\n");
    printf("Tests run: %d\n", tests_run);
    printf("Tests passed: %d\n", tests_passed);
    
    if (tests_passed == tests_run) {
        printf("All tests PASSED! ✅\n");
        return 0;
    } else {
        printf("Some tests FAILED! ❌\n");
        return 1;
    }
}