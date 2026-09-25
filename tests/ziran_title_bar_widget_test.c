#include "kryon_portable_host.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static int fills, strokes, labels, titles;

static int width(void *context, uint8_t *value, size_t length,
                 int font, uint8_t *face, size_t face_length)
{
    (void)context; (void)value; (void)face;
    assert(font >= 12 && font <= 18 && face_length == 0);
    return (int)(length * (size_t)font / 2);
}

static int height(void *context, int font, uint8_t *face,
                  size_t face_length)
{
    (void)context; (void)face;
    assert(font >= 12 && font <= 18 && face_length == 0);
    return font;
}

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

static void fill(void *context, float x, float y, float w, float h,
                 float radius, int segments,
                 uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    (void)context; (void)radius; (void)segments;
    if (!(x >= 0 && y >= 0 && x + w <= 240 &&
          y + h <= 130 && a > 0))
        fprintf(stderr, "TitleBar fill bounds %.1f %.1f %.1f %.1f alpha %u\n",
                x, y, w, h, a);
    assert(x >= 0 && y >= 0 && x + w <= 240 &&
           y + h <= 130 && a > 0);
    if (x == 0 && y == 0 && w == 240 && h == 48)
        assert(r == 18 && g == 52 && b == 86);
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
    assert(x1 >= 0 && x1 <= 240 && x2 >= 0 && x2 <= 240 &&
           y1 >= 0 && y1 <= 130 && y2 >= 0 && y2 <= 130 &&
           a > 0);
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
    (void)context; (void)r; (void)g; (void)b;
    assert(bytes > 0 && x >= 0 && x <= 240 && y >= 0 &&
           y <= 130 && font >= 12 && font <= 18 &&
           clip[0] >= 0 && clip[0] + clip[2] <= 240 && a > 0);
    if (bytes == 7 && memcmp(value, "Voyager", 7) == 0) titles++;
    if (bytes == 23 &&
        memcmp(value, "Interstellar Navigation", 23) == 0)
        assert(font == 12);
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
    for (int phase = 0; phase < 5; phase++) {
        long long value = -1;
        int has_value = 0;
        assert(BundleInstanceRun(instance, &value, &has_value));
        if (!has_value || value != phase)
            fprintf(stderr, "TitleBar phase %d returned %lld\n",
                    phase, value);
        assert(has_value && value == phase);
    }
    assert(fills >= 12 && strokes >= 16 &&
           labels >= 5 && titles == 1);
    BundleInstanceClose(instance);
    BundleClose(bundle);
    return 0;
}
