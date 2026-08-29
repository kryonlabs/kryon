#include "kryon.h"

#include <stdio.h>

static int failures;

int GetScreenWidth(void) { return 320; }
int GetScreenHeight(void) { return 560; }

static void
check_int(const char *name, int got, int want)
{
    if(got == want)
        return;
    fprintf(stderr, "FAIL: %s got %d want %d\n", name, got, want);
    failures++;
}

int
main(void)
{
    AndroidWindowInsets insets = {0};
    KrySafeArea safe;

    check_int("insets initially not ready", GetAndroidWindowInsets(&insets), 0);
    safe = GetAndroidSafeArea();
    check_int("initial safe left", safe.left, 0);
    check_int("initial safe top", safe.top, 0);
    check_int("initial safe right", safe.right, 0);
    check_int("initial safe bottom", safe.bottom, 0);

    SetAndroidWindowInsets(-1, 53, 84, 24, 120, 4, 60, 80, 8);
    check_int("insets ready", GetAndroidWindowInsets(&insets), 1);
    check_int("system left clamps negative", insets.system_left, 0);
    check_int("system top", insets.system_top, 53);
    check_int("system right", insets.system_right, 84);
    check_int("system bottom", insets.system_bottom, 24);
    check_int("ime bottom stored separately", insets.ime_bottom, 120);
    check_int("cutout left", insets.cutout_left, 4);
    check_int("cutout top", insets.cutout_top, 60);
    check_int("cutout right", insets.cutout_right, 80);
    check_int("cutout bottom", insets.cutout_bottom, 8);

    safe = GetAndroidSafeArea();
    check_int("safe left max", safe.left, 4);
    check_int("safe top max", safe.top, 60);
    check_int("safe right max", safe.right, 84);
    check_int("safe bottom excludes ime", safe.bottom, 24);

    return failures == 0 ? 0 : 1;
}
