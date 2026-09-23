#include "kryon_portable_host.h"

#include <assert.h>
#include <string.h>

static int timer_calls;
static int last_target;

void
SetTargetFPS(int fps)
{
    timer_calls++;
    last_target = fps;
}

int
main(int argc, char **argv)
{
    long long answer = 0;
    int has_answer = 0;
    assert(argc == 2);
    Bundle *bundle = BundleOpen(argv[1]);
    assert(bundle != NULL);
    assert(BundleCapabilityCount(bundle) == 1);
    assert(strcmp(BundleCapabilityModule(bundle, 0), "frame_pacing") == 0);
    assert(strcmp(BundleCapabilityFunction(bundle, 0),
                  "ApplyTargetFPS") == 0);
    HostBinding timer = FramePacingBinding();
    assert(BundleRun(bundle, &timer, 1, &answer, &has_answer));
    assert(has_answer && answer == 30);
    assert(timer_calls == 1 && last_target == 30);
    BundleClose(bundle);
    return 0;
}
