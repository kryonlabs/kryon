#include <assert.h>

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

int
main(void)
{
    TextInputMetrics metrics = TextInputMetricsFor(0, 0, -1, -1,
                                                   16, 6, 8, 4);
    TextNavigationDecision decision;
    TextFieldScroll scroll;

    assert(metrics.font == 16);
    assert(metrics.padding_x == 6);
    assert(metrics.padding_y == 8);
    assert(metrics.line_gap == 4);
    assert(metrics.line_height == 20);
    assert(TextInputContentWidth(40.0f, 8) == 24);
    assert(TextInputContentWidth(10.0f, 8) == 0);
    assert(TextAreaPageRows(72.0f, metrics.font, metrics.line_gap,
                            metrics.padding_y) == 2);

    scroll = TextFieldScrollFor(20.0f, 100.0f, 8, 180, 500);
    assert(scroll.scroll == 96);
    assert(scroll.max_scroll == 96);
    assert(scroll.clip_width == 84);
    assert(scroll.text_origin_x == -68);
    assert(TextFieldRevealScroll(50, 100, 80, 4, 8) == 0);
    assert(TextFieldRevealScroll(0, 100, 80, 120, 8) == 48);

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

    return 0;
}
