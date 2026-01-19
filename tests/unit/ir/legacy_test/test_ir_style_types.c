/**
 * @file test_ir_style_types.c
 * @brief Tests for IR style type system
 */

#define _POSIX_C_SOURCE 200809L
#include "../tests/test_framework.h"
#include "ir_style_types.h"
#include "ir_color_utils.h"
#include <string.h>

// ============================================================================
// TEST CASES
// ============================================================================

// Color Type Tests

TEST(test_color_type_solid_value) {
    ASSERT_EQ(0, IR_COLOR_SOLID);
}

TEST(test_color_type_transparent_value) {
    ASSERT_EQ(1, IR_COLOR_TRANSPARENT);
}

TEST(test_color_type_gradient_value) {
    ASSERT_EQ(2, IR_COLOR_GRADIENT);
}

TEST(test_color_type_var_ref_value) {
    ASSERT_EQ(3, IR_COLOR_VAR_REF);
}

// IRColor Tests

TEST(test_color_rgba_macro) {
    IRColor color = IR_COLOR_RGBA(255, 128, 64, 255);
    ASSERT_EQ(IR_COLOR_SOLID, color.type);
    ASSERT_EQ(255, color.data.r);
    ASSERT_EQ(128, color.data.g);
    ASSERT_EQ(64, color.data.b);
    ASSERT_EQ(255, color.data.a);
}

TEST(test_color_rgba_function) {
    IRColor color = ir_color_rgba(200, 100, 50, 128);
    ASSERT_EQ(IR_COLOR_SOLID, color.type);
    ASSERT_EQ(200, color.data.r);
    ASSERT_EQ(100, color.data.g);
    ASSERT_EQ(50, color.data.b);
    ASSERT_EQ(128, color.data.a);
}

TEST(test_color_equals_same) {
    IRColor color1 = IR_COLOR_RGBA(100, 100, 100, 255);
    IRColor color2 = IR_COLOR_RGBA(100, 100, 100, 255);
    ASSERT_TRUE(ir_color_equals(color1, color2));
}

TEST(test_color_equals_different) {
    IRColor color1 = IR_COLOR_RGBA(100, 100, 100, 255);
    IRColor color2 = IR_COLOR_RGBA(101, 100, 100, 255);
    ASSERT_FALSE(ir_color_equals(color1, color2));
}

TEST(test_color_equals_alpha_difference) {
    IRColor color1 = IR_COLOR_RGBA(100, 100, 100, 255);
    IRColor color2 = IR_COLOR_RGBA(100, 100, 100, 128);
    ASSERT_FALSE(ir_color_equals(color1, color2));
}

TEST(test_color_equals_different_types) {
    IRColor color1 = IR_COLOR_RGBA(100, 100, 100, 255);
    IRColor color2 = { .type = IR_COLOR_TRANSPARENT };
    ASSERT_FALSE(ir_color_equals(color1, color2));
}

TEST(test_color_var_ref_macro) {
    IRColor color = IR_COLOR_VAR(42);
    ASSERT_EQ(IR_COLOR_VAR_REF, color.type);
    ASSERT_EQ(42, color.data.var_id);
}

TEST(test_color_gradient_macro) {
    IRGradient grad = {0};
    IRColor color = IR_COLOR_GRADIENT(&grad);
    ASSERT_EQ(IR_COLOR_GRADIENT, color.type);
    // Note: gradient pointer can't be directly compared without the actual struct
}

TEST(test_color_transparent_macro) {
    IRColor color = IR_COLOR_TRANSPARENT_VALUE;
    // Type 1 = IR_COLOR_TRANSPARENT
    ASSERT_EQ(1, color.type);
    ASSERT_EQ(0, color.data.a);
}

// Gradient Type Tests

TEST(test_gradient_type_linear_value) {
    ASSERT_EQ(0, IR_GRADIENT_LINEAR);
}

TEST(test_gradient_type_radial_value) {
    ASSERT_EQ(1, IR_GRADIENT_RADIAL);
}

TEST(test_gradient_type_conic_value) {
    ASSERT_EQ(2, IR_GRADIENT_CONIC);
}

// Gradient Structure Tests

TEST(test_gradient_initial_state) {
    IRGradient grad = {0};
    ASSERT_EQ(0, grad.stop_count);
    ASSERT_EQ(0.0f, grad.angle);
}

TEST(test_gradient_max_stops) {
    // IRGradientStop stops[8] - max 8 stops
    IRGradient grad = {0};
    for (int i = 0; i < 8; i++) {
        grad.stops[i].position = (float)i / 7.0f;
        grad.stops[i].r = 255;
        grad.stops[i].g = 0;
        grad.stops[i].b = 0;
        grad.stops[i].a = 255;
    }
    grad.stop_count = 8;
    ASSERT_EQ(8, grad.stop_count);
}

TEST(test_gradient_stop_position_range) {
    IRGradientStop stop = {0};
    stop.position = 0.5f;
    ASSERT_TRUE(stop.position >= 0.0f && stop.position <= 1.0f);
}

TEST(test_gradient_stop_color_values) {
    IRGradientStop stop = { .position = 0.5f, .r = 128, .g = 64, .b = 32, .a = 255 };
    ASSERT_EQ(128, stop.r);
    ASSERT_EQ(64, stop.g);
    ASSERT_EQ(32, stop.b);
    ASSERT_EQ(255, stop.a);
}

TEST(test_gradient_center_defaults) {
    IRGradient grad = {0};
    ASSERT_EQ(0.0f, grad.center_x);
    ASSERT_EQ(0.0f, grad.center_y);
}

TEST(test_gradient_center_values) {
    IRGradient grad = {0};
    grad.center_x = 0.5f;
    grad.center_y = 0.5f;
    ASSERT_TRUE(grad.center_x > 0.49f && grad.center_x < 0.51f);
    ASSERT_TRUE(grad.center_y > 0.49f && grad.center_y < 0.51f);
}

// Color Free Tests

TEST(test_color_free_null_safe) {
    // Should handle NULL gracefully
    ir_color_free(NULL);
    // No assertion - just checking it doesn't crash
}

TEST(test_color_free_solid) {
    IRColor color = IR_COLOR_RGBA(255, 0, 0, 255);
    ir_color_free(&color);
    // Should handle solid color (no dynamic allocation)
}

TEST(test_color_free_with_var_name) {
    IRColor color = IR_COLOR_RGBA(255, 0, 0, 255);
    color.var_name = NULL;  // No var_name to free
    ir_color_free(&color);
}

// ============================================================================
// MAIN
// ============================================================================

static void run_all_tests(void) {
    BEGIN_SUITE("IRStyleTypes Tests");

    // Color type enum tests
    RUN_TEST(test_color_type_solid_value);
    RUN_TEST(test_color_type_transparent_value);
    RUN_TEST(test_color_type_gradient_value);
    RUN_TEST(test_color_type_var_ref_value);

    // IRColor tests
    RUN_TEST(test_color_rgba_macro);
    RUN_TEST(test_color_rgba_function);
    RUN_TEST(test_color_equals_same);
    RUN_TEST(test_color_equals_different);
    RUN_TEST(test_color_equals_alpha_difference);
    RUN_TEST(test_color_equals_different_types);
    RUN_TEST(test_color_var_ref_macro);
    RUN_TEST(test_color_gradient_macro);
    RUN_TEST(test_color_transparent_macro);

    // Gradient type enum tests
    RUN_TEST(test_gradient_type_linear_value);
    RUN_TEST(test_gradient_type_radial_value);
    RUN_TEST(test_gradient_type_conic_value);

    // Gradient structure tests
    RUN_TEST(test_gradient_initial_state);
    RUN_TEST(test_gradient_max_stops);
    RUN_TEST(test_gradient_stop_position_range);
    RUN_TEST(test_gradient_stop_color_values);
    RUN_TEST(test_gradient_center_defaults);
    RUN_TEST(test_gradient_center_values);

    // Color free tests
    RUN_TEST(test_color_free_null_safe);
    RUN_TEST(test_color_free_solid);
    RUN_TEST(test_color_free_with_var_name);

    END_SUITE();
}

RUN_TEST_SUITE(run_all_tests);
