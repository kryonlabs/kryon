#include "kryon.h"
#include "kry_inject.h"

#include <stdio.h>
#include <stdlib.h>

#define VIEW_W 320
#define VIEW_H 240

static int score = 0;

static void
check_int(const char *name, int got, int want)
{
    if(got == want)
        return;
    fprintf(stderr, "%s: got %d want %d\n", name, got, want);
    exit(1);
}

static ScoreControlProps
props(void)
{
    ScoreControlProps control = {0};

    control.bounds = (Rectangle){20, 20, 176, 80};
    control.id = 77;
    control.min_value = -3;
    control.max_value = 3;
    control.value = &score;
    control.gap = 4;
    control.height = 30;
    control.min_item_width = 40;
    control.wrap = 1;
    return control;
}

static ScoreControlResult
step(void)
{
    ScoreControlResult result;

    InjectPump();
    BeginUIFrame(VIEW_W, VIEW_H, 1.0f);
    result = ScoreControl(props());
    EndUIFrame();
    return result;
}

int
main(void)
{
    ScoreControlProps control = props();
    ScoreControlResult result;

    check_int("wrapped height", GetScoreControlHeight(control), 64);

    result = step();
    check_int("initial value", result.value, 0);
    check_int("initial clicked", result.clicked, 0);
    check_int("initial clicked value", result.clicked_value, 0);

    InjectTap(36, 36);
    step();
    result = step();
    check_int("tap negative score", score, -3);
    check_int("tap negative clicked", result.clicked, 1);
    check_int("tap negative value", result.clicked_value, -3);
    check_int("tap negative changed", result.changed, 1);

    result = step();
    check_int("post-click changed clears", result.changed, 0);
    check_int("post-click value persists", result.value, -3);

    InjectTap(168, 70);
    step();
    result = step();
    check_int("tap wrapped positive score", score, 3);
    check_int("tap wrapped positive clicked", result.clicked, 1);
    check_int("tap wrapped positive value", result.clicked_value, 3);

    InjectTap(168, 36);
    step();
    result = step();
    check_int("tap zero score", score, 0);
    check_int("tap zero clicked", result.clicked, 1);
    check_int("tap zero clicked value", result.clicked_value, 0);
    check_int("tap zero changed", result.changed, 1);

    return 0;
}
