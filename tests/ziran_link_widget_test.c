#include "kryon_portable_host.h"

#include <assert.h>
#include <string.h>

static int texts, lines;

static int width(void *context, const char *value, size_t length,
                 int font, const char *typeface, size_t typeface_length)
{
    (void)context; (void)value; (void)typeface;
    assert(font == 16 && typeface_length == 0);
    return (int)length * 10;
}

static int line_height(void *context, int font,
                       const char *typeface, size_t typeface_length)
{
    (void)context; (void)typeface;
    assert(font == 16 && typeface_length == 0);
    return 12;
}

static void draw_text_clipped(void *context, const char *value,
                              size_t length, int x, int y, int font,
                              const float clip[4], uint8_t r, uint8_t g,
                              uint8_t b, uint8_t a)
{
    (void)context;
    (void)r; (void)g; (void)b; (void)a;
    assert(texts < 4 && length == 4 && memcmp(value, "Docs", 4) == 0);
    assert(x == 10 && y == 20 && font == 16);
    assert(clip[0] == 10 && clip[1] == 20 &&
           clip[2] == 40 && clip[3] == 12);
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
    assert(0 && "Link should not draw a fill");
}

static void underline(void *context, float x1, float y1,
                            float x2, float y2, uint8_t r, uint8_t g,
                            uint8_t b, uint8_t a)
{
    (void)context;
    (void)r; (void)g; (void)b; (void)a;
    assert(lines == 0 && x1 == 10 && y1 == 30 &&
           x2 == 50 && y2 == 30);
    lines++;
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
    TextRenderer text = {NULL, NULL, draw_text_clipped};
    RoundedRectangleRenderer shape = {fill, unexpected_outline, NULL};
    LineRenderer line = {underline, NULL};
    ImageRasterizer image = {unexpected_size, unexpected_image, NULL};
    HostBinding bindings[] = {
        TextSliceBinding(),
        MeasureGlyphWidthBinding(&fonts),
        MeasureGlyphLineHeightBinding(&fonts),
        RasterTextBinding(&text),
        RasterTextClippedBinding(&text),
        RasterRoundedRectangleBinding(&shape),
        RasterRoundedRectangleOutlineBinding(&shape),
        RasterLineBinding(&line),
        RasterImageBinding(&image),
    };
    BundleInstance *instance = BundleInstantiate(bundle, bindings, 9);
    assert(instance != NULL);
    long long result = 0;
    int has_result = 0;
    for(int phase = 0; phase < 4; phase++) {
        assert(BundleInstanceRun(instance, &result, &has_result));
        assert(has_result && result == phase && texts == phase + 1);
        assert(lines == (phase >= 1 ? 1 : 0));
    }
    BundleInstanceClose(instance);
    BundleClose(bundle);
    return 0;
}
