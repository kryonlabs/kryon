#include "kryon_portable_host.h"

#include <assert.h>
#include <stdio.h>

static int fills, strokes, labels, action_fills;

static int image_size(void *context, uint8_t *path, size_t length,
                      int *width_out, int *height_out)
{
    (void)context; (void)path; (void)length;
    (void)width_out; (void)height_out;
    assert(0);
    return 0;
}

static void image(void *context, uint8_t *path, size_t bytes,
                  uint32_t texture_id, float source[4],
                  float destination[4], float clip[4],
                  float origin[2], float rotation, float radius,
                  uint8_t tint[4])
{
    (void)context; (void)path; (void)bytes; (void)texture_id;
    (void)source; (void)destination; (void)clip; (void)origin;
    (void)rotation; (void)radius; (void)tint;
    assert(0);
}

static int width(void *context, uint8_t *value, size_t length,
                 int font, uint8_t *face, size_t face_length)
{
    (void)context; (void)value; (void)face;
    assert(font == 14 && face_length == 0);
    return (int)length * 8;
}

static int height(void *context, int font, uint8_t *face,
                  size_t face_length)
{
    (void)context; (void)face;
    assert(font == 14 && face_length == 0);
    return 14;
}

static void fill(void *context, float x, float y, float w, float h,
                 float radius, int segments,
                 uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    (void)context; (void)radius; (void)segments;
    assert(x >= 10 && y >= 10 && x + w <= 310 &&
           y + h <= 130 && a > 0);
    if (x == 224 && y == 13 && w == 34 && h == 34) {
        assert(r == 18 && g == 52 && b == 86);
        action_fills++;
    }
    fills++;
}

static void outline(void *context, float x, float y, float w, float h,
                    float radius, int segments, float line_width,
                    uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    (void)context; (void)x; (void)y; (void)w; (void)h;
    (void)radius; (void)segments; (void)line_width;
    (void)r; (void)g; (void)b; (void)a;
}

static void line(void *context, float x1, float y1, float x2, float y2,
                 uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    (void)context; (void)r; (void)g; (void)b;
    assert(x1 >= 10 && x2 <= 310 && y1 >= 10 && y2 <= 130 && a > 0);
    strokes++;
}

static void text(void *context, uint8_t *value, size_t bytes,
                 int x, int y, int font,
                 uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    (void)context; (void)value; (void)bytes; (void)x; (void)y;
    (void)font; (void)r; (void)g; (void)b; (void)a;
    assert(0);
}

static void clipped(void *context, uint8_t *value, size_t bytes,
                    int x, int y, int font, float clip[4],
                    uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    (void)context; (void)value; (void)r; (void)g; (void)b;
    assert(bytes > 0 && x >= 10 && x <= 310 && y >= 10 &&
           y <= 130 && font == 14 && clip[0] >= 10 &&
           clip[0] + clip[2] <= 310 && a > 0);
    labels++;
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    Bundle *bundle = BundleOpen(argv[1]);
    assert(bundle != NULL);
    FontMeasurer fonts = {width, height, NULL};
    RoundedRectangleRenderer shape = {fill, outline, NULL};
    LineRenderer lines = {line, NULL};
    TextRenderer texts = {text, NULL, clipped};
    ImageRasterizer images = {image_size, image, NULL};
    HostBinding bindings[] = {
        MeasureGlyphWidthBinding(&fonts),
        MeasureGlyphLineHeightBinding(&fonts),
        RasterRoundedRectangleBinding(&shape),
        RasterRoundedRectangleOutlineBinding(&shape),
        RasterLineBinding(&lines),
        RasterTextBinding(&texts),
        RasterTextClippedBinding(&texts),
        RasterImageBinding(&images),
        ImageWidthBinding(&images),
        ImageHeightBinding(&images),
    };
    BundleInstance *instance = BundleInstantiate(bundle, bindings, 10);
    assert(instance != NULL);
    for (int phase = 0; phase < 4; phase++) {
        long long value = -1;
        int has_value = 0;
        assert(BundleInstanceRun(instance, &value, &has_value));
        if (!has_value || value != phase)
            fprintf(stderr, "Toolbar phase %d returned %lld\n", phase, value);
        assert(has_value && value == phase);
    }
    assert(fills >= 16 && strokes >= 12 &&
           labels >= 12 && action_fills == 4);
    BundleInstanceClose(instance);
    BundleClose(bundle);
    return 0;
}
