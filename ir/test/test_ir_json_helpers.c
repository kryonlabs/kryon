/**
 * @file test_ir_json_helpers.c
 * @brief Tests for cJSON helper wrappers
 */

#define _POSIX_C_SOURCE 200809L
#include "../tests/test_framework.h"
#include "ir_json_helpers.h"
#include <string.h>

// ============================================================================
// TEST CASES
// ============================================================================

TEST(test_json_add_string_or_null_with_valid_string) {
    cJSON* obj = cJSON_CreateObject();

    cJSON_AddStringOrNull(obj, "key", "value");

    const char* result = cJSON_GetObjectItemCaseSensitive(obj, "key")->valuestring;
    ASSERT_STR_EQ("value", result);

    cJSON_Delete(obj);
}

TEST(test_json_add_string_or_null_with_null) {
    cJSON* obj = cJSON_CreateObject();

    cJSON_AddStringOrNull(obj, "key", NULL);

    // When value is NULL, a null node is added (not skipped)
    cJSON* item = cJSON_GetObjectItemCaseSensitive(obj, "key");
    ASSERT_NONNULL(item);
    ASSERT_EQ(cJSON_NULL, item->type);

    cJSON_Delete(obj);
}

TEST(test_json_create_string_or_null_with_valid_string) {
    cJSON* node = cJSON_CreateStringOrNull("test");

    ASSERT_NONNULL(node);
    ASSERT_EQ(cJSON_String, node->type);
    ASSERT_STR_EQ("test", node->valuestring);

    cJSON_Delete(node);
}

TEST(test_json_create_string_or_null_with_null) {
    cJSON* node = cJSON_CreateStringOrNull(NULL);

    ASSERT_NONNULL(node);
    ASSERT_EQ(cJSON_NULL, node->type);

    cJSON_Delete(node);
}

TEST(test_json_get_string_safe_existing) {
    cJSON* obj = cJSON_CreateObject();
    cJSON_AddStringToObject(obj, "key", "value");

    const char* result = cJSON_GetStringSafe(obj, "key");
    ASSERT_STR_EQ("value", result);

    cJSON_Delete(obj);
}

TEST(test_json_get_string_safe_missing) {
    cJSON* obj = cJSON_CreateObject();

    const char* result = cJSON_GetStringSafe(obj, "missing");
    ASSERT_NULL(result);

    cJSON_Delete(obj);
}

TEST(test_json_get_string_safe_wrong_type) {
    cJSON* obj = cJSON_CreateObject();
    cJSON_AddNumberToObject(obj, "key", 42);

    const char* result = cJSON_GetStringSafe(obj, "key");
    ASSERT_NULL(result);

    cJSON_Delete(obj);
}

TEST(test_json_get_number_safe_existing) {
    cJSON* obj = cJSON_CreateObject();
    cJSON_AddNumberToObject(obj, "key", 42.5);

    double result = cJSON_GetNumberSafe(obj, "key", 0.0);
    ASSERT_TRUE(result > 42.0 && result < 43.0);

    cJSON_Delete(obj);
}

TEST(test_json_get_number_safe_missing_with_default) {
    cJSON* obj = cJSON_CreateObject();

    double result = cJSON_GetNumberSafe(obj, "missing", 99.9);
    ASSERT_TRUE(result > 99.0 && result < 100.0);

    cJSON_Delete(obj);
}

TEST(test_json_get_number_safe_wrong_type_with_default) {
    cJSON* obj = cJSON_CreateObject();
    cJSON_AddStringToObject(obj, "key", "not a number");

    double result = cJSON_GetNumberSafe(obj, "key", 123.0);
    ASSERT_TRUE(result > 122.0 && result < 124.0);

    cJSON_Delete(obj);
}

TEST(test_json_get_bool_safe_true) {
    cJSON* obj = cJSON_CreateObject();
    cJSON_AddBoolToObject(obj, "key", 1);

    bool result = cJSON_GetBoolSafe(obj, "key", false);
    ASSERT_TRUE(result == true);

    cJSON_Delete(obj);
}

TEST(test_json_get_bool_safe_false) {
    cJSON* obj = cJSON_CreateObject();
    cJSON_AddBoolToObject(obj, "key", 0);

    bool result = cJSON_GetBoolSafe(obj, "key", true);
    ASSERT_TRUE(result == false);

    cJSON_Delete(obj);
}

TEST(test_json_get_bool_safe_missing_with_default) {
    cJSON* obj = cJSON_CreateObject();

    bool result = cJSON_GetBoolSafe(obj, "missing", true);
    ASSERT_TRUE(result == true);

    result = cJSON_GetBoolSafe(obj, "missing", false);
    ASSERT_TRUE(result == false);

    cJSON_Delete(obj);
}

TEST(test_json_get_bool_safe_wrong_type_with_default) {
    cJSON* obj = cJSON_CreateObject();
    cJSON_AddStringToObject(obj, "key", "not a bool");

    bool result = cJSON_GetBoolSafe(obj, "key", true);
    ASSERT_TRUE(result == true);

    cJSON_Delete(obj);
}

TEST(test_json_add_string_formatted) {
    cJSON* obj = cJSON_CreateObject();

    cJSON_AddStringFormatted(obj, "key", "value: %d", 42);

    const char* result = cJSON_GetObjectItemCaseSensitive(obj, "key")->valuestring;
    ASSERT_STR_EQ("value: 42", result);

    cJSON_Delete(obj);
}

TEST(test_json_add_string_or_null_empty_string) {
    cJSON* obj = cJSON_CreateObject();

    // Empty string is valid, should be added
    cJSON_AddStringOrNull(obj, "key", "");

    cJSON* item = cJSON_GetObjectItemCaseSensitive(obj, "key");
    ASSERT_NONNULL(item);
    ASSERT_STR_EQ("", item->valuestring);

    cJSON_Delete(obj);
}

// ============================================================================
// MAIN
// ============================================================================

static void run_all_tests(void) {
    BEGIN_SUITE("IRJsonHelpers Tests");

    // AddStringOrNull tests
    RUN_TEST(test_json_add_string_or_null_with_valid_string);
    RUN_TEST(test_json_add_string_or_null_with_null);
    RUN_TEST(test_json_add_string_or_null_empty_string);

    // CreateStringOrNull tests
    RUN_TEST(test_json_create_string_or_null_with_valid_string);
    RUN_TEST(test_json_create_string_or_null_with_null);

    // GetStringSafe tests
    RUN_TEST(test_json_get_string_safe_existing);
    RUN_TEST(test_json_get_string_safe_missing);
    RUN_TEST(test_json_get_string_safe_wrong_type);

    // GetNumberSafe tests
    RUN_TEST(test_json_get_number_safe_existing);
    RUN_TEST(test_json_get_number_safe_missing_with_default);
    RUN_TEST(test_json_get_number_safe_wrong_type_with_default);

    // GetBoolSafe tests
    RUN_TEST(test_json_get_bool_safe_true);
    RUN_TEST(test_json_get_bool_safe_false);
    RUN_TEST(test_json_get_bool_safe_missing_with_default);
    RUN_TEST(test_json_get_bool_safe_wrong_type_with_default);

    // AddStringFormatted tests
    RUN_TEST(test_json_add_string_formatted);

    END_SUITE();
}

RUN_TEST_SUITE(run_all_tests);
