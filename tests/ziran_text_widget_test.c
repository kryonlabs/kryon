#include "kryon_portable_host.h"

#include <assert.h>
#include <string.h>

static const char *expected[] = {"Alpha", "beta", "Gamma"};
static const int expected_x[] = {10, 15, 10};
static const int expected_y[] = {20, 36, 52};
static const int expected_width[] = {50, 40, 50};
static int commands;

static int width(void *context, const char *value, size_t length,
                 int font, const char *typeface, size_t typeface_length)
{
    (void)context; (void)value; (void)typeface;
    assert(font == 14 && typeface_length == 0);
    return (int)length * 10;
}

static int line_height(void *context, int font,
                       const char *typeface, size_t typeface_length)
{
    (void)context; (void)typeface;
    assert(font == 14 && typeface_length == 0);
    return 12;
}

static void draw_text(void *context, const char *value, size_t length,
                      int x, int y, int font,
                      uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    (void)context;
    int index = commands / 2;
    assert(index < 3 && commands % 2 == 0);
    assert(strlen(expected[index]) == length &&
           memcmp(expected[index], value, length) == 0);
    assert(x == expected_x[index] && y == expected_y[index]);
    assert(font == 14 && r == 0x11 && g == 0x22 &&
           b == 0x33 && a == 127);
    commands++;
}

static void fill(void *context, float x, float y, float w, float h,
                 float radius, int segments,
                 uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    (void)context;
    int index = commands / 2;
    assert(index < 3 && commands % 2 == 1);
    assert(x == expected_x[index] && y == expected_y[index] + 6 &&
           w == expected_width[index] && h == 1 &&
           radius == 0 && segments == 1);
    assert(r == 0x11 && g == 0x22 && b == 0x33 && a == 127);
    commands++;
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
    assert(0 && "Text should not draw an image");
}

static int unexpected_size(void *context, const char *path,
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
    TextRenderer text = {draw_text, NULL};
    RoundedRectangleRenderer shape = {fill, unexpected_outline, NULL};
    LineRenderer line = {unexpected_line, NULL};
    ImageRasterizer image = {unexpected_size, unexpected_image, NULL};
    HostBinding bindings[] = {
        TextSliceBinding(),
        MeasureGlyphWidthBinding(&fonts),
        MeasureGlyphLineHeightBinding(&fonts),
        RasterTextBinding(&text),
        RasterRoundedRectangleBinding(&shape),
        RasterRoundedRectangleOutlineBinding(&shape),
        RasterLineBinding(&line),
        RasterImageBinding(&image),
    };
    BundleInstance *instance = BundleInstantiate(bundle, bindings, 8);
    assert(instance != NULL);
    long long result = 0;
    int has_result = 0;
    assert(BundleInstanceRun(instance, &result, &has_result));
    assert(has_result && result == 0 && commands == 0);
    assert(BundleInstanceRun(instance, &result, &has_result));
    assert(has_result && result == 42 && commands == 6);
    BundleInstanceClose(instance);
    BundleClose(bundle);
    return 0;
}
