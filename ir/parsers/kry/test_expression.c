/**
 * Test program for KRY Expression Transpiler
 */

#include "kry_expression.h"
#include "kry_arrow_registry.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int test_count = 0;
static int pass_count = 0;

#define TEST(name, expr, expected_lua, expected_js) \
    do { \
        test_count++; \
        printf("Test %d: %s\n", test_count, name); \
        printf("  Expression: %s\n", expr); \
        \
        char* error = NULL; \
        KryExprNode* node = kry_expr_parse(expr, &error); \
        if (!node) { \
            printf("  FAIL: Parse error: %s\n", error ? error : "unknown"); \
            free(error); \
        } else { \
            size_t lua_len; \
            char* lua = kry_expr_transpile(node, &(KryExprOptions){KRY_TARGET_LUA, false, 0}, &lua_len); \
            \
            size_t js_len; \
            char* js = kry_expr_transpile(node, &(KryExprOptions){KRY_TARGET_JAVASCRIPT, false, 0}, &js_len); \
            \
            bool lua_match = lua && strcmp(lua, expected_lua) == 0; \
            bool js_match = js && strcmp(js, expected_js) == 0; \
            \
            printf("  Lua:   %s %s\n", lua, lua_match ? "✓" : "✗"); \
            printf("  JS:    %s %s\n", js, js_match ? "✓" : "✗"); \
            \
            if (!lua_match) printf("    Expected: %s\n", expected_lua); \
            if (!js_match) printf("    Expected: %s\n", expected_js); \
            \
            if (lua_match && js_match) pass_count++; \
            \
            free(lua); \
            free(js); \
            kry_expr_free(node); \
        } \
        printf("\n"); \
    } while(0)

int main(void) {
    printf("========================================\n");
    printf("KRY Expression Transpiler Tests\n");
    printf("========================================\n\n");

    // Literals
    TEST("Number literal", "42", "42", "42");
    TEST("String literal", "\"hello\"", "\"hello\"", "\"hello\"");
    TEST("Boolean true", "true", "true", "true");
    TEST("Boolean false", "false", "false", "false");
    TEST("Null", "null", "nil", "null");

    // Identifiers
    TEST("Identifier", "myVariable", "myVariable", "myVariable");

    // Binary operators
    TEST("Addition", "a + b", "(a + b)", "(a + b)");
    TEST("Subtraction", "x - y", "(x - y)", "(x - y)");
    TEST("Multiplication", "a * b", "(a * b)", "(a * b)");
    TEST("Division", "a / b", "(a / b)", "(a / b)");

    // Comparison
    TEST("Equal", "a == b", "(a == b)", "(a == b)");
    TEST("Not equal", "a != b", "(a ~= b)", "(a != b)");
    TEST("Less than", "a < b", "(a < b)", "(a < b)");
    TEST("Greater than", "a > b", "(a > b)", "(a > b)");

    // Logical operators
    TEST("Logical AND", "a && b", "(a and b)", "(a && b)");
    TEST("Logical OR", "a || b", "(a or b)", "(a || b)");
    TEST("Logical NOT", "!a", "not a", "!a");

    // Property access
    TEST("Property access", "obj.prop", "obj.prop", "obj.prop");
    TEST("Chained property", "obj.prop.name", "obj.prop.name", "obj.prop.name");

    // Array access
    TEST("Array access", "arr[0]", "arr[(0 + 1)]", "arr[0]");
    TEST("Array access with expr", "arr[i]", "arr[(i + 1)]", "arr[i]");

    // Function calls
    TEST("Function call", "func()", "func()", "func()");
    TEST("Function call with args", "add(a, b)", "add(a, b)", "add(a, b)");

    // Array literals
    TEST("Array literal", "[1, 2, 3]", "{1, 2, 3}", "[1, 2, 3]");
    TEST("Empty array", "[]", "{}", "[]");
    TEST("Nested arrays", "[[1, 2], [3, 4]]", "{{1, 2}, {3, 4}}", "[[1, 2], [3, 4]]");

    // Object literals
    TEST("Object literal", "{key: val}", "{key = val}", "{key: val}");
    TEST("Object multiple props", "{a: 1, b: 2}", "{a = 1, b = 2}", "{a: 1, b: 2}");

    // Arrow functions
    TEST("Arrow function", "x => x * 2", "function(x) return (x * 2); end", "(x) => (x * 2)");
    TEST("Arrow function add", "(a, b) => a + b", "function(a, b) return (a + b); end", "(a, b) => (a + b)");

    // Complex expressions
    TEST("Complex expression", "items.map(x => x * 2)", "items.map(function(x) return (x * 2); end)", "items.map((x) => (x * 2))");

    // Test C arrow function code generation
    printf("========================================\n");
    printf("C Arrow Function Tests\n");
    printf("========================================\n\n");

    // Test 1: Simple arrow function without captures
    {
        printf("Test: Simple arrow function (no captures)\n");
        char* error = NULL;
        KryExprNode* node = kry_expr_parse("x => x * 2", &error);
        if (node) {
            KryArrowRegistry* reg = kry_arrow_registry_create();
            KryExprOptions opts = {KRY_TARGET_C, false, 0, reg, "test"};
            char* c_code = kry_expr_transpile(node, &opts, NULL);

            printf("  Expression: x => x * 2\n");
            printf("  Inline C:   %s\n", c_code);

            char* stubs = kry_arrow_generate_stubs(reg);
            printf("  Generated stubs:\n%s", stubs);

            free(c_code);
            free(stubs);
            kry_arrow_registry_free(reg);
            kry_expr_free(node);
        } else {
            printf("  FAIL: Parse error: %s\n", error);
            free(error);
        }
        printf("\n");
    }

    // Test 2: Arrow function with captures
    {
        printf("Test: Arrow function with captures\n");
        char* error = NULL;
        KryExprNode* node = kry_expr_parse("() => deleteHabit(habit, index)", &error);
        if (node) {
            KryArrowRegistry* reg = kry_arrow_registry_create();
            KryExprOptions opts = {KRY_TARGET_C, false, 0, reg, "click"};
            char* c_code = kry_expr_transpile(node, &opts, NULL);

            printf("  Expression: () => deleteHabit(habit, index)\n");
            printf("  Inline C:   %s\n", c_code);

            char* stubs = kry_arrow_generate_stubs(reg);
            printf("  Generated stubs:\n%s", stubs);

            // Generate context init
            char* ctx_init = kry_arrow_generate_ctx_init(reg, 0);
            if (ctx_init) {
                printf("  Context init: %s\n", ctx_init);
                free(ctx_init);
            }

            free(c_code);
            free(stubs);
            kry_arrow_registry_free(reg);
            kry_expr_free(node);
        } else {
            printf("  FAIL: Parse error: %s\n", error);
            free(error);
        }
        printf("\n");
    }

    // Test 3: Arrow function with parameter and captures
    {
        printf("Test: Arrow function with parameter and captures\n");
        char* error = NULL;
        KryExprNode* node = kry_expr_parse("value => habit.name = value", &error);
        if (node) {
            KryArrowRegistry* reg = kry_arrow_registry_create();
            KryExprOptions opts = {KRY_TARGET_C, false, 0, reg, "change"};
            char* c_code = kry_expr_transpile(node, &opts, NULL);

            printf("  Expression: value => habit.name = value\n");
            printf("  Inline C:   %s\n", c_code);

            char* stubs = kry_arrow_generate_stubs(reg);
            printf("  Generated stubs:\n%s", stubs);

            free(c_code);
            free(stubs);
            kry_arrow_registry_free(reg);
            kry_expr_free(node);
        } else {
            printf("  FAIL: Parse error: %s\n", error);
            free(error);
        }
        printf("\n");
    }

    printf("========================================\n");
    printf("Results: %d/%d tests passed\n", pass_count, test_count);
    printf("========================================\n");

    return (pass_count == test_count) ? 0 : 1;
}
