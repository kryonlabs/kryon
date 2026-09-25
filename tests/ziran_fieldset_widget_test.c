#include "kryon_portable_host.h"

#include <assert.h>
#include <string.h>

static int fills, outlines, labels;

static int width(void *context, uint8_t *value, size_t bytes,
                 int font, uint8_t *typeface, size_t typeface_length)
{
    (void)context; (void)typeface;
    assert(bytes == 5 && memcmp(value, "Group", 5) == 0);
    assert(font == 18 && typeface_length == 0);
    return 35;
}

static void fill(void *context, float x, float y, float w, float h,
                 float radius, int segments,
                 uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    (void)context;
    if(fills < 2) {
        assert(r == 0x12 && g == 0x34 && b == 0x56 && a == 127);
    }
    if(fills == 1) {
        assert(x == 18 && y == 22 && w == 51 && h == 18);
        assert(radius == 0 && segments == 4);
    }
    fills++;
}

static void outline(void *context, float x, float y, float w, float h,
                    float radius, int segments, float line_width,
                    uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    (void)context; (void)x; (void)y; (void)w; (void)h;
    (void)r; (void)g; (void)b; (void)a;
    assert(radius == 6 && segments == 12 && line_width == 1);
    outlines++;
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
    (void)context;
    assert(bytes == 5 && memcmp(value, "Group", 5) == 0);
    assert(x == 26 && y == 21 && font == 18);
    assert(clip[0] == 26 && clip[1] == 21 &&
           clip[2] == 35 && clip[3] == 18);
    assert(r == 0xaa && g == 0xbb && b == 0xcc && a == 127);
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
    FontMeasurer fonts = {width, NULL, NULL};
    RoundedRectangleRenderer shape = {fill, outline, NULL};
    LineRenderer line_host = {line, NULL};
    TextRenderer text_host = {unclipped, NULL, clipped};
    ImageRasterizer raster_image = {NULL, image, NULL};
    HostBinding bindings[] = {
        MeasureGlyphWidthBinding(&fonts),
        RasterRoundedRectangleBinding(&shape),
        RasterRoundedRectangleOutlineBinding(&shape),
        RasterLineBinding(&line_host),
        RasterTextBinding(&text_host),
        RasterTextClippedBinding(&text_host),
        RasterImageBinding(&raster_image),
    };
    BundleInstance *instance = BundleInstantiate(bundle, bindings, 7);
    assert(instance != NULL);
    long long result = -1;
    int has_result = 0;
    assert(BundleInstanceRun(instance, &result, &has_result));
    assert(has_result && result == 7);
    assert(fills == 3 && outlines == 2 && labels == 1);
    BundleInstanceClose(instance);
    BundleClose(bundle);
    return 0;
}
