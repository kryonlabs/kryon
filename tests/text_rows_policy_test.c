#include <assert.h>
#ifdef __cplusplus
#include "runtime/text_rows.hpp"
#else
#include "runtime/text_rows.h"
#endif

int main(void)
{
    String text = StringView("a\r\n\n", 4);
    TextLogicalLine line = TextLogicalLineFor(text, 0);
    assert(line.valid && line.start == 0 && line.end == 1 && line.next == 3);
    line = TextLogicalLineFor(text, line.next);
    assert(line.valid && line.start == 3 && line.end == 3 && line.next == 4);
    line = TextLogicalLineFor(text, line.next);
    assert(line.valid && line.start == 4 && line.end == 4);
    assert(!TextLogicalLineFor(text, line.next).valid);
    TextRowBreak state = {0};
    state = TextRowAdvance(state, 0, 3, 10, false, 20, 8, false);
    assert(state.done && state.end == 3); /* indivisible oversized grapheme */
    assert(!TextRowHasCursor(0, 3, 10, 3));
    assert(TextRowHasCursor(3, 6, 10, 3));
    assert(TextRowHasCursor(6, 10, 10, 10));
    assert(!TextRowAtY(0, 18, 18, false));
    assert(TextRowAtY(18, 18, 18, false));
    assert(TextRowAtY(18, 18, 200, true));
    return 0;
}
