/**
 * @file test_ir_properties.c
 * @brief Tests for IR property types and structures
 */

#define _POSIX_C_SOURCE 200809L
#include "../tests/test_framework.h"
#include "ir_properties.h"

// ============================================================================
// TEST CASES
// ============================================================================

// Direction Tests

TEST(test_direction_auto_value) {
    ASSERT_EQ(0, IR_DIRECTION_AUTO);
}

TEST(test_direction_ltr_value) {
    ASSERT_EQ(1, IR_DIRECTION_LTR);
}

TEST(test_direction_rtl_value) {
    ASSERT_EQ(2, IR_DIRECTION_RTL);
}

TEST(test_direction_inherit_value) {
    ASSERT_EQ(3, IR_DIRECTION_INHERIT);
}

// Unicode Bidi Tests

TEST(test_unicode_bidi_normal_value) {
    ASSERT_EQ(0, IR_UNICODE_BIDI_NORMAL);
}

TEST(test_unicode_bidi_embed_value) {
    ASSERT_EQ(1, IR_UNICODE_BIDI_EMBED);
}

TEST(test_unicode_bidi_isolate_value) {
    ASSERT_EQ(2, IR_UNICODE_BIDI_ISOLATE);
}

TEST(test_unicode_bidi_plaintext_value) {
    ASSERT_EQ(3, IR_UNICODE_BIDI_PLAINTEXT);
}

// Layout Mode Tests

TEST(test_layout_mode_flex_value) {
    ASSERT_EQ(0, IR_LAYOUT_MODE_FLEX);
}

TEST(test_layout_mode_inline_flex_value) {
    ASSERT_EQ(1, IR_LAYOUT_MODE_INLINE_FLEX);
}

TEST(test_layout_mode_grid_value) {
    ASSERT_EQ(2, IR_LAYOUT_MODE_GRID);
}

TEST(test_layout_mode_block_value) {
    ASSERT_EQ(4, IR_LAYOUT_MODE_BLOCK);
}

TEST(test_layout_mode_none_value) {
    ASSERT_EQ(7, IR_LAYOUT_MODE_NONE);
}

// IRSpacing Tests

TEST(test_spacing_initial_state) {
    IRSpacing spacing = {0};
    ASSERT_EQ(0.0f, spacing.top);
    ASSERT_EQ(0.0f, spacing.right);
    ASSERT_EQ(0.0f, spacing.bottom);
    ASSERT_EQ(0.0f, spacing.left);
    ASSERT_EQ(0, spacing.set_flags);
}

TEST(test_spacing_set_all) {
    IRSpacing spacing = {
        .top = 10.0f,
        .right = 20.0f,
        .bottom = 30.0f,
        .left = 40.0f,
        .set_flags = IR_SPACING_SET_ALL
    };
    ASSERT_TRUE(spacing.top > 9.0f);
    ASSERT_TRUE(spacing.right > 19.0f);
    ASSERT_TRUE(spacing.bottom > 29.0f);
    ASSERT_TRUE(spacing.left > 39.0f);
    ASSERT_EQ(IR_SPACING_SET_ALL, spacing.set_flags);
}

TEST(test_spacing_auto_value) {
    ASSERT_TRUE(IR_SPACING_AUTO < -999000.0f);
}

// IRBorder Tests

TEST(test_border_initial_state) {
    IRBorder border = {0};
    ASSERT_EQ(0.0f, border.width);
    ASSERT_EQ(0, border.radius);
}

TEST(test_border_set_values) {
    IRBorder border = {
        .width = 2.0f,
        .radius = 8
    };
    ASSERT_TRUE(border.width > 1.0f && border.width < 3.0f);
    ASSERT_EQ(8, border.radius);
}

// IRTypography Tests

TEST(test_typography_initial_state) {
    IRTypography font = {0};
    ASSERT_EQ(0.0f, font.size);
    ASSERT_FALSE(font.bold);
    ASSERT_FALSE(font.italic);
    ASSERT_NULL(font.family);
}

TEST(test_typography_set_values) {
    IRTypography font = {0};
    font.size = 16.0f;
    font.bold = true;
    font.italic = false;
    font.weight = 700;
    font.line_height = 1.5f;

    ASSERT_TRUE(font.size > 15.0f && font.size < 17.0f);
    ASSERT_TRUE(font.bold);
    ASSERT_FALSE(font.italic);
    ASSERT_EQ(700, font.weight);
    ASSERT_TRUE(font.line_height > 1.4f && font.line_height < 1.6f);
}

// Text Align Tests

TEST(test_text_align_left_value) {
    ASSERT_EQ(0, IR_TEXT_ALIGN_LEFT);
}

TEST(test_text_align_center_value) {
    ASSERT_EQ(2, IR_TEXT_ALIGN_CENTER);
}

TEST(test_text_align_right_value) {
    ASSERT_EQ(1, IR_TEXT_ALIGN_RIGHT);
}

TEST(test_text_align_justify_value) {
    ASSERT_EQ(3, IR_TEXT_ALIGN_JUSTIFY);
}

// Text Decoration Tests

TEST(test_text_decoration_none_value) {
    ASSERT_EQ(0x00, IR_TEXT_DECORATION_NONE);
}

TEST(test_text_decoration_underline_value) {
    ASSERT_EQ(0x01, IR_TEXT_DECORATION_UNDERLINE);
}

TEST(test_text_decoration_overline_value) {
    ASSERT_EQ(0x02, IR_TEXT_DECORATION_OVERLINE);
}

TEST(test_text_decoration_line_through_value) {
    ASSERT_EQ(0x04, IR_TEXT_DECORATION_LINE_THROUGH);
}

// IRFlexbox Tests

TEST(test_flexbox_initial_state) {
    IRFlexbox flex = {0};
    ASSERT_FALSE(flex.wrap);
    ASSERT_EQ(0, flex.gap);
    ASSERT_EQ(0, flex.grow);
    ASSERT_EQ(0, flex.shrink);
    ASSERT_EQ(0, flex.direction);
}

TEST(test_flexbox_set_values) {
    IRFlexbox flex = {0};
    flex.wrap = true;
    flex.gap = 16;
    flex.grow = 1;
    flex.shrink = 1;
    flex.direction = 1;  // row

    ASSERT_TRUE(flex.wrap);
    ASSERT_EQ(16, flex.gap);
    ASSERT_EQ(1, flex.grow);
    ASSERT_EQ(1, flex.shrink);
    ASSERT_EQ(1, flex.direction);
}

// IRGrid Tests

TEST(test_grid_max_tracks) {
    ASSERT_EQ(12, IR_MAX_GRID_TRACKS);
}

TEST(test_grid_initial_state) {
    IRGrid grid = {0};
    ASSERT_EQ(0, grid.row_count);
    ASSERT_EQ(0, grid.column_count);
    ASSERT_FALSE(grid.use_column_repeat);
    ASSERT_FALSE(grid.use_row_repeat);
}

// IRDimension Tests

TEST(test_dimension_auto_type) {
    ASSERT_EQ(0, IR_DIMENSION_AUTO);
}

TEST(test_dimension_px_type) {
    ASSERT_EQ(1, IR_DIMENSION_PX);
}

TEST(test_dimension_percent_type) {
    ASSERT_EQ(2, IR_DIMENSION_PERCENT);
}

// IRAlignment Tests

TEST(test_alignment_start) {
    ASSERT_EQ(0, IR_ALIGNMENT_START);
}

TEST(test_alignment_center) {
    ASSERT_EQ(1, IR_ALIGNMENT_CENTER);
}

TEST(test_alignment_end) {
    ASSERT_EQ(2, IR_ALIGNMENT_END);
}

// Position Mode Tests

TEST(test_position_relative_value) {
    ASSERT_EQ(0, IR_POSITION_RELATIVE);
}

TEST(test_position_absolute_value) {
    ASSERT_EQ(1, IR_POSITION_ABSOLUTE);
}

TEST(test_position_fixed_value) {
    ASSERT_EQ(2, IR_POSITION_FIXED);
}

// Overflow Mode Tests

TEST(test_overflow_visible_value) {
    ASSERT_EQ(0, IR_OVERFLOW_VISIBLE);
}

TEST(test_overflow_hidden_value) {
    ASSERT_EQ(1, IR_OVERFLOW_HIDDEN);
}

TEST(test_overflow_scroll_value) {
    ASSERT_EQ(2, IR_OVERFLOW_SCROLL);
}

TEST(test_overflow_auto_value) {
    ASSERT_EQ(3, IR_OVERFLOW_AUTO);
}

// Object Fit Tests

TEST(test_object_fit_fill_value) {
    ASSERT_EQ(0, IR_OBJECT_FIT_FILL);
}

TEST(test_object_fit_contain_value) {
    ASSERT_EQ(1, IR_OBJECT_FIT_CONTAIN);
}

TEST(test_object_fit_cover_value) {
    ASSERT_EQ(2, IR_OBJECT_FIT_COVER);
}

// IRStyle Tests

TEST(test_style_initial_state) {
    IRStyle style = {0};
    // Width/height should be AUTO (type 0)
    ASSERT_EQ(IR_DIMENSION_AUTO, style.width.type);
    ASSERT_EQ(IR_DIMENSION_AUTO, style.height.type);
    // Visible is zero-initialized to false (0)
    ASSERT_FALSE(style.visible);
    // Opacity is zero-initialized to 0.0
    ASSERT_TRUE(style.opacity >= 0.0f && style.opacity < 0.01f);
}

// IRLayout Tests

TEST(test_layout_initial_state) {
    IRLayout layout = {0};
    ASSERT_EQ(IR_LAYOUT_MODE_FLEX, layout.mode);
    ASSERT_FALSE(layout.display_explicit);
    ASSERT_EQ(0.0f, layout.aspect_ratio);
}

// ============================================================================
// MAIN
// ============================================================================

static void run_all_tests(void) {
    BEGIN_SUITE("IRProperties Tests");

    // Direction tests
    RUN_TEST(test_direction_auto_value);
    RUN_TEST(test_direction_ltr_value);
    RUN_TEST(test_direction_rtl_value);
    RUN_TEST(test_direction_inherit_value);

    // Unicode Bidi tests
    RUN_TEST(test_unicode_bidi_normal_value);
    RUN_TEST(test_unicode_bidi_embed_value);
    RUN_TEST(test_unicode_bidi_isolate_value);
    RUN_TEST(test_unicode_bidi_plaintext_value);

    // Layout Mode tests
    RUN_TEST(test_layout_mode_flex_value);
    RUN_TEST(test_layout_mode_inline_flex_value);
    RUN_TEST(test_layout_mode_grid_value);
    RUN_TEST(test_layout_mode_block_value);
    RUN_TEST(test_layout_mode_none_value);

    // IRSpacing tests
    RUN_TEST(test_spacing_initial_state);
    RUN_TEST(test_spacing_set_all);
    RUN_TEST(test_spacing_auto_value);

    // IRBorder tests
    RUN_TEST(test_border_initial_state);
    RUN_TEST(test_border_set_values);

    // IRTypography tests
    RUN_TEST(test_typography_initial_state);
    RUN_TEST(test_typography_set_values);

    // Text Align tests
    RUN_TEST(test_text_align_left_value);
    RUN_TEST(test_text_align_center_value);
    RUN_TEST(test_text_align_right_value);
    RUN_TEST(test_text_align_justify_value);

    // Text Decoration tests
    RUN_TEST(test_text_decoration_none_value);
    RUN_TEST(test_text_decoration_underline_value);
    RUN_TEST(test_text_decoration_overline_value);
    RUN_TEST(test_text_decoration_line_through_value);

    // IRFlexbox tests
    RUN_TEST(test_flexbox_initial_state);
    RUN_TEST(test_flexbox_set_values);

    // IRGrid tests
    RUN_TEST(test_grid_max_tracks);
    RUN_TEST(test_grid_initial_state);

    // IRDimension tests
    RUN_TEST(test_dimension_auto_type);
    RUN_TEST(test_dimension_px_type);
    RUN_TEST(test_dimension_percent_type);

    // IRAlignment tests
    RUN_TEST(test_alignment_start);
    RUN_TEST(test_alignment_center);
    RUN_TEST(test_alignment_end);

    // Position Mode tests
    RUN_TEST(test_position_relative_value);
    RUN_TEST(test_position_absolute_value);
    RUN_TEST(test_position_fixed_value);

    // Overflow Mode tests
    RUN_TEST(test_overflow_visible_value);
    RUN_TEST(test_overflow_hidden_value);
    RUN_TEST(test_overflow_scroll_value);
    RUN_TEST(test_overflow_auto_value);

    // Object Fit tests
    RUN_TEST(test_object_fit_fill_value);
    RUN_TEST(test_object_fit_contain_value);
    RUN_TEST(test_object_fit_cover_value);

    // IRStyle tests
    RUN_TEST(test_style_initial_state);

    // IRLayout tests
    RUN_TEST(test_layout_initial_state);

    END_SUITE();
}

RUN_TEST_SUITE(run_all_tests);
