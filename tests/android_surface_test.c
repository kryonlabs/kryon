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
    AndroidSafeArea clean_safe;
    AndroidViewport viewport;
    AndroidViewportPolicy policy;
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

    clean_safe = GetAndroidSafeAreaInsets();
    check_int("clean safe left max", clean_safe.left, 4);
    check_int("clean safe top max", clean_safe.top, 60);
    check_int("clean safe right max", clean_safe.right, 84);
    check_int("clean safe bottom excludes ime", clean_safe.bottom, 24);

    check_int("full viewport ready",
              ResolveAndroidViewport(320, 560, AndroidViewportPolicyFull(),
                                     &viewport),
              1);
    check_int("full viewport x", viewport.x, 0);
    check_int("full viewport y", viewport.y, 0);
    check_int("full viewport width", viewport.width, 320);
    check_int("full viewport height", viewport.height, 560);
    check_int("full viewport inset bottom", viewport.insets.bottom, 0);

    check_int("safe viewport ready",
              ResolveAndroidViewport(320, 560, AndroidViewportPolicySafeArea(),
                                     &viewport),
              1);
    check_int("safe viewport x", viewport.x, 4);
    check_int("safe viewport y", viewport.y, 60);
    check_int("safe viewport width", viewport.width, 232);
    check_int("safe viewport height", viewport.height, 476);
    check_int("safe viewport inset bottom", viewport.insets.bottom, 24);

    check_int("ime viewport ready",
              ResolveAndroidViewport(320, 560,
                                     AndroidViewportPolicyResizeForIme(),
                                     &viewport),
              1);
    check_int("ime viewport bottom uses ime", viewport.insets.bottom, 120);
    check_int("ime viewport height", viewport.height, 380);

    policy = AndroidViewportPolicySafeArea();
    policy.min_width = 260;
    check_int("min width fallback ready",
              ResolveAndroidViewport(320, 560, policy, &viewport), 1);
    check_int("min width fallback x", viewport.x, 0);
    check_int("min width fallback width", viewport.width, 320);

    return failures == 0 ? 0 : 1;
}
