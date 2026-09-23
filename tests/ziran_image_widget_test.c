#include "kryon_portable_host.h"

#include <assert.h>
#include <string.h>

static int sizes;
static int images;
static int labels;
static int metrics;

static int size(void *context, const char *path, size_t length,
                int *width, int *height)
{
    (void)context;
    sizes++;
    if(length == 15 && memcmp(path, "assets/hero.png", length) == 0) {
        *width = 200;
        *height = 100;
        return 1;
    }
    assert(length == 11 && memcmp(path, "missing.png", length) == 0);
    *width = *height = 0;
    return 0;
}

static void image(void *context, const char *path, size_t length,
                  uint32_t texture_id, const float source[4],
                  const float destination[4], const float clip[4],
                  const float origin[2], float rotation, float radius,
                  const uint8_t tint[4])
{
    (void)context;
    assert(tint[0] == 0x10 && tint[1] == 0x20 &&
           tint[2] == 0x30 && tint[3] == 0x7f);
    assert(radius == 6.0f && origin[0] == 0 && origin[1] == 0);
    if(images == 0) {
        assert(texture_id == 0 && length == 15 &&
               memcmp(path, "assets/hero.png", length) == 0);
        assert(source[0] == 0 && source[1] == 0 &&
               source[2] == 200 && source[3] == 100);
        assert(destination[0] == 10 && destination[1] == 45 &&
               destination[2] == 100 && destination[3] == 50);
        assert(clip[0] == 10 && clip[1] == 20 &&
               clip[2] == 100 && clip[3] == 100);
        assert(rotation == 15.0f);
    } else {
        assert(images == 1 && texture_id == 77 && length == 11 &&
               memcmp(path, "ignored.png", length) == 0);
        assert(source[2] == 40 && source[3] == 40);
        assert(destination[0] == 20 && destination[1] == 20 &&
               destination[2] == 60 && destination[3] == 60);
        assert(clip[0] == 20 && clip[1] == 30 &&
               clip[2] == 60 && clip[3] == 40);
        assert(rotation == 0.0f);
    }
    images++;
}

static void unexpected_line(void *context, float x1, float y1,
                            float x2, float y2, uint8_t r, uint8_t g,
                            uint8_t b, uint8_t a)
{
    (void)context; (void)x1; (void)y1; (void)x2; (void)y2;
    (void)r; (void)g; (void)b; (void)a;
    assert(0 && "Image should not draw lines");
}

static void unexpected_shape(void *context, float x, float y, float w,
                             float h, float radius, int segments,
                             uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    (void)context; (void)x; (void)y; (void)w; (void)h;
    (void)radius; (void)segments; (void)r; (void)g; (void)b; (void)a;
    assert(0 && "Image should not draw a fill");
}

static void unexpected_outline(void *context, float x, float y,
                               float w, float h, float radius,
                               int segments, float line_width,
                               uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    (void)context; (void)x; (void)y; (void)w; (void)h;
    (void)radius; (void)segments; (void)line_width;
    (void)r; (void)g; (void)b; (void)a;
    assert(0 && "Image should not draw an outline");
}

static void text(void *context, const char *value, size_t length,
                 int x, int y, int font, uint8_t r, uint8_t g,
                 uint8_t b, uint8_t a)
{
    (void)context;
    assert(length == 12 && memcmp(value, "Missing hero", length) == 0);
    assert(x == 20 && y == 5 && font == 14);
    assert(r == 0x10 && g == 0x20 && b == 0x30 && a == 0x7f);
    labels++;
}

static int width(void *context, const char *value, size_t length,
                 int font, const char *typeface, size_t typeface_length)
{
    (void)context; (void)typeface; (void)typeface_length;
    assert(length == 12 && memcmp(value, "Missing hero", length) == 0);
    assert(font == 14);
    metrics++;
    return 60;
}

static int height(void *context, int font, const char *typeface,
                  size_t typeface_length)
{
    (void)context; (void)typeface; (void)typeface_length;
    assert(font == 14);
    metrics++;
    return 10;
}

static void run(Bundle *bundle)
{
    ImageRasterizer image_host = {size, image, NULL};
    LineRenderer line_host = {unexpected_line, NULL};
    RoundedRectangleRenderer shape_host = {
        unexpected_shape, unexpected_outline, NULL};
    TextRenderer text_host = {text, NULL};
    FontMeasurer font_host = {width, height, NULL};
    HostBinding bindings[] = {
        ImageWidthBinding(&image_host),
        ImageHeightBinding(&image_host),
        RasterImageBinding(&image_host),
        RasterLineBinding(&line_host),
        RasterRoundedRectangleBinding(&shape_host),
        RasterRoundedRectangleOutlineBinding(&shape_host),
        RasterTextBinding(&text_host),
        MeasureGlyphWidthBinding(&font_host),
        MeasureGlyphLineHeightBinding(&font_host),
    };
    BundleInstance *instance = BundleInstantiate(bundle, bindings, 9);
    assert(instance != NULL);
    long long result = -1;
    int has_result = 0;
    assert(BundleInstanceRun(instance, &result, &has_result));
    assert(has_result && result == 0 && images == 0 && sizes == 2);
    assert(BundleInstanceRun(instance, &result, &has_result));
    assert(has_result && result == 1 && images == 1 && sizes == 2);
    assert(BundleInstanceRun(instance, &result, &has_result));
    assert(has_result && result == 2 && images == 2 && sizes == 2);
    assert(BundleInstanceRun(instance, &result, &has_result));
    assert(has_result && result == 3 && images == 2 && sizes == 4 &&
           labels == 1 && metrics == 2);
    BundleInstanceClose(instance);
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    Bundle *bundle = BundleOpen(argv[1]);
    assert(bundle != NULL);
    run(bundle);
    BundleClose(bundle);
    return 0;
}
