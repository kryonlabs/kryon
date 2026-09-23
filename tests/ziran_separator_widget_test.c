#include "kryon_portable_host.h"

#include <assert.h>
#include <string.h>

static int lines;
static int labels;
static int measures;

static void line(void *context, float x1, float y1, float x2, float y2,
                 uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    (void)context;
    assert(r == 0x11 && g == 0x22 && b == 0x33 && a == 0x44);
    if(lines == 0 || lines == 2)
        assert(x1 == 20 && y1 == 20 && x2 == 20 && y2 == 60);
    else
        assert(x1 == 58 && y1 == 30 && x2 == 140 && y2 == 30);
    lines++;
}

static void text(void *context, const char *value, size_t length,
                 int x, int y, int font, uint8_t r, uint8_t g,
                 uint8_t b, uint8_t a)
{
    (void)context;
    assert(length == 1 && memcmp(value, "A", length) == 0);
    assert(x == 40 && y == 23 && font == 14);
    assert(r == 0xaa && g == 0xbb && b == 0xcc && a == 0x6e);
    labels++;
}

static int width(void *context, const char *value, size_t length,
                 int font, const char *typeface, size_t typeface_length)
{
    (void)context; (void)typeface; (void)typeface_length;
    assert(length == 1 && memcmp(value, "A", length) == 0 && font == 14);
    measures++;
    return 10;
}

static int height(void *context, int font, const char *typeface,
                  size_t typeface_length)
{
    (void)context; (void)font; (void)typeface; (void)typeface_length;
    assert(0 && "Separator should not measure line height");
    return 0;
}

static void rounded(void *context, float x, float y, float width,
                    float height, float radius, int segments,
                    uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    (void)context; (void)x; (void)y; (void)width; (void)height;
    (void)radius; (void)segments; (void)r; (void)g; (void)b; (void)a;
    assert(0 && "Separator should not draw rounded rectangles");
}

static void outline(void *context, float x, float y, float width,
                    float height, float radius, int segments,
                    float line_width, uint8_t r, uint8_t g,
                    uint8_t b, uint8_t a)
{
    (void)context; (void)x; (void)y; (void)width; (void)height;
    (void)radius; (void)segments; (void)line_width;
    (void)r; (void)g; (void)b; (void)a;
    assert(0 && "Separator should not draw rounded outlines");
}

static void run(Bundle *bundle)
{
    LineRenderer lines_host = {line, NULL};
    TextRenderer text_host = {text, NULL};
    FontMeasurer font_host = {width, height, NULL};
    RoundedRectangleRenderer shape_host = {rounded, outline, NULL};
    HostBinding bindings[] = {
        RasterLineBinding(&lines_host),
        RasterTextBinding(&text_host),
        MeasureGlyphWidthBinding(&font_host),
        RasterRoundedRectangleBinding(&shape_host),
        RasterRoundedRectangleOutlineBinding(&shape_host),
    };
    BundleInstance *instance = BundleInstantiate(bundle, bindings, 5);
    assert(instance != NULL);
    long long value = -1;
    int has_value = 0;
    assert(BundleInstanceRun(instance, &value, &has_value));
    assert(has_value && value == 0 && lines == 0 && labels == 0 &&
           measures == 1);
    assert(BundleInstanceRun(instance, &value, &has_value));
    assert(has_value && value == 1 && lines == 2 && labels == 1 &&
           measures == 1);
    assert(BundleInstanceRun(instance, &value, &has_value));
    assert(has_value && value == 2 && lines == 3 && labels == 1 &&
           measures == 1);
    BundleInstanceClose(instance);
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    Bundle *bundle = BundleOpen(argv[1]);
    assert(bundle != NULL);
    assert(BundleCapabilityCount(bundle) == 5);
    run(bundle);
    BundleClose(bundle);
    return 0;
}
