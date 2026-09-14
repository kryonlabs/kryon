#include <assert.h>
#include <math.h>

#include "runtime/text_input.h"

static void
check_zero(TextNavigationDecision decision)
{
    assert(!decision.consumed);
    assert(!decision.collapse_selection_start);
    assert(!decision.collapse_selection_end);
    assert(decision.char_direction == 0);
    assert(decision.word_direction == 0);
    assert(decision.document_edge == 0);
    assert(decision.line_edge == 0);
    assert(decision.vertical_direction == 0);
    assert(decision.page_direction == 0);
    assert(!decision.extend_selection);
}

static void
check_rect(Rectangle got, float x, float y, float width, float height)
{
    assert(fabsf(got.x - x) < 0.001f);
    assert(fabsf(got.y - y) < 0.001f);
    assert(fabsf(got.width - width) < 0.001f);
    assert(fabsf(got.height - height) < 0.001f);
}

int
main(void)
{
    TextInputMetrics metrics = TextInputMetricsFor(0, 0, 0, -1, -1,
                                                   16, 6, 8, 4);
    TextInputMetrics zero_metrics = TextInputMetricsFor(
        StylePaddingX | StylePaddingY | StyleGap, 0, 0, 0, 0, 16, 6, 8, 4);
    TextNavigationDecision decision;
    TextDeleteDecision delete_decision;
    TextInsertDecision insert_decision;
    TextSelectionRange selection;
    TextSelectionState moved;
    TextFieldScroll scroll;
    TextFieldPaint field_paint;
    TextAreaPaint area_paint;

    assert(metrics.font == 16);
    assert(metrics.padding_x == 6);
    assert(metrics.padding_y == 8);
    assert(metrics.line_gap == 4);
    assert(metrics.line_height == 20);
    assert(zero_metrics.padding_x == 0);
    assert(zero_metrics.padding_y == 0);
    assert(zero_metrics.line_gap == 0);
    assert(zero_metrics.line_height == 16);
    assert(TextInputDefaultPaddingX(1.0f) == 10);
    assert(TextInputDefaultPaddingY(1.0f) == 8);
    assert(TextInputDefaultPaddingX(2.0f) == 20);
    assert(TextInputDefaultPaddingY(2.0f) == 16);
    assert(TextInputContentWidth(40.0f, 8) == 24);
    assert(TextInputContentWidth(10.0f, 8) == 0);
    assert(TextAreaPageRows(72.0f, metrics.font, metrics.line_gap,
                            metrics.padding_y) == 2);
    assert(TextAreaMinWrapWidth(1.0f) == 24);
    assert(TextAreaWrapWidthFor(100.0f, 8, 1,
                                TextAreaMinWrapWidth(1.0f)) == 84);
    assert(TextAreaWrapWidthFor(30.0f, 8, 1,
                                TextAreaMinWrapWidth(1.0f)) == 0);
    assert(TextAreaWrapWidthFor(100.0f, 8, 0,
                                TextAreaMinWrapWidth(1.0f)) == 0);
    area_paint = TextAreaPaintFor((Rectangle){10, 20, 100, 80},
                                  16, 4, 8, 6, 1, 200, 500, 18,
                                  TextAreaMinWrapWidth(1.0f));
    check_rect(area_paint.clip_bounds, 18, 26, 84, 68);
    assert(area_paint.wrap_width == 84);
    assert(area_paint.viewport_height == 68);
    assert(area_paint.max_scroll == 132);
    assert(area_paint.scroll_y == 132);
    assert(area_paint.placeholder_x == 18);
    assert(area_paint.placeholder_y == 27);
    assert(TextInputBufferLimit(16, 0) == 15);
    assert(TextInputBufferLimit(16, 4) == 4);
    assert(TextInputBufferLimit(0, 4) == 0);

    scroll = TextFieldScrollFor(20.0f, 100.0f, 8, 180, 500);
    assert(scroll.scroll == 96);
    assert(scroll.max_scroll == 96);
    assert(scroll.clip_width == 84);
    assert(scroll.text_origin_x == -68);
    assert(TextFieldRevealMargin(1.0f) == 8);
    assert(TextFieldRevealScroll(50, 100, 80, 4,
                                 TextFieldRevealMargin(1.0f)) == 0);
    assert(TextFieldRevealScroll(0, 100, 80, 120,
                                 TextFieldRevealMargin(1.0f)) == 48);
    assert(TextFieldMinCursorHeight(1.0f) == 8);
    assert(TextFieldCursorVerticalPadding(1.0f) == 8);
    assert(TextFieldClipGuard(1.0f) == 1);
    assert(TextInputStrokeWidth(1.0f) == 2);
    assert(TextInputStrokeWidth(0.25f) == 1);
    assert(TextInputDoubleClickSlopFor(2.0f) == 12);
    assert(TextFieldPanDragThresholdFor(2.0f) == 10);
    assert(TextFieldCursorHeightFor(18, 40.0f, 20,
                                    TextFieldMinCursorHeight(1.0f),
                                    TextFieldCursorVerticalPadding(1.0f)) == 20);
    assert(TextFieldCursorHeightFor(30, 24.0f, 20,
                                    TextFieldMinCursorHeight(1.0f),
                                    TextFieldCursorVerticalPadding(1.0f)) == 16);
    assert(TextFieldCursorHeightFor(2, 4.0f, 2,
                                    TextFieldMinCursorHeight(1.0f),
                                    TextFieldCursorVerticalPadding(1.0f)) == 8);
    field_paint = TextFieldPaintFor((Rectangle){20, 30, 100, 40},
                                    8, 12, 18, 20,
                                    TextFieldMinCursorHeight(1.0f),
                                    TextFieldCursorVerticalPadding(1.0f),
                                    TextFieldClipGuard(1.0f));
    check_rect(field_paint.clip_bounds, 28, 29, 84, 42);
    assert(field_paint.text_x == 16);
    assert(field_paint.cursor_y == 40);
    assert(field_paint.cursor_height == 20);

    selection = TextSelectionRangeFor(9, 3);
    assert(selection.start == 3);
    assert(selection.end == 9);
    assert(selection.has_selection);

    selection = TextSelectionRangeFor(5, 5);
    assert(selection.start == 5);
    assert(selection.end == 5);
    assert(!selection.has_selection);

    moved = TextSelectionAfterMove(5, 5, 2, true);
    assert(moved.anchor == 5);
    assert(moved.cursor == 2);
    assert(moved.has_selection);

    moved = TextSelectionAfterMove(9, 3, 1, true);
    assert(moved.anchor == 9);
    assert(moved.cursor == 1);
    assert(moved.has_selection);

    moved = TextSelectionAfterMove(9, 3, 1, false);
    assert(moved.anchor == 1);
    assert(moved.cursor == 1);
    assert(!moved.has_selection);

    moved = TextSelectionCollapsed(7);
    assert(moved.anchor == 7);
    assert(moved.cursor == 7);
    assert(!moved.has_selection);

    moved = TextSelectionAll(4);
    assert(moved.anchor == 0);
    assert(moved.cursor == 4);
    assert(moved.has_selection);

    moved = TextSelectionAll(0);
    assert(moved.anchor == 0);
    assert(moved.cursor == 0);
    assert(!moved.has_selection);

    moved = TextSelectionAll(-2);
    assert(moved.anchor == 0);
    assert(moved.cursor == 0);
    assert(!moved.has_selection);

    check_zero(TextNavigationDecisionFor(TextNavNone(), false, false, false,
                                         false, false));
    decision = TextNavigationDecisionFor(TextNavLeft(), false, false, false,
                                         false, true);
    assert(decision.consumed);
    assert(decision.collapse_selection_start);
    assert(!decision.extend_selection);

    decision = TextNavigationDecisionFor(TextNavRight(), false, false, false,
                                         false, true);
    assert(decision.consumed);
    assert(decision.collapse_selection_end);

    decision = TextNavigationDecisionFor(TextNavLeft(), false, false, true,
                                         false, false);
    assert(decision.consumed);
    assert(decision.word_direction == -1);

    decision = TextNavigationDecisionFor(TextNavRight(), false, false, true,
                                         true, false);
    assert(decision.consumed);
    assert(decision.document_edge == 1);
    assert(decision.word_direction == 0);

    decision = TextNavigationDecisionFor(TextNavHome(), true, false, false,
                                         false, false);
    assert(decision.consumed);
    assert(decision.line_edge == -1);

    decision = TextNavigationDecisionFor(TextNavEnd(), true, false, true,
                                         false, false);
    assert(decision.consumed);
    assert(decision.document_edge == 1);

    check_zero(TextNavigationDecisionFor(TextNavPageDown(), false, false,
                                         false, false, false));
    decision = TextNavigationDecisionFor(TextNavPageDown(), true, true,
                                         false, false, false);
    assert(decision.consumed);
    assert(decision.page_direction == 1);
    assert(decision.extend_selection);

    decision = TextNavigationDecisionFor(TextNavUp(), true, false, false,
                                         false, false);
    assert(decision.consumed);
    assert(decision.vertical_direction == -1);

    delete_decision = TextDeleteDecisionFor(TextDeleteBackspace(), true,
                                            false, false);
    assert(delete_decision.consumed);
    assert(delete_decision.word_direction == -1);

    delete_decision = TextDeleteDecisionFor(TextDeleteForward(), true,
                                            true, false);
    assert(delete_decision.consumed);
    assert(delete_decision.document_edge == 1);
    assert(delete_decision.word_direction == 0);

    delete_decision = TextDeleteDecisionFor(TextDeleteForward(), false,
                                            false, true);
    assert(delete_decision.consumed);
    assert(delete_decision.char_direction == 0);
    assert(delete_decision.word_direction == 0);
    assert(delete_decision.document_edge == 0);

    insert_decision = TextInsertDecisionFor('a', 1, 2, 4, 2, 0, 4, false);
    assert(insert_decision.accept);
    assert(!insert_decision.skip);
    assert(!insert_decision.stop);

    insert_decision = TextInsertDecisionFor('\r', 1, 2, 4, 2, 0, 4, true);
    assert(!insert_decision.accept);
    assert(insert_decision.skip);
    assert(!insert_decision.stop);

    insert_decision = TextInsertDecisionFor('\n', 1, 2, 4, 2, 0, 4, false);
    assert(!insert_decision.accept);
    assert(insert_decision.skip);

    insert_decision = TextInsertDecisionFor('a', 1, 3, 4, 3, 0, 0, false);
    assert(!insert_decision.accept);
    assert(insert_decision.stop);

    insert_decision = TextInsertDecisionFor('a', 1, 2, 8, 4, 0, 4, false);
    assert(!insert_decision.accept);
    assert(insert_decision.stop);

    return 0;
}
