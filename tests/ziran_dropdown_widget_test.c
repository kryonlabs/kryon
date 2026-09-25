#include "kryon_portable_host.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static int fills, outlines, strokes, labels, image_draws;

static int width(void *context, uint8_t *value, size_t length,
                 int font, uint8_t *face, size_t face_length)
{
    (void)context; (void)value; (void)face;
    assert(font == 14 && face_length == 0);
    return (int)length * 8;
}

static int image_size(void *context, uint8_t *path, size_t length,
                      int *width_out, int *height_out)
{
    (void)context;
    assert(length == 9 && memcmp(path, "badge.png", 9) == 0);
    *width_out = 32;
    *height_out = 16;
    return 1;
}

static int height(void *context, int font, uint8_t *face,
                  size_t face_length)
{
    (void)context; (void)face;
    assert(font == 14 && face_length == 0);
    return 14;
}

static void fill(void *context, float x, float y, float width_value,
                 float height_value, float radius, int segments,
                 uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    (void)context; (void)radius; (void)segments;
    if (x == 10 && y == 10 && width_value == 120 &&
        height_value == 28)
        assert(r == 18 && g == 52 && b == 86);
    assert(x >= 10 && y >= 10 && x + width_value <= 130 &&
           y + height_value <= 190 && a > 0);
    fills++;
}

static void outline(void *context, float x, float y, float width_value,
                    float height_value, float radius, int segments,
                    float line_width, uint8_t r, uint8_t g,
                    uint8_t b, uint8_t a)
{
    (void)context; (void)radius; (void)segments;
    (void)r; (void)g; (void)b;
    assert(x >= 10 && y >= 10 && x + width_value <= 130 &&
           y + height_value <= 190 && line_width == 1 && a > 0);
    outlines++;
}

static void line(void *context, float x1, float y1, float x2, float y2,
                 uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    (void)context; (void)r; (void)g; (void)b;
    assert(x1 >= 10 && x1 <= 130 && x2 >= 10 && x2 <= 130 &&
           y1 >= 10 && y1 <= 190 && y2 >= 10 && y2 <= 190 && a > 0);
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
    assert(bytes > 0 && x >= 10 && x <= 130 && y >= 10 &&
           y <= 190 && font == 14 && clip[0] >= 10 &&
           clip[0] + clip[2] <= 130 && a > 0);
    labels++;
}

static void image(void *context, uint8_t *path, size_t bytes,
                  uint32_t texture_id, float source[4],
                  float destination[4], float clip[4],
                  float origin[2], float rotation, float radius,
                  uint8_t tint[4])
{
    (void)context; (void)origin; (void)rotation; (void)radius;
    assert(bytes == 9 && memcmp(path, "badge.png", 9) == 0 &&
           texture_id == 0 && source[2] == 32 && source[3] == 16 &&
           destination[0] >= 10 && destination[1] >= 10 &&
           clip[0] == 10 && clip[2] == 120 && tint[3] == 255);
    image_draws++;
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
    int expected_fills = 0, expected_outlines = 0;
    int expected_strokes = 0, expected_labels = 0;
    const int fill_steps[] = {1, 3, 3, 1, 3, 4, 1, 3, 1, 1, 4, 4, 4, 1};
    const int outline_steps[] = {1, 2, 2, 1, 2, 2, 1, 2, 1, 1, 2, 2, 2, 1};
    const int stroke_steps[] = {2, 4, 4, 2, 4, 4, 2, 4, 2, 2, 2, 2, 2, 2};
    const int label_steps[] = {1, 4, 4, 1, 4, 4, 1, 4, 1, 1, 6, 6, 6, 1};
    for (int phase = 0; phase < 14; phase++) {
        long long value = -1;
        int has_value = 0;
        assert(BundleInstanceRun(instance, &value, &has_value));
        if (!has_value || value != phase)
            fprintf(stderr, "Dropdown phase %d returned %lld\n",
                    phase, value);
        assert(has_value && value == phase);
        expected_fills += fill_steps[phase];
        expected_outlines += outline_steps[phase];
        expected_strokes += stroke_steps[phase];
        expected_labels += label_steps[phase];
        if (fills != expected_fills || outlines != expected_outlines ||
            strokes != expected_strokes || labels != expected_labels)
            fprintf(stderr, "Dropdown paint phase %d: %d/%d/%d/%d expected %d/%d/%d/%d\n",
                    phase, fills, outlines, strokes, labels,
                    expected_fills, expected_outlines,
                    expected_strokes, expected_labels);
        assert(fills == expected_fills && outlines == expected_outlines &&
               strokes == expected_strokes && labels == expected_labels);
        assert(image_draws == (phase == 13 ? 1 : 0));
    }
    BundleInstanceClose(instance);
    BundleClose(bundle);
    return 0;
}
