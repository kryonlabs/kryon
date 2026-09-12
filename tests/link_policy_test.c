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
    const unsigned int theme_link = 0x0044ccffu;
    const unsigned int theme_hover = 0x2266eeffu;
    const unsigned int disabled = 0x667788ffu;
    LinkAppearance paint;

    check_u32("default color", LinkDefaultColor(0, theme_link), theme_link);
    check_u32("explicit color", LinkDefaultColor(0xaa5500ffu, theme_link),
              0xaa5500ffu);

    paint = ResolveLinkAppearance(0, 0, theme_link, theme_hover, disabled,
                                  false, false);
    check_u32("normal link color", paint.color, theme_link);
    check_bool("normal underline", paint.underline, 0);

    paint = ResolveLinkAppearance(0, 0, theme_link, theme_hover, disabled,
                                  true, false);
    check_u32("theme hover color", paint.color, theme_hover);
    check_bool("hover underline", paint.underline, 1);

    paint = ResolveLinkAppearance(0, 0x113355ffu, theme_link, theme_hover,
                                  disabled, true, false);
    check_u32("explicit hover color", paint.color, 0x113355ffu);

    paint = ResolveLinkAppearance(0, 0x113355ffu, theme_link, theme_hover,
                                  disabled, true, true);
    check_u32("disabled suppresses hover",
              paint.color,
              LinkMixColor(disabled, theme_link, 35));
    check_bool("disabled underline", paint.underline, 0);

    return failures == 0 ? 0 : 1;
}
