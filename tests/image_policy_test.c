#include "runtime/image.h"

#include <math.h>
#include <stdio.h>

static int failures;

static void
check_rect(const char *label, Rectangle actual, Rectangle expected)
{
    const float epsilon = 0.001f;

    if(fabsf(actual.x - expected.x) > epsilon ||
       fabsf(actual.y - expected.y) > epsilon ||
       fabsf(actual.width - expected.width) > epsilon ||
       fabsf(actual.height - expected.height) > epsilon) {
        fprintf(stderr,
                "%s: got {%.3f, %.3f, %.3f, %.3f}, want {%.3f, %.3f, %.3f, %.3f}\n",
                label, actual.x, actual.y, actual.width, actual.height,
                expected.x, expected.y, expected.width, expected.height);
        failures++;
    }
}

int
main(void)
{
    Rectangle bounds = {10, 20, 200, 100};
    Rectangle empty = {0};
    Rectangle wide = {0, 0, 400, 100};

    check_rect("stretch keeps bounds",
               ImageFitBounds(bounds, wide, 400, 100, 0),
               bounds);
    check_rect("contain centers by source aspect",
               ImageFitBounds(bounds, wide, 400, 100, 1),
               (Rectangle){10, 45, 200, 50});
    check_rect("cover centers by source aspect",
               ImageFitBounds(bounds, wide, 400, 100, 2),
               (Rectangle){-90, 20, 400, 100});
    check_rect("empty source uses texture size",
               ImageFitBounds(bounds, empty, 400, 100, 1),
               (Rectangle){10, 45, 200, 50});
    check_rect("negative source dimensions use absolute size",
               ImageFitBounds(bounds, (Rectangle){0, 0, -400, -100}, 1, 1, 1),
               (Rectangle){10, 45, 200, 50});

    ImagePlaceholderLayout placeholder =
        ImagePlaceholderLayoutFor((Rectangle){10, 20, 200, 100}, 48, 16);
    check_rect("placeholder keeps bounds", placeholder.bounds,
               (Rectangle){10, 20, 200, 100});
    if(placeholder.label_x != 86 || placeholder.label_y != 62) {
        fprintf(stderr, "placeholder label: got {%d, %d}, want {86, 62}\n",
                placeholder.label_x, placeholder.label_y);
        failures++;
    }
    if(ImagePlaceholderFontFor(16, 0) != 16 ||
       ImagePlaceholderFontFor(16, 21) != 21) {
        fprintf(stderr, "placeholder font fallback/override policy failed\n");
        failures++;
    }

    return failures == 0 ? 0 : 1;
}
