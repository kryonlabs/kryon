#include "kryon.h"

#include <stdio.h>

static int callback_count;
static int events[8];

static void
text_input_callback(int active)
{
    if(callback_count < (int)(sizeof(events) / sizeof(events[0])))
        events[callback_count] = active != 0;
    callback_count++;
}

static int
check_event(int index, int expected)
{
    if(index >= callback_count) {
        fprintf(stderr, "FAIL: missing callback %d\n", index);
        return 0;
    }
    if(events[index] != expected) {
        fprintf(stderr, "FAIL: callback %d got %d want %d\n",
                index, events[index], expected);
        return 0;
    }
    return 1;
}

int
main(void)
{
    int ok = 1;

    SetTextInputPlatformCallback(text_input_callback);

    BeginUIFrame(640, 480, 1.0f);
    SetUIFocusTextInputActive(1);
    EndUIFrame();

    BeginUIFrame(640, 480, 1.0f);
    SetUIFocusTextInputActive(0);
    EndUIFrame();

    BeginUIFrame(640, 480, 1.0f);
    SetUIFocusTextInputActive(1);
    EndUIFrame();

    BeginUIFrame(640, 480, 1.0f);
    SetUIFocusTextInputActive(0);
    SetUIFocusTextInputActive(1);
    EndUIFrame();

    ok &= callback_count == 3;
    if(callback_count != 3)
        fprintf(stderr, "FAIL: callback count got %d want 3\n", callback_count);
    ok &= check_event(0, 1);
    ok &= check_event(1, 0);
    ok &= check_event(2, 1);

    SetTextInputPlatformCallback(NULL);

    if(ok)
        printf("text_input_platform_test: OK\n");
    return ok ? 0 : 1;
}
