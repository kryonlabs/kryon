#include "kryon_portable_host.h"

#include <assert.h>
#include <string.h>

static char *expected[] = {"Title", "Body", "A", "BB", "Plain", "Clipped"};
static const int expected_x[] = {10, 10, 5, 5, 10, 30};
static const int expected_y[] = {20, 40, 5, 20, 10, 30};
static const int expected_clip_x[] = {10, 10, 5, 5, 20, 30};
static const int expected_clip_y[] = {20, 40, 5, 20, 20, 30};
static const int expected_clip_w[] = {50, 190, 10, 20, 40, 30};
static const int expected_clip_h[] = {12, 12, 12, 12, 40, 30};
static const int expected_font[] = {24, 16, 16, 16, 16, 16};
static int texts;
static int images;

static int width(void *context, uint8_t *value, size_t length,
                 int font, uint8_t *typeface, size_t typeface_length)
{
    (void)context; (void)value; (void)typeface;
    assert((font == 24 || font == 16) && typeface_length == 0);
    return (int)length * 10;
}

static int line_height(void *context, int font,
                       uint8_t *typeface, size_t typeface_length)
{
    (void)context; (void)typeface;
    assert((font == 24 || font == 16) && typeface_length == 0);
    return 12;
}

static void draw_text_clipped(void *context, uint8_t *value,
                              size_t length, int x, int y, int font,
                              float clip[4], uint8_t r, uint8_t g,
                              uint8_t b, uint8_t a)
{
    (void)context;
    int index = texts;
    assert(index < 6);
    assert(strlen(expected[index]) == length &&
           memcmp(expected[index], value, length) == 0);
    assert(x == expected_x[index] && y == expected_y[index]);
    assert(font == expected_font[index] && r == 0x17 &&
           g == 0x17 && b == 0x17 && a == 255);
    assert(clip[0] == expected_clip_x[index] &&
           clip[1] == expected_clip_y[index] &&
           clip[2] == expected_clip_w[index] &&
           clip[3] == expected_clip_h[index]);
    texts++;
}

static void fill(void *context, float x, float y, float w, float h,
                 float radius, int segments,
                 uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    (void)context;
    (void)x; (void)y; (void)w; (void)h;
    (void)radius; (void)segments;
    (void)r; (void)g; (void)b; (void)a;
    assert(0 && "Page text should not draw a fill");
}

static void unexpected_line(void *context, float x1, float y1,
                            float x2, float y2, uint8_t r, uint8_t g,
                            uint8_t b, uint8_t a)
{
    (void)context; (void)x1; (void)y1; (void)x2; (void)y2;
    (void)r; (void)g; (void)b; (void)a;
    assert(0 && "Text should not draw a line");
}

static void unexpected_outline(void *context, float x, float y, float w,
                               float h, float radius, int segments,
                               float line_width, uint8_t r, uint8_t g,
                               uint8_t b, uint8_t a)
{
    (void)context; (void)x; (void)y; (void)w; (void)h;
    (void)radius; (void)segments; (void)line_width;
    (void)r; (void)g; (void)b; (void)a;
    assert(0 && "Text should not draw an outline");
}

static void draw_image_clipped(void *context, uint8_t *path,
                             size_t length, uint32_t texture_id,
                             float source[4],
                             float destination[4],
                             float clip[4], float origin[2],
                             float rotation, float radius,
                             uint8_t tint[4])
{
    (void)context;
    assert(path != NULL && length == 0 && texture_id == 7);
    assert(source[0] == 0 && source[1] == 0 &&
           source[2] == 70 && source[3] == 70);
    assert(destination[0] == 10 && destination[1] == 10 &&
           destination[2] == 70 && destination[3] == 70);
    assert(clip[0] == 20 && clip[1] == 20 &&
           clip[2] == 40 && clip[3] == 40);
    assert(origin[0] == 0 && origin[1] == 0 &&
           rotation == 0 && radius == 0);
    assert(tint[0] == 0x17 && tint[1] == 0x17 &&
           tint[2] == 0x17 && tint[3] == 255);
    images++;
}

static int unexpected_size(void *context, uint8_t *path,
                           size_t length, int *width_out, int *height_out)
{
    (void)context; (void)path; (void)length;
    (void)width_out; (void)height_out;
    assert(0 && "Text should not measure an image");
    return 0;
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    Bundle *bundle = BundleOpen(argv[1]);
    assert(bundle != NULL);
    FontMeasurer fonts = {width, line_height, NULL};
    TextRenderer text = {NULL, NULL, draw_text_clipped};
    RoundedRectangleRenderer shape = {fill, unexpected_outline, NULL};
    LineRenderer line = {unexpected_line, NULL};
    ImageRasterizer image = {unexpected_size, draw_image_clipped, NULL};
    HostBinding bindings[] = {
        MeasureGlyphWidthBinding(&fonts),
        MeasureGlyphLineHeightBinding(&fonts),
        RasterTextBinding(&text),
        RasterTextClippedBinding(&text),
        RasterRoundedRectangleBinding(&shape),
        RasterRoundedRectangleOutlineBinding(&shape),
        RasterLineBinding(&line),
        RasterImageBinding(&image),
    };
    BundleInstance *instance = BundleInstantiate(bundle, bindings, sizeof(bindings) / sizeof(bindings[0]));
    assert(instance != NULL);
    long long result = 0;
    int has_result = 0;
    assert(BundleInstanceRun(instance, &result, &has_result));
    assert(has_result && result == 42 && texts == 6 && images == 1);
    BundleInstanceClose(instance);
    BundleClose(bundle);
    return 0;
}
