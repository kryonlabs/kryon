#include "kryon_portable_host.h"

#include <assert.h>
#include <string.h>

static int fills, outlines, labels;

static int width(void *context, const char *value, size_t length,
                 int font, const char *typeface, size_t typeface_length)
{
    (void)context; (void)typeface;
    assert(length == 4 && memcmp(value, "Gain", length) == 0);
    assert(font == 14 && typeface_length == 0);
    return 28;
}

static int height(void *context, int font,
                  const char *typeface, size_t typeface_length)
{
    (void)context; (void)typeface;
    assert(font == 14 && typeface_length == 0);
    return 12;
}

static void fill(void *context, float x, float y, float w, float h,
                 float radius, int segments,
                 uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    (void)context; (void)h; (void)a;
    assert(radius == 0.5f && segments == 16);
    if(fills % 6 == 0) {
        assert(x == 10 && y == 69 && w == 100);
        assert(r == 0x12 && g == 0x34 && b == 0x56);
    }
    fills++;
}

static void outline(void *context, float x, float y, float w, float h,
                    float radius, int segments, float line_width,
                    uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    (void)context; (void)x; (void)y; (void)g; (void)b; (void)a;
    assert(radius == 0.5f && segments == 16 && line_width == 1);
    assert(w == 22 && h == 22 && r == 0x94);
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

static void unexpected_text(void *context, const char *value,
                            size_t length, int x, int y, int font,
                            uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    (void)context; (void)value; (void)length;
    (void)x; (void)y; (void)font;
    (void)r; (void)g; (void)b; (void)a;
    assert(0 && "Slider label must be clipped");
}

static void text(void *context, const char *value, size_t length,
                 int x, int y, int font, const float clip[4],
                 uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    (void)context;
    assert(length == 4 && memcmp(value, "Gain", length) == 0);
    assert(x == 16 && y == 20 && font == 14);
    assert(clip[0] == 10 && clip[1] == 20 &&
           clip[2] == 88 && clip[3] == 12);
    assert(r == 0xaa && g == 0xbb && b == 0xcc && a == 127);
    labels++;
}

static void unexpected_image(void *context, const char *path,
                             size_t length, uint32_t texture_id,
                             const float source[4],
                             const float destination[4],
                             const float clip[4], const float origin[2],
                             float rotation, float radius,
                             const uint8_t tint[4])
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
        assert(fills == (phase + 1) * 6 &&
               outlines == (phase + 1) * 2 &&
               labels == phase + 1);
    }
    BundleInstanceClose(instance);
    BundleClose(bundle);
    return 0;
}
