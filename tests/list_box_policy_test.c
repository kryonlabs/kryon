#include <assert.h>

#include "runtime/list_box.h"

int
main(void)
{
    StyleFrame item = {0};
    ListBoxLayout layout;
    ListBoxNavigation nav;

    assert(ListBoxKeyNone() == 0);
    assert(ListBoxKeyHome() == 1);
    assert(ListBoxKeyEnd() == 2);
    assert(ListBoxKeyUp() == 3);
    assert(ListBoxKeyDown() == 4);
    assert(ListBoxKeyFor(false, false, false, false) == ListBoxKeyNone());
    assert(ListBoxKeyFor(true, false, false, false) == ListBoxKeyHome());
    assert(ListBoxKeyFor(false, true, false, false) == ListBoxKeyEnd());
    assert(ListBoxKeyFor(false, false, true, false) == ListBoxKeyUp());
    assert(ListBoxKeyFor(false, false, false, true) == ListBoxKeyDown());
    assert(ListBoxKeyFor(true, true, true, true) == ListBoxKeyHome());

    layout = ListBoxLayoutFor((Rectangle){10, 20, 100, 95}, 10, 24, 0, 0,
                              1.0f, item);
    assert(layout.row_height == 24);
    assert(layout.content_height == 240);
    assert(layout.max_scroll == 145);

    nav = ListBoxNavigate(2, 10, ListBoxKeyDown(), 0, 24, 95.0f,
                          layout.max_scroll, 1.0f, item);
    assert(nav.changed);
    assert(nav.selected == 3);
    assert(nav.scroll == 1);

    nav = ListBoxNavigate(2, 10, ListBoxKeyUp(), 0, 24, 95.0f,
                          layout.max_scroll, 1.0f, item);
    assert(nav.changed);
    assert(nav.selected == 1);

    nav = ListBoxNavigate(2, 10, ListBoxKeyHome(), 0, 24, 95.0f,
                          layout.max_scroll, 1.0f, item);
    assert(nav.changed);
    assert(nav.selected == 0);

    nav = ListBoxNavigate(2, 10, ListBoxKeyEnd(), 0, 24, 95.0f,
                          layout.max_scroll, 1.0f, item);
    assert(nav.changed);
    assert(nav.selected == 9);
    assert(nav.scroll == layout.max_scroll);

    nav = ListBoxNavigate(0, 10, ListBoxKeyUp(), 0, 24, 95.0f,
                          layout.max_scroll, 1.0f, item);
    assert(!nav.changed);
    assert(nav.selected == 0);

    nav = ListBoxNavigate(9, 10, ListBoxKeyDown(), 0, 24, 95.0f,
                          layout.max_scroll, 1.0f, item);
    assert(!nav.changed);
    assert(nav.selected == 9);

    nav = ListBoxNavigate(-1, 10, ListBoxKeyUp(), 0, 24, 95.0f,
                          layout.max_scroll, 1.0f, item);
    assert(nav.changed);
    assert(nav.selected == 9);

    nav = ListBoxNavigate(4, 10, ListBoxKeyNone(), 0, 24, 95.0f,
                          layout.max_scroll, 1.0f, item);
    assert(!nav.changed);
    assert(nav.selected == 4);

    nav = ListBoxNavigate(4, 0, ListBoxKeyDown(), 0, 24, 95.0f,
                          layout.max_scroll, 1.0f, item);
    assert(!nav.changed);

    {
        ListBoxRowDecision decision = ListBoxRowDecisionFor(1, 1, 1, 2, 4);
        assert(decision.select);
        assert(decision.consume_release);
        assert(decision.selected == 4);
        assert(decision.changed);
        decision = ListBoxRowDecisionFor(0, 1, 1, 2, 4);
        assert(!decision.select);
        assert(!decision.consume_release);
        decision = ListBoxRowDecisionFor(1, 0, 1, 2, 4);
        assert(!decision.select);
        decision = ListBoxRowDecisionFor(1, 1, 0, 2, 4);
        assert(!decision.select);
    }

    return 0;
}
