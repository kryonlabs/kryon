/**
 * @file test_ir_component_types.c
 * @brief Tests for IR component type system
 */

#define _POSIX_C_SOURCE 200809L
#include "../tests/test_framework.h"
#include "ir_component_types.h"
#include <string.h>

// ============================================================================
// TEST CASES
// ============================================================================

TEST(test_component_type_to_string_container) {
    const char* str = ir_component_type_to_string(IR_COMPONENT_CONTAINER);
    ASSERT_NONNULL(str);
    ASSERT_STR_EQ("Container", str);
}

TEST(test_component_type_to_string_text) {
    const char* str = ir_component_type_to_string(IR_COMPONENT_TEXT);
    ASSERT_NONNULL(str);
    ASSERT_STR_EQ("Text", str);
}

TEST(test_component_type_to_string_button) {
    const char* str = ir_component_type_to_string(IR_COMPONENT_BUTTON);
    ASSERT_NONNULL(str);
    ASSERT_STR_EQ("Button", str);
}

TEST(test_component_type_to_string_input) {
    const char* str = ir_component_type_to_string(IR_COMPONENT_INPUT);
    ASSERT_NONNULL(str);
    ASSERT_STR_EQ("Input", str);
}

TEST(test_component_type_to_string_checkbox) {
    const char* str = ir_component_type_to_string(IR_COMPONENT_CHECKBOX);
    ASSERT_NONNULL(str);
    ASSERT_STR_EQ("Checkbox", str);
}

TEST(test_component_type_to_string_dropdown) {
    const char* str = ir_component_type_to_string(IR_COMPONENT_DROPDOWN);
    ASSERT_NONNULL(str);
    ASSERT_STR_EQ("Dropdown", str);
}

TEST(test_component_type_to_string_row) {
    const char* str = ir_component_type_to_string(IR_COMPONENT_ROW);
    ASSERT_NONNULL(str);
    ASSERT_STR_EQ("Row", str);
}

TEST(test_component_type_to_string_column) {
    const char* str = ir_component_type_to_string(IR_COMPONENT_COLUMN);
    ASSERT_NONNULL(str);
    ASSERT_STR_EQ("Column", str);
}

TEST(test_component_type_to_string_center) {
    const char* str = ir_component_type_to_string(IR_COMPONENT_CENTER);
    ASSERT_NONNULL(str);
    ASSERT_STR_EQ("Center", str);
}

TEST(test_component_type_to_string_image) {
    const char* str = ir_component_type_to_string(IR_COMPONENT_IMAGE);
    ASSERT_NONNULL(str);
    ASSERT_STR_EQ("Image", str);
}

TEST(test_component_type_to_string_canvas) {
    const char* str = ir_component_type_to_string(IR_COMPONENT_CANVAS);
    ASSERT_NONNULL(str);
    ASSERT_STR_EQ("Canvas", str);
}

TEST(test_component_type_to_string_markdown) {
    const char* str = ir_component_type_to_string(IR_COMPONENT_MARKDOWN);
    ASSERT_NONNULL(str);
    ASSERT_STR_EQ("Markdown", str);
}

TEST(test_component_type_to_string_tab_group) {
    const char* str = ir_component_type_to_string(IR_COMPONENT_TAB_GROUP);
    ASSERT_NONNULL(str);
    ASSERT_STR_EQ("TabGroup", str);
}

TEST(test_component_type_to_string_modal) {
    const char* str = ir_component_type_to_string(IR_COMPONENT_MODAL);
    ASSERT_NONNULL(str);
    ASSERT_STR_EQ("Modal", str);
}

TEST(test_component_type_to_string_table) {
    const char* str = ir_component_type_to_string(IR_COMPONENT_TABLE);
    ASSERT_NONNULL(str);
    ASSERT_STR_EQ("Table", str);
}

TEST(test_component_type_to_string_heading) {
    const char* str = ir_component_type_to_string(IR_COMPONENT_HEADING);
    ASSERT_NONNULL(str);
    ASSERT_STR_EQ("Heading", str);
}

TEST(test_component_type_to_string_paragraph) {
    const char* str = ir_component_type_to_string(IR_COMPONENT_PARAGRAPH);
    ASSERT_NONNULL(str);
    ASSERT_STR_EQ("Paragraph", str);
}

TEST(test_component_type_to_string_list) {
    const char* str = ir_component_type_to_string(IR_COMPONENT_LIST);
    ASSERT_NONNULL(str);
    ASSERT_STR_EQ("List", str);
}

TEST(test_component_type_to_string_link) {
    const char* str = ir_component_type_to_string(IR_COMPONENT_LINK);
    ASSERT_NONNULL(str);
    ASSERT_STR_EQ("Link", str);
}

TEST(test_component_type_to_string_custom) {
    const char* str = ir_component_type_to_string(IR_COMPONENT_CUSTOM);
    ASSERT_NONNULL(str);
    ASSERT_STR_EQ("Custom", str);
}

TEST(test_component_type_from_string_container) {
    IRComponentType type = ir_component_type_from_string("Container");
    ASSERT_EQ(IR_COMPONENT_CONTAINER, type);
}

TEST(test_component_type_from_string_text) {
    IRComponentType type = ir_component_type_from_string("Text");
    ASSERT_EQ(IR_COMPONENT_TEXT, type);
}

TEST(test_component_type_from_string_button) {
    IRComponentType type = ir_component_type_from_string("Button");
    ASSERT_EQ(IR_COMPONENT_BUTTON, type);
}

TEST(test_component_type_from_string_unknown) {
    // Unknown types typically return a default (often CONTAINER or CUSTOM)
    IRComponentType type = ir_component_type_from_string("unknownTypeXYZ");
    // Implementation may return CONTAINER as default
    ASSERT_TRUE(type == IR_COMPONENT_CONTAINER || type == IR_COMPONENT_CUSTOM);
}

TEST(test_component_type_from_string_insensitive) {
    // Case-insensitive lookup
    IRComponentType lower = ir_component_type_from_string_insensitive("button");
    IRComponentType upper = ir_component_type_from_string_insensitive("BUTTON");
    IRComponentType mixed = ir_component_type_from_string_insensitive("Button");

    ASSERT_EQ(IR_COMPONENT_BUTTON, lower);
    ASSERT_EQ(IR_COMPONENT_BUTTON, upper);
    ASSERT_EQ(IR_COMPONENT_BUTTON, mixed);
}

// ============================================================================
// MAIN
// ============================================================================

static void run_all_tests(void) {
    BEGIN_SUITE("IRComponentTypes Tests");

    // to_string tests
    RUN_TEST(test_component_type_to_string_container);
    RUN_TEST(test_component_type_to_string_text);
    RUN_TEST(test_component_type_to_string_button);
    RUN_TEST(test_component_type_to_string_input);
    RUN_TEST(test_component_type_to_string_checkbox);
    RUN_TEST(test_component_type_to_string_dropdown);
    RUN_TEST(test_component_type_to_string_row);
    RUN_TEST(test_component_type_to_string_column);
    RUN_TEST(test_component_type_to_string_center);
    RUN_TEST(test_component_type_to_string_image);
    RUN_TEST(test_component_type_to_string_canvas);
    RUN_TEST(test_component_type_to_string_markdown);
    RUN_TEST(test_component_type_to_string_tab_group);
    RUN_TEST(test_component_type_to_string_modal);
    RUN_TEST(test_component_type_to_string_table);
    RUN_TEST(test_component_type_to_string_heading);
    RUN_TEST(test_component_type_to_string_paragraph);
    RUN_TEST(test_component_type_to_string_list);
    RUN_TEST(test_component_type_to_string_link);
    RUN_TEST(test_component_type_to_string_custom);

    // from_string tests
    RUN_TEST(test_component_type_from_string_container);
    RUN_TEST(test_component_type_from_string_text);
    RUN_TEST(test_component_type_from_string_button);
    RUN_TEST(test_component_type_from_string_unknown);
    RUN_TEST(test_component_type_from_string_insensitive);

    END_SUITE();
}

RUN_TEST_SUITE(run_all_tests);
