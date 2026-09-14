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
    LinkInteraction interaction;
    Rectangle bounds;

    normal.value.foreground = 0x0044ccffu;
    hover.value.foreground = 0x2266eeffu;
    disabled.value.foreground = 0x667788ffu;

    bounds = LinkBoundsFor((Rectangle){10, 20, 0, 0}, 72, 18, 16);
    check_bool("link fills zero width", (int)bounds.width == 72, 1);
    check_bool("link fills zero height", (int)bounds.height == 18, 1);
    bounds = LinkBoundsFor((Rectangle){10, 20, 0, 0}, 0, 0, 16);
    check_bool("link falls back to font height", (int)bounds.height == 16, 1);
    check_bool("link underline y",
               LinkUnderlineYFor((Rectangle){10, 20, 72, 18}, 2.0f) == 34,
               1);

    interaction = LinkInteractionFor(false, false, true, true, true, false,
                                     true);
    check_bool("link active", interaction.active, 1);
    check_bool("link hovered", interaction.hovered, 1);
    check_bool("link normal not disabled marker", interaction.disabled_marker,
               0);
    check_bool("link activated", interaction.activated, 1);
    check_bool("link consumes release", interaction.consume_release, 1);
    check_bool("link hover state", interaction.state == ButtonStateHover, 1);

    interaction = LinkInteractionFor(false, true, true, true, true, false,
                                     true);
    check_bool("captured link inactive", interaction.active, 0);
    check_bool("captured link no hover", interaction.hovered, 0);
    check_bool("captured link not activated", interaction.activated, 0);
    check_bool("captured link normal state",
               interaction.state == ButtonStateNormal, 1);

    interaction = LinkInteractionFor(false, false, true, true, true, true,
                                     true);
    check_bool("consumed release suppresses link", interaction.activated, 0);
    check_bool("consumed release not consumed again",
               interaction.consume_release, 0);

    interaction = LinkInteractionFor(true, false, true, true, true, false,
                                     true);
    check_bool("disabled link inactive", interaction.active, 0);
    check_bool("disabled link marker", interaction.disabled_marker, 1);
    check_bool("disabled link state",
               interaction.state == ButtonStateDisabled, 1);

    check_bool("clicked link activates", LinkActivated(false, true, false), 1);
    check_bool("focus link activates", LinkActivated(false, false, true), 1);
    check_bool("disabled link suppresses activation",
               LinkActivated(true, true, true), 0);

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
