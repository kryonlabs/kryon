#include "kryon.h"
#include "kry_inject.h"

#include <stdio.h>
#include <stdlib.h>

#define VIEW_W 320
#define VIEW_H 240

static SegmentOption options[] = {
    {"A", 0},
    {"B", 0},
    {"C", 0},
};

static int selected = 0;

static void
check_int(const char *name, int got, int want)
{
    if(got == want)
        return;
    fprintf(stderr, "%s: got %d want %d\n", name, got, want);
    exit(1);
}

static SegmentedControlProps
props(void)
{
    SegmentedControlProps control = {0};

    control.bounds = (Rectangle){20, 20, 130, 80};
    control.id = 42;
    control.options = options;
    control.option_count = 3;
    control.selected_index = &selected;
    control.gap = 4;
    control.height = 30;
    control.min_item_width = 60;
    control.wrap = 1;
    return control;
}

static SegmentedControlResult
step(void)
{
    SegmentedControlResult result;

    InjectPump();
    BeginUIFrame(VIEW_W, VIEW_H, 1.0f);
    result = SegmentedControl(props());
    EndUIFrame();
    return result;
}

int
main(void)
{
    SegmentedControlProps control = props();
    SegmentedControlResult result;

    check_int("wrapped height", GetSegmentedControlHeight(control), 64);

    result = step();
    check_int("initial selected", result.selected_index, 0);
    check_int("initial clicked", result.clicked_index, -1);

    InjectTap(112, 36);
    step();
    result = step();
    check_int("tap second selected", selected, 1);
    check_int("tap second clicked", result.clicked_index, 1);
    check_int("tap second changed", result.changed, 1);

    result = step();
    check_int("post-click changed clears", result.changed, 0);
    check_int("post-click selected persists", result.selected_index, 1);

    InjectTap(84, 70);
    step();
    result = step();
    check_int("wrapped third selected", selected, 2);
    check_int("wrapped third clicked", result.clicked_index, 2);

    return 0;
}
