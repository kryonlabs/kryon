#include "kryon_frame.h"

#include <stdio.h>

static int set_target_fps_calls;
static int last_target_fps = -1;

void
SetTargetFPS(int fps)
{
    set_target_fps_calls++;
    last_target_fps = fps;
}

static int
check_int(const char *name, int got, int expected)
{
    if(got == expected)
        return 0;
    fprintf(stderr, "%s: got %d expected %d\n", name, got, expected);
    return 1;
}

int
main(void)
{
    int failures = 0;

    ConfigureFramePacing(15, 30);
    failures += check_int("initial target", last_target_fps, 15);
    failures += check_int("initial getter", GetFramePacingTargetFPS(), 15);
    failures += check_int("initial calls", set_target_fps_calls, 1);

    UpdateFramePacing();
    failures += check_int("unchanged calls", set_target_fps_calls, 1);

    SetFramePacingActive(1);
    UpdateFramePacing();
    failures += check_int("active target", last_target_fps, 30);
    failures += check_int("active getter", GetFramePacingTargetFPS(), 30);
    failures += check_int("active calls", set_target_fps_calls, 2);

    SetFramePacingActive(0);
    UpdateFramePacing();
    failures += check_int("idle target", last_target_fps, 15);
    failures += check_int("idle calls", set_target_fps_calls, 3);

    ConfigureFramePacing(0, 24);
    failures += check_int("fallback target", last_target_fps, 24);

    DisableFramePacing();
    failures += check_int("disabled target", last_target_fps, 0);
    failures += check_int("disabled getter", GetFramePacingTargetFPS(), 0);

    if(failures) {
        fprintf(stderr, "frame pacing tests failed: %d\n", failures);
        return 1;
    }

    printf("frame pacing tests passed\n");
    return 0;
}
