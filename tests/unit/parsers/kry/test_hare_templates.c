/**
 * Hare Template String Tests
 * Tests template string parsing and code generation for all target languages
 */

#define _POSIX_C_SOURCE 200809L
#include "kry_expression.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) \
    do { \
        tests_run++; \
        printf("Test %d: %s...", tests_run, name); \
        fflush(stdout); \
    } while(0)

#define PASS() \
    do { \
        tests_passed++; \
        printf(" PASS\n"); \
    } while(0)

#define FAIL(msg) \
    do { \
        tests_failed++; \
        printf(" FAIL: %s\n", msg); \
    } while(0)

#define ASSERT_TRUE(cond, msg) \
    do { \
        if (!(cond)) { \
            FAIL(msg); \
            return; \
        } \
    } while(0)

#define ASSERT_NOT_NULL(ptr, msg) \
    ASSERT_TRUE((ptr) != NULL, msg)

#define ASSERT_STR_CONTAINS(haystack, needle, msg) \
    ASSERT_TRUE(strstr((haystack), (needle)) != NULL, msg)

#define ASSERT_STR_EQ(a, b, msg) \
    ASSERT_TRUE(strcmp((a), (b)) == 0, msg)

// ============================================================================
// Helper Functions
// ============================================================================

static char* transpile_to_target(const char* source, KryExprTarget target) {
    char* error = NULL;
    KryExprNode* node = kry_expr_parse(source, &error);
    if (!node) {
        if (error) {
            printf(" [Parse error: %s]", error);
            free(error);
        }
        return NULL;
    }

    KryExprOptions options = {
        .target = target,
        .pretty_print = true,
        .indent_level = 0,
        .arrow_registry = NULL,
        .context_hint = NULL
    };

    char* result = kry_expr_transpile(node, &options, NULL);
    kry_expr_free(node);
    return result;
}

// ============================================================================
// Hare Template String Tests
// ============================================================================

static void test_hare_simple_template(void) {
    TEST("Hare: simple template");

    char* result = transpile_to_target("`Hello ${name}!`", KRY_TARGET_HARE);
    ASSERT_NOT_NULL(result, "should generate Hare code");
    ASSERT_STR_CONTAINS(result, "fmt::fprintf", "should use fmt::fprintf");
    ASSERT_STR_CONTAINS(result, "Hello", "should contain 'Hello'");
    ASSERT_STR_CONTAINS(result, "name", "should contain 'name'");

    free(result);
    PASS();
}

static void test_hare_multiple_vars(void) {
    TEST("Hare: multiple variables");

    char* result = transpile_to_target("`Value: ${x}, Count: ${y}`", KRY_TARGET_HARE);
    ASSERT_NOT_NULL(result, "should generate Hare code");
    ASSERT_STR_CONTAINS(result, "fmt::fprintf", "should use fmt::fprintf");
    ASSERT_STR_CONTAINS(result, "Value:", "should contain 'Value:'");
    ASSERT_STR_CONTAINS(result, "Count:", "should contain 'Count:'");
    ASSERT_STR_CONTAINS(result, "x", "should contain 'x'");
    ASSERT_STR_CONTAINS(result, "y", "should contain 'y'");

    free(result);
    PASS();
}

static void test_hare_nested_expressions(void) {
    TEST("Hare: nested expressions");

    char* result = transpile_to_target("`Sum: ${a + b}`", KRY_TARGET_HARE);
    ASSERT_NOT_NULL(result, "should generate Hare code");
    ASSERT_STR_CONTAINS(result, "fmt::fprintf", "should use fmt::fprintf");
    ASSERT_STR_CONTAINS(result, "Sum:", "should contain 'Sum:'");
    ASSERT_STR_CONTAINS(result, "+", "should contain '+' operator");

    free(result);
    PASS();
}

static void test_hare_escaped_braces(void) {
    TEST("Hare: escaped braces");

    // In the template string format, literal braces need to be escaped
    // The implementation should handle this by doubling them: {{ and }}
    char* result = transpile_to_target("`Literal {braces}`", KRY_TARGET_HARE);
    ASSERT_NOT_NULL(result, "should generate Hare code");
    ASSERT_STR_CONTAINS(result, "fmt::fprintf", "should use fmt::fprintf");

    free(result);
    PASS();
}

static void test_hare_empty_template(void) {
    TEST("Hare: empty template");

    char* result = transpile_to_target("``", KRY_TARGET_HARE);
    ASSERT_NOT_NULL(result, "should generate Hare code (empty string)");

    free(result);
    PASS();
}

// ============================================================================
// Lua Template String Tests
// ============================================================================

static void test_lua_simple_template(void) {
    TEST("Lua: simple template");

    char* result = transpile_to_target("`Hello ${name}!`", KRY_TARGET_LUA);
    ASSERT_NOT_NULL(result, "should generate Lua code");
    ASSERT_STR_CONTAINS(result, "Hello", "should contain 'Hello'");
    ASSERT_STR_CONTAINS(result, "name", "should contain 'name'");
    ASSERT_STR_CONTAINS(result, "..", "should use concatenation operator");

    free(result);
    PASS();
}

static void test_lua_multiple_vars(void) {
    TEST("Lua: multiple variables");

    char* result = transpile_to_target("`Value: ${x}, Count: ${y}`", KRY_TARGET_LUA);
    ASSERT_NOT_NULL(result, "should generate Lua code");
    ASSERT_STR_CONTAINS(result, "Value:", "should contain 'Value:'");
    ASSERT_STR_CONTAINS(result, "Count:", "should contain 'Count:'");
    ASSERT_STR_CONTAINS(result, "x", "should contain 'x'");
    ASSERT_STR_CONTAINS(result, "y", "should contain 'y'");

    free(result);
    PASS();
}

// ============================================================================
// JavaScript Template String Tests
// ============================================================================

static void test_js_simple_template(void) {
    TEST("JavaScript: simple template");

    char* result = transpile_to_target("`Hello ${name}!`", KRY_TARGET_JAVASCRIPT);
    ASSERT_NOT_NULL(result, "should generate JavaScript code");
    ASSERT_STR_CONTAINS(result, "Hello", "should contain 'Hello'");
    ASSERT_STR_CONTAINS(result, "name", "should contain 'name'");
    ASSERT_STR_CONTAINS(result, "${", "should preserve template syntax");

    free(result);
    PASS();
}

static void test_js_multiple_vars(void) {
    TEST("JavaScript: multiple variables");

    char* result = transpile_to_target("`Value: ${x}, Count: ${y}`", KRY_TARGET_JAVASCRIPT);
    ASSERT_NOT_NULL(result, "should generate JavaScript code");
    ASSERT_STR_CONTAINS(result, "Value:", "should contain 'Value:'");
    ASSERT_STR_CONTAINS(result, "Count:", "should contain 'Count:'");

    free(result);
    PASS();
}

// ============================================================================
// C Template String Tests
// ============================================================================

static void test_c_simple_template(void) {
    TEST("C: simple template");

    char* result = transpile_to_target("`Hello ${name}!`", KRY_TARGET_C);
    ASSERT_NOT_NULL(result, "should generate C code");
    ASSERT_STR_CONTAINS(result, "Hello", "should contain 'Hello'");
    ASSERT_STR_CONTAINS(result, "name", "should contain 'name'");

    free(result);
    PASS();
}

static void test_c_multiple_vars(void) {
    TEST("C: multiple variables");

    char* result = transpile_to_target("`Value: ${x}, Count: ${y}`", KRY_TARGET_C);
    ASSERT_NOT_NULL(result, "should generate C code");
    ASSERT_STR_CONTAINS(result, "Value:", "should contain 'Value:'");
    ASSERT_STR_CONTAINS(result, "Count:", "should contain 'Count:'");

    free(result);
    PASS();
}

// ============================================================================
// Edge Case Tests
// ============================================================================

static void test_template_with_property_access(void) {
    TEST("Template: property access");

    char* result = transpile_to_target("`Hello ${user.name}!`", KRY_TARGET_HARE);
    ASSERT_NOT_NULL(result, "should generate code with property access");
    ASSERT_STR_CONTAINS(result, ".", "should contain property access");

    free(result);
    PASS();
}

static void test_template_with_function_call(void) {
    TEST("Template: function call");

    char* result = transpile_to_target("`Result: ${getValue()}`", KRY_TARGET_HARE);
    ASSERT_NOT_NULL(result, "should generate code with function call");
    ASSERT_STR_CONTAINS(result, "getValue", "should contain function name");
    ASSERT_STR_CONTAINS(result, "()", "should contain function call");

    free(result);
    PASS();
}

static void test_template_with_array_access(void) {
    TEST("Template: array access");

    char* result = transpile_to_target("`First: ${items[0]}`", KRY_TARGET_HARE);
    ASSERT_NOT_NULL(result, "should generate code with array access");
    ASSERT_STR_CONTAINS(result, "items", "should contain array name");
    ASSERT_STR_CONTAINS(result, "[", "should contain array index");

    free(result);
    PASS();
}

static void test_template_with_complex_expression(void) {
    TEST("Template: complex expression");

    char* result = transpile_to_target("`Sum: ${a + b * 2}`", KRY_TARGET_HARE);
    ASSERT_NOT_NULL(result, "should generate code with complex expression");
    ASSERT_STR_CONTAINS(result, "+", "should contain '+'");
    ASSERT_STR_CONTAINS(result, "*", "should contain '*'");

    free(result);
    PASS();
}

static void test_template_with_ternary(void) {
    TEST("Template: ternary expression");

    char* result = transpile_to_target("`Result: ${x > 5 ? 'big' : 'small'}`", KRY_TARGET_HARE);
    ASSERT_NOT_NULL(result, "should generate code with ternary");
    // Hare uses match expressions instead of ternary, so check for match or proper condition
    ASSERT_STR_CONTAINS(result, "match", "should contain match expression for ternary");

    free(result);
    PASS();
}

static void test_template_with_string_literal(void) {
    TEST("Template: string literal in expression");

    char* result = transpile_to_target("`Hello ${'World'}`", KRY_TARGET_HARE);
    ASSERT_NOT_NULL(result, "should generate code with string literal");
    ASSERT_STR_CONTAINS(result, "fmt::fprintf", "should use fmt::fprintf");

    free(result);
    PASS();
}

static void test_template_with_number_literal(void) {
    TEST("Template: number literal in expression");

    char* result = transpile_to_target("`Value: ${42}`", KRY_TARGET_HARE);
    ASSERT_NOT_NULL(result, "should generate code with number literal");
    ASSERT_STR_CONTAINS(result, "42", "should contain the number");

    free(result);
    PASS();
}

// ============================================================================
// MAIN
// ============================================================================

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    printf("============================================================================\n");
    printf("HARE TEMPLATE STRING TESTS\n");
    printf("============================================================================\n\n");

    // Hare template tests
    printf("--- Hare Template Tests ---\n");
    test_hare_simple_template();
    test_hare_multiple_vars();
    test_hare_nested_expressions();
    test_hare_escaped_braces();
    test_hare_empty_template();

    // Lua template tests
    printf("\n--- Lua Template Tests ---\n");
    test_lua_simple_template();
    test_lua_multiple_vars();

    // JavaScript template tests
    printf("\n--- JavaScript Template Tests ---\n");
    test_js_simple_template();
    test_js_multiple_vars();

    // C template tests
    printf("\n--- C Template Tests ---\n");
    test_c_simple_template();
    test_c_multiple_vars();

    // Edge case tests
    printf("\n--- Edge Case Tests ---\n");
    test_template_with_property_access();
    test_template_with_function_call();
    test_template_with_array_access();
    test_template_with_complex_expression();
    test_template_with_ternary();
    test_template_with_string_literal();
    test_template_with_number_literal();

    printf("\n============================================================================\n");
    printf("RESULTS: %d total, %d passed, %d failed\n", tests_run, tests_passed, tests_failed);
    printf("============================================================================\n");

    return tests_failed > 0 ? 1 : 0;
}
