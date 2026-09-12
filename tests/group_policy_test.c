#include <assert.h>
#include <math.h>

#include "runtime/group.h"

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
    GroupPolicy group = GroupPolicyFor((Rectangle){10, 20, 100, 80}, 6, 4);
    assert(group.gap == 6);
    assert(group.padding == 4);
    check_rect(group.bounds, 10, 20, 100, 80);
    check_rect(group.content, 14, 24, 92, 72);

    group = GroupPolicyFor((Rectangle){10, 20, 8, 6}, -1, -2);
    assert(group.gap == 0);
    assert(group.padding == 0);
    check_rect(group.content, 10, 20, 8, 6);

    group = ScreenGroupPolicyFor((Rectangle){5, 6, 0, -1}, 320, 180, 3, 2);
    assert(group.gap == 3);
    assert(group.padding == 2);
    check_rect(group.bounds, 5, 6, 320, 180);
    check_rect(group.content, 7, 8, 316, 176);
    return 0;
}
