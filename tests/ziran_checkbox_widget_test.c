#include "kryon_portable_host.h"

#include <assert.h>
#include <string.h>

static int fills, outlines, lines, labels;

static int width(void *context, const char *value, size_t length,
                 int font, const char *typeface, size_t typeface_length)
{
    (void)context; (void)typeface;
    assert(length == 5 && memcmp(value, "Agree", length) == 0);
    assert(font == 14 && typeface_length == 0);
    return 35;
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
    (void)context; (void)a;
    assert(x == 11 && y == 25 && w == 20 && h == 20);
    assert(radius == 0.18f && segments == 8);
    assert(r == 0x12 && g == 0x34 && b == 0x56);
    fills++;
}

static void outline(void *context, float x, float y, float w, float h,
                    float radius, int segments, float line_width,
                    uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    (void)context; (void)g; (void)b; (void)a;
    assert(x == 11 && y == 25 && w == 20 && h == 20);
    assert(radius == 0.18f && segments == 8 && line_width == 1);
    assert(r == (outlines == 0 ? 0x73 : 0x12));
    outlines++;
}

static void line(void *context, float x1, float y1, float x2, float y2,
                 uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    (void)context; (void)a;
    assert(x1 >= 16 && x1 <= 20 && y1 >= 30 && y1 <= 41);
    assert(x2 > x1 && r == 255 && g == 255 && b == 255);
    lines++;
}

static void clipped_text(void *context, const char *value, size_t length,
                         int x, int y, int font, const float clip[4],
                         uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    (void)context; (void)a;
    assert(length == 5 && memcmp(value, "Agree", length) == 0);
    assert(x == 42 && y == 29 && font == 14);
    assert(clip[0] == 10 && clip[1] == 20 &&
           clip[2] == 120 && clip[3] == 30);
    assert(r == 0x17 && g == 0x17 && b == 0x17);
    labels++;
}

static void unexpected_text(void *context, const char *value,
                            size_t length, int x, int y, int font,
                            uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    (void)context; (void)value; (void)length;
    (void)x; (void)y; (void)font;
    (void)r; (void)g; (void)b; (void)a;
    assert(0 && "Checkbox label must be clipped");
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
    LineRenderer line_host = {line, NULL};
    TextRenderer text_host = {unexpected_text, NULL, clipped_text};
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
    long long result = -1;
    int has_result = 0;
    assert(BundleInstanceRun(instance, &result, &has_result));
    assert(has_result && result == 0 && fills == 0 && outlines == 1 &&
           lines == 0 && labels == 1);
    assert(BundleInstanceRun(instance, &result, &has_result));
    assert(has_result && result == 1 && fills == 1 && outlines == 2 &&
           lines == 4 && labels == 2);
    assert(BundleInstanceRun(instance, &result, &has_result));
    assert(has_result && result == 2 && fills == 2 && outlines == 3 &&
           lines == 8 && labels == 3);
    BundleInstanceClose(instance);
    BundleClose(bundle);
    return 0;
}
