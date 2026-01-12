// ============================================================================
// EXPRESSION ENGINE PHASE 1 - Unit Tests
// ============================================================================
// Tests for new expression types and IRValue/IRObject helpers

#define _POSIX_C_SOURCE 200809L
#include "ir_expression.h"
#include "ir_expression_compiler.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

// Test counters
static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST_START(name) \
    do { \
        tests_run++; \
        printf("  Testing: %s...", name); \
        fflush(stdout); \
    } while(0)

#define TEST_PASS() \
    do { \
        tests_passed++; \
        printf(" PASS\n"); \
    } while(0)

#define TEST_FAIL(msg) \
    do { \
        tests_failed++; \
        printf(" FAIL: %s\n", msg); \
    } while(0)

#define ASSERT_TRUE(cond, msg) \
    do { \
        if (!(cond)) { \
            TEST_FAIL(msg); \
            return; \
        } \
    } while(0)

#define ASSERT_FALSE(cond, msg) ASSERT_TRUE(!(cond), msg)
#define ASSERT_EQ(a, b, msg) ASSERT_TRUE((a) == (b), msg)
#define ASSERT_STR_EQ(a, b, msg) ASSERT_TRUE(strcmp(a, b) == 0, msg)
#define ASSERT_NULL(ptr, msg) ASSERT_TRUE((ptr) == NULL, msg)
#define ASSERT_NOT_NULL(ptr, msg) ASSERT_TRUE((ptr) != NULL, msg)

// ============================================================================
// IRValue TESTS
// ============================================================================

static void test_ir_value_null(void) {
    TEST_START("ir_value_null");
    IRValue v = ir_value_null();
    ASSERT_EQ(v.type, VAR_TYPE_NULL, "null value type");
    TEST_PASS();
}

static void test_ir_value_int(void) {
    TEST_START("ir_value_int");
    IRValue v = ir_value_int(42);
    ASSERT_EQ(v.type, VAR_TYPE_INT, "int value type");
    ASSERT_EQ(v.int_val, 42, "int value");
    TEST_PASS();
}

static void test_ir_value_float(void) {
    TEST_START("ir_value_float");
    IRValue v = ir_value_float(3.14);
    ASSERT_EQ(v.type, VAR_TYPE_FLOAT, "float value type");
    ASSERT_TRUE(v.float_val > 3.13 && v.float_val < 3.15, "float value");
    TEST_PASS();
}

static void test_ir_value_string(void) {
    TEST_START("ir_value_string");
    IRValue v = ir_value_string("hello");
    ASSERT_EQ(v.type, VAR_TYPE_STRING, "string value type");
    ASSERT_NOT_NULL(v.string_val, "string value not null");
    ASSERT_STR_EQ(v.string_val, "hello", "string value");
    ir_value_free(&v);
    TEST_PASS();
}

static void test_ir_value_bool(void) {
    TEST_START("ir_value_bool");
    IRValue v1 = ir_value_bool(true);
    IRValue v2 = ir_value_bool(false);
    ASSERT_EQ(v1.type, VAR_TYPE_BOOL, "bool value type");
    ASSERT_EQ(v1.bool_val, true, "bool true value");
    ASSERT_EQ(v2.bool_val, false, "bool false value");
    TEST_PASS();
}

static void test_ir_value_copy(void) {
    TEST_START("ir_value_copy");

    // Test int copy
    IRValue v1 = ir_value_int(42);
    IRValue v1_copy = ir_value_copy(&v1);
    ASSERT_EQ(v1_copy.type, VAR_TYPE_INT, "copied int type");
    ASSERT_EQ(v1_copy.int_val, 42, "copied int value");

    // Test string copy (deep copy)
    IRValue v2 = ir_value_string("test");
    IRValue v2_copy = ir_value_copy(&v2);
    ASSERT_EQ(v2_copy.type, VAR_TYPE_STRING, "copied string type");
    ASSERT_STR_EQ(v2_copy.string_val, "test", "copied string value");
    ASSERT_TRUE(v2.string_val != v2_copy.string_val, "string deep copy");
    ir_value_free(&v2);
    ir_value_free(&v2_copy);

    TEST_PASS();
}

static void test_ir_value_equals(void) {
    TEST_START("ir_value_equals");

    IRValue v1 = ir_value_int(42);
    IRValue v2 = ir_value_int(42);
    IRValue v3 = ir_value_int(43);

    ASSERT_TRUE(ir_value_equals(&v1, &v2), "equal ints");
    ASSERT_FALSE(ir_value_equals(&v1, &v3), "unequal ints");

    IRValue s1 = ir_value_string("hello");
    IRValue s2 = ir_value_string("hello");
    IRValue s3 = ir_value_string("world");

    ASSERT_TRUE(ir_value_equals(&s1, &s2), "equal strings");
    ASSERT_FALSE(ir_value_equals(&s1, &s3), "unequal strings");
    ir_value_free(&s1);
    ir_value_free(&s2);
    ir_value_free(&s3);

    TEST_PASS();
}

static void test_ir_value_is_truthy(void) {
    TEST_START("ir_value_is_truthy");

    IRValue v;

    v = ir_value_int(1);
    ASSERT_TRUE(ir_value_is_truthy(&v), "int 1 is truthy");

    v = ir_value_int(0);
    ASSERT_FALSE(ir_value_is_truthy(&v), "int 0 is falsey");

    v = ir_value_bool(true);
    ASSERT_TRUE(ir_value_is_truthy(&v), "bool true is truthy");

    v = ir_value_bool(false);
    ASSERT_FALSE(ir_value_is_truthy(&v), "bool false is falsey");

    v = ir_value_string("text");
    ASSERT_TRUE(ir_value_is_truthy(&v), "non-empty string is truthy");
    ir_value_free(&v);

    v = ir_value_string("");
    ASSERT_FALSE(ir_value_is_truthy(&v), "empty string is falsey");
    ir_value_free(&v);

    v = ir_value_null();
    ASSERT_FALSE(ir_value_is_truthy(&v), "null is falsey");

    TEST_PASS();
}

// ============================================================================
// IRObject TESTS
// ============================================================================

static void test_ir_object_create(void) {
    TEST_START("ir_object_create");

    IRObject* obj = ir_object_create(4);
    ASSERT_NOT_NULL(obj, "object created");
    ASSERT_EQ(obj->capacity, 4, "object capacity");
    ASSERT_EQ(obj->count, 0, "object initial count");
    ir_object_free(obj);

    TEST_PASS();
}

static void test_ir_object_set_get(void) {
    TEST_START("ir_object_set_get");

    IRObject* obj = ir_object_create(4);
    ir_object_set(obj, "name", ir_value_string("Alice"));
    ir_object_set(obj, "age", ir_value_int(30));

    ASSERT_EQ(obj->count, 2, "object count after sets");

    IRValue name = ir_object_get(obj, "name");
    ASSERT_EQ(name.type, VAR_TYPE_STRING, "name type");
    ASSERT_STR_EQ(name.string_val, "Alice", "name value");
    ir_value_free(&name);

    IRValue age = ir_object_get(obj, "age");
    ASSERT_EQ(age.type, VAR_TYPE_INT, "age type");
    ASSERT_EQ(age.int_val, 30, "age value");
    ir_value_free(&age);

    ir_object_free(obj);

    TEST_PASS();
}

static void test_ir_object_has(void) {
    TEST_START("ir_object_has");

    IRObject* obj = ir_object_create(4);
    ir_object_set(obj, "exists", ir_value_int(1));

    ASSERT_TRUE(ir_object_has(obj, "exists"), "has existing key");
    ASSERT_FALSE(ir_object_has(obj, "missing"), "doesn't have missing key");

    ir_object_free(obj);

    TEST_PASS();
}

static void test_ir_object_update(void) {
    TEST_START("ir_object_update");

    IRObject* obj = ir_object_create(4);
    ir_object_set(obj, "key", ir_value_int(1));
    ir_object_set(obj, "key", ir_value_int(2));  // Update

    IRValue val = ir_object_get(obj, "key");
    ASSERT_EQ(val.int_val, 2, "updated value");
    ASSERT_EQ(obj->count, 1, "count unchanged after update");
    ir_value_free(&val);

    ir_object_free(obj);

    TEST_PASS();
}

static void test_ir_object_delete(void) {
    TEST_START("ir_object_delete");

    IRObject* obj = ir_object_create(4);
    ir_object_set(obj, "key1", ir_value_int(1));
    ir_object_set(obj, "key2", ir_value_int(2));

    ASSERT_TRUE(ir_object_delete(obj, "key1"), "delete success");
    ASSERT_EQ(obj->count, 1, "count after delete");
    ASSERT_FALSE(ir_object_has(obj, "key1"), "key deleted");
    ASSERT_FALSE(ir_object_delete(obj, "missing"), "delete missing returns false");

    ir_object_free(obj);

    TEST_PASS();
}

static void test_ir_object_keys(void) {
    TEST_START("ir_object_keys");

    IRObject* obj = ir_object_create(4);
    ir_object_set(obj, "a", ir_value_int(1));
    ir_object_set(obj, "b", ir_value_int(2));
    ir_object_set(obj, "c", ir_value_int(3));

    uint32_t count = 0;
    char** keys = ir_object_keys(obj, &count);

    ASSERT_EQ(count, 3, "keys count");
    ASSERT_NOT_NULL(keys, "keys array");

    // Free keys
    for (uint32_t i = 0; i < count; i++) {
        free(keys[i]);
    }
    free(keys);

    ir_object_free(obj);

    TEST_PASS();
}

// ============================================================================
// IRArray TESTS
// ============================================================================

static void test_ir_array_create(void) {
    TEST_START("ir_array_create");

    IRArray* arr = ir_array_create(4);
    ASSERT_NOT_NULL(arr, "array created");
    ASSERT_EQ(arr->capacity, 4, "array capacity");
    ASSERT_EQ(arr->count, 0, "array initial count");
    ir_array_free(arr);

    TEST_PASS();
}

static void test_ir_array_push_pop(void) {
    TEST_START("ir_array_push_pop");

    IRArray* arr = ir_array_create(4);
    ir_array_push(arr, ir_value_int(1));
    ir_array_push(arr, ir_value_int(2));
    ir_array_push(arr, ir_value_int(3));

    ASSERT_EQ(arr->count, 3, "count after pushes");

    IRValue v3 = ir_array_pop(arr);
    ASSERT_EQ(v3.type, VAR_TYPE_INT, "popped value type");
    ASSERT_EQ(v3.int_val, 3, "popped value");
    ASSERT_EQ(arr->count, 2, "count after pop");
    ir_value_free(&v3);

    ir_array_free(arr);

    TEST_PASS();
}

static void test_ir_array_get_set(void) {
    TEST_START("ir_array_get_set");

    IRArray* arr = ir_array_create(4);
    ir_array_push(arr, ir_value_int(1));
    ir_array_push(arr, ir_value_int(2));
    ir_array_push(arr, ir_value_int(3));

    IRValue v = ir_array_get(arr, 1);
    ASSERT_EQ(v.int_val, 2, "get value at index");
    ir_value_free(&v);

    ir_array_set(arr, 1, ir_value_int(20));
    v = ir_array_get(arr, 1);
    ASSERT_EQ(v.int_val, 20, "set value at index");
    ir_value_free(&v);

    ir_array_free(arr);

    TEST_PASS();
}

static void test_ir_array_length(void) {
    TEST_START("ir_array_length");

    IRArray* arr = ir_array_create(4);
    ASSERT_EQ(ir_array_length(arr), 0, "initial length");

    ir_array_push(arr, ir_value_int(1));
    ir_array_push(arr, ir_value_int(2));

    ASSERT_EQ(ir_array_length(arr), 2, "length after pushes");

    ir_array_free(arr);

    TEST_PASS();
}

// ============================================================================
// NEW EXPRESSION TYPE TESTS
// ============================================================================

static void test_expr_member_access(void) {
    TEST_START("expr_member_access");

    IRExpression* obj = ir_expr_var("user");
    IRExpression* expr = ir_expr_member_access(obj, "name");

    ASSERT_EQ(expr->type, EXPR_MEMBER_ACCESS, "expression type");
    ASSERT_NOT_NULL(expr->member_access.object, "object not null");
    ASSERT_STR_EQ(expr->member_access.property, "name", "property name");
    ASSERT_TRUE(expr->member_access.property_hash != 0, "hash computed");

    ir_expr_free(expr);

    TEST_PASS();
}

static void test_expr_computed_member(void) {
    TEST_START("expr_computed_member");

    IRExpression* obj = ir_expr_var("items");
    IRExpression* key = ir_expr_var("index");
    IRExpression* expr = ir_expr_computed_member(obj, key);

    ASSERT_EQ(expr->type, EXPR_COMPUTED_MEMBER, "expression type");
    ASSERT_NOT_NULL(expr->computed_member.object, "object not null");
    ASSERT_NOT_NULL(expr->computed_member.key, "key not null");

    ir_expr_free(expr);

    TEST_PASS();
}

static void test_expr_method_call(void) {
    TEST_START("expr_method_call");

    IRExpression* receiver = ir_expr_var("array");
    IRExpression* args[] = { ir_expr_int(1), ir_expr_int(2) };
    IRExpression* expr = ir_expr_method_call(receiver, "push", args, 2);

    ASSERT_EQ(expr->type, EXPR_METHOD_CALL, "expression type");
    ASSERT_NOT_NULL(expr->method_call.receiver, "receiver not null");
    ASSERT_STR_EQ(expr->method_call.method_name, "push", "method name");
    ASSERT_EQ(expr->method_call.arg_count, 2, "arg count");

    ir_expr_free(expr);

    TEST_PASS();
}

static void test_expr_group(void) {
    TEST_START("expr_group");

    IRExpression* inner = ir_expr_int(42);
    IRExpression* expr = ir_expr_group(inner);

    ASSERT_EQ(expr->type, EXPR_GROUP, "expression type");
    ASSERT_NOT_NULL(expr->group.inner, "inner not null");

    ir_expr_free(expr);

    TEST_PASS();
}

// ============================================================================
// JSON SERIALIZATION TESTS
// ============================================================================

static void test_json_member_access(void) {
    TEST_START("json_member_access");

    IRExpression* original = ir_expr_member_access(ir_expr_var("obj"), "prop");

    cJSON* json = ir_expr_to_json(original);
    ASSERT_NOT_NULL(json, "json not null");

    IRExpression* restored = ir_expr_from_json(json);
    ASSERT_NOT_NULL(restored, "restored not null");
    ASSERT_EQ(restored->type, EXPR_MEMBER_ACCESS, "restored type");

    cJSON_Delete(json);
    ir_expr_free(original);
    ir_expr_free(restored);

    TEST_PASS();
}

static void test_json_computed_member(void) {
    TEST_START("json_computed_member");

    IRExpression* original = ir_expr_computed_member(
        ir_expr_var("arr"),
        ir_expr_int(0)
    );

    cJSON* json = ir_expr_to_json(original);
    ASSERT_NOT_NULL(json, "json not null");

    IRExpression* restored = ir_expr_from_json(json);
    ASSERT_NOT_NULL(restored, "restored not null");
    ASSERT_EQ(restored->type, EXPR_COMPUTED_MEMBER, "restored type");

    cJSON_Delete(json);
    ir_expr_free(original);
    ir_expr_free(restored);

    TEST_PASS();
}

static void test_json_method_call(void) {
    TEST_START("json_method_call");

    IRExpression* args[] = { ir_expr_int(1) };
    IRExpression* original = ir_expr_method_call(
        ir_expr_var("obj"),
        "method",
        args,
        1
    );

    cJSON* json = ir_expr_to_json(original);
    ASSERT_NOT_NULL(json, "json not null");

    IRExpression* restored = ir_expr_from_json(json);
    ASSERT_NOT_NULL(restored, "restored not null");
    ASSERT_EQ(restored->type, EXPR_METHOD_CALL, "restored type");

    cJSON_Delete(json);
    ir_expr_free(original);
    ir_expr_free(restored);

    TEST_PASS();
}

static void test_json_group(void) {
    TEST_START("json_group");

    IRExpression* original = ir_expr_group(ir_expr_int(42));

    cJSON* json = ir_expr_to_json(original);
    ASSERT_NOT_NULL(json, "json not null");

    IRExpression* restored = ir_expr_from_json(json);
    ASSERT_NOT_NULL(restored, "restored not null");
    ASSERT_EQ(restored->type, EXPR_GROUP, "restored type");

    cJSON_Delete(json);
    ir_expr_free(original);
    ir_expr_free(restored);

    TEST_PASS();
}

// ============================================================================
// HASH TESTS
// ============================================================================

static void test_ir_hash_string(void) {
    TEST_START("ir_hash_string");

    uint64_t h1 = ir_hash_string("hello");
    uint64_t h2 = ir_hash_string("hello");
    uint64_t h3 = ir_hash_string("world");

    ASSERT_EQ(h1, h2, "same string same hash");
    ASSERT_TRUE(h1 != h3, "different strings different hash");

    TEST_PASS();
}

static void test_ir_next_power_of_2(void) {
    TEST_START("ir_next_power_of_2");

    ASSERT_EQ(ir_next_power_of_2(0), 1, "0 -> 1");
    ASSERT_EQ(ir_next_power_of_2(1), 1, "1 -> 1");
    ASSERT_EQ(ir_next_power_of_2(5), 8, "5 -> 8");
    ASSERT_EQ(ir_next_power_of_2(16), 16, "16 -> 16");
    ASSERT_EQ(ir_next_power_of_2(17), 32, "17 -> 32");

    TEST_PASS();
}

// ============================================================================
// MAIN
// ============================================================================

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    printf("============================================================================\n");
    printf("EXPRESSION ENGINE PHASE 1 - Unit Tests\n");
    printf("============================================================================\n\n");

    printf("IRValue Tests:\n");
    test_ir_value_null();
    test_ir_value_int();
    test_ir_value_float();
    test_ir_value_string();
    test_ir_value_bool();
    test_ir_value_copy();
    test_ir_value_equals();
    test_ir_value_is_truthy();

    printf("\nIRObject Tests:\n");
    test_ir_object_create();
    test_ir_object_set_get();
    test_ir_object_has();
    test_ir_object_update();
    test_ir_object_delete();
    test_ir_object_keys();

    printf("\nIRArray Tests:\n");
    test_ir_array_create();
    test_ir_array_push_pop();
    test_ir_array_get_set();
    test_ir_array_length();

    printf("\nNew Expression Type Tests:\n");
    test_expr_member_access();
    test_expr_computed_member();
    test_expr_method_call();
    test_expr_group();

    printf("\nJSON Serialization Tests:\n");
    test_json_member_access();
    test_json_computed_member();
    test_json_method_call();
    test_json_group();

    printf("\nUtility Tests:\n");
    test_ir_hash_string();
    test_ir_next_power_of_2();

    printf("\n============================================================================\n");
    printf("RESULTS: %d total, %d passed, %d failed\n", tests_run, tests_passed, tests_failed);
    printf("============================================================================\n");

    return tests_failed > 0 ? 1 : 0;
}
