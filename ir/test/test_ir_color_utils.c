/**
 * @file test_ir_color_utils.c
 * @brief Tests for color utility functions
 */

#define _POSIX_C_SOURCE 200809L
#include "../tests/test_framework.h"
#include "ir_color_utils.h"
#include "ir_style_types.h"
#include <string.h>

// ============================================================================
// TEST CASES
// ============================================================================

TEST(test_color_rgb_creates_solid_color) {
    IRColor color = ir_color_rgb(255, 128, 64);

    ASSERT_EQ(IR_COLOR_SOLID, color.type);
    ASSERT_EQ(255, color.data.r);
    ASSERT_EQ(128, color.data.g);
    ASSERT_EQ(64, color.data.b);
    ASSERT_EQ(255, color.data.a);  // Alpha should be 255 (solid)
}

TEST(test_color_rgb_black) {
    IRColor color = ir_color_rgb(0, 0, 0);

    ASSERT_EQ(0, color.data.r);
    ASSERT_EQ(0, color.data.g);
    ASSERT_EQ(0, color.data.b);
    ASSERT_EQ(255, color.data.a);
}

TEST(test_color_rgb_white) {
    IRColor color = ir_color_rgb(255, 255, 255);

    ASSERT_EQ(255, color.data.r);
    ASSERT_EQ(255, color.data.g);
    ASSERT_EQ(255, color.data.b);
    ASSERT_EQ(255, color.data.a);
}

TEST(test_color_transparent) {
    IRColor color = ir_color_transparent();

    // ir_color_transparent returns RGBA with alpha 0, not IR_COLOR_TRANSPARENT type
    ASSERT_EQ(0, color.data.a);
}

TEST(test_color_named_red) {
    IRColor color = ir_color_named("red");

    ASSERT_EQ(255, color.data.r);
    ASSERT_EQ(0, color.data.g);
    ASSERT_EQ(0, color.data.b);
    ASSERT_EQ(255, color.data.a);
}

TEST(test_color_named_green) {
    IRColor color = ir_color_named("green");

    // Named green is (0, 255, 0) - full green, not CSS green (0, 128, 0)
    ASSERT_EQ(0, color.data.r);
    ASSERT_EQ(255, color.data.g);
    ASSERT_EQ(0, color.data.b);
    ASSERT_EQ(255, color.data.a);
}

TEST(test_color_named_blue) {
    IRColor color = ir_color_named("blue");

    ASSERT_EQ(0, color.data.r);
    ASSERT_EQ(0, color.data.g);
    ASSERT_EQ(255, color.data.b);
    ASSERT_EQ(255, color.data.a);
}

TEST(test_color_named_white) {
    IRColor color = ir_color_named("white");

    ASSERT_EQ(255, color.data.r);
    ASSERT_EQ(255, color.data.g);
    ASSERT_EQ(255, color.data.b);
    ASSERT_EQ(255, color.data.a);
}

TEST(test_color_named_black) {
    IRColor color = ir_color_named("black");

    ASSERT_EQ(0, color.data.r);
    ASSERT_EQ(0, color.data.g);
    ASSERT_EQ(0, color.data.b);
    ASSERT_EQ(255, color.data.a);
}

TEST(test_color_named_transparent) {
    IRColor color = ir_color_named("transparent");

    // Named transparent has alpha 0
    ASSERT_EQ(0, color.data.a);
}

TEST(test_color_named_unknown_defaults_to_white) {
    IRColor color = ir_color_named("unknownColorName123");

    ASSERT_EQ(255, color.data.r);
    ASSERT_EQ(255, color.data.g);
    ASSERT_EQ(255, color.data.b);
    ASSERT_EQ(255, color.data.a);
}

TEST(test_color_named_case_insensitive) {
    // Color names should work regardless of case
    IRColor color_lower = ir_color_named("red");
    IRColor color_upper = ir_color_named("RED");
    IRColor color_mixed = ir_color_named("Red");

    // All should return red
    ASSERT_EQ(255, color_lower.data.r);
    ASSERT_EQ(255, color_upper.data.r);
    ASSERT_EQ(255, color_mixed.data.r);
}

// ============================================================================
// MAIN
// ============================================================================

static void run_all_tests(void) {
    BEGIN_SUITE("IRColorUtils Tests");

    // RGB helper tests
    RUN_TEST(test_color_rgb_creates_solid_color);
    RUN_TEST(test_color_rgb_black);
    RUN_TEST(test_color_rgb_white);

    // Transparent helper tests
    RUN_TEST(test_color_transparent);

    // Named color tests - basic
    RUN_TEST(test_color_named_red);
    RUN_TEST(test_color_named_green);
    RUN_TEST(test_color_named_blue);
    RUN_TEST(test_color_named_white);
    RUN_TEST(test_color_named_black);
    RUN_TEST(test_color_named_transparent);

    // Named color tests - edge cases
    RUN_TEST(test_color_named_unknown_defaults_to_white);
    RUN_TEST(test_color_named_case_insensitive);

    END_SUITE();
}

RUN_TEST_SUITE(run_all_tests);
