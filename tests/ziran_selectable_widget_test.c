#include "kryon_portable_host.h"

#include <assert.h>
#include <string.h>

static int fills, labels;

static int height(void *context, int font,
                  uint8_t *typeface, size_t typeface_length)
{
    (void)context; (void)typeface;
    assert(font == 14 && typeface_length == 0);
    return 12;
}

static void fill(void *context, float x, float y, float w, float h,
                 float radius, int segments,
                 uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    (void)context; (void)a;
    assert(x == 10 && y == 20 && w == 100 && h == 36);
    assert(radius == 4 && segments == 12);
    assert(r == 0x12 && g == 0x34 && b == 0x56);
    fills++;
}

static void outline(void *context, float x, float y, float w, float h,
                    float radius, int segments, float line_width,
                    uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    (void)context; (void)x; (void)y; (void)w; (void)h;
    (void)radius; (void)segments; (void)line_width;
    (void)r; (void)g; (void)b; (void)a; assert(0);
}

static void line(void *context, float x1, float y1, float x2, float y2,
                 uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    (void)context; (void)x1; (void)y1; (void)x2; (void)y2;
    (void)r; (void)g; (void)b; (void)a; assert(0);
}

static void unclipped(void *context, uint8_t *value, size_t bytes,
                      int x, int y, int font,
                      uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    (void)context; (void)value; (void)bytes; (void)x; (void)y;
    (void)font; (void)r; (void)g; (void)b; (void)a; assert(0);
}

static void clipped(void *context, uint8_t *value, size_t bytes,
                    int x, int y, int font, float clip[4],
                    uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    (void)context; (void)a;
    assert(bytes == 5 && memcmp(value, "Alpha", 5) == 0);
    assert(x == 18 && y == 32 && font == 14);
    assert(clip[0] == 10 && clip[1] == 20 &&
           clip[2] == 100 && clip[3] == 36);
    assert(r == 0xaa && g == 0xbb && b == 0xcc);
    labels++;
}

static void image(void *context, uint8_t *path, size_t bytes,
                  uint32_t id, float source[4],
                  float destination[4], float clip[4],
                  float origin[2], float rotation, float radius,
                  uint8_t tint[4])
{
    (void)context; (void)path; (void)bytes; (void)id; (void)source;
    (void)destination; (void)clip; (void)origin;
    (void)rotation; (void)radius; (void)tint; assert(0);
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    Bundle *bundle = BundleOpen(argv[1]);
    assert(bundle != NULL);
    FontMeasurer fonts = {NULL, height, NULL};
    RoundedRectangleRenderer shape = {fill, outline, NULL};
    LineRenderer line_host = {line, NULL};
    TextRenderer text_host = {unclipped, NULL, clipped};
    ImageRasterizer raster_image = {NULL, image, NULL};
    HostBinding bindings[] = {
        MeasureGlyphLineHeightBinding(&fonts),
        RasterRoundedRectangleBinding(&shape),
        RasterRoundedRectangleOutlineBinding(&shape),
        RasterLineBinding(&line_host),
        RasterTextBinding(&text_host),
        RasterTextClippedBinding(&text_host),
        RasterImageBinding(&raster_image),
    };
    BundleInstance *instance = BundleInstantiate(bundle, bindings, 7);
    assert(instance != NULL);
    const int expected_fills[] = {0, 1, 2, 2};
    for(int phase = 0; phase < 4; phase++) {
        long long result = -1;
        int has_result = 0;
        assert(BundleInstanceRun(instance, &result, &has_result));
        assert(has_result && result == phase);
        assert(fills == expected_fills[phase] && labels == phase + 1);
    }
    BundleInstanceClose(instance);
    BundleClose(bundle);
    return 0;
}
