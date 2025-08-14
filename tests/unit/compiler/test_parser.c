/**
 * @file test_parser.c
 * @brief Unit tests for Kryon parser
 */

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

// Test basic element parsing
TEST(basic_element_parsing) {
    const char *source = "Button { text: \"Hello\" }";
    
    KryonLexer *lexer = kryon_lexer_create(source, 0, "test.kry", NULL);
    ASSERT(lexer != NULL);
    
    ASSERT(kryon_lexer_tokenize(lexer));
    
    KryonParser *parser = kryon_parser_create(lexer, NULL);
    ASSERT(parser != NULL);
    
    ASSERT(kryon_parser_parse(parser));
    
    const KryonASTNode *root = kryon_parser_get_root(parser);
    ASSERT(root != NULL);
    ASSERT(root->type == KRYON_AST_ROOT);
    
    // Should have one child element
    ASSERT(root->data.element.child_count == 1);
    
    const KryonASTNode *button = root->data.element.children[0];
    ASSERT(button->type == KRYON_AST_ELEMENT);
    ASSERT_STR_EQ(button->data.element.element_type, "Button");
    
    // Button should have one property
    ASSERT(button->data.element.property_count == 1);
    
    const KryonASTNode *text_prop = button->data.element.properties[0];
    ASSERT(text_prop->type == KRYON_AST_PROPERTY);
    ASSERT_STR_EQ(text_prop->data.property.name, "text");
    
    // Property value should be a string literal
    const KryonASTNode *value = text_prop->data.property.value;
    ASSERT(value != NULL);
    ASSERT(value->type == KRYON_AST_LITERAL);
    ASSERT(value->data.literal.value.type == KRYON_VALUE_STRING);
    ASSERT_STR_EQ(value->data.literal.value.data.string_value, "Hello");
    
    kryon_parser_destroy(parser);
    kryon_lexer_destroy(lexer);
}

// Test nested elements
TEST(nested_elements) {
    const char *source = 
        "Container {\n"
        "    posX: 100\n"
        "    posY: 200\n"
        "    Button {\n"
        "        text: \"Click me\"\n"
        "    }\n"
        "}";
    
    KryonLexer *lexer = kryon_lexer_create(source, 0, "test.kry", NULL);
    ASSERT(lexer != NULL);
    
    ASSERT(kryon_lexer_tokenize(lexer));
    
    KryonParser *parser = kryon_parser_create(lexer, NULL);
    ASSERT(parser != NULL);
    
    ASSERT(kryon_parser_parse(parser));
    
    const KryonASTNode *root = kryon_parser_get_root(parser);
    ASSERT(root != NULL);
    
    // Should have one container
    ASSERT(root->data.element.child_count == 1);
    
    const KryonASTNode *container = root->data.element.children[0];
    ASSERT(container->type == KRYON_AST_ELEMENT);
    ASSERT_STR_EQ(container->data.element.element_type, "Container");
    
    // Container should have 2 properties and 1 child
    ASSERT(container->data.element.property_count == 2);
    ASSERT(container->data.element.child_count == 1);
    
    // Check properties
    const KryonASTNode *pos_x = container->data.element.properties[0];
    ASSERT_STR_EQ(pos_x->data.property.name, "posX");
    
    const KryonASTNode *pos_y = container->data.element.properties[1];
    ASSERT_STR_EQ(pos_y->data.property.name, "posY");
    
    // Check child button
    const KryonASTNode *button = container->data.element.children[0];
    ASSERT(button->type == KRYON_AST_ELEMENT);
    ASSERT_STR_EQ(button->data.element.element_type, "Button");
    
    kryon_parser_destroy(parser);
    kryon_lexer_destroy(lexer);
}

// Test style block parsing
TEST(style_block_parsing) {
    const char *source = 
        "@style \"button_primary\" {\n"
        "    backgroundColor: \"#0066cc\"\n"
        "    color: \"white\"\n"
        "}\n"
        "\n"
        "Button {\n"
        "    text: \"Styled button\"\n"
        "    class: \"button_primary\"\n"
        "}";
    
    KryonLexer *lexer = kryon_lexer_create(source, 0, "test.kry", NULL);
    ASSERT(lexer != NULL);
    
    ASSERT(kryon_lexer_tokenize(lexer));
    
    KryonParser *parser = kryon_parser_create(lexer, NULL);
    ASSERT(parser != NULL);
    
    ASSERT(kryon_parser_parse(parser));
    
    const KryonASTNode *root = kryon_parser_get_root(parser);
    ASSERT(root != NULL);
    
    // Should have style block and button element
    ASSERT(root->data.element.child_count == 2);
    
    // First child should be style block
    const KryonASTNode *style = root->data.element.children[0];
    ASSERT(style->type == KRYON_AST_STYLE_BLOCK);
    ASSERT_STR_EQ(style->data.style.name, "button_primary");
    
    // Style should have 2 properties
    ASSERT(style->data.style.property_count == 2);
    
    // Check style properties
    const KryonASTNode *bg_color = style->data.style.properties[0];
    ASSERT_STR_EQ(bg_color->data.property.name, "backgroundColor");
    
    const KryonASTNode *color = style->data.style.properties[1];
    ASSERT_STR_EQ(color->data.property.name, "color");
    
    // Second child should be button
    const KryonASTNode *button = root->data.element.children[1];
    ASSERT(button->type == KRYON_AST_ELEMENT);
    ASSERT_STR_EQ(button->data.element.element_type, "Button");
    
    kryon_parser_destroy(parser);
    kryon_lexer_destroy(lexer);
}

// Test variable parsing
TEST(variable_parsing) {
    const char *source = "Text { text: $userName }";
    
    KryonLexer *lexer = kryon_lexer_create(source, 0, "test.kry", NULL);
    ASSERT(lexer != NULL);
    
    ASSERT(kryon_lexer_tokenize(lexer));
    
    KryonParser *parser = kryon_parser_create(lexer, NULL);
    ASSERT(parser != NULL);
    
    ASSERT(kryon_parser_parse(parser));
    
    const KryonASTNode *root = kryon_parser_get_root(parser);
    ASSERT(root != NULL);
    
    const KryonASTNode *text = root->data.element.children[0];
    ASSERT(text->type == KRYON_AST_ELEMENT);
    
    const KryonASTNode *text_prop = text->data.element.properties[0];
    const KryonASTNode *value = text_prop->data.property.value;
    
    ASSERT(value->type == KRYON_AST_VARIABLE);
    ASSERT_STR_EQ(value->data.variable.name, "userName");
    
    kryon_parser_destroy(parser);
    kryon_lexer_destroy(lexer);
}

// Test error handling
TEST(error_handling) {
    const char *source = "Button { text: }"; // Missing value
    
    KryonLexer *lexer = kryon_lexer_create(source, 0, "test.kry", NULL);
    ASSERT(lexer != NULL);
    
    ASSERT(kryon_lexer_tokenize(lexer));
    
    KryonParser *parser = kryon_parser_create(lexer, NULL);
    ASSERT(parser != NULL);
    
    // Parsing should fail
    ASSERT(!kryon_parser_parse(parser));
    
    // Should have errors
    size_t error_count;
    const char **errors = kryon_parser_get_errors(parser, &error_count);
    ASSERT(error_count > 0);
    ASSERT(errors != NULL);
    
    kryon_parser_destroy(parser);
    kryon_lexer_destroy(lexer);
}

// Test different literal types
TEST(literal_types) {
    const char *source = 
        "Element {\n"
        "    stringProp: \"hello\"\n"
        "    intProp: 42\n"
        "    floatProp: 3.14\n"
        "    boolProp: true\n"
        "    nullProp: null\n"
        "}";
    
    KryonLexer *lexer = kryon_lexer_create(source, 0, "test.kry", NULL);
    ASSERT(lexer != NULL);
    
    ASSERT(kryon_lexer_tokenize(lexer));
    
    KryonParser *parser = kryon_parser_create(lexer, NULL);
    ASSERT(parser != NULL);
    
    ASSERT(kryon_parser_parse(parser));
    
    const KryonASTNode *root = kryon_parser_get_root(parser);
    ASSERT(root != NULL);
    
    const KryonASTNode *element = root->data.element.children[0];
    ASSERT(element->data.element.property_count == 5);
    
    // Check each property type
    const KryonASTNode *string_prop = element->data.element.properties[0];
    ASSERT(string_prop->data.property.value->data.literal.value.type == KRYON_VALUE_STRING);
    
    const KryonASTNode *int_prop = element->data.element.properties[1];
    ASSERT(int_prop->data.property.value->data.literal.value.type == KRYON_VALUE_INTEGER);
    ASSERT(int_prop->data.property.value->data.literal.value.data.int_value == 42);
    
    const KryonASTNode *float_prop = element->data.element.properties[2];
    ASSERT(float_prop->data.property.value->data.literal.value.type == KRYON_VALUE_FLOAT);
    
    const KryonASTNode *bool_prop = element->data.element.properties[3];
    ASSERT(bool_prop->data.property.value->data.literal.value.type == KRYON_VALUE_BOOLEAN);
    ASSERT(bool_prop->data.property.value->data.literal.value.data.bool_value == true);
    
    const KryonASTNode *null_prop = element->data.element.properties[4];
    ASSERT(null_prop->data.property.value->data.literal.value.type == KRYON_VALUE_NULL);
    
    kryon_parser_destroy(parser);
    kryon_lexer_destroy(lexer);
}

// Main test runner
int main(void) {
    printf("=== Kryon Parser Unit Tests ===\n\n");
    
    run_test_basic_element_parsing();
    run_test_nested_elements();
    run_test_style_block_parsing();
    run_test_variable_parsing();
    run_test_error_handling();
    run_test_literal_types();
    
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