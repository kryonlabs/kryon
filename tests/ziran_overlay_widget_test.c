#include "kryon_portable_host.h"

#include <assert.h>
#include <stdio.h>
#include "unused_image_host.h"

static int fills;

static void fill(void *context, float x, float y, float width,
                 float height, float radius, int segments,
                 uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    (void)context;
    assert(x == 0 && y == 0 && width == 240 && height == 120);
    assert(radius == 0 && segments == 4);
    assert(r == 0 && g == 0 && b == 0 && a == 160);
    fills++;
}

static void outline(void *context, float x, float y, float width,
                    float height, float radius, int segments,
                    float line_width, uint8_t r, uint8_t g,
                    uint8_t b, uint8_t a)
{
    (void)context; (void)x; (void)y; (void)width; (void)height;
    (void)radius; (void)segments; (void)line_width;
    (void)r; (void)g; (void)b; (void)a;
    assert(0);
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
    (void)context; (void)value; (void)bytes; (void)x; (void)y;
    (void)font; (void)clip; (void)r; (void)g; (void)b; (void)a;
    assert(0);
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    Bundle *bundle = BundleOpen(argv[1]);
    assert(bundle != NULL);
    RoundedRectangleRenderer shape = {fill, outline, NULL};
    LineRenderer lines = {line, NULL};
    TextRenderer texts = {text, NULL, clipped};
    HostBinding bindings[] = {
        RasterRoundedRectangleBinding(&shape),
        RasterRoundedRectangleOutlineBinding(&shape),
        RasterLineBinding(&lines),
        RasterTextBinding(&texts),
        RasterTextClippedBinding(&texts),
        RasterImageBinding(&unused_image_renderer),
    };
    BundleInstance *instance = BundleInstantiate(bundle, bindings, 6);
    assert(instance != NULL);
    long long value = -1;
    int has_value = 0;
    for (int phase = 0; phase < 6; phase++) {
        assert(BundleInstanceRun(instance, &value, &has_value));
        if (!has_value || value != phase)
            fprintf(stderr, "Overlay phase %d returned %lld\n", phase, value);
        assert(has_value && value == phase);
        assert(fills == phase + 1);
    }
    BundleInstanceClose(instance);
    BundleClose(bundle);
    return 0;
}
