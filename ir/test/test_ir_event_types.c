/**
 * @file test_ir_event_types.c
 * @brief Tests for IR event type system
 */

#define _POSIX_C_SOURCE 200809L
#include "../tests/test_framework.h"
#include "ir_event_types.h"

// ============================================================================
// TEST CASES
// ============================================================================

// Core Event Type Value Tests

TEST(test_event_click_value) {
    ASSERT_EQ(0, IR_EVENT_CLICK);
}

TEST(test_event_hover_value) {
    ASSERT_EQ(1, IR_EVENT_HOVER);
}

TEST(test_event_focus_value) {
    ASSERT_EQ(2, IR_EVENT_FOCUS);
}

TEST(test_event_blur_value) {
    ASSERT_EQ(3, IR_EVENT_BLUR);
}

TEST(test_event_text_change_value) {
    ASSERT_EQ(4, IR_EVENT_TEXT_CHANGE);
}

TEST(test_event_key_value) {
    ASSERT_EQ(5, IR_EVENT_KEY);
}

TEST(test_event_scroll_value) {
    ASSERT_EQ(6, IR_EVENT_SCROLL);
}

TEST(test_event_timer_value) {
    ASSERT_EQ(7, IR_EVENT_TIMER);
}

TEST(test_event_custom_value) {
    ASSERT_EQ(8, IR_EVENT_CUSTOM);
}

// Plugin Event Range Tests

TEST(test_event_plugin_start_value) {
    ASSERT_EQ(100, IR_EVENT_PLUGIN_START);
}

TEST(test_event_plugin_end_value) {
    ASSERT_EQ(255, IR_EVENT_PLUGIN_END);
}

TEST(test_event_plugin_range_size) {
    int range = IR_EVENT_PLUGIN_END - IR_EVENT_PLUGIN_START + 1;
    ASSERT_EQ(156, range);  // 255 - 100 + 1 = 156 plugin events
}

// to_string Tests

TEST(test_event_to_string_click) {
    const char* str = ir_event_type_to_string(IR_EVENT_CLICK);
    ASSERT_NONNULL(str);
    ASSERT_STR_EQ("Click", str);
}

TEST(test_event_to_string_hover) {
    const char* str = ir_event_type_to_string(IR_EVENT_HOVER);
    ASSERT_NONNULL(str);
    ASSERT_STR_EQ("Hover", str);
}

TEST(test_event_to_string_focus) {
    const char* str = ir_event_type_to_string(IR_EVENT_FOCUS);
    ASSERT_NONNULL(str);
    ASSERT_STR_EQ("Focus", str);
}

TEST(test_event_to_string_blur) {
    const char* str = ir_event_type_to_string(IR_EVENT_BLUR);
    ASSERT_NONNULL(str);
    ASSERT_STR_EQ("Blur", str);
}

// ============================================================================
// MAIN
// ============================================================================

static void run_all_tests(void) {
    BEGIN_SUITE("IREventTypes Tests");

    // Core event type value tests
    RUN_TEST(test_event_click_value);
    RUN_TEST(test_event_hover_value);
    RUN_TEST(test_event_focus_value);
    RUN_TEST(test_event_blur_value);
    RUN_TEST(test_event_text_change_value);
    RUN_TEST(test_event_key_value);
    RUN_TEST(test_event_scroll_value);
    RUN_TEST(test_event_timer_value);
    RUN_TEST(test_event_custom_value);

    // Plugin event range tests
    RUN_TEST(test_event_plugin_start_value);
    RUN_TEST(test_event_plugin_end_value);
    RUN_TEST(test_event_plugin_range_size);

    // to_string tests
    RUN_TEST(test_event_to_string_click);
    RUN_TEST(test_event_to_string_hover);
    RUN_TEST(test_event_to_string_focus);
    RUN_TEST(test_event_to_string_blur);

    END_SUITE();
}

RUN_TEST_SUITE(run_all_tests);
