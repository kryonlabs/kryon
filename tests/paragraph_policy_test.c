#include <assert.h>

#ifdef __cplusplus
#include "runtime/paragraph.hpp"
#else
#include "runtime/paragraph.h"
#endif

int
main(void)
{
    ParagraphMetrics metrics = ParagraphResolveMetrics(
        0, 16, 0, 4, 0, 0, 240, 0, 30);
    ParagraphLayoutPolicy policy;
    ParagraphLine line = {0};
    ParagraphLineDecision decision;

    assert(metrics.font == 16);
    assert(metrics.line_gap == 4);
    assert(metrics.icon_size == 16);
    assert(metrics.width == 240);
    assert(metrics.height == 20);
    assert(metrics.next_y == 50);
    assert(ParagraphCanLayout(metrics.width));

    metrics = ParagraphResolveMetrics(18, 16, 6, 4, 22, 180, 240, 72, 12);
    assert(metrics.font == 18);
    assert(metrics.line_gap == 6);
    assert(metrics.icon_size == 22);
    assert(metrics.width == 180);
    assert(metrics.height == 72);
    assert(metrics.next_y == 84);

    metrics = ParagraphResolveMetrics(14, 16, 3, 4, 0, -10, -20, -1, 5);
    assert(metrics.width == 0);
    assert(metrics.height == 17);
    assert(!ParagraphCanLayout(metrics.width));
    assert(ParagraphDefaultLineGap(1.0f) == 4);
    assert(ParagraphDefaultLineGap(2.0f) == 8);
    assert(ParagraphDefaultLineGap(0.0f) == 4);

    policy = ParagraphLayoutPolicyFor(7, 1, 0, 4, 2.0f);
    assert(policy.space_width == 9);
    assert(policy.icon_spacing == 8);
    assert(policy.line_gap == 4);

    policy = ParagraphLayoutPolicyFor(5, -4, 6, 4, 1.0f);
    assert(policy.space_width == 0);
    assert(policy.icon_spacing == 4);
    assert(policy.line_gap == 6);
    assert(ParagraphLayoutTotalHeight(3, 18, 4) == 62);
    assert(ParagraphLayoutTotalHeight(0, 18, 4) == 0);
    decision = ParagraphLineAdvance(line, 0, false, false, 70, 70, 60);
    assert(!decision.emit);
    assert(decision.next.width == 70);
    assert(decision.next.has_content);
    line = decision.next;
    decision = ParagraphLineAdvance(line, 1, false, false, 30, 104, 60);
    assert(decision.emit);
    assert(decision.line.width == 70);
    assert(decision.line.start == 0 && decision.line.end == 1);
    assert(decision.next.width == 30 && decision.next.start == 1);
    decision = ParagraphLineAdvance(decision.next, 2, true, false, 0, 0, 60);
    assert(decision.emit && decision.line.width == 30);
    assert(!decision.next.has_content && decision.next.start == 3);
    decision = ParagraphLineAdvance(decision.next, 3, false, true, 0, 0, 60);
    assert(decision.emit && decision.line.width == 0);
    assert(decision.line.start == 3 && decision.line.end == 3);
    decision = ParagraphLineAdvance(line, 1, false, false, 30, 104, 0);
    assert(!decision.emit && decision.next.width == 104);
    assert(ParagraphElementSpacing(false, true, policy) == 0);
    assert(ParagraphElementSpacing(true, true, policy) == policy.icon_spacing);
    assert(ParagraphElementSpacing(true, false, policy) == policy.space_width);

    const char *source = " \talpha\r\n\n日本語\xC2\xA0" "word %i";
    String text = StringView(source, strlen(source));
    ParagraphToken token = ParagraphTokenNext(text, 0, true);
    assert(token.valid && token.start == 2 && token.end == 7);
    token = ParagraphTokenNext(text, token.next, true);
    assert(token.line_break && token.end == 9);
    token = ParagraphTokenNext(text, token.next, true);
    assert(token.line_break && token.end == 10);
    token = ParagraphTokenNext(text, token.next, true);
    assert(!token.line_break && token.end - token.start == 15);
    token = ParagraphTokenNext(text, token.next, true);
    assert(token.icon && token.end - token.start == 2);
    assert(!ParagraphTokenNext(text, token.next, true).valid);
    token = ParagraphTokenNext(StringView("%i", 2), 0, false);
    assert(token.valid && !token.icon && token.end == 2);
    assert(ParagraphLineXFor(10, 100, 60, TextAlignStart) == 10);
    assert(ParagraphLineXFor(10, 100, 60, TextAlignCenter) == 30);
    assert(ParagraphLineXFor(10, 100, 60, TextAlignEnd) == 50);
    assert(ParagraphLineXFor(10, 50, 60, TextAlignEnd) == 0);
    assert(ParagraphNextLineY(20, 18, 4, true) == 42);
    assert(ParagraphNextLineY(20, 18, 4, false) == 38);
    assert(ParagraphLineStride(18, 4) == 22);
    assert(ParagraphLineStride(0, -8) == 1);
    assert(ParagraphLineIndexFor(15.0f, 10.0f, 20, 3) == 0);
    assert(ParagraphLineIndexFor(55.0f, 10.0f, 20, 3) == 2);
    assert(ParagraphLineIndexFor(90.0f, 10.0f, 20, 3) == 2);
    assert(ParagraphLineIndexFor(0.0f, 10.0f, 20, 3) == 0);
    assert(ParagraphLineIndexFor(10.0f, 10.0f, 0, 3) == 0);
    assert(ParagraphLineIndexFor(10.0f, 10.0f, 20, 0) == -1);
    assert(ParagraphSelectionLocalOffsetFor(5.0f, 10.0f, 80, 12, 6) == 0);
    assert(ParagraphSelectionLocalOffsetFor(95.0f, 10.0f, 80, 12, 6) == 12);
    assert(ParagraphSelectionLocalOffsetFor(40.0f, 10.0f, 80, 12, 6) == 6);
    assert(ParagraphSelectionLocalOffsetFor(40.0f, 10.0f, 80, 12, -2) == 0);
    assert(ParagraphSelectionLocalOffsetFor(40.0f, 10.0f, 80, 12, 20) == 12);
    assert(ParagraphSelectionLocalOffsetFor(95.0f, 10.0f, 80, -1, 6) == 0);
    return 0;
}
