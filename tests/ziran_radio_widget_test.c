#include "kryon_portable_host.h"

#include <assert.h>
#include <string.h>

static int fills, outlines, labels;

static int width(void *context, uint8_t *value, size_t length,
                 int font, uint8_t *typeface, size_t typeface_length)
{
    (void)context; (void)typeface;
    assert(length == 3 && font == 14 && typeface_length == 0);
    assert(memcmp(value, "One", length) == 0 ||
           memcmp(value, "Two", length) == 0);
    return 21;
}

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
    (void)context;
    assert(radius == 0.5f && segments == 32);
    assert(x == 22 && y == (fills < 2 ? 27 : 67));
    assert(w == 16 && h == 16 &&
           r == 0x12 && g == 0x34 && b == 0x56 && a == 127);
    fills++;
}

static void outline(void *context, float x, float y, float w, float h,
                    float radius, int segments, float line_width,
                    uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    (void)context; (void)g; (void)b;
    assert(radius == 0.5f && segments == 32 && line_width == 2);
    assert(x == 20 && y == (outlines % 2 == 0 ? 25 : 65));
    assert(w == 20 && h == 20);
    if(outlines < 4) {
        assert(r == (outlines % 2 == 0 ? 0x12 : 0x73));
        assert(a == (outlines % 2 == 0 ? 127 : 255));
    } else {
        assert(r == (outlines % 2 == 0 ? 0x73 : 0x12));
        assert(a == (outlines % 2 == 0 ? 255 : 127));
    }
    outlines++;
}

static void unexpected_line(void *context, float x1, float y1,
                            float x2, float y2, uint8_t r, uint8_t g,
                            uint8_t b, uint8_t a)
{
    (void)context; (void)x1; (void)y1; (void)x2; (void)y2;
    (void)r; (void)g; (void)b; (void)a;
    assert(0 && "unexpected line");
}

static void unexpected_text(void *context, uint8_t *value,
                            size_t length, int x, int y, int font,
                            uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    (void)context; (void)value; (void)length;
    (void)x; (void)y; (void)font;
    (void)r; (void)g; (void)b; (void)a;
    assert(0 && "Radio label must be clipped");
}

static void text(void *context, uint8_t *value, size_t length,
                 int x, int y, int font, float clip[4],
                 uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    (void)context; (void)a;
    assert(length == 3 && x == 58 && font == 14);
    assert(memcmp(value, labels % 2 == 0 ? "One" : "Two", length) == 0);
    assert(y == (labels % 2 == 0 ? 29 : 69));
    assert(clip[0] == 10 && clip[2] == 120 && clip[3] == 30);
    assert(r == 0xaa && g == 0xbb && b == 0xcc);
    labels++;
}

static void unexpected_image(void *context, uint8_t *path,
                             size_t length, uint32_t texture_id,
                             float source[4],
                             float destination[4],
                             float clip[4], float origin[2],
                             float rotation, float radius,
                             uint8_t tint[4])
{
    (void)context; (void)path; (void)length; (void)texture_id;
    (void)source; (void)destination; (void)clip; (void)origin;
    (void)rotation; (void)radius; (void)tint;
    assert(0 && "unexpected image");
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    Bundle *bundle = BundleOpen(argv[1]);
    assert(bundle != NULL);
    FontMeasurer fonts = {width, height, NULL};
    RoundedRectangleRenderer shape = {fill, outline, NULL};
    LineRenderer line_host = {unexpected_line, NULL};
    TextRenderer text_host = {unexpected_text, NULL, text};
    ImageRasterizer image = {NULL, unexpected_image, NULL};
    HostBinding bindings[] = {
        MeasureGlyphWidthBinding(&fonts),
        MeasureGlyphLineHeightBinding(&fonts),
        RasterRoundedRectangleBinding(&shape),
        RasterRoundedRectangleOutlineBinding(&shape),
        RasterLineBinding(&line_host),
        RasterTextBinding(&text_host),
        RasterTextClippedBinding(&text_host),
        RasterImageBinding(&image),
    };
    BundleInstance *instance = BundleInstantiate(bundle, bindings, 8);
    assert(instance != NULL);
    for(int phase = 0; phase < 4; phase++) {
        long long result = -1;
        int has_result = 0;
        assert(BundleInstanceRun(instance, &result, &has_result));
        assert(has_result && result == phase);
        assert(fills == phase + 1 &&
               outlines == (phase + 1) * 2 &&
               labels == (phase + 1) * 2);
    }
    BundleInstanceClose(instance);
    BundleClose(bundle);
    return 0;
}
