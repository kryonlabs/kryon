/**
 * @file test_ir_layout_types.c
 * @brief Tests for IR layout type system
 */

#define _POSIX_C_SOURCE 200809L
#include "../tests/test_framework.h"
#include "ir_layout_types.h"

// ============================================================================
// TEST CASES
// ============================================================================

// Dimension Type Tests

TEST(test_dimension_type_auto_value) {
    // IR_DIMENSION_AUTO must be 0 for memset compatibility
    ASSERT_EQ(0, IR_DIMENSION_AUTO);
}

TEST(test_dimension_create_px) {
    IRDimension dim = ir_dimension_px(100.0f);
    ASSERT_EQ(IR_DIMENSION_PX, dim.type);
    ASSERT_TRUE(dim.value > 99.0f && dim.value < 101.0f);
}

TEST(test_dimension_create_percent) {
    IRDimension dim = ir_dimension_percent(50.0f);
    ASSERT_EQ(IR_DIMENSION_PERCENT, dim.type);
    ASSERT_TRUE(dim.value > 49.0f && dim.value < 51.0f);
}

TEST(test_dimension_create_auto) {
    IRDimension dim = ir_dimension_auto();
    ASSERT_EQ(IR_DIMENSION_AUTO, dim.type);
    ASSERT_EQ(0.0f, dim.value);
}

TEST(test_dimension_initializer_zero_is_auto) {
    // Zero-initialization should create AUTO dimension
    IRDimension dim = {0};
    ASSERT_EQ(IR_DIMENSION_AUTO, dim.type);
}

// Alignment Tests

TEST(test_alignment_start_value) {
    ASSERT_EQ(0, IR_ALIGNMENT_START);
}

TEST(test_alignment_center_value) {
    ASSERT_EQ(1, IR_ALIGNMENT_CENTER);
}

TEST(test_alignment_end_value) {
    ASSERT_EQ(2, IR_ALIGNMENT_END);
}

TEST(test_alignment_stretch_value) {
    ASSERT_EQ(3, IR_ALIGNMENT_STRETCH);
}

TEST(test_alignment_space_between_value) {
    ASSERT_EQ(4, IR_ALIGNMENT_SPACE_BETWEEN);
}

TEST(test_alignment_space_around_value) {
    ASSERT_EQ(5, IR_ALIGNMENT_SPACE_AROUND);
}

TEST(test_alignment_space_evenly_value) {
    ASSERT_EQ(6, IR_ALIGNMENT_SPACE_EVENLY);
}

// Dimension Resolution Tests

TEST(test_dimension_resolve_px) {
    IRDimension dim = ir_dimension_px(100.0f);
    float resolved = ir_dimension_resolve(dim, 1000.0f);
    ASSERT_TRUE(resolved > 99.0f && resolved < 101.0f);
}

TEST(test_dimension_resolve_percent) {
    IRDimension dim = ir_dimension_percent(50.0f);
    float resolved = ir_dimension_resolve(dim, 200.0f);
    ASSERT_TRUE(resolved > 99.0f && resolved < 101.0f);
}

TEST(test_dimension_resolve_auto) {
    IRDimension dim = ir_dimension_auto();
    float resolved = ir_dimension_resolve(dim, 200.0f);
    // AUTO typically returns 0 or a sentinel
    ASSERT_TRUE(resolved >= 0.0f);
}

// Layout Constraints Tests

TEST(test_layout_constraints_size) {
    IRLayoutConstraints constraints;
    constraints.max_width = 800.0f;
    constraints.max_height = 600.0f;
    constraints.min_width = 100.0f;
    constraints.min_height = 50.0f;

    ASSERT_TRUE(constraints.max_width > 799.0f);
    ASSERT_TRUE(constraints.max_height > 599.0f);
    ASSERT_TRUE(constraints.min_width > 99.0f);
    ASSERT_TRUE(constraints.min_height > 49.0f);
}

// Computed Layout Tests

TEST(test_computed_layout_initial_state) {
    IRComputedLayout layout = {0};
    ASSERT_EQ(0.0f, layout.x);
    ASSERT_EQ(0.0f, layout.y);
    ASSERT_EQ(0.0f, layout.width);
    ASSERT_EQ(0.0f, layout.height);
    ASSERT_FALSE(layout.valid);
}

TEST(test_computed_layout_set_values) {
    IRComputedLayout layout = {0};
    layout.x = 10.5f;
    layout.y = 20.5f;
    layout.width = 100.0f;
    layout.height = 200.0f;
    layout.valid = true;

    ASSERT_TRUE(layout.x > 10.0f);
    ASSERT_TRUE(layout.y > 20.0f);
    ASSERT_TRUE(layout.width > 99.0f);
    ASSERT_TRUE(layout.height > 199.0f);
    ASSERT_TRUE(layout.valid);
}

// Layout State Tests

TEST(test_layout_state_initial_dirty) {
    IRLayoutState state = {0};
    // Zero-initialization means dirty=false, layout_valid=false
    ASSERT_FALSE(state.dirty);
    ASSERT_FALSE(state.layout_valid);
}

// ============================================================================
// MAIN
// ============================================================================

static void run_all_tests(void) {
    BEGIN_SUITE("IRLayoutTypes Tests");

    // Dimension type tests
    RUN_TEST(test_dimension_type_auto_value);
    RUN_TEST(test_dimension_create_px);
    RUN_TEST(test_dimension_create_percent);
    RUN_TEST(test_dimension_create_auto);
    RUN_TEST(test_dimension_initializer_zero_is_auto);

    // Alignment tests
    RUN_TEST(test_alignment_start_value);
    RUN_TEST(test_alignment_center_value);
    RUN_TEST(test_alignment_end_value);
    RUN_TEST(test_alignment_stretch_value);
    RUN_TEST(test_alignment_space_between_value);
    RUN_TEST(test_alignment_space_around_value);
    RUN_TEST(test_alignment_space_evenly_value);

    // Dimension resolution tests
    RUN_TEST(test_dimension_resolve_px);
    RUN_TEST(test_dimension_resolve_percent);
    RUN_TEST(test_dimension_resolve_auto);

    // Layout constraints tests
    RUN_TEST(test_layout_constraints_size);

    // Computed layout tests
    RUN_TEST(test_computed_layout_initial_state);
    RUN_TEST(test_computed_layout_set_values);

    // Layout state tests
    RUN_TEST(test_layout_state_initial_dirty);

    END_SUITE();
}

RUN_TEST_SUITE(run_all_tests);
