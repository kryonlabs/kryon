#include "kryon_portable_host.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static int keyword_paint_count;
static int composition_paint_count;
static int preedit_paint_count;
static int preedit_underline_count;
static int active_phase;

static int width(void *context, const char *value, size_t length,
                 int font, const char *face, size_t face_length)
{
    (void)context; (void)value; (void)face; (void)face_length;
    return (int)length * font / 2;
}

static int height(void *context, int font, const char *face,
                  size_t face_length)
{
    (void)context; (void)face; (void)face_length;
    return font;
}

static int image_size(void *context, const char *path, size_t length,
                      int *width_out, int *height_out)
{
    (void)context; (void)path; (void)length;
    *width_out = 0; *height_out = 0;
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
}

static void shape(void *context, float x, float y, float w, float h,
                  float radius, int segments,
                  uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    (void)context; (void)x; (void)y; (void)w; (void)h;
    (void)radius; (void)segments; (void)a;
    if (h <= 3.0f && r == 59 && g == 130 && b == 246)
        composition_paint_count++;
    if (active_phase == 37 && h <= 3.0f &&
        r == 59 && g == 130 && b == 246)
        preedit_underline_count++;
}

static void outline(void *context, float x, float y, float w, float h,
                    float radius, int segments, float line_width,
                    uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    (void)line_width;
    shape(context, x, y, w, h, radius, segments, r, g, b, a);
}

static void line(void *context, float x1, float y1, float x2, float y2,
                 uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    (void)context; (void)x1; (void)y1; (void)x2; (void)y2;
    (void)r; (void)g; (void)b; (void)a;
}

static void text(void *context, const char *value, size_t bytes,
                 int x, int y, int font,
                 uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    (void)context; (void)value; (void)bytes; (void)x; (void)y;
    (void)font; (void)r; (void)g; (void)b; (void)a;
}

static void clipped(void *context, const char *value, size_t bytes,
                    int x, int y, int font, const float clip[4],
                    uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    (void)clip;
    if (r == 36 && g == 72 && b == 172)
        keyword_paint_count++;
    if (active_phase == 37 && bytes == 2 &&
        memcmp(value, "é", 2) == 0)
        preedit_paint_count++;
    text(context, value, bytes, x, y, font, r, g, b, a);
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    Bundle *bundle = BundleOpen(argv[1]);
    assert(bundle != NULL);
    FontMeasurer fonts = {width, height, NULL};
    RoundedRectangleRenderer shapes = {shape, outline, NULL};
    LineRenderer lines = {line, NULL};
    TextRenderer texts = {text, NULL, clipped};
    ImageRasterizer images = {image_size, image, NULL};
    HostBinding bindings[] = {
        MeasureGlyphWidthBinding(&fonts),
        MeasureGlyphLineHeightBinding(&fonts),
        RasterRoundedRectangleBinding(&shapes),
        RasterRoundedRectangleOutlineBinding(&shapes),
        RasterLineBinding(&lines),
        RasterTextBinding(&texts),
        RasterTextClippedBinding(&texts),
        RasterImageBinding(&images),
        ImageWidthBinding(&images),
        ImageHeightBinding(&images),
        TextSliceBinding(),
    };
    BundleInstance *instance = BundleInstantiate(bundle, bindings, 11);
    assert(instance != NULL);
    for (int phase = 0; phase < 43; phase++) {
        active_phase = phase;
        long long value = -1;
        int has_value = 0;
        assert(BundleInstanceRun(instance, &value, &has_value));
        if (!has_value || value != phase)
            fprintf(stderr, "TextArea phase %d returned %lld\n",
                    phase, value);
        assert(has_value && value == phase);
    }
    assert(keyword_paint_count > 0);
    assert(composition_paint_count > 0);
    assert(preedit_paint_count > 0);
    assert(preedit_underline_count > 0);
    BundleInstanceClose(instance);
    BundleClose(bundle);
    return 0;
}
