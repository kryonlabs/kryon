#include "kryon_portable_host.h"

#include <assert.h>
#include <string.h>

static int fills, labels, images;

static int image_size(void *context, uint8_t *path, size_t length,
                      int *width, int *height)
{
    (void)context;
    assert(length == 9 && memcmp(path, "badge.png", length) == 0);
    *width = 32;
    *height = 16;
    return 1;
}

static int width(void *context, uint8_t *value, size_t length,
                 int font, uint8_t *typeface, size_t typeface_length)
{
    (void)context; (void)typeface;
    assert(length == 3 && memcmp(value, "Run", 3) == 0);
    assert(font == 14 && typeface_length == 0);
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
    assert(x == 10 && y == 20 && w == 80 && h == 30);
    assert(radius == 4 && segments == 12);
    assert(r == 0x12 && g == 0x34 && b == 0x56 && a == 127);
    fills++;
}

static void text(void *context, uint8_t *value, size_t length,
                 int x, int y, int font, float clip[4],
                 uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    (void)context;
    assert(length == 3 && memcmp(value, "Run", 3) == 0);
    assert(x == (images == 0 ? 39 : 26) && y == 29 && font == 14);
    assert(clip[0] == 10 && clip[1] == 20 &&
           clip[2] == 80 && clip[3] == 30);
    assert(r == 0xaa && g == 0xbb && b == 0xcc && a == 127);
    labels++;
}

static void unexpected_line(void *context, float x1, float y1,
                            float x2, float y2, uint8_t r, uint8_t g,
                            uint8_t b, uint8_t a)
{
    (void)context; (void)x1; (void)y1; (void)x2; (void)y2;
    (void)r; (void)g; (void)b; (void)a;
    assert(0 && "Button should not draw a line");
}

static void unexpected_outline(void *context, float x, float y, float w,
                               float h, float radius, int segments,
                               float line_width, uint8_t r, uint8_t g,
                               uint8_t b, uint8_t a)
{
    (void)context; (void)x; (void)y; (void)w; (void)h;
    (void)radius; (void)segments; (void)line_width;
    (void)r; (void)g; (void)b; (void)a;
    assert(0 && "Button should not draw an outline");
}

static void unexpected_text(void *context, uint8_t *value,
                            size_t length, int x, int y, int font,
                            uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    (void)context; (void)value; (void)length;
    (void)x; (void)y; (void)font;
    (void)r; (void)g; (void)b; (void)a;
    assert(0 && "Button should clip its label");
}

static void draw_image(void *context, uint8_t *path,
                             size_t length, uint32_t texture_id,
                             float source[4],
                             float destination[4],
                             float clip[4], float origin[2],
                             float rotation, float radius,
                             uint8_t tint[4])
{
    (void)context;
    assert(length == 9 && memcmp(path, "badge.png", length) == 0);
    assert(texture_id == 0 && source[0] == 0 && source[1] == 0 &&
           source[2] == 32 && source[3] == 16);
    assert(destination[0] == 55.5f && destination[1] == 30.5f &&
           destination[2] == 18 && destination[3] == 9);
    assert(clip[0] == 10 && clip[1] == 20 &&
           clip[2] == 80 && clip[3] == 30);
    assert(origin[0] == 0 && origin[1] == 0 &&
           rotation == 0 && radius == 4);
    assert(tint[0] == 255 && tint[1] == 255 &&
           tint[2] == 255 && tint[3] == 127);
    images++;
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    Bundle *bundle = BundleOpen(argv[1]);
    assert(bundle != NULL);
    FontMeasurer fonts = {width, height, NULL};
    RoundedRectangleRenderer shape = {fill, unexpected_outline, NULL};
    TextRenderer labels_host = {unexpected_text, NULL, text};
    LineRenderer line = {unexpected_line, NULL};
    ImageRasterizer image = {image_size, draw_image, NULL};
    HostBinding bindings[] = {
        MeasureGlyphWidthBinding(&fonts),
        MeasureGlyphLineHeightBinding(&fonts),
        RasterRoundedRectangleBinding(&shape),
        RasterRoundedRectangleOutlineBinding(&shape),
        RasterTextBinding(&labels_host),
        RasterTextClippedBinding(&labels_host),
        RasterLineBinding(&line),
        RasterImageBinding(&image),
        ImageWidthBinding(&image),
        ImageHeightBinding(&image),
    };
    BundleInstance *instance = BundleInstantiate(bundle, bindings, 10);
    assert(instance != NULL);
    long long result = 0;
    int has_result = 0;
    assert(BundleInstanceRun(instance, &result, &has_result));
    assert(has_result && result == 0 && fills == 1 && labels == 1);
    assert(BundleInstanceRun(instance, &result, &has_result));
    assert(has_result && result == 42 && fills == 2 && labels == 2);
    assert(BundleInstanceRun(instance, &result, &has_result));
    assert(has_result && result == 43 && fills == 3 && labels == 3);
    assert(BundleInstanceRun(instance, &result, &has_result));
    assert(has_result && result == 44 && fills == 4 &&
           labels == 4 && images == 1);
    BundleInstanceClose(instance);
    BundleClose(bundle);
    return 0;
}
