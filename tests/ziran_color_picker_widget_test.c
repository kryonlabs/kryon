#include "kryon_portable_host.h"

#include <assert.h>
#include <stdio.h>
#include "unused_image_host.h"

static int swatches;
static int labels;

static int height(void *context, int font, const char *typeface,
                  size_t typeface_length)
{
    (void)context; (void)typeface;
    assert(font == 14 && typeface_length == 0);
    return 12;
}

static void fill(void *context, float x, float y, float width,
                 float h, float radius, int segments,
                 uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    (void)context; (void)radius; (void)segments; (void)a;
    if (!(x >= 10 && y >= 10 && x + width <= 190 && y + h <= 250))
        fprintf(stderr, "fill out of bounds: %g %g %g %g\n",
                x, y, width, h);
    assert(x >= 10 && y >= 10 && x + width <= 190 && y + h <= 250);
    if (y == 214 && h == 36) {
        assert(r == 64 && g == 128 && b == 191);
        swatches++;
    }
}

static void outline(void *context, float x, float y, float width,
                    float h, float radius, int segments, float line_width,
                    uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    (void)context; (void)x; (void)y; (void)width; (void)h;
    (void)radius; (void)segments; (void)line_width;
    (void)r; (void)g; (void)b; (void)a;
}

static void line(void *context, float x1, float y1, float x2, float y2,
                 uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    (void)context; (void)x1; (void)y1; (void)x2; (void)y2;
    (void)r; (void)g; (void)b; (void)a; assert(0);
}

static void text(void *context, const char *value, size_t bytes,
                 int x, int y, int font,
                 uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    (void)context; (void)value; (void)bytes; (void)x; (void)y;
    (void)font; (void)r; (void)g; (void)b; (void)a; assert(0);
}

static void clipped(void *context, const char *value, size_t bytes,
                    int x, int y, int font, const float clip[4],
                    uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    (void)context; (void)value; (void)x; (void)y;
    (void)r; (void)g; (void)b; (void)a;
    assert(bytes > 0 && font == 14);
    assert(clip[0] >= 10 && clip[1] >= 10 &&
           clip[0] + clip[2] <= 190 && clip[1] + clip[3] <= 250);
    labels++;
}

static void run(Bundle *bundle)
{
    FontMeasurer fonts = {NULL, height, NULL};
    RoundedRectangleRenderer shape = {fill, outline, NULL};
    LineRenderer lines = {line, NULL};
    TextRenderer texts = {text, NULL, clipped};
    HostBinding bindings[] = {
        MeasureGlyphLineHeightBinding(&fonts),
        RasterRoundedRectangleBinding(&shape),
        RasterRoundedRectangleOutlineBinding(&shape),
        RasterLineBinding(&lines),
        RasterTextBinding(&texts),
        RasterTextClippedBinding(&texts),
        RasterImageBinding(&unused_image_renderer),
    };
    BundleInstance *instance = BundleInstantiate(bundle, bindings, 7);
    assert(instance != NULL);
    long long value = -1;
    int has_value = 0;
    for (int phase = 0; phase < 5; phase++) {
        assert(BundleInstanceRun(instance, &value, &has_value));
        if (!has_value || value != phase)
            fprintf(stderr, "ColorPicker phase %d returned %lld\n",
                    phase, value);
        assert(has_value && value == phase);
        assert(swatches == phase + 1 && labels == (phase + 1) * 9);
    }
    BundleInstanceClose(instance);
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    Bundle *bundle = BundleOpen(argv[1]);
    assert(bundle != NULL);
    run(bundle);
    BundleClose(bundle);
    return 0;
}
