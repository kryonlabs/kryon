#include "kryon_portable_host.h"

#include <assert.h>

static int fills;

static int height(void *context, int font, uint8_t *face, size_t length)
{
    (void)context; (void)font; (void)face; (void)length;
    return 12;
}

static void fill(void *context, float x, float y, float w, float h,
                 float radius, int segments,
                 uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    (void)context; (void)radius; (void)segments;
    (void)r; (void)g; (void)b; (void)a;
    assert(x >= 10 && y >= 20 && x + w <= 130 && y + h <= 100);
    fills++;
}

static void outline(void *context, float x, float y, float w, float h,
                    float radius, int segments, float line_width,
                    uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    (void)context; (void)x; (void)y; (void)w; (void)h;
    (void)radius; (void)segments; (void)line_width;
    (void)r; (void)g; (void)b; (void)a;
    assert(0 && "unexpected outline");
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
    assert(0 && "unexpected text");
}

static void clipped(void *context, uint8_t *value, size_t bytes,
                    int x, int y, int font, float clip[4],
                    uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    (void)clip;
    text(context, value, bytes, x, y, font, r, g, b, a);
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
    FontMeasurer fonts = {NULL, height, NULL};
    RoundedRectangleRenderer shape = {fill, outline, NULL};
    LineRenderer line_host = {line, NULL};
    TextRenderer text_host = {text, NULL, clipped};
    ImageRasterizer raster_image = {NULL, image, NULL};
    HostBinding bindings[] = {
        MeasureGlyphLineHeightBinding(&fonts),
        RasterRoundedRectangleBinding(&shape),
        RasterRoundedRectangleOutlineBinding(&shape),
        RasterLineBinding(&line_host),
        RasterTextBinding(&text_host),
        RasterTextClippedBinding(&text_host),
        RasterImageBinding(&raster_image),
    };
    BundleInstance *instance = BundleInstantiate(bundle, bindings, 7);
    assert(instance != NULL);
    for (int phase = 0; phase < 7; phase++) {
        long long result = -1;
        int has_result = 0;
        assert(BundleInstanceRun(instance, &result, &has_result));
        assert(has_result && result == phase);
    }
    assert(fills == 7);
    BundleInstanceClose(instance);
    BundleClose(bundle);
    return 0;
}
