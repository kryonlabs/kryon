#include "kryon_portable_host.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static int panels, scrims, labels, strokes;

static int width(void *context, const char *value, size_t length,
                 int font, const char *face, size_t face_length)
{
    (void)context; (void)value; (void)face;
    assert(font >= 12 && font <= 18 && face_length == 0);
    return (int)(length * (size_t)font / 2);
}

static int height(void *context, int font, const char *face,
                  size_t face_length)
{
    (void)context; (void)face;
    assert(font >= 12 && font <= 18 && face_length == 0);
    return font;
}

static int image_size(void *context, const char *path, size_t length,
                      int *width_out, int *height_out)
{
    (void)context; (void)path; (void)length;
    (void)width_out; (void)height_out;
    assert(0);
    return 0;
}

static void image(void *context, const char *path, size_t bytes,
                  uint32_t texture_id, const float source[4],
                  const float destination[4], const float clip[4],
                  const float origin[2], float rotation, float radius,
                  const uint8_t tint[4])
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
    assert(x >= 0 && y >= 0 && x + w <= 300 &&
           y + h <= 220 && a > 0);
    if (w >= 180 && h >= 120 && x >= 50 && y >= 20) {
        assert(r == 18 && g == 52 && b == 86);
        panels++;
    } else if (x == 0 && y == 0 && w == 300 && h == 220) {
        assert(a == 128);
        scrims++;
    }
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
    assert(x1 >= 60 && x1 <= 245 && x2 >= 60 && x2 <= 245 &&
           y1 >= 20 && y1 <= 80 && y2 >= 20 && y2 <= 80 && a > 0);
    strokes++;
}

static void text(void *context, const char *value, size_t bytes,
                 int x, int y, int font,
                 uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    (void)context; (void)value; (void)bytes; (void)x; (void)y;
    (void)font; (void)r; (void)g; (void)b; (void)a;
    assert(0);
}

static void clipped(void *context, const char *value, size_t bytes,
                    int x, int y, int font, const float clip[4],
                    uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    (void)context; (void)r; (void)g; (void)b;
    assert(x >= 50 && y >= 20 &&
           clip[0] >= 50 && clip[0] + clip[2] <= 250 && a > 0);
    if (bytes == 8) {
        assert(memcmp(value, "Settings", 8) == 0 && font == 18);
    } else {
        assert(bytes == 23 &&
               memcmp(value, "Interstellar Navigation", 23) == 0 &&
               font == 12);
    }
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
            fprintf(stderr, "ModalFrame phase %d returned %lld\n",
                    phase, value);
        assert(has_value && value == phase);
    }
    assert(panels == 4 && scrims == 5 &&
           labels == 4 && strokes == 8);
    BundleInstanceClose(instance);
    BundleClose(bundle);
    return 0;
}
