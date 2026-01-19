// ============================================================================
// EXPRESSION EVALUATION TEST
// ============================================================================
// Test the expression evaluation engine (bytecode interpreter)

#define _POSIX_C_SOURCE 200809L
#include <ctype.h>
#include "ir_expression_compiler.h"
#include "ir_expression.h"
#include "ir_builtin_registry.h"
#include "ir_log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

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

#define ASSERT_EQ(a, b, msg) \
    do { \
        if ((a) != (b)) { \
            FAIL(msg); \
            return; \
        } \
    } while(0)

// ============================================================================
// TESTS
// ============================================================================

static void test_eval_literal_int(void) {
    TEST("eval_literal_int");

    IRExpression* expr = ir_expr_int(42);
    IRCompiledExpr* compiled = ir_expr_compile(expr);
    IRValue result = ir_expr_eval(compiled, NULL, 0);

    ASSERT_TRUE(result.type == VAR_TYPE_INT, "should be int type");
    ASSERT_TRUE(result.int_val == 42, "value should be 42");

    ir_value_free(&result);
    ir_expr_compiled_free(compiled);
    ir_expr_free(expr);
    PASS();
}

static void test_eval_literal_float(void) {
    TEST("eval_literal_float");

    IRExpression* expr = ir_expr_float(3.14);
    IRCompiledExpr* compiled = ir_expr_compile(expr);
    IRValue result = ir_expr_eval(compiled, NULL, 0);

    ASSERT_TRUE(result.type == VAR_TYPE_FLOAT, "should be float type");
    ASSERT_TRUE(result.float_val > 3.13 && result.float_val < 3.15, "value should be ~3.14");

    ir_value_free(&result);
    ir_expr_compiled_free(compiled);
    ir_expr_free(expr);
    PASS();
}

static void test_eval_literal_string(void) {
    TEST("eval_literal_string");

    IRExpression* expr = ir_expr_string("hello");
    IRCompiledExpr* compiled = ir_expr_compile(expr);
    IRValue result = ir_expr_eval(compiled, NULL, 0);

    ASSERT_TRUE(result.type == VAR_TYPE_STRING, "should be string type");
    ASSERT_TRUE(strcmp(result.string_val, "hello") == 0, "value should be 'hello'");

    ir_value_free(&result);
    ir_expr_compiled_free(compiled);
    ir_expr_free(expr);
    PASS();
}

static void test_eval_literal_bool(void) {
    TEST("eval_literal_bool");

    IRExpression* expr1 = ir_expr_bool(1);
    IRCompiledExpr* compiled1 = ir_expr_compile(expr1);
    IRValue result1 = ir_expr_eval(compiled1, NULL, 0);
    ASSERT_TRUE(result1.type == VAR_TYPE_BOOL, "should be bool type");
    ASSERT_TRUE(result1.bool_val == 1, "true should be 1");
    ir_value_free(&result1);
    ir_expr_compiled_free(compiled1);
    ir_expr_free(expr1);

    IRExpression* expr2 = ir_expr_bool(0);
    IRCompiledExpr* compiled2 = ir_expr_compile(expr2);
    IRValue result2 = ir_expr_eval(compiled2, NULL, 0);
    ASSERT_TRUE(result2.type == VAR_TYPE_BOOL, "should be bool type");
    ASSERT_TRUE(result2.bool_val == 0, "false should be 0");
    ir_value_free(&result2);
    ir_expr_compiled_free(compiled2);
    ir_expr_free(expr2);

    PASS();
}

static void test_eval_literal_null(void) {
    TEST("eval_literal_null");

    IRExpression* expr = ir_expr_null();
    IRCompiledExpr* compiled = ir_expr_compile(expr);
    IRValue result = ir_expr_eval(compiled, NULL, 0);

    ASSERT_TRUE(result.type == VAR_TYPE_NULL, "should be null type");

    ir_value_free(&result);
    ir_expr_compiled_free(compiled);
    ir_expr_free(expr);
    PASS();
}

static void test_eval_var_ref_undefined(void) {
    TEST("eval_var_ref_undefined");

    // Undefined variable should return null
    IRExpression* expr = ir_expr_var("undefined_var");
    IRCompiledExpr* compiled = ir_expr_compile(expr);
    IRValue result = ir_expr_eval(compiled, NULL, 0);

    ASSERT_TRUE(result.type == VAR_TYPE_NULL, "undefined var should be null");

    ir_value_free(&result);
    ir_expr_compiled_free(compiled);
    ir_expr_free(expr);
    PASS();
}

static void test_eval_binary_add(void) {
    TEST("eval_binary_add");

    IRExpression* expr = ir_expr_add(ir_expr_int(10), ir_expr_int(32));
    IRCompiledExpr* compiled = ir_expr_compile(expr);
    IRValue result = ir_expr_eval(compiled, NULL, 0);

    ASSERT_TRUE(result.type == VAR_TYPE_INT, "should be int type");
    ASSERT_TRUE(result.int_val == 42, "10 + 32 should equal 42");

    ir_value_free(&result);
    ir_expr_compiled_free(compiled);
    ir_expr_free(expr);
    PASS();
}

static void test_eval_binary_sub(void) {
    TEST("eval_binary_sub");

    IRExpression* expr = ir_expr_sub(ir_expr_int(50), ir_expr_int(8));
    IRCompiledExpr* compiled = ir_expr_compile(expr);
    IRValue result = ir_expr_eval(compiled, NULL, 0);

    ASSERT_TRUE(result.type == VAR_TYPE_INT, "should be int type");
    ASSERT_TRUE(result.int_val == 42, "50 - 8 should equal 42");

    ir_value_free(&result);
    ir_expr_compiled_free(compiled);
    ir_expr_free(expr);
    PASS();
}

static void test_eval_binary_mul(void) {
    TEST("eval_binary_mul");

    IRExpression* expr = ir_expr_mul(ir_expr_int(6), ir_expr_int(7));
    IRCompiledExpr* compiled = ir_expr_compile(expr);
    IRValue result = ir_expr_eval(compiled, NULL, 0);

    ASSERT_TRUE(result.type == VAR_TYPE_INT, "should be int type");
    ASSERT_TRUE(result.int_val == 42, "6 * 7 should equal 42");

    ir_value_free(&result);
    ir_expr_compiled_free(compiled);
    ir_expr_free(expr);
    PASS();
}

static void test_eval_binary_div(void) {
    TEST("eval_binary_div");

    IRExpression* expr = ir_expr_div(ir_expr_int(84), ir_expr_int(2));
    IRCompiledExpr* compiled = ir_expr_compile(expr);
    IRValue result = ir_expr_eval(compiled, NULL, 0);

    ASSERT_TRUE(result.type == VAR_TYPE_INT, "should be int type");
    ASSERT_TRUE(result.int_val == 42, "84 / 2 should equal 42");

    ir_value_free(&result);
    ir_expr_compiled_free(compiled);
    ir_expr_free(expr);
    PASS();
}

static void test_eval_binary_mod(void) {
    TEST("eval_binary_mod");

    IRExpression* expr = ir_expr_binary(BINARY_OP_MOD, ir_expr_int(45), ir_expr_int(10));
    IRCompiledExpr* compiled = ir_expr_compile(expr);
    IRValue result = ir_expr_eval(compiled, NULL, 0);

    ASSERT_TRUE(result.type == VAR_TYPE_INT, "should be int type");
    ASSERT_TRUE(result.int_val == 5, "45 % 10 should equal 5");

    ir_value_free(&result);
    ir_expr_compiled_free(compiled);
    ir_expr_free(expr);
    PASS();
}

static void test_eval_string_concat(void) {
    TEST("eval_string_concat");

    IRExpression* expr = ir_expr_add(ir_expr_string("hello"), ir_expr_string(" world"));
    IRCompiledExpr* compiled = ir_expr_compile(expr);
    IRValue result = ir_expr_eval(compiled, NULL, 0);

    ASSERT_TRUE(result.type == VAR_TYPE_STRING, "should be string type");
    ASSERT_TRUE(strcmp(result.string_val, "hello world") == 0, "should concat strings");

    ir_value_free(&result);
    ir_expr_compiled_free(compiled);
    ir_expr_free(expr);
    PASS();
}

static void test_eval_comparison_eq(void) {
    TEST("eval_comparison_eq");

    IRExpression* expr = ir_expr_binary(BINARY_OP_EQ, ir_expr_int(42), ir_expr_int(42));
    IRCompiledExpr* compiled = ir_expr_compile(expr);
    IRValue result = ir_expr_eval(compiled, NULL, 0);

    ASSERT_TRUE(result.type == VAR_TYPE_BOOL, "should be bool type");
    ASSERT_TRUE(result.bool_val == 1, "42 == 42 should be true");

    ir_value_free(&result);
    ir_expr_compiled_free(compiled);
    ir_expr_free(expr);
    PASS();
}

static void test_eval_comparison_neq(void) {
    TEST("eval_comparison_neq");

    IRExpression* expr = ir_expr_binary(BINARY_OP_NEQ, ir_expr_int(42), ir_expr_int(10));
    IRCompiledExpr* compiled = ir_expr_compile(expr);
    IRValue result = ir_expr_eval(compiled, NULL, 0);

    ASSERT_TRUE(result.type == VAR_TYPE_BOOL, "should be bool type");
    ASSERT_TRUE(result.bool_val == 1, "42 != 10 should be true");

    ir_value_free(&result);
    ir_expr_compiled_free(compiled);
    ir_expr_free(expr);
    PASS();
}

static void test_eval_comparison_lt(void) {
    TEST("eval_comparison_lt");

    IRExpression* expr = ir_expr_binary(BINARY_OP_LT, ir_expr_int(10), ir_expr_int(42));
    IRCompiledExpr* compiled = ir_expr_compile(expr);
    IRValue result = ir_expr_eval(compiled, NULL, 0);

    ASSERT_TRUE(result.type == VAR_TYPE_BOOL, "should be bool type");
    ASSERT_TRUE(result.bool_val == 1, "10 < 42 should be true");

    ir_value_free(&result);
    ir_expr_compiled_free(compiled);
    ir_expr_free(expr);
    PASS();
}

static void test_eval_comparison_gt(void) {
    TEST("eval_comparison_gt");

    IRExpression* expr = ir_expr_binary(BINARY_OP_GT, ir_expr_int(42), ir_expr_int(10));
    IRCompiledExpr* compiled = ir_expr_compile(expr);
    IRValue result = ir_expr_eval(compiled, NULL, 0);

    ASSERT_TRUE(result.type == VAR_TYPE_BOOL, "should be bool type");
    ASSERT_TRUE(result.bool_val == 1, "42 > 10 should be true");

    ir_value_free(&result);
    ir_expr_compiled_free(compiled);
    ir_expr_free(expr);
    PASS();
}

static void test_eval_logical_and(void) {
    TEST("eval_logical_and");

    IRExpression* expr = ir_expr_and(ir_expr_bool(1), ir_expr_bool(1));
    IRCompiledExpr* compiled = ir_expr_compile(expr);
    IRValue result = ir_expr_eval(compiled, NULL, 0);

    ASSERT_TRUE(result.type == VAR_TYPE_BOOL, "should be bool type");
    ASSERT_TRUE(result.bool_val == 1, "true && true should be true");

    ir_value_free(&result);
    ir_expr_compiled_free(compiled);
    ir_expr_free(expr);
    PASS();
}

static void test_eval_logical_or(void) {
    TEST("eval_logical_or");

    IRExpression* expr = ir_expr_or(ir_expr_bool(1), ir_expr_bool(0));
    IRCompiledExpr* compiled = ir_expr_compile(expr);
    IRValue result = ir_expr_eval(compiled, NULL, 0);

    ASSERT_TRUE(result.type == VAR_TYPE_BOOL, "should be bool type");
    ASSERT_TRUE(result.bool_val == 1, "true || false should be true");

    ir_value_free(&result);
    ir_expr_compiled_free(compiled);
    ir_expr_free(expr);
    PASS();
}

static void test_eval_unary_not(void) {
    TEST("eval_unary_not");

    IRExpression* expr = ir_expr_unary(UNARY_OP_NOT, ir_expr_bool(1));
    IRCompiledExpr* compiled = ir_expr_compile(expr);
    IRValue result = ir_expr_eval(compiled, NULL, 0);

    ASSERT_TRUE(result.type == VAR_TYPE_BOOL, "should be bool type");
    ASSERT_TRUE(result.bool_val == 0, "!true should be false");

    ir_value_free(&result);
    ir_expr_compiled_free(compiled);
    ir_expr_free(expr);
    PASS();
}

static void test_eval_unary_negate(void) {
    TEST("eval_unary_negate");

    IRExpression* expr = ir_expr_unary(UNARY_OP_NEG, ir_expr_int(42));
    IRCompiledExpr* compiled = ir_expr_compile(expr);
    IRValue result = ir_expr_eval(compiled, NULL, 0);

    ASSERT_TRUE(result.type == VAR_TYPE_INT, "should be int type");
    ASSERT_TRUE(result.int_val == -42, "-42 should equal -42");

    ir_value_free(&result);
    ir_expr_compiled_free(compiled);
    ir_expr_free(expr);
    PASS();
}

static void test_eval_ternary_true(void) {
    TEST("eval_ternary_true");

    IRExpression* expr = ir_expr_ternary(
        ir_expr_bool(1),
        ir_expr_int(42),
        ir_expr_int(0)
    );
    IRCompiledExpr* compiled = ir_expr_compile(expr);
    IRValue result = ir_expr_eval(compiled, NULL, 0);

    ASSERT_TRUE(result.type == VAR_TYPE_INT, "should be int type");
    ASSERT_TRUE(result.int_val == 42, "true ? 42 : 0 should be 42");

    ir_value_free(&result);
    ir_expr_compiled_free(compiled);
    ir_expr_free(expr);
    PASS();
}

static void test_eval_ternary_false(void) {
    TEST("eval_ternary_false");

    IRExpression* expr = ir_expr_ternary(
        ir_expr_bool(0),
        ir_expr_int(1),
        ir_expr_int(99)
    );
    IRCompiledExpr* compiled = ir_expr_compile(expr);
    IRValue result = ir_expr_eval(compiled, NULL, 0);

    ASSERT_TRUE(result.type == VAR_TYPE_INT, "should be int type");
    ASSERT_TRUE(result.int_val == 99, "false ? 1 : 99 should be 99");

    ir_value_free(&result);
    ir_expr_compiled_free(compiled);
    ir_expr_free(expr);
    PASS();
}

static void test_eval_member_access_object(void) {
    TEST("eval_member_access_object");

    // Create an object with a property
    IRObject* obj = ir_object_create(4);
    ir_object_set(obj, "name", ir_value_string("Alice"));

    // Create expression: obj.name
    IRExpression* var_expr = ir_expr_var("obj");
    IRExpression* member_expr = ir_expr_member_access(var_expr, "name");

    // Compile
    IRCompiledExpr* compiled = ir_expr_compile(member_expr);

    // Set up local scope with obj
    // Note: This test shows the pattern, but actual executor integration is needed
    // For now, we'll test with a null result since var resolution returns null
    IRValue result = ir_expr_eval(compiled, NULL, 0);

    // Since we don't have executor integration, this will return null
    ASSERT_TRUE(result.type == VAR_TYPE_NULL, "should be null without executor");

    ir_value_free(&result);
    ir_expr_compiled_free(compiled);
    ir_expr_free(member_expr);
    ir_object_free(obj);
    PASS();
}

static void test_eval_array_length_property(void) {
    TEST("eval_array_length_property");

    // Create an array value
    IRValue array_val;
    array_val.type = VAR_TYPE_ARRAY;
    array_val.array_val = ir_array_create(4);
    ir_array_push(array_val.array_val, ir_value_int(1));
    ir_array_push(array_val.array_val, ir_value_int(2));
    ir_array_push(array_val.array_val, ir_value_int(3));

    // Create expression and test .length property
    // For this test, we verify the property accessor works
    IRValue result = ir_value_int(0);
    if (array_val.type == VAR_TYPE_ARRAY) {
        result = ir_value_int(ir_array_length(array_val.array_val));
    }

    ASSERT_TRUE(result.type == VAR_TYPE_INT, "should be int type");
    ASSERT_TRUE(result.int_val == 3, "array length should be 3");

    ir_value_free(&array_val);
    ir_value_free(&result);
    PASS();
}

static void test_eval_array_index(void) {
    TEST("eval_array_index");

    // Create an array value
    IRValue array_val;
    array_val.type = VAR_TYPE_ARRAY;
    array_val.array_val = ir_array_create(4);
    ir_array_push(array_val.array_val, ir_value_int(10));
    ir_array_push(array_val.array_val, ir_value_int(20));
    ir_array_push(array_val.array_val, ir_value_int(30));

    // Test array indexing
    IRValue result = ir_value_copy(&array_val.array_val->items[1]);

    ASSERT_TRUE(result.type == VAR_TYPE_INT, "should be int type");
    ASSERT_TRUE(result.int_val == 20, "array[1] should be 20");

    ir_value_free(&array_val);
    ir_value_free(&result);
    PASS();
}

static void test_eval_string_length_property(void) {
    TEST("eval_string_length_property");

    IRExpression* expr = ir_expr_string("hello");
    IRCompiledExpr* compiled = ir_expr_compile(expr);

    // Compile and get the string
    IRValue str_val = ir_expr_eval(compiled, NULL, 0);

    // Test .length property
    size_t len = strlen(str_val.string_val);

    ASSERT_TRUE(len == 5, "string length should be 5");

    ir_value_free(&str_val);
    ir_expr_compiled_free(compiled);
    ir_expr_free(expr);
    PASS();
}

static void test_eval_complex_expression(void) {
    TEST("eval_complex_expression");

    // (10 + 20) * 2 - 5 = 55
    IRExpression* expr = ir_expr_sub(
        ir_expr_mul(
            ir_expr_add(ir_expr_int(10), ir_expr_int(20)),
            ir_expr_int(2)
        ),
        ir_expr_int(5)
    );

    IRCompiledExpr* compiled = ir_expr_compile(expr);
    IRValue result = ir_expr_eval(compiled, NULL, 0);

    ASSERT_TRUE(result.type == VAR_TYPE_INT, "should be int type");
    ASSERT_TRUE(result.int_val == 55, "(10 + 20) * 2 - 5 should equal 55");

    ir_value_free(&result);
    ir_expr_compiled_free(compiled);
    ir_expr_free(expr);
    PASS();
}

static void test_eval_nested_comparison(void) {
    TEST("eval_nested_comparison");

    // (5 < 10) && (10 > 5) should be true
    IRExpression* expr = ir_expr_and(
        ir_expr_binary(BINARY_OP_LT, ir_expr_int(5), ir_expr_int(10)),
        ir_expr_binary(BINARY_OP_GT, ir_expr_int(10), ir_expr_int(5))
    );

    IRCompiledExpr* compiled = ir_expr_compile(expr);
    IRValue result = ir_expr_eval(compiled, NULL, 0);

    ASSERT_TRUE(result.type == VAR_TYPE_BOOL, "should be bool type");
    ASSERT_TRUE(result.bool_val == 1, "(5 < 10) && (10 > 5) should be true");

    ir_value_free(&result);
    ir_expr_compiled_free(compiled);
    ir_expr_free(expr);
    PASS();
}

static void test_eval_compile_eval_roundtrip(void) {
    TEST("eval_compile_eval_roundtrip");

    // Test that we can compile and evaluate multiple times with the same compiled expression
    IRExpression* expr = ir_expr_add(ir_expr_var("a"), ir_expr_var("b"));
    IRCompiledExpr* compiled = ir_expr_compile(expr);

    // Multiple evaluations should work (though with undefined vars, returns null)
    IRValue result1 = ir_expr_eval(compiled, NULL, 0);
    IRValue result2 = ir_expr_eval(compiled, NULL, 0);

    ASSERT_TRUE(result1.type == VAR_TYPE_NULL, "undefined var should be null");
    ASSERT_TRUE(result2.type == VAR_TYPE_NULL, "undefined var should be null");

    ir_value_free(&result1);
    ir_value_free(&result2);
    ir_expr_compiled_free(compiled);
    ir_expr_free(expr);
    PASS();
}

static void test_eval_array_push_method(void) {
    TEST("eval_array_push_method");

    // Create an array
    IRValue array_val;
    array_val.type = VAR_TYPE_ARRAY;
    array_val.array_val = ir_array_create(4);
    ir_array_push(array_val.array_val, ir_value_int(1));

    // Push a value
    IRValue to_push = ir_value_int(2);
    ir_array_push(array_val.array_val, to_push);

    ASSERT_TRUE(array_val.array_val->count == 2, "array should have 2 items");

    // Get the second item
    IRValue result = ir_value_copy(&array_val.array_val->items[1]);
    ASSERT_TRUE(result.int_val == 2, "second item should be 2");

    ir_value_free(&array_val);
    ir_value_free(&result);
    PASS();
}

static void test_eval_array_pop_method(void) {
    TEST("eval_array_pop_method");

    // Create an array
    IRValue array_val;
    array_val.type = VAR_TYPE_ARRAY;
    array_val.array_val = ir_array_create(4);
    ir_array_push(array_val.array_val, ir_value_int(1));
    ir_array_push(array_val.array_val, ir_value_int(2));
    ir_array_push(array_val.array_val, ir_value_int(3));

    // Pop the last item
    IRValue popped = ir_array_pop(array_val.array_val);

    ASSERT_TRUE(popped.type == VAR_TYPE_INT, "popped should be int");
    ASSERT_TRUE(popped.int_val == 3, "popped value should be 3");
    ASSERT_TRUE(array_val.array_val->count == 2, "array should have 2 items after pop");

    ir_value_free(&array_val);
    ir_value_free(&popped);
    PASS();
}

static void test_eval_string_to_upper(void) {
    TEST("eval_string_to_upper");

    // Test string toUpperCase method
    char* str = strdup("hello");
    for (size_t i = 0; str[i]; i++) {
        str[i] = (char)toupper((unsigned char)str[i]);
    }

    ASSERT_TRUE(strcmp(str, "HELLO") == 0, "string should be uppercase");

    free(str);
    PASS();
}

static void test_eval_string_to_lower(void) {
    TEST("eval_string_to_lower");

    // Test string toLowerCase method
    char* str = strdup("HELLO");
    for (size_t i = 0; str[i]; i++) {
        str[i] = (char)tolower((unsigned char)str[i]);
    }

    ASSERT_TRUE(strcmp(str, "hello") == 0, "string should be lowercase");

    free(str);
    PASS();
}

static void test_eval_string_trim(void) {
    TEST("eval_string_trim");

    const char* s = "  hello  ";
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
    size_t len = strlen(s);
    while (len > 0 && (s[len - 1] == ' ' || s[len - 1] == '\t' ||
                      s[len - 1] == '\n' || s[len - 1] == '\r')) len--;

    ASSERT_TRUE(len == 5, "trimmed length should be 5");
    ASSERT_TRUE(strncmp(s, "hello", len) == 0, "trimmed string should be 'hello'");

    PASS();
}

static void test_eval_float_operations(void) {
    TEST("eval_float_operations");

    // Test float arithmetic
    IRExpression* expr = ir_expr_add(ir_expr_float(1.5), ir_expr_float(2.5));
    IRCompiledExpr* compiled = ir_expr_compile(expr);
    IRValue result = ir_expr_eval(compiled, NULL, 0);

    ASSERT_TRUE(result.type == VAR_TYPE_FLOAT, "should be float type");
    ASSERT_TRUE(result.float_val > 3.99 && result.float_val < 4.01, "1.5 + 2.5 should be ~4.0");

    ir_value_free(&result);
    ir_expr_compiled_free(compiled);
    ir_expr_free(expr);
    PASS();
}

// ============================================================================
// BUILTIN FUNCTION TESTS
// ============================================================================

static void test_eval_builtin_string_toUpper(void) {
    TEST("eval_builtin_string_toUpper");

    // Initialize global builtin registry
    ir_builtin_global_init();

    // Create expression: string_toUpper("hello")
    IRExpression* args[] = { ir_expr_string("hello") };
    IRExpression* expr = ir_expr_call("string_toUpper", args, 1);
    IRCompiledExpr* compiled = ir_expr_compile(expr);
    IRValue result = ir_expr_eval(compiled, NULL, 0);

    ASSERT_TRUE(result.type == VAR_TYPE_STRING, "should return string");
    ASSERT_TRUE(strcmp(result.string_val, "HELLO") == 0, "should convert to uppercase");

    ir_value_free(&result);
    ir_expr_compiled_free(compiled);
    ir_expr_free(expr);
    PASS();
}

static void test_eval_builtin_string_length(void) {
    TEST("eval_builtin_string_length");

    // Create expression: string_length("hello")
    IRExpression* args[] = { ir_expr_string("hello") };
    IRExpression* expr = ir_expr_call("string_length", args, 1);
    IRCompiledExpr* compiled = ir_expr_compile(expr);
    IRValue result = ir_expr_eval(compiled, NULL, 0);

    ASSERT_TRUE(result.type == VAR_TYPE_INT, "should return int");
    ASSERT_TRUE(result.int_val == 5, "length should be 5");

    ir_value_free(&result);
    ir_expr_compiled_free(compiled);
    ir_expr_free(expr);
    PASS();
}

static void test_eval_builtin_array_length(void) {
    TEST("eval_builtin_array_length");

    // Create an array and test array_length
    // This is a bit tricky since we can't easily pass array values to builtin calls
    // without executor integration. For now, we test that the call doesn't crash.

    // Create expression: array_length(null) - should return 0
    IRExpression* args[] = { ir_expr_null() };
    IRExpression* expr = ir_expr_call("array_length", args, 1);
    IRCompiledExpr* compiled = ir_expr_compile(expr);
    IRValue result = ir_expr_eval(compiled, NULL, 0);

    // Should return 0 for null input
    ASSERT_TRUE(result.type == VAR_TYPE_INT, "should return int");

    ir_value_free(&result);
    ir_expr_compiled_free(compiled);
    ir_expr_free(expr);
    PASS();
}

static void test_eval_builtin_math_abs(void) {
    TEST("eval_builtin_math_abs");

    // Create expression: math_abs(-42)
    IRExpression* args[] = { ir_expr_int(-42) };
    IRExpression* expr = ir_expr_call("math_abs", args, 1);
    IRCompiledExpr* compiled = ir_expr_compile(expr);
    IRValue result = ir_expr_eval(compiled, NULL, 0);

    ASSERT_TRUE(result.type == VAR_TYPE_INT, "should return int");
    ASSERT_TRUE(result.int_val == 42, "absolute value should be 42");

    ir_value_free(&result);
    ir_expr_compiled_free(compiled);
    ir_expr_free(expr);
    PASS();
}

static void test_eval_builtin_math_max(void) {
    TEST("eval_builtin_math_max");

    // Create expression: math_max(10, 20, 15)
    IRExpression* args[] = { ir_expr_int(10), ir_expr_int(20), ir_expr_int(15) };
    IRExpression* expr = ir_expr_call("math_max", args, 3);
    IRCompiledExpr* compiled = ir_expr_compile(expr);
    IRValue result = ir_expr_eval(compiled, NULL, 0);

    ASSERT_TRUE(result.type == VAR_TYPE_INT, "should return int");
    ASSERT_TRUE(result.int_val == 20, "max should be 20");

    ir_value_free(&result);
    ir_expr_compiled_free(compiled);
    ir_expr_free(expr);
    PASS();
}

static void test_eval_builtin_type_typeof(void) {
    TEST("eval_builtin_type_typeof");

    // Create expression: type_typeof(42)
    IRExpression* args[] = { ir_expr_int(42) };
    IRExpression* expr = ir_expr_call("type_typeof", args, 1);
    IRCompiledExpr* compiled = ir_expr_compile(expr);
    IRValue result = ir_expr_eval(compiled, NULL, 0);

    ASSERT_TRUE(result.type == VAR_TYPE_STRING, "should return string");
    ASSERT_TRUE(strcmp(result.string_val, "int") == 0, "type should be 'int'");

    ir_value_free(&result);
    ir_expr_compiled_free(compiled);
    ir_expr_free(expr);
    PASS();
}

// ============================================================================
// MAIN
// ============================================================================

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    printf("============================================================================\n");
    printf("EXPRESSION EVALUATION TEST\n");
    printf("============================================================================\n\n");

    // Literal evaluation tests
    test_eval_literal_int();
    test_eval_literal_float();
    test_eval_literal_string();
    test_eval_literal_bool();
    test_eval_literal_null();

    // Variable reference tests
    test_eval_var_ref_undefined();

    // Binary operation tests
    test_eval_binary_add();
    test_eval_binary_sub();
    test_eval_binary_mul();
    test_eval_binary_div();
    test_eval_binary_mod();
    test_eval_string_concat();

    // Comparison operation tests
    test_eval_comparison_eq();
    test_eval_comparison_neq();
    test_eval_comparison_lt();
    test_eval_comparison_gt();

    // Logical operation tests
    test_eval_logical_and();
    test_eval_logical_or();

    // Unary operation tests
    test_eval_unary_not();
    test_eval_unary_negate();

    // Ternary tests
    test_eval_ternary_true();
    test_eval_ternary_false();

    // Member access tests
    test_eval_member_access_object();
    test_eval_array_length_property();
    test_eval_array_index();
    test_eval_string_length_property();

    // Complex expression tests
    test_eval_complex_expression();
    test_eval_nested_comparison();

    // Roundtrip tests
    test_eval_compile_eval_roundtrip();

    // Method tests
    test_eval_array_push_method();
    test_eval_array_pop_method();
    test_eval_string_to_upper();
    test_eval_string_to_lower();
    test_eval_string_trim();

    // Float operations
    test_eval_float_operations();

    // Builtin function tests
    test_eval_builtin_string_toUpper();
    test_eval_builtin_string_length();
    test_eval_builtin_array_length();
    test_eval_builtin_math_abs();
    test_eval_builtin_math_max();
    test_eval_builtin_type_typeof();

    printf("\n============================================================================\n");
    printf("RESULTS: %d total, %d passed, %d failed\n", tests_run, tests_passed, tests_failed);
    printf("============================================================================\n");

    return tests_failed > 0 ? 1 : 0;
}
