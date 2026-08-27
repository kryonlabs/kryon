#include "kryon.h"
#include <stdio.h>
#include <string.h>

static int failures;

static void
check_int(const char *name, int got, int want)
{
    if(got == want)
        return;
    fprintf(stderr, "FAIL: %s got %d want %d\n", name, got, want);
    failures++;
}

static void
check_str(const char *name, const char *got, const char *want)
{
    if(got != 0 && want != 0 && strcmp(got, want) == 0)
        return;
    fprintf(stderr, "FAIL: %s got %s want %s\n",
            name, got != 0 ? got : "(null)", want != 0 ? want : "(null)");
    failures++;
}

int
main(void)
{
    int routes[4] = {0};
    KryRouteStack stack;
    KryAppShellLayout layout;
    Rectangle content;
    int volume = 500;
    int enabled = 42;

    KryRouteStackInit(&stack, routes, 4, 10);
    check_int("route stack root count", stack.count, 1);
    check_int("route stack root", KryRouteStackCurrent(&stack), 10);
    check_int("route stack push", KryRouteStackPush(&stack, 20), 1);
    check_int("route stack current", KryRouteStackCurrent(&stack), 20);
    check_int("route stack duplicate ok", KryRouteStackPush(&stack, 20), 1);
    check_int("route stack duplicate count", stack.count, 2);
    check_int("route stack pop", KryRouteStackPop(&stack), 10);
    check_int("route stack root pop sticks", KryRouteStackPop(&stack), 10);
    KryRouteStackReset(&stack, 30);
    check_int("route stack reset", KryRouteStackCurrent(&stack), 30);

    layout = KryAppShellMeasure((KryAppShellLayoutSpec){
        .view_width = 900,
        .view_height = 700,
        .safe_left = 4,
        .safe_top = 8,
        .safe_right = 6,
        .safe_bottom = 10,
        .padding = 16,
        .nav_height = 64,
        .sidebar_breakpoint = 480,
        .sidebar_width = 240,
        .min_content_width = 280,
        .max_content_width = 520
    });
    check_int("shell expanded", layout.compact, 0);
    check_int("shell sidebar", layout.sidebar_width, 240);
    check_int("shell content x", layout.content_x, 260);
    check_int("shell content width", layout.content_width, 520);
    check_int("shell nav y", layout.nav_y, 626);

    layout = KryAppShellMeasure((KryAppShellLayoutSpec){
        .view_width = 360,
        .view_height = 640,
        .padding = 12,
        .nav_height = 56,
        .sidebar_breakpoint = 480,
        .sidebar_width = 240,
        .min_content_width = 280
    });
    check_int("shell compact", layout.compact, 1);
    check_int("shell compact sidebar hidden", layout.sidebar_width, 0);
    check_int("shell compact content width", layout.content_width, 336);

    check_int("cap has clipboard",
              KryCapabilitiesHas(KRY_CAP_CLIPBOARD | KRY_CAP_FILE_PICKER,
                                 KRY_CAP_CLIPBOARD), 1);
    check_int("cap lacks share",
              KryCapabilitiesHas(KRY_CAP_CLIPBOARD, KRY_CAP_SHARE), 0);
    check_str("cap name", KryCapabilityName(KRY_CAP_SECURE_STORE),
              "secure-store");

    content = KrySafeContentRect((KryViewportSpec){
        .width = 400,
        .height = 700,
        .safe_area = {.left = 2, .top = 10, .right = 4, .bottom = 20},
        .padding = 16,
        .reserved_top = 8,
        .reserved_bottom = 64,
        .min_content_width = 320
    });
    check_int("safe rect x", (int)content.x, 18);
    check_int("safe rect y", (int)content.y, 34);
    check_int("safe rect width", (int)content.width, 362);
    check_int("safe rect height", (int)content.height, 566);

    check_int("clamp swaps range", KryClampInt(5, 10, 0), 5);
    check_int("clamp low", KryClampInt(-2, 0, 10), 0);
    check_int("clamp high", KryClampInt(42, 0, 10), 10);
    check_int("normalize int",
              KryNormalizeIntSetting((KryIntSetting){
                  .key = "volume",
                  .value = &volume,
                  .default_value = 80,
                  .min_value = 0,
                  .max_value = 100
              }), 100);
    check_int("normalize int writes", volume, 100);
    check_int("normalize bool",
              KryNormalizeBoolSetting((KryBoolSetting){
                  .key = "enabled",
                  .value = &enabled,
                  .default_value = 0
              }), 1);
    check_int("normalize bool writes", enabled, 1);

    return failures == 0 ? 0 : 1;
}
