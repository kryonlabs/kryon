#include "ir_builtin_registry.h"
#include "ir_expression.h"
#include "ir_expression_compiler.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

// ============================================================================
// TEST HELPERS
// ============================================================================

#define TEST(name) printf("Test: %s...\n", name)
#define PASS() printf("  PASS\n")
#define ASSERT_TRUE(cond, msg) \
    do { \
        if (!(cond)) { \
            printf("  FAIL: %s\n", msg); \
            exit(1); \
        } \
    } while(0)

// ============================================================================
// REGISTRY TESTS
// ============================================================================

static void test_registry_create_destroy(void) {
    TEST("registry_create_destroy");

    IRBuiltinRegistry* registry = ir_builtin_registry_create();

    ASSERT_TRUE(registry != NULL, "registry should not be NULL");
    ASSERT_TRUE(registry->count > 0, "registry should have builtins");

    ir_builtin_registry_destroy(registry);
    PASS();
}

static void test_registry_lookup(void) {
    TEST("registry_lookup");

    IRBuiltinRegistry* registry = ir_builtin_registry_create();

    IRBuiltinDef* def = ir_builtin_lookup(registry, "string_toUpper");
    ASSERT_TRUE(def != NULL, "should find string_toUpper");
    ASSERT_TRUE(strcmp(def->name, "string_toUpper") == 0, "name should match");
    ASSERT_TRUE(def->min_args == 1, "min_args should be 1");
    ASSERT_TRUE(def->is_pure == true, "should be pure");

    // Unknown function
    def = ir_builtin_lookup(registry, "unknown_function_xyz");
    ASSERT_TRUE(def == NULL, "unknown function should return NULL");

    ir_builtin_registry_destroy(registry);
    PASS();
}

// ============================================================================
// STRING FUNCTION TESTS
// ============================================================================

static void test_string_toUpper(void) {
    TEST("string_toUpper");

    IRBuiltinRegistry* registry = ir_builtin_registry_create();

    IRValue args[1] = { ir_value_string("hello") };
    IRValue result = ir_builtin_call(registry, "string_toUpper", NULL, args, 1);

    ASSERT_TRUE(result.type == VAR_TYPE_STRING, "should return string");
    ASSERT_TRUE(strcmp(result.string_val, "HELLO") == 0, "should convert to uppercase");

    ir_value_free(&args[0]);
    ir_value_free(&result);
    ir_builtin_registry_destroy(registry);
    PASS();
}

static void test_string_toLower(void) {
    TEST("string_toLower");

    IRBuiltinRegistry* registry = ir_builtin_registry_create();

    IRValue args[1] = { ir_value_string("HELLO") };
    IRValue result = ir_builtin_call(registry, "string_toLower", NULL, args, 1);

    ASSERT_TRUE(result.type == VAR_TYPE_STRING, "should return string");
    ASSERT_TRUE(strcmp(result.string_val, "hello") == 0, "should convert to lowercase");

    ir_value_free(&args[0]);
    ir_value_free(&result);
    ir_builtin_registry_destroy(registry);
    PASS();
}

static void test_string_trim(void) {
    TEST("string_trim");

    IRBuiltinRegistry* registry = ir_builtin_registry_create();

    IRValue args[1] = { ir_value_string("  hello  ") };
    IRValue result = ir_builtin_call(registry, "string_trim", NULL, args, 1);

    ASSERT_TRUE(result.type == VAR_TYPE_STRING, "should return string");
    ASSERT_TRUE(strcmp(result.string_val, "hello") == 0, "should trim whitespace");

    ir_value_free(&args[0]);
    ir_value_free(&result);
    ir_builtin_registry_destroy(registry);
    PASS();
}

static void test_string_substring(void) {
    TEST("string_substring");

    IRBuiltinRegistry* registry = ir_builtin_registry_create();

    IRValue args[3] = {
        ir_value_string("hello"),
        ir_value_int(1),
        ir_value_int(4)
    };
    IRValue result = ir_builtin_call(registry, "string_substring", NULL, args, 3);

    ASSERT_TRUE(result.type == VAR_TYPE_STRING, "should return string");
    ASSERT_TRUE(strcmp(result.string_val, "ell") == 0, "should extract substring");

    ir_value_free(&args[0]);
    ir_value_free(&result);
    ir_builtin_registry_destroy(registry);
    PASS();
}

static void test_string_split(void) {
    TEST("string_split");

    IRBuiltinRegistry* registry = ir_builtin_registry_create();

    IRValue args[2] = {
        ir_value_string("a,b,c"),
        ir_value_string(",")
    };
    IRValue result = ir_builtin_call(registry, "string_split", NULL, args, 2);

    ASSERT_TRUE(result.type == VAR_TYPE_ARRAY, "should return array");
    ASSERT_TRUE(ir_array_length(result.array_val) == 3, "should have 3 elements");

    // Check first element
    IRValue elem0 = ir_array_get(result.array_val, 0);
    ASSERT_TRUE(elem0.type == VAR_TYPE_STRING, "element should be string");
    ASSERT_TRUE(strcmp(elem0.string_val, "a") == 0, "first element should be 'a'");
    ir_value_free(&elem0);

    ir_value_free(&args[0]);
    ir_value_free(&args[1]);
    ir_value_free(&result);
    ir_builtin_registry_destroy(registry);
    PASS();
}

static void test_string_length(void) {
    TEST("string_length");

    IRBuiltinRegistry* registry = ir_builtin_registry_create();

    IRValue args[1] = { ir_value_string("hello") };
    IRValue result = ir_builtin_call(registry, "string_length", NULL, args, 1);

    ASSERT_TRUE(result.type == VAR_TYPE_INT, "should return int");
    ASSERT_TRUE(result.int_val == 5, "length should be 5");

    ir_value_free(&args[0]);
    ir_value_free(&result);
    ir_builtin_registry_destroy(registry);
    PASS();
}

// ============================================================================
// ARRAY FUNCTION TESTS
// ============================================================================

static void test_array_length(void) {
    TEST("array_length");

    IRBuiltinRegistry* registry = ir_builtin_registry_create();

    IRValue arr;
    arr.type = VAR_TYPE_ARRAY;
    arr.array_val = ir_array_create(3);
    ir_array_push(arr.array_val, ir_value_int(1));
    ir_array_push(arr.array_val, ir_value_int(2));
    ir_array_push(arr.array_val, ir_value_int(3));

    IRValue args[1] = { arr };
    IRValue result = ir_builtin_call(registry, "array_length", NULL, args, 1);

    ASSERT_TRUE(result.type == VAR_TYPE_INT, "should return int");
    ASSERT_TRUE(result.int_val == 3, "length should be 3");

    ir_value_free(&arr);
    ir_value_free(&result);
    ir_builtin_registry_destroy(registry);
    PASS();
}

static void test_array_push(void) {
    TEST("array_push");

    IRBuiltinRegistry* registry = ir_builtin_registry_create();

    IRValue arr;
    arr.type = VAR_TYPE_ARRAY;
    arr.array_val = ir_array_create(3);

    IRValue elem = ir_value_int(42);
    IRValue args[2] = { arr, elem };
    IRValue result = ir_builtin_call(registry, "array_push", NULL, args, 2);

    ASSERT_TRUE(result.type == VAR_TYPE_ARRAY, "should return array");
    ASSERT_TRUE(ir_array_length(result.array_val) == 1, "array should have 1 element");

    IRValue pushed = ir_array_get(result.array_val, 0);
    ASSERT_TRUE(pushed.type == VAR_TYPE_INT, "element should be int");
    ASSERT_TRUE(pushed.int_val == 42, "pushed element should be 42");
    ir_value_free(&pushed);

    ir_value_free(&arr);
    ir_value_free(&elem);
    ir_value_free(&result);
    ir_builtin_registry_destroy(registry);
    PASS();
}

static void test_array_pop(void) {
    TEST("array_pop");

    IRBuiltinRegistry* registry = ir_builtin_registry_create();

    IRValue arr;
    arr.type = VAR_TYPE_ARRAY;
    arr.array_val = ir_array_create(3);
    ir_array_push(arr.array_val, ir_value_int(1));
    ir_array_push(arr.array_val, ir_value_int(2));

    IRValue args[1] = { arr };
    IRValue result = ir_builtin_call(registry, "array_pop", NULL, args, 1);

    ASSERT_TRUE(result.type == VAR_TYPE_INT, "should return int");
    ASSERT_TRUE(result.int_val == 2, "should return last element");
    ASSERT_TRUE(ir_array_length(arr.array_val) == 1, "array should have 1 element left");

    ir_value_free(&arr);
    ir_value_free(&result);
    ir_builtin_registry_destroy(registry);
    PASS();
}

static void test_array_indexOf(void) {
    TEST("array_indexOf");

    IRBuiltinRegistry* registry = ir_builtin_registry_create();

    IRValue arr;
    arr.type = VAR_TYPE_ARRAY;
    arr.array_val = ir_array_create(3);
    ir_array_push(arr.array_val, ir_value_int(10));
    ir_array_push(arr.array_val, ir_value_int(20));
    ir_array_push(arr.array_val, ir_value_int(30));

    IRValue search = ir_value_int(20);
    IRValue args[2] = { arr, search };
    IRValue result = ir_builtin_call(registry, "array_indexOf", NULL, args, 2);

    ASSERT_TRUE(result.type == VAR_TYPE_INT, "should return int");
    ASSERT_TRUE(result.int_val == 1, "should find element at index 1");

    ir_value_free(&arr);
    ir_value_free(&search);
    ir_value_free(&result);
    ir_builtin_registry_destroy(registry);
    PASS();
}

static void test_array_join(void) {
    TEST("array_join");

    IRBuiltinRegistry* registry = ir_builtin_registry_create();

    IRValue arr;
    arr.type = VAR_TYPE_ARRAY;
    arr.array_val = ir_array_create(3);
    ir_array_push(arr.array_val, ir_value_string("a"));
    ir_array_push(arr.array_val, ir_value_string("b"));
    ir_array_push(arr.array_val, ir_value_string("c"));

    IRValue args[1] = { arr };
    IRValue result = ir_builtin_call(registry, "array_join", NULL, args, 1);

    ASSERT_TRUE(result.type == VAR_TYPE_STRING, "should return string");
    ASSERT_TRUE(strcmp(result.string_val, "a,b,c") == 0, "should join with comma");

    ir_value_free(&arr);
    ir_value_free(&result);
    ir_builtin_registry_destroy(registry);
    PASS();
}

// ============================================================================
// MATH FUNCTION TESTS
// ============================================================================

static void test_math_abs(void) {
    TEST("math_abs");

    IRBuiltinRegistry* registry = ir_builtin_registry_create();

    IRValue args[1] = { ir_value_int(-42) };
    IRValue result = ir_builtin_call(registry, "math_abs", NULL, args, 1);

    ASSERT_TRUE(result.type == VAR_TYPE_INT, "should return int");
    ASSERT_TRUE(result.int_val == 42, "should return absolute value");

    ir_value_free(&args[0]);
    ir_value_free(&result);
    ir_builtin_registry_destroy(registry);
    PASS();
}

static void test_math_min(void) {
    TEST("math_min");

    IRBuiltinRegistry* registry = ir_builtin_registry_create();

    IRValue args[3] = {
        ir_value_int(10),
        ir_value_int(5),
        ir_value_int(20)
    };
    IRValue result = ir_builtin_call(registry, "math_min", NULL, args, 3);

    ASSERT_TRUE(result.type == VAR_TYPE_INT, "should return int");
    ASSERT_TRUE(result.int_val == 5, "should return minimum");

    ir_value_free(&result);
    ir_builtin_registry_destroy(registry);
    PASS();
}

static void test_math_max(void) {
    TEST("math_max");

    IRBuiltinRegistry* registry = ir_builtin_registry_create();

    IRValue args[3] = {
        ir_value_int(10),
        ir_value_int(5),
        ir_value_int(20)
    };
    IRValue result = ir_builtin_call(registry, "math_max", NULL, args, 3);

    ASSERT_TRUE(result.type == VAR_TYPE_INT, "should return int");
    ASSERT_TRUE(result.int_val == 20, "should return maximum");

    ir_value_free(&result);
    ir_builtin_registry_destroy(registry);
    PASS();
}

static void test_math_clamp(void) {
    TEST("math_clamp");

    IRBuiltinRegistry* registry = ir_builtin_registry_create();

    IRValue args[3] = {
        ir_value_int(15),
        ir_value_int(10),
        ir_value_int(20)
    };
    IRValue result = ir_builtin_call(registry, "math_clamp", NULL, args, 3);

    ASSERT_TRUE(result.type == VAR_TYPE_INT, "should return int");
    ASSERT_TRUE(result.int_val == 15, "should keep value in range");

    // Test clamp below
    args[0].int_val = 5;
    result = ir_builtin_call(registry, "math_clamp", NULL, args, 3);
    ASSERT_TRUE(result.int_val == 10, "should clamp to min");

    // Test clamp above
    args[0].int_val = 25;
    result = ir_builtin_call(registry, "math_clamp", NULL, args, 3);
    ASSERT_TRUE(result.int_val == 20, "should clamp to max");

    ir_value_free(&result);
    ir_builtin_registry_destroy(registry);
    PASS();
}

// ============================================================================
// TYPE FUNCTION TESTS
// ============================================================================

static void test_type_toInt(void) {
    TEST("type_toInt");

    IRBuiltinRegistry* registry = ir_builtin_registry_create();

    // String to int
    IRValue args[1] = { ir_value_string("42") };
    IRValue result = ir_builtin_call(registry, "type_toInt", NULL, args, 1);
    ASSERT_TRUE(result.type == VAR_TYPE_INT, "should return int");
    ASSERT_TRUE(result.int_val == 42, "should convert string to int");

    ir_value_free(&result);

    // Bool to int
    args[0] = ir_value_bool(true);
    result = ir_builtin_call(registry, "type_toInt", NULL, args, 1);
    ASSERT_TRUE(result.int_val == 1, "true should convert to 1");

    ir_value_free(&result);
    ir_value_free(&args[0]);
    ir_builtin_registry_destroy(registry);
    PASS();
}

static void test_type_toString(void) {
    TEST("type_toString");

    IRBuiltinRegistry* registry = ir_builtin_registry_create();

    IRValue args[1] = { ir_value_int(42) };
    IRValue result = ir_builtin_call(registry, "type_toString", NULL, args, 1);

    ASSERT_TRUE(result.type == VAR_TYPE_STRING, "should return string");
    ASSERT_TRUE(strcmp(result.string_val, "42") == 0, "should convert int to string");

    ir_value_free(&args[0]);
    ir_value_free(&result);
    ir_builtin_registry_destroy(registry);
    PASS();
}

static void test_type_typeof(void) {
    TEST("type_typeof");

    IRBuiltinRegistry* registry = ir_builtin_registry_create();

    // Test int type
    IRValue args[1] = { ir_value_int(42) };
    IRValue result = ir_builtin_call(registry, "type_typeof", NULL, args, 1);

    ASSERT_TRUE(result.type == VAR_TYPE_STRING, "should return string");
    ASSERT_TRUE(strcmp(result.string_val, "int") == 0, "type should be 'int'");

    ir_value_free(&result);

    // Test string type
    args[0] = ir_value_string("hello");
    result = ir_builtin_call(registry, "type_typeof", NULL, args, 1);
    ASSERT_TRUE(strcmp(result.string_val, "string") == 0, "type should be 'string'");

    ir_value_free(&result);
    ir_value_free(&args[0]);
    ir_builtin_registry_destroy(registry);
    PASS();
}

// ============================================================================
// ERROR HANDLING TESTS
// ============================================================================

static void test_unknown_function(void) {
    TEST("unknown_function");

    IRBuiltinRegistry* registry = ir_builtin_registry_create();

    IRValue args[1] = { ir_value_int(42) };
    IRValue result = ir_builtin_call(registry, "unknown_func_xyz", NULL, args, 1);

    ASSERT_TRUE(result.type == VAR_TYPE_NULL, "should return null for unknown function");

    ir_value_free(&args[0]);
    ir_builtin_registry_destroy(registry);
    PASS();
}

static void test_wrong_arg_count(void) {
    TEST("wrong_arg_count");

    IRBuiltinRegistry* registry = ir_builtin_registry_create();

    // string_toUpper requires 1 arg, give 0
    IRValue result = ir_builtin_call(registry, "string_toUpper", NULL, NULL, 0);
    ASSERT_TRUE(result.type == VAR_TYPE_NULL, "should return null for wrong arg count");

    ir_builtin_registry_destroy(registry);
    PASS();
}

// ============================================================================
// MAIN TEST RUNNER
// ============================================================================

int main(void) {
    printf("============================================================================\n");
    printf("BUILTIN FUNCTION TEST\n");
    printf("============================================================================\n\n");

    // Registry tests
    printf("\nRegistry Tests:\n");
    test_registry_create_destroy();
    test_registry_lookup();

    // String function tests
    printf("\nString Function Tests:\n");
    test_string_toUpper();
    test_string_toLower();
    test_string_trim();
    test_string_substring();
    test_string_split();
    test_string_length();

    // Array function tests
    printf("\nArray Function Tests:\n");
    test_array_length();
    test_array_push();
    test_array_pop();
    test_array_indexOf();
    test_array_join();

    // Math function tests
    printf("\nMath Function Tests:\n");
    test_math_abs();
    test_math_min();
    test_math_max();
    test_math_clamp();

    // Type function tests
    printf("\nType Function Tests:\n");
    test_type_toInt();
    test_type_toString();
    test_type_typeof();

    // Error handling tests
    printf("\nError Handling Tests:\n");
    test_unknown_function();
    test_wrong_arg_count();

    printf("\n============================================================================\n");
    printf("ALL TESTS PASSED\n");
    printf("============================================================================\n");

    return 0;
}
