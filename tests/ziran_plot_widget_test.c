#include "kryon_portable_host.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include "unused_image_host.h"

static int backgrounds, marks, outlines, lines, labels;

static int width(void *context, const char *value, size_t length,
                 int font, const char *face, size_t face_length)
{
    (void)context; (void)value; (void)face;
    assert(font == 14 && face_length == 0);
    return (int)length * 8;
}

static void fill(void *context, float x, float y, float w, float h,
                 float radius, int segments,
                 uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    (void)context; (void)segments;
    assert(x >= 10 && y >= 10 && x + w <= 110 && y + h <= 150);
    if (radius == 0) {
        assert(r == 59 && g == 96 && b == 155 && a == 255);
        if (marks % 3 == 0)
            assert(x > 10.9f && x < 11.1f && y == 120 && h == 30);
        else if (marks % 3 == 1)
            assert(x > 44.2f && x < 44.5f && y == 90 && h == 60);
        else
            assert(x > 77.5f && x < 77.8f && y == 135 && h == 15);
        marks++;
    } else {
        assert(radius == 4 && r == 245 && g == 245 && b == 245 && a == 255);
        backgrounds++;
    }
}

static void outline(void *context, float x, float y, float w,
                    float h, float radius, int segments, float line_width,
                    uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    (void)context; (void)segments;
    assert(x == 10 && w == 100 && h == 60 &&
           (y == 10 || y == 90) && radius == 4 && line_width == 1);
    assert(r == 115 && g == 115 && b == 115 && a == 255);
    outlines++;
}

static void line(void *context, float x1, float y1,
                 float x2, float y2,
                 uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    (void)context;
    assert(x1 >= 10 && x2 <= 110 && y1 >= 10 && y1 <= 70 &&
           y2 >= 10 && y2 <= 70);
    assert(r == 59 && g == 96 && b == 155 && a == 255);
    lines++;
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
    (void)context; (void)value;
    assert(bytes > 0 && x >= 10 && x <= 110 && y >= 10 && y <= 150);
    assert(font == 14 && clip[0] == 10 && clip[2] == 100 &&
           (clip[1] == 10 || clip[1] == 90) && clip[3] == 60);
    assert(r == 23 && g == 23 && b == 23 && a == 255);
    labels++;
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    Bundle *bundle = BundleOpen(argv[1]);
    assert(bundle != NULL);
    FontMeasurer fonts = {width, NULL, NULL};
    RoundedRectangleRenderer shape = {fill, outline, NULL};
    LineRenderer line_host = {line, NULL};
    TextRenderer text_host = {text, NULL, clipped};
    HostBinding bindings[] = {
        MeasureGlyphWidthBinding(&fonts),
        RasterRoundedRectangleBinding(&shape),
        RasterRoundedRectangleOutlineBinding(&shape),
        RasterLineBinding(&line_host),
        RasterTextBinding(&text_host),
        RasterTextClippedBinding(&text_host),
        RasterImageBinding(&unused_image_renderer),
    };
    BundleInstance *instance = BundleInstantiate(bundle, bindings, 7);
    assert(instance != NULL);
    for (int phase = 0; phase < 2; phase++) {
        long long value = -1;
        int has_value = 0;
        assert(BundleInstanceRun(instance, &value, &has_value));
        assert(has_value && value == phase);
        assert(backgrounds == (phase + 1) * 2);
        assert(marks == (phase + 1) * 3);
        assert(outlines == (phase + 1) * 2);
        assert(lines == (phase + 1) * 2);
        assert(labels == (phase + 1) * 4);
    }
    BundleInstanceClose(instance);
    BundleClose(bundle);
    return 0;
}
