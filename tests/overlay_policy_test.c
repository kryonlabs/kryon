#include <assert.h>

#include "runtime/overlay.h"

int
main(void)
{
    DismissibleOverlayPolicy policy;

    assert(OverlayViewExtent(320, 640) == 320);
    assert(OverlayViewExtent(0, 640) == 640);
    assert(OverlayViewExtent(-1, -1) == 0);

    policy = DismissibleOverlayPolicyFor(true, false, false, false);
    assert(policy.closed);
    assert(policy.outside_released);
    assert(policy.release_consumed);

    policy = DismissibleOverlayPolicyFor(true, false, false, true);
    assert(!policy.closed);
    assert(!policy.outside_released);
    assert(!policy.release_consumed);

    policy = DismissibleOverlayPolicyFor(true, true, false, false);
    assert(!policy.closed);
    assert(!policy.outside_released);
    assert(policy.release_consumed);

    policy = DismissibleOverlayPolicyFor(true, false, true, false);
    assert(!policy.closed);
    assert(!policy.outside_released);
    assert(!policy.release_consumed);

    return 0;
}
