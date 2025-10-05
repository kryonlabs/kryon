/**
 * @file test_lexer.c
 * @brief Unit tests for Kryon lexer
 */

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

// Test basic tokenization
TEST(basic_tokenization) {
    const char *source = "Button { text = \"Hello\" }";
    
    KryonLexer *lexer = kryon_lexer_create(source, 0, "test.kry", NULL);
    ASSERT(lexer != NULL);
    
    ASSERT(kryon_lexer_tokenize(lexer));
    
    size_t token_count;
    const KryonToken *tokens = kryon_lexer_get_tokens(lexer, &token_count);
    
    ASSERT(token_count == 6); // Button, {, text, :, "Hello", }
    
    ASSERT(tokens[0].type == KRYON_TOKEN_ELEMENT_TYPE);
    ASSERT(kryon_token_lexeme_equals(&tokens[0], "Button"));
    
    ASSERT(tokens[1].type == KRYON_TOKEN_LEFT_BRACE);
    ASSERT(tokens[2].type == KRYON_TOKEN_IDENTIFIER);
    ASSERT(kryon_token_lexeme_equals(&tokens[2], "text"));
    
    ASSERT(tokens[3].type == KRYON_TOKEN_ASSIGN);
    
    ASSERT(tokens[4].type == KRYON_TOKEN_STRING);
    ASSERT(tokens[4].has_value);
    ASSERT_STR_EQ(tokens[4].value.string_value, "Hello");
    
    ASSERT(tokens[5].type == KRYON_TOKEN_RIGHT_BRACE);
    
    kryon_lexer_destroy(lexer);
}

// Test variable tokenization
TEST(variable_tokenization) {
    const char *source = "$user_name $count ${template}";
    
    KryonLexer *lexer = kryon_lexer_create(source, 0, "test.kry", NULL);
    ASSERT(lexer != NULL);
    
    ASSERT(kryon_lexer_tokenize(lexer));
    
    size_t token_count;
    const KryonToken *tokens = kryon_lexer_get_tokens(lexer, &token_count);
    
    ASSERT(token_count == 4); // $user_name, $count, ${, template, }
    
    ASSERT(tokens[0].type == KRYON_TOKEN_VARIABLE);
    ASSERT(kryon_token_lexeme_equals(&tokens[0], "$user_name"));
    
    ASSERT(tokens[1].type == KRYON_TOKEN_VARIABLE);
    ASSERT(kryon_token_lexeme_equals(&tokens[1], "$count"));
    
    ASSERT(tokens[2].type == KRYON_TOKEN_TEMPLATE_START);
    ASSERT(kryon_token_lexeme_equals(&tokens[2], "${"));
    
    ASSERT(tokens[3].type == KRYON_TOKEN_IDENTIFIER);
    ASSERT(kryon_token_lexeme_equals(&tokens[3], "template"));
    
    kryon_lexer_destroy(lexer);
}

// Test directive tokenization
TEST(directive_tokenization) {
    const char *source = "@style @store @watch @on_mount";
    
    KryonLexer *lexer = kryon_lexer_create(source, 0, "test.kry", NULL);
    ASSERT(lexer != NULL);
    
    ASSERT(kryon_lexer_tokenize(lexer));
    
    size_t token_count;
    const KryonToken *tokens = kryon_lexer_get_tokens(lexer, &token_count);
    
    ASSERT(token_count == 5); // 4 directives + EOF
    
    ASSERT(tokens[0].type == KRYON_TOKEN_STYLE_DIRECTIVE);
    ASSERT(tokens[1].type == KRYON_TOKEN_STORE_DIRECTIVE);
    ASSERT(tokens[2].type == KRYON_TOKEN_WATCH_DIRECTIVE);
    ASSERT(tokens[3].type == KRYON_TOKEN_ON_MOUNT_DIRECTIVE);
    
    kryon_lexer_destroy(lexer);
}

// Test number tokenization
TEST(number_tokenization) {
    const char *source = "42 3.14 -10 1.5e-3";
    
    KryonLexer *lexer = kryon_lexer_create(source, 0, "test.kry", NULL);
    ASSERT(lexer != NULL);
    
    ASSERT(kryon_lexer_tokenize(lexer));
    
    size_t token_count;
    const KryonToken *tokens = kryon_lexer_get_tokens(lexer, &token_count);
    
    ASSERT(token_count == 5); // 42, 3.14, -, 10, 1.5e-3 + EOF
    
    ASSERT(tokens[0].type == KRYON_TOKEN_INTEGER);
    ASSERT(tokens[0].has_value);
    ASSERT(tokens[0].value.int_value == 42);
    
    ASSERT(tokens[1].type == KRYON_TOKEN_FLOAT);
    ASSERT(tokens[1].has_value);
    ASSERT(tokens[1].value.float_value == 3.14);
    
    ASSERT(tokens[2].type == KRYON_TOKEN_MINUS);
    
    ASSERT(tokens[3].type == KRYON_TOKEN_INTEGER);
    ASSERT(tokens[3].has_value);
    ASSERT(tokens[3].value.int_value == 10);
    
    ASSERT(tokens[4].type == KRYON_TOKEN_FLOAT);
    ASSERT(tokens[4].has_value);
    
    kryon_lexer_destroy(lexer);
}

// Test string with escapes
TEST(string_escapes) {
    const char *source = "\"Hello\\nWorld\\t\\\"Test\\\"\"";
    
    KryonLexer *lexer = kryon_lexer_create(source, 0, "test.kry", NULL);
    ASSERT(lexer != NULL);
    
    ASSERT(kryon_lexer_tokenize(lexer));
    
    size_t token_count;
    const KryonToken *tokens = kryon_lexer_get_tokens(lexer, &token_count);
    
    ASSERT(token_count == 2); // string + EOF
    
    ASSERT(tokens[0].type == KRYON_TOKEN_STRING);
    ASSERT(tokens[0].has_value);
    ASSERT_STR_EQ(tokens[0].value.string_value, "Hello\nWorld\t\"Test\"");
    
    kryon_lexer_destroy(lexer);
}

// Test complete KRY example
TEST(complete_kry_example) {
    const char *source = 
        "@style \"button_primary\" {\n"
        "    backgroundColor = \"#0066cc\"\n"
        "    color = \"white\"\n"
        "}\n"
        "\n"
        "Button {\n"
        "    text = \"Click me\"\n"
        "    class = \"button_primary\"\n"
        "    onClick = \"handle_click\"\n"
        "    enabled = true\n"
        "    count = $counter\n"
        "}\n";
    
    KryonLexer *lexer = kryon_lexer_create(source, 0, "test.kry", NULL);
    ASSERT(lexer != NULL);
    
    ASSERT(kryon_lexer_tokenize(lexer));
    
    size_t token_count;
    const KryonToken *tokens = kryon_lexer_get_tokens(lexer, &token_count);
    
    // Should have many tokens - just verify we got some
    ASSERT(token_count > 20);
    
    // Check some key tokens
    ASSERT(tokens[0].type == KRYON_TOKEN_STYLE_DIRECTIVE);
    
    // Find the Button token
    bool found_button = false;
    for (size_t i = 0; i < token_count; i++) {
        if (tokens[i].type == KRYON_TOKEN_ELEMENT_TYPE && 
            kryon_token_lexeme_equals(&tokens[i], "Button")) {
            found_button = true;
            break;
        }
    }
    ASSERT(found_button);
    
    kryon_lexer_destroy(lexer);
}

// Test error handling
TEST(error_handling) {
    const char *source = "\"unterminated string";
    
    KryonLexer *lexer = kryon_lexer_create(source, 0, "test.kry", NULL);
    ASSERT(lexer != NULL);
    
    ASSERT(!kryon_lexer_tokenize(lexer)); // Should fail
    
    const char *error = kryon_lexer_get_error(lexer);
    ASSERT(error != NULL);
    ASSERT(strstr(error, "Unterminated string") != NULL);
    
    kryon_lexer_destroy(lexer);
}

// Main test runner
int main(void) {
    printf("=== Kryon Lexer Unit Tests ===\n\n");
    
    run_test_basic_tokenization();
    run_test_variable_tokenization();
    run_test_directive_tokenization();
    run_test_number_tokenization();
    run_test_string_escapes();
    run_test_complete_kry_example();
    run_test_error_handling();
    
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
