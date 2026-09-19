#include <assert.h>
#ifdef __cplusplus
#include "runtime/popup_ownership.hpp"
#else
#include "runtime/popup_ownership.h"
#endif

static int parent[] = {-1, 0, 1, 0, 3, -1, 5};
static unsigned long order[] = {1, 100, 500, 200, 201, 300, 301};

static int above(int a, int b)
{
    int origin_a = a, origin_b = b, steps = 0;
    PopupOrder state = {0};
    while (!state.done) {
        assert(++steps < 100);
        state = PopupOrderAdvance(state, a >= 0, b >= 0, a == b,
            a >= 0 && b >= 0 && parent[a] == parent[b],
            a >= 0 ? order[a] : 0, b >= 0 ? order[b] : 0);
        if (state.reset_a) a = origin_a;
        else if (state.move_a) a = parent[a];
        if (state.reset_b) b = origin_b;
        else if (state.move_b) b = parent[b];
    }
    return state.above;
}

int main(void)
{
    for (int a = 0; a < 7; a++) {
        for (int b = 0; b < 7; b++) {
            assert(above(a, b) == (a > b));
        }
    }
    assert(!PopupAncestryAdvance(false, false, false).contains);
    assert(PopupAncestryAdvance(true, true, true).contains);
    assert(PopupAncestryAdvance(true, false, false).done);
    assert(!PopupAncestryAdvance(true, false, true).done);
    assert(!PopupInputCaptured(false, false, false, false));
    assert(PopupInputCaptured(false, false, true, false));
    assert(PopupInputCaptured(true, false, false, false));
    assert(!PopupInputCaptured(true, true, true, true));
    assert(PopupInputCaptured(true, true, true, false));
    assert(PopupOwnerAlive(false, false));
    assert(!PopupOwnerAlive(true, false));
    assert(PopupOwnerAlive(true, true));
    assert(PopupOwnerRetired(true, 1, 2));
    assert(!PopupOwnerRetired(true, 2, 2));
    assert(PopupOwnerRetired(false, 2, 2));

    PopupFocusState focus = {0};
    focus = PopupFocusInitialize(focus, false, 42);
    assert(focus.restore_focus == 42 && focus.autofocus);
    PopupFocusDecision result = PopupFocusRegister(focus, 7, 42, true, false, false);
    assert(!result.acquire && result.state.autofocus);
    result = PopupFocusRegister(focus, 7, 42, true, true, true);
    assert(!result.acquire && result.state.autofocus);
    result = PopupFocusRegister(focus, 7, 42, false, true, false);
    assert(!result.acquire && result.state.autofocus);
    result = PopupFocusRegister(focus, 7, 42, true, true, false);
    assert(result.acquire && !result.state.autofocus);
    assert(result.state.has_last_focus && result.state.last_focus == 7);
    focus = PopupFocusInitialize(result.state, true, 9);
    assert(focus.restore_focus == 42 && !focus.autofocus);
    assert(PopupFocusRestore(focus, 7, true, false));
    assert(!PopupFocusRestore(focus, 9, true, false));
    assert(!PopupFocusRestore(focus, 7, false, true));
    assert(PopupFocusRestore(focus, 9, true, true));
    return 0;
}
