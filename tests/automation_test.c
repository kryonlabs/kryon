#include "automation.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;

static void
check_int(const char *name, int got, int expected)
{
    if(got != expected) {
        printf("FAIL %s: got %d expected %d\n", name, got, expected);
        failures++;
    }
}

static void
check_str(const char *name, const char *got, const char *expected)
{
    if(strcmp(got, expected) != 0) {
        printf("FAIL %s: got '%s' expected '%s'\n", name, got, expected);
        failures++;
    }
}

int
main(void)
{
    char out[64];
    int value;

    unsetenv("AUTOMATION_AUTOPLAY");
    unsetenv("AUTOMATION_SEED");
    unsetenv("AUTOMATION_STAGE_MODE");

    check_int("missing option", AutomationGetOption("autoplay", "off",
                                                   out, sizeof(out)), 0);
    check_str("missing fallback", out, "off");

    setenv("AUTOMATION_AUTOPLAY", "", 1);
    check_int("empty env option", AutomationGetOption("autoplay", "off",
                                                    out, sizeof(out)), 0);
    check_str("empty env fallback", out, "off");

    setenv("AUTOMATION_AUTOPLAY", "stage1", 1);
    check_int("env option", AutomationGetOption("autoplay", "off",
                                               out, sizeof(out)), 1);
    check_str("env option value", out, "stage1");

    setenv("AUTOMATION_STAGE_MODE", "hazards", 1);
    check_int("sanitized key", AutomationGetOption("stage-mode", "",
                                                  out, sizeof(out)), 1);
    check_str("sanitized key value", out, "hazards");

    setenv("AUTOMATION_SEED", "17", 1);
    check_int("int option", AutomationGetInt("seed", 1, &value), 1);
    check_int("int value", value, 17);
    check_int("seed value", (int)AutomationGetSeed(3), 17);

    setenv("AUTOMATION_SEED", "bad", 1);
    check_int("bad int", AutomationGetInt("seed", 5, &value), 0);
    check_int("bad int fallback", value, 5);
    check_int("bad seed fallback", (int)AutomationGetSeed(9), 9);

    return failures == 0 ? 0 : 1;
}
