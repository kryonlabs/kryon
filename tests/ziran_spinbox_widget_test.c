#include "kryon_portable_host.h"

#include <assert.h>
#include <string.h>

static int fills;
static char letters[64];
static int length;

static int width(void *context, uint8_t *value, size_t bytes,
                 int font, uint8_t *typeface, size_t typeface_length)
{
    (void)context; (void)value; (void)typeface;
    assert(font == 14 && typeface_length == 0);
    return (int)bytes * 7;
}

static int height(void *context, int font,
                  uint8_t *typeface, size_t typeface_length)
{
    (void)context; (void)typeface;
    assert(font == 14 && typeface_length == 0);
    return 12;
}

static void fill(void *context, float x, float y, float w, float h,
                 float radius, int segments,
                 uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    (void)context; (void)x; (void)y; (void)w; (void)h;
    (void)radius; (void)segments; (void)a;
    if(fills % 4 == 1) { assert(r == 0x12 && g == 0x34 && b == 0x56); }
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
    (void)context; (void)x1; (void)y1; (void)x2; (void)y2;
    (void)r; (void)g; (void)b; (void)a; assert(0);
}

static void unclipped(void *context, uint8_t *value, size_t bytes,
                      int x, int y, int font,
                      uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    (void)context; (void)value; (void)bytes; (void)x; (void)y;
    (void)font; (void)r; (void)g; (void)b; (void)a; assert(0);
}

static void clipped(void *context, uint8_t *value, size_t bytes,
                    int x, int y, int font, float clip[4],
                    uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    (void)context; (void)x; (void)y; (void)clip;
    (void)r; (void)g; (void)b; (void)a;
    assert(font == 14 && length + (int)bytes < 64);
    memcpy(letters + length, value, bytes);
    length += (int)bytes;
    letters[length] = 0;
}

static void image(void *context, uint8_t *path, size_t bytes,
                  uint32_t id, float source[4],
                  float destination[4], float clip[4],
                  float origin[2], float rotation, float radius,
                  uint8_t tint[4])
{
    (void)context; (void)path; (void)bytes; (void)id; (void)source;
    (void)destination; (void)clip; (void)origin;
    (void)rotation; (void)radius; (void)tint; assert(0);
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    Bundle *bundle = BundleOpen(argv[1]);
    assert(bundle != NULL);
    FontMeasurer fonts = {width, height, NULL};
    RoundedRectangleRenderer shape = {fill, outline, NULL};
    LineRenderer line_host = {line, NULL};
    TextRenderer text_host = {unclipped, NULL, clipped};
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
        ImageWidthBinding(&raster_image),
        ImageHeightBinding(&raster_image),
    };
    BundleInstance *instance = BundleInstantiate(bundle, bindings, 10);
    assert(instance != NULL);
    char *expected[] = {"-+-12", "-+-10", "-+-12", "-+12", "-+twelve", "-+-2147483648"};
    for(int phase = 0; phase < 6; phase++) {
        long long result = -1;
        int has_result = 0;
        length = 0;
        assert(BundleInstanceRun(instance, &result, &has_result));
        assert(has_result && result == phase);
        assert(strcmp(letters, expected[phase]) == 0);
        assert(fills == (phase + 1) * 4);
    }
    BundleInstanceClose(instance);
    BundleClose(bundle);
    return 0;
}
