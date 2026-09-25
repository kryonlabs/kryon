#include "kryon_portable_host.h"

#include <assert.h>

static int fills, outlines, labels;

static int width(void *context, uint8_t *value, size_t length,
                 int font, uint8_t *face, size_t face_length)
{
    (void)context; (void)value; (void)face;
    assert(font == 16 && face_length == 0);
    return (int)length * 8;
}

static int height(void *context, int font,
                  uint8_t *face, size_t face_length)
{
    (void)context; (void)face;
    assert(font == 16 && face_length == 0);
    return 16;
}

static void fill(void *context, float x, float y, float w, float h,
                 float radius, int segments,
                 uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    (void)context; (void)radius; (void)segments;
    (void)r; (void)g; (void)b; (void)a;
    assert(x >= 0 && y >= 0 && x + w <= 200 && y + h <= 150);
    fills++;
}

static void outline(void *context, float x, float y, float w, float h,
                    float radius, int segments, float line_width,
                    uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    (void)context; (void)radius; (void)segments; (void)line_width;
    (void)r; (void)g; (void)b; (void)a;
    assert(x >= 0 && y >= 0 && x + w <= 200 && y + h <= 150);
    outlines++;
}

static void line(void *context, float x1, float y1, float x2, float y2,
                 uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    (void)context; (void)x1; (void)y1; (void)x2; (void)y2;
    (void)r; (void)g; (void)b; (void)a;
    assert(0 && "unexpected line");
}

static void text(void *context, uint8_t *value, size_t bytes,
                 int x, int y, int font,
                 uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    (void)context; (void)value; (void)bytes; (void)x; (void)y;
    (void)font; (void)r; (void)g; (void)b; (void)a;
    assert(0 && "collapsible text must be clipped");
}

static void clipped(void *context, uint8_t *value, size_t bytes,
                    int x, int y, int font, float clip[4],
                    uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    (void)context; (void)value; (void)bytes; (void)x; (void)y;
    (void)r; (void)g; (void)b; (void)a;
    assert(font == 16 && clip[0] >= 0 && clip[1] >= 0 &&
           clip[0] + clip[2] <= 200 &&
           clip[1] + clip[3] <= 150);
    labels++;
}

static void image(void *context, uint8_t *path, size_t bytes,
                  uint32_t id, float source[4],
                  float destination[4], float clip[4],
                  float origin[2], float rotation, float radius,
                  uint8_t tint[4])
{
    (void)context; (void)path; (void)bytes; (void)id; (void)source;
    (void)destination; (void)clip; (void)origin;
    (void)rotation; (void)radius; (void)tint;
    assert(0 && "unexpected image");
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    Bundle *bundle = BundleOpen(argv[1]);
    assert(bundle != NULL);
    FontMeasurer fonts = {width, height, NULL};
    RoundedRectangleRenderer shape = {fill, outline, NULL};
    LineRenderer line_host = {line, NULL};
    TextRenderer text_host = {text, NULL, clipped};
    ImageRasterizer raster_image = {NULL, image, NULL};
    HostBinding bindings[] = {
        MeasureGlyphWidthBinding(&fonts),
        MeasureGlyphLineHeightBinding(&fonts),
        RasterRoundedRectangleBinding(&shape),
        RasterRoundedRectangleOutlineBinding(&shape),
        RasterLineBinding(&line_host),
        RasterTextBinding(&text_host),
        RasterTextClippedBinding(&text_host),
        RasterImageBinding(&raster_image),
    };
    BundleInstance *instance = BundleInstantiate(bundle, bindings, 8);
    assert(instance != NULL);
    for (int phase = 0; phase < 8; phase++) {
        long long result = -1;
        int has_result = 0;
        assert(BundleInstanceRun(instance, &result, &has_result));
        assert(has_result && result == phase);
    }
    assert(fills >= 2 && outlines >= 3 && labels >= 12);
    BundleInstanceClose(instance);
    BundleClose(bundle);
    return 0;
}
