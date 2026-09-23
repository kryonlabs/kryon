#include "kryon_portable_host.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static int fills, outlines, labels, images, bar_fills;

static int width(void *context, const char *value, size_t length,
                 int font, const char *face, size_t face_length)
{
    (void)context; (void)value; (void)face;
    assert(font == 12 && face_length == 0);
    return (int)length * 7;
}

static int height(void *context, int font, const char *face,
                  size_t face_length)
{
    (void)context; (void)face;
    assert(font == 12 && face_length == 0);
    return 12;
}

static int image_size(void *context, const char *path, size_t length,
                      int *width_out, int *height_out)
{
    (void)context;
    assert(length == 8 && memcmp(path, "icon.png", 8) == 0);
    *width_out = 24;
    *height_out = 24;
    return 1;
}

static void image(void *context, const char *path, size_t bytes,
                  uint32_t texture_id, const float source[4],
                  const float destination[4], const float clip[4],
                  const float origin[2], float rotation, float radius,
                  const uint8_t tint[4])
{
    (void)context; (void)origin; (void)rotation; (void)radius;
    assert(bytes == 8 && memcmp(path, "icon.png", 8) == 0 &&
           texture_id == 0 && source[2] == 24 && source[3] == 24 &&
           destination[0] >= 0 && destination[1] >= 100 &&
           clip[1] == 100 && clip[3] == 80 && tint[3] > 0);
    images++;
}

static void fill(void *context, float x, float y, float w, float h,
                 float radius, int segments,
                 uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    (void)context; (void)radius; (void)segments;
    assert(x >= 0 && y >= 100 && x + w <= 300 &&
           y + h <= 180 && a > 0);
    if (x == 1 && y == 101 && w == 298 && h == 78) {
        assert(r == 18 && g == 52 && b == 86);
        bar_fills++;
    }
    fills++;
}

static void outline(void *context, float x, float y, float w, float h,
                    float radius, int segments, float line_width,
                    uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    (void)context; (void)radius; (void)segments;
    (void)r; (void)g; (void)b;
    assert(x >= 0 && y >= 100 && x + w <= 300 &&
           y + h <= 180 && line_width == 1 && a > 0);
    outlines++;
}

static void line(void *context, float x1, float y1, float x2, float y2,
                 uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    (void)context; (void)x1; (void)y1; (void)x2; (void)y2;
    (void)r; (void)g; (void)b; (void)a;
    assert(0);
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
    (void)context; (void)value; (void)r; (void)g; (void)b;
    assert(bytes > 0 && x >= 0 && x <= 300 && y >= 100 &&
           y <= 180 && font == 12 && clip[1] >= 100 &&
           clip[1] + clip[3] <= 180 && a > 0);
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
    ImageRasterizer raster = {image_size, image, NULL};
    HostBinding bindings[] = {
        MeasureGlyphWidthBinding(&fonts),
        MeasureGlyphLineHeightBinding(&fonts),
        RasterRoundedRectangleBinding(&shape),
        RasterRoundedRectangleOutlineBinding(&shape),
        RasterLineBinding(&lines),
        RasterTextBinding(&texts),
        RasterTextClippedBinding(&texts),
        RasterImageBinding(&raster),
        ImageWidthBinding(&raster),
        ImageHeightBinding(&raster),
    };
    BundleInstance *instance = BundleInstantiate(bundle, bindings, 10);
    assert(instance != NULL);
    for (int phase = 0; phase < 3; phase++) {
        long long value = -1;
        int has_value = 0;
        assert(BundleInstanceRun(instance, &value, &has_value));
        if (!has_value || value != phase)
            fprintf(stderr, "NavigationBar phase %d returned %lld\n",
                    phase, value);
        assert(has_value && value == phase);
    }
    assert(fills >= 6 && outlines == 3 &&
           labels == 9 && images == 6 && bar_fills == 3);
    BundleInstanceClose(instance);
    BundleClose(bundle);
    return 0;
}
