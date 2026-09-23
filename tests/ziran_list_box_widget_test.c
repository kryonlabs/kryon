#include "kryon_portable_host.h"

#include <assert.h>
#include <stdio.h>
#include "unused_image_host.h"

static int labels;

static void rounded(void *context, float x, float y, float w, float h,
                    float radius, int segments,
                    uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    (void)context; (void)radius; (void)segments;
    (void)r; (void)g; (void)b; (void)a;
    assert(w >= 0 && h >= 0 && x >= 10 && y >= 10 &&
           x + w <= 220 && y + h <= 70);
}

static void outline(void *context, float x, float y, float w, float h,
                    float radius, int segments, float line_width,
                    uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    (void)context; (void)radius; (void)segments; (void)line_width;
    (void)r; (void)g; (void)b; (void)a;
    assert(w >= 0 && h >= 0 && x >= 10 && y >= 10 &&
           x + w <= 220 && y + h <= 70);
}

static void line(void *context, float x1, float y1, float x2, float y2,
                 uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    (void)context; (void)x1; (void)y1; (void)x2; (void)y2;
    (void)r; (void)g; (void)b; (void)a;
    assert(0 && "unexpected line");
}

static void text(void *context, const char *value, size_t bytes,
                 int x, int y, int font,
                 uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    (void)context; (void)value; (void)bytes; (void)x; (void)y;
    (void)font; (void)r; (void)g; (void)b; (void)a;
    assert(0 && "unexpected unclipped text");
}

static void clipped(void *context, const char *value, size_t bytes,
                    int x, int y, int font, const float clip[4],
                    uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    (void)context; (void)value; (void)x; (void)y;
    (void)r; (void)g; (void)b; (void)a;
    assert(bytes == 1 && font == 14);
    assert((clip[0] >= 10 && clip[0] + clip[2] <= 110 &&
            clip[1] >= 10 && clip[1] + clip[3] <= 52) ||
           (clip[0] >= 120 && clip[0] + clip[2] <= 220 &&
            clip[1] >= 10 && clip[1] + clip[3] <= 70));
    labels++;
}

static int height(void *context, int font, const char *typeface,
                  size_t typeface_length)
{
    (void)context;
    (void)typeface;
    assert(font == 14 && typeface_length == 0);
    return 12;
}

static void run(Bundle *bundle)
{
    FontMeasurer fonts = {NULL, height, NULL};
    RoundedRectangleRenderer shape = {rounded, outline, NULL};
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
    for (int phase = 0; phase < 6; phase++) {
        assert(BundleInstanceRun(instance, &value, &has_value));
        if (!has_value || value != phase)
            fprintf(stderr, "ListBox phase %d returned %lld\n", phase, value);
        assert(has_value && value == phase);
        assert(labels == (phase + 1) * 6);
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
