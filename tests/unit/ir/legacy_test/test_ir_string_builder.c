/**
 * @file test_ir_string_builder.c
 * @brief Tests for IRStringBuilder - dynamic string concatenation
 */

#define _POSIX_C_SOURCE 200809L
#include "../tests/test_framework.h"
#include "ir_string_builder.h"
#include <string.h>

// ============================================================================
// TEST CASES
// ============================================================================

TEST(test_sb_create_with_capacity) {
    IRStringBuilder* sb = ir_sb_create(100);
    ASSERT_NONNULL(sb);
    ASSERT_GTE(sb->capacity, 100);
    ASSERT_EQ(0, sb->size);

    ir_sb_free(sb);
}

TEST(test_sb_create_with_zero_capacity) {
    IRStringBuilder* sb = ir_sb_create(0);
    ASSERT_NONNULL(sb);
    ASSERT_GT(sb->capacity, 0);

    ir_sb_free(sb);
}

TEST(test_sb_append_string) {
    IRStringBuilder* sb = ir_sb_create(100);
    ASSERT_NONNULL(sb);

    ASSERT_TRUE(ir_sb_append(sb, "Hello"));
    ASSERT_EQ(5, ir_sb_length(sb));

    char* result = ir_sb_build(sb);
    ASSERT_STR_EQ("Hello", result);

    // Note: ir_sb_build transfers ownership, caller must free the result
    free(result);
    // Don't call ir_sb_free after build - it would double-free
}

TEST(test_sb_append_multiple) {
    IRStringBuilder* sb = ir_sb_create(100);

    ASSERT_TRUE(ir_sb_append(sb, "Hello, "));
    ASSERT_TRUE(ir_sb_append(sb, "World!"));
    ASSERT_EQ(13, ir_sb_length(sb));

    char* result = ir_sb_build(sb);
    ASSERT_STR_EQ("Hello, World!", result);

    free(result);
}

TEST(test_sb_append_n) {
    IRStringBuilder* sb = ir_sb_create(100);

    ASSERT_TRUE(ir_sb_append_n(sb, "Hello World", 5));
    ASSERT_EQ(5, ir_sb_length(sb));

    char* result = ir_sb_build(sb);
    ASSERT_STR_EQ("Hello", result);

    free(result);
}

TEST(test_sb_appendf) {
    IRStringBuilder* sb = ir_sb_create(100);

    ASSERT_TRUE(ir_sb_appendf(sb, "%s %d", "test", 42));
    ASSERT_EQ(7, ir_sb_length(sb));

    char* result = ir_sb_build(sb);
    ASSERT_STR_EQ("test 42", result);

    free(result);
}

TEST(test_sb_append_line) {
    IRStringBuilder* sb = ir_sb_create(100);

    ASSERT_TRUE(ir_sb_append_line(sb, "line1"));
    ASSERT_TRUE(ir_sb_append_line(sb, "line2"));

    char* result = ir_sb_build(sb);
    ASSERT_STR_EQ("line1\nline2\n", result);

    free(result);
}

TEST(test_sb_append_linef) {
    IRStringBuilder* sb = ir_sb_create(100);

    ASSERT_TRUE(ir_sb_append_linef(sb, "value: %d", 42));

    char* result = ir_sb_build(sb);
    ASSERT_STR_EQ("value: 42\n", result);

    free(result);
}

TEST(test_sb_indent) {
    IRStringBuilder* sb = ir_sb_create(100);

    ASSERT_TRUE(ir_sb_indent(sb, 2));
    ASSERT_EQ(4, ir_sb_length(sb));

    char* result = ir_sb_build(sb);
    ASSERT_STR_EQ("    ", result);

    free(result);
}

TEST(test_sb_indent_zero) {
    IRStringBuilder* sb = ir_sb_create(100);

    ASSERT_TRUE(ir_sb_indent(sb, 0));
    ASSERT_EQ(0, ir_sb_length(sb));

    char* result = ir_sb_build(sb);
    ASSERT_STR_EQ("", result);

    free(result);
}

TEST(test_sb_clear) {
    IRStringBuilder* sb = ir_sb_create(100);

    ir_sb_append(sb, "some content");
    ASSERT_GT(ir_sb_length(sb), 0);

    ir_sb_clear(sb);
    ASSERT_EQ(0, ir_sb_length(sb));

    // Can still append after clear
    ASSERT_TRUE(ir_sb_append(sb, "new"));
    ASSERT_EQ(3, ir_sb_length(sb));

    ir_sb_free(sb);
}

TEST(test_sb_reserve) {
    IRStringBuilder* sb = ir_sb_create(10);

    ASSERT_TRUE(ir_sb_reserve(sb, 1000));
    ASSERT_GTE(sb->capacity, 1000);

    ir_sb_free(sb);
}

TEST(test_sb_reserve_smaller_than_current) {
    IRStringBuilder* sb = ir_sb_create(100);

    size_t original_capacity = sb->capacity;
    ASSERT_TRUE(ir_sb_reserve(sb, 50));
    ASSERT_EQ(original_capacity, sb->capacity);  // doesn't shrink

    ir_sb_free(sb);
}

TEST(test_sb_growth_on_large_append) {
    IRStringBuilder* sb = ir_sb_create(100);

    // Append more than initial capacity
    char large_str[200];
    memset(large_str, 'A', 199);
    large_str[199] = '\0';

    ASSERT_TRUE(ir_sb_append(sb, large_str));
    ASSERT_EQ(199, ir_sb_length(sb));

    char* result = ir_sb_build(sb);
    ASSERT_EQ(199, strlen(result));

    free(result);
}

TEST(test_sb_combined_operations) {
    IRStringBuilder* sb = ir_sb_create(100);

    // Combine multiple operations
    ir_sb_indent(sb, 1);
    ir_sb_append(sb, "key");
    ir_sb_append(sb, ": ");
    ir_sb_appendf(sb, "%d", 42);
    ir_sb_append_line(sb, ",");
    ir_sb_indent(sb, 2);
    ir_sb_append_line(sb, "nested");

    char* result = ir_sb_build(sb);
    const char* expected = "  key: 42,\n    nested\n";
    ASSERT_STR_EQ(expected, result);

    free(result);
}

// ============================================================================
// MAIN
// ============================================================================

static void run_all_tests(void) {
    BEGIN_SUITE("IRStringBuilder Tests");

    // Creation and destruction
    RUN_TEST(test_sb_create_with_capacity);
    RUN_TEST(test_sb_create_with_zero_capacity);

    // Append operations
    RUN_TEST(test_sb_append_string);
    RUN_TEST(test_sb_append_multiple);
    RUN_TEST(test_sb_append_n);
    RUN_TEST(test_sb_appendf);
    RUN_TEST(test_sb_append_line);
    RUN_TEST(test_sb_append_linef);

    // Indentation
    RUN_TEST(test_sb_indent);
    RUN_TEST(test_sb_indent_zero);

    // Clear and reserve
    RUN_TEST(test_sb_clear);
    RUN_TEST(test_sb_reserve);
    RUN_TEST(test_sb_reserve_smaller_than_current);

    // Growth
    RUN_TEST(test_sb_growth_on_large_append);

    // Integration
    RUN_TEST(test_sb_combined_operations);

    END_SUITE();
}

RUN_TEST_SUITE(run_all_tests);
