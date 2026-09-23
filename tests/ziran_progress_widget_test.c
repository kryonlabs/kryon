#include "kryon_portable_host.h"

#include <assert.h>
#include <string.h>
#include "unused_image_host.h"

static int measures;
static int draws;

static void
line(void *context, float x1, float y1, float x2, float y2,
     uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    (void)context; (void)x1; (void)y1; (void)x2; (void)y2;
    (void)r; (void)g; (void)b; (void)a;
    assert(0 && "Progress should not draw separator lines");
}

static int
width(void *context, const char *text, size_t text_length, int font,
      const char *typeface, size_t typeface_length)
{
    (void)context;
    assert(measures++ == 0 && font == 14);
    assert(text_length == 3 && memcmp(text, "50%", 3) == 0);
    assert(typeface_length == 4 && memcmp(typeface, "body", 4) == 0);
    return 20;
}

static int
line_height(void *context, int font,
            const char *typeface, size_t typeface_length)
{
    (void)context;
    assert(measures++ == 1 && font == 14);
    assert(typeface_length == 4 && memcmp(typeface, "body", 4) == 0);
    return 10;
}

static void
fill(void *context, float x, float y, float width, float height,
     float radius, int segments, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    (void)context;
    assert(x == 10 && y == 20 && height == 20);
    assert(radius == 0.25f && segments == 12);
    if(draws == 0 || draws == 4) {
        assert(width == 100 && r == 0x11 && g == 0x22 &&
               b == 0x33 && a == 0x44);
    } else {
        assert(draws == 1 && width == 25 && r == 0x55 && g == 0x66 &&
               b == 0x77 && a == 0x88);
    }
    draws++;
}

static void
outline(void *context, float x, float y, float width, float height,
        float radius, int segments, float line_width,
        uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    (void)context;
    assert(draws++ == 2);
    assert(x == 10 && y == 20 && width == 100 && height == 20);
    assert(radius == 0.25f && segments == 12 && line_width == 2);
    assert(r == 0x99 && g == 0xaa && b == 0xbb && a == 0xcc);
}

static void
text(void *context, const char *value, size_t length, int x, int y,
     int font, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    (void)context;
    assert(draws++ == 3);
    assert(length == 3 && memcmp(value, "50%", 3) == 0);
    assert(x == 41 && y == 25 && font == 14);
    assert(r == 0x10 && g == 0x20 && b == 0x30 && a == 0x40);
}

int
main(int argc, char **argv)
{
    assert(argc == 2);
    Bundle *bundle = BundleOpen(argv[1]);
    assert(bundle != NULL && BundleCapabilityCount(bundle) == 7);
    FontMeasurer fonts = {width, line_height, NULL};
    RoundedRectangleRenderer shapes = {fill, outline, NULL};
    TextRenderer labels = {text, NULL};
    LineRenderer lines = {line, NULL};
    HostBinding bindings[] = {
        MeasureGlyphWidthBinding(&fonts),
        MeasureGlyphLineHeightBinding(&fonts),
        RasterRoundedRectangleBinding(&shapes),
        RasterRoundedRectangleOutlineBinding(&shapes),
        RasterTextBinding(&labels),
        RasterLineBinding(&lines),
        RasterImageBinding(&unused_image_renderer),
    };
    long long result = 0;
    int has_result = 0;
    assert(!BundleRun(bundle, bindings, 6, &result, &has_result));
    assert(measures == 0 && draws == 0);
    assert(BundleRun(bundle, bindings, 7, &result, &has_result));
    assert(has_result && result == 42 && measures == 2 && draws == 5);
    BundleClose(bundle);
    return 0;
}
