#include "runtime/link.h"
#include <stdio.h>

static int failures;

static void
check_u32(const char *label, unsigned int got, unsigned int want)
{
    if(got != want) {
        fprintf(stderr, "%s: got 0x%08x want 0x%08x\n", label, got, want);
        failures++;
    }
}

static void
check_bool(const char *label, int got, int want)
{
    if((got != 0) != (want != 0)) {
        fprintf(stderr, "%s: got %d want %d\n", label, got != 0, want != 0);
        failures++;
    }
}

int
main(void)
{
    StyleFrame normal = {0};
    StyleFrame hover = {0};
    StyleFrame disabled = {0};
    LinkAppearance paint;

    normal.value.foreground = 0x0044ccffu;
    hover.value.foreground = 0x2266eeffu;
    disabled.value.foreground = 0x667788ffu;

    paint = ResolveLinkAppearance(normal, false, false);
    check_u32("normal link color", paint.color, normal.value.foreground);
    check_bool("normal underline", paint.underline, 0);

    paint = ResolveLinkAppearance(hover, true, false);
    check_u32("hover color", paint.color, hover.value.foreground);
    check_bool("hover underline", paint.underline, 1);

    paint = ResolveLinkAppearance(disabled, true, true);
    check_u32("disabled suppresses hover",
              paint.color,
              disabled.value.foreground);
    check_bool("disabled underline", paint.underline, 0);

    return failures == 0 ? 0 : 1;
}
