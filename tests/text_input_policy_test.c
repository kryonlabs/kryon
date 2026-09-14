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
    TextAreaGutterMetrics gutter_metrics;
    TextInputDoubleClickDecision double_click;
    TextCompositionInputDecision composition_input;
    TextCompositionSessionDecision composition_session;
    TextCompositionPhaseDecision composition_phase;
    TextCompositionApplyDecision composition_apply;
    TextCompositionViewRange composition_range;
    TextCompositionPaintSpan composition_span;
    TextSelectionPaintSpan selection_span;
    TextContextMenuState context_menu;
    TextContextCommandDecision context_command;
    TextFieldPanDecision pan_decision;

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
    gutter_metrics = TextAreaGutterMetricsFor(2.0f);
    assert(gutter_metrics.top_inset == 20);
    assert(gutter_metrics.active_y_inset == 4);
    assert(gutter_metrics.label_x_inset == 12);
    assert(gutter_metrics.label_font == 20);
    assert(gutter_metrics.extra_rows == 3);
    assert(TextAreaGutterRowsFor(80.0f, 20, gutter_metrics) == 7);
    assert(TextAreaGutterRowsFor(80.0f, 0, gutter_metrics) == 0);
    assert(TextAreaGutterFirstY(10.0f, 23, 20, gutter_metrics) == 27);
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
    composition_input = TextCompositionInputDecisionFor(true, false);
    assert(composition_input.accept_events);
    assert(!composition_input.cancel);
    assert(!composition_input.drain_events);
    composition_input = TextCompositionInputDecisionFor(false, false);
    assert(!composition_input.accept_events);
    assert(composition_input.cancel);
    assert(!composition_input.drain_events);
    composition_input = TextCompositionInputDecisionFor(true, true);
    assert(!composition_input.accept_events);
    assert(composition_input.cancel);
    assert(composition_input.drain_events);
    composition_session = TextCompositionCancelDecisionFor(true, true, true);
    assert(composition_session.cancel);
    composition_session = TextCompositionCancelDecisionFor(true, true, false);
    assert(!composition_session.cancel);
    composition_session = TextCompositionCancelDecisionFor(true, false, false);
    assert(composition_session.cancel);
    composition_session = TextCompositionCancelDecisionFor(false, false, false);
    assert(!composition_session.cancel);
    composition_session = TextCompositionGetDecisionFor(true, true, true);
    assert(composition_session.visible);
    composition_session = TextCompositionGetDecisionFor(true, true, false);
    assert(!composition_session.visible);
    composition_session = TextCompositionGetDecisionFor(true, false, true);
    assert(!composition_session.visible);
    composition_phase = TextCompositionPhaseDecisionFor(1);
    assert(composition_phase.store_preedit);
    assert(!composition_phase.commit);
    assert(!composition_phase.cancel);
    composition_phase = TextCompositionPhaseDecisionFor(2);
    assert(composition_phase.store_preedit);
    composition_phase = TextCompositionPhaseDecisionFor(3);
    assert(!composition_phase.store_preedit);
    assert(composition_phase.commit);
    composition_phase = TextCompositionPhaseDecisionFor(4);
    assert(composition_phase.cancel);
    composition_phase = TextCompositionPhaseDecisionFor(99);
    assert(!composition_phase.store_preedit);
    assert(!composition_phase.commit);
    assert(!composition_phase.cancel);
    composition_apply = TextCompositionApplyDecisionFor(1, false);
    assert(!composition_apply.text_changed);
    assert(composition_apply.presentation_changed);
    assert(!composition_apply.selection_changed);
    composition_apply = TextCompositionApplyDecisionFor(3, true);
    assert(composition_apply.text_changed);
    assert(composition_apply.presentation_changed);
    assert(composition_apply.selection_changed);
    composition_apply = TextCompositionApplyDecisionFor(3, false);
    assert(!composition_apply.text_changed);
    assert(composition_apply.presentation_changed);
    assert(composition_apply.selection_changed);
    composition_apply = TextCompositionApplyDecisionFor(4, false);
    assert(!composition_apply.text_changed);
    assert(composition_apply.presentation_changed);
    assert(!composition_apply.selection_changed);
    composition_apply = TextCompositionApplyDecisionFor(99, true);
    assert(!composition_apply.text_changed);
    assert(!composition_apply.presentation_changed);
    assert(!composition_apply.selection_changed);
    assert(TextCompositionSelectionLength(6, 2, 3) == 3);
    assert(TextCompositionSelectionLength(6, 2, 99) == 4);
    assert(TextCompositionSelectionLength(6, 9, 2) == 0);
    assert(TextCompositionSelectionLength(-1, -2, -3) == 0);
    composition_range = TextCompositionViewRangeFor(4, 1, 5, 2, 4);
    assert(composition_range.replace_start == 1);
    assert(composition_range.replace_end == 4);
    assert(composition_range.cursor == 3);
    assert(composition_range.selection_start == 3);
    assert(composition_range.selection_end == 5);
    assert(composition_range.composition_start == 1);
    assert(composition_range.composition_end == 6);
    composition_range = TextCompositionViewRangeFor(-3, -1, -2, -9, -4);
    assert(composition_range.replace_start == 0);
    assert(composition_range.replace_end == 0);
    assert(composition_range.cursor == 0);
    assert(composition_range.selection_start == 0);
    assert(composition_range.selection_end == 0);
    assert(composition_range.composition_start == 0);
    assert(composition_range.composition_end == 0);
    composition_span = TextCompositionPaintSpanForText(-3, 99, 12);
    assert(composition_span.visible);
    assert(composition_span.start == 0);
    assert(composition_span.end == 12);
    composition_span = TextCompositionPaintSpanForText(8, 2, 12);
    assert(!composition_span.visible);
    composition_span = TextCompositionPaintSpanForLine(2, 10, 5, 8);
    assert(composition_span.visible);
    assert(composition_span.start == 5);
    assert(composition_span.end == 8);
    composition_span = TextCompositionPaintSpanForLine(2, 4, 5, 8);
    assert(!composition_span.visible);
    assert(TextCompositionUnderlineEndX(20, 18, 3) == 23);
    assert(TextCompositionUnderlineEndX(20, 30, 3) == 30);
    assert(TextCompositionUnderlineY(40, 18, 3) == 55);
    selection_span = TextSelectionPaintSpanForLine(2, 10, 5, 8);
    assert(selection_span.visible);
    assert(selection_span.start == 5);
    assert(selection_span.end == 8);
    assert(selection_span.continues_past_line);
    selection_span = TextSelectionPaintSpanForLine(0, 20, 5, 8);
    assert(selection_span.visible);
    assert(selection_span.start == 5);
    assert(selection_span.end == 8);
    assert(selection_span.continues_past_line);
    selection_span = TextSelectionPaintSpanForLine(0, 20, 8, 8);
    assert(selection_span.visible);
    assert(selection_span.start == 8);
    assert(selection_span.end == 8);
    assert(selection_span.continues_past_line);
    selection_span = TextSelectionPaintSpanForLine(2, 4, 5, 8);
    assert(!selection_span.visible);
    assert(fabsf(TextInputDoubleClickMaxSeconds() - 0.45f) < 0.001f);
    double_click = TextInputDoubleClickDecisionFor(true, true, 0.30f,
                                                   4, -4, 6);
    assert(double_click.double_click);
    double_click = TextInputDoubleClickDecisionFor(false, true, 0.30f,
                                                   4, 4, 6);
    assert(!double_click.double_click);
    double_click = TextInputDoubleClickDecisionFor(true, false, 0.30f,
                                                   4, 4, 6);
    assert(!double_click.double_click);
    double_click = TextInputDoubleClickDecisionFor(true, true, 0.60f,
                                                   4, 4, 6);
    assert(!double_click.double_click);
    double_click = TextInputDoubleClickDecisionFor(true, true, 0.30f,
                                                   7, 4, 6);
    assert(!double_click.double_click);
    assert(TextFieldPanDragThresholdFor(2.0f) == 10);
    pan_decision = TextFieldPanDecisionFor(false, 11, 6, 10);
    assert(pan_decision.pan);
    pan_decision = TextFieldPanDecisionFor(false, -11, 6, 10);
    assert(pan_decision.pan);
    pan_decision = TextFieldPanDecisionFor(false, 10, 1, 10);
    assert(!pan_decision.pan);
    pan_decision = TextFieldPanDecisionFor(false, 12, 20, 10);
    assert(!pan_decision.pan);
    pan_decision = TextFieldPanDecisionFor(true, 0, 20, 10);
    assert(pan_decision.pan);
    assert(TextAreaScrollbarWidthFor(2.0f) == 24);
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
    assert(TextSelectionOwnerMatches(true, true, 0, 0));
    assert(!TextSelectionOwnerMatches(true, false, 0, 0));
    assert(TextSelectionOwnerMatches(false, false, 9, 9));
    assert(!TextSelectionOwnerMatches(false, false, 9, 0));
    assert(!TextSelectionOwnerMatches(false, false, 9, 8));
    context_menu = TextContextMenuStateFor(true, false, true, false, true);
    assert(context_menu.cut_enabled);
    assert(context_menu.copy_enabled);
    assert(context_menu.paste_enabled);
    assert(context_menu.select_all_enabled);
    context_menu = TextContextMenuStateFor(false, true, true, false, false);
    assert(context_menu.cut_enabled);
    assert(context_menu.copy_enabled);
    assert(!context_menu.paste_enabled);
    assert(context_menu.select_all_enabled);
    context_menu = TextContextMenuStateFor(true, false, true, true, true);
    assert(!context_menu.cut_enabled);
    assert(context_menu.copy_enabled);
    assert(!context_menu.paste_enabled);
    assert(context_menu.select_all_enabled);
    context_menu = TextContextMenuStateFor(false, false, false, false, true);
    assert(!context_menu.cut_enabled);
    assert(!context_menu.copy_enabled);
    assert(context_menu.paste_enabled);
    assert(!context_menu.select_all_enabled);
    context_command = TextContextCommandDecisionFor(
        TextContextCommandCut(), true, false, true, false);
    assert(context_command.copy_selection);
    assert(!context_command.copy_all);
    assert(context_command.delete_selection);
    assert(!context_command.clear_all);
    assert(!context_command.paste);
    assert(!context_command.select_all);
    assert(context_command.collapse_selection);
    context_command = TextContextCommandDecisionFor(
        TextContextCommandCut(), false, true, true, false);
    assert(!context_command.copy_selection);
    assert(context_command.copy_all);
    assert(!context_command.delete_selection);
    assert(context_command.clear_all);
    assert(context_command.collapse_selection);
    context_command = TextContextCommandDecisionFor(
        TextContextCommandCut(), true, false, true, true);
    assert(context_command.copy_selection);
    assert(!context_command.delete_selection);
    assert(context_command.collapse_selection);
    context_command = TextContextCommandDecisionFor(
        TextContextCommandCopy(), false, true, true, true);
    assert(!context_command.copy_selection);
    assert(context_command.copy_all);
    assert(!context_command.collapse_selection);
    context_command = TextContextCommandDecisionFor(
        TextContextCommandPaste(), true, false, true, false);
    assert(context_command.delete_selection);
    assert(context_command.paste);
    assert(context_command.collapse_selection);
    context_command = TextContextCommandDecisionFor(
        TextContextCommandPaste(), true, false, true, true);
    assert(!context_command.delete_selection);
    assert(!context_command.paste);
    context_command = TextContextCommandDecisionFor(
        TextContextCommandSelectAll(), false, false, false, false);
    assert(context_command.select_all);

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
