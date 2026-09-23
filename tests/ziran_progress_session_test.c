#include "kryon_portable_host.h"

#include <assert.h>
#include <string.h>
#include "unused_image_host.h"

static int draws;
static int labels_drawn;
static int measured;

static void line(void *context, float x1, float y1, float x2, float y2,
                 uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    (void)context; (void)x1; (void)y1; (void)x2; (void)y2;
    (void)r; (void)g; (void)b; (void)a;
    assert(0 && "Progress should not draw separator lines");
}

static int width(void *context, const char *text, size_t length, int font,
                 const char *typeface, size_t typeface_length)
{
    (void)context; (void)font; (void)typeface; (void)typeface_length;
    assert(length == 3 && memcmp(text, "25%", length) == 0);
    measured++;
    return 20;
}

static int height(void *context, int font, const char *typeface,
                  size_t typeface_length)
{
    (void)context; (void)font; (void)typeface; (void)typeface_length;
    measured++;
    return 10;
}

static void fill(void *context, float x, float y, float width_value,
                 float height_value, float radius, int segments,
                 uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    (void)context;
    assert(x == 10 && y == 20 && height_value == 20);
    assert(radius == 0.2f && segments == 12);
    if(draws % 3 == 0) {
        assert(width_value == 100 && r == 0x11 && g == 0x22 &&
               b == 0x33 && a == 0x44);
    } else {
        assert(draws % 3 == 1 && width_value == 25 &&
               r == 0x55 && g == 0x66 && b == 0x77 && a == 0x88);
    }
    draws++;
}

static void outline(void *context, float x, float y, float width_value,
                    float height_value, float radius, int segments,
                    float line_width, uint8_t r, uint8_t g, uint8_t b,
                    uint8_t a)
{
    (void)context;
    assert(draws % 3 == 2 && x == 10 && y == 20 &&
           width_value == 100 && height_value == 20);
    assert(radius == 0.2f && segments == 12 && line_width == 1);
    assert(r == 0x66 && g == 0x64 && b == 0x5f && a == 0xff);
    draws++;
}

static void text(void *context, const char *value, size_t length,
                 int x, int y, int font, uint8_t r, uint8_t g,
                 uint8_t b, uint8_t a)
{
    (void)context; (void)x; (void)y;
    (void)font; (void)r; (void)g; (void)b; (void)a;
    assert(length == 3 && memcmp(value, "25%", length) == 0);
    labels_drawn++;
}

static void run(Bundle *bundle)
{
    FontMeasurer fonts = {width, height, NULL};
    RoundedRectangleRenderer shapes = {fill, outline, NULL};
    TextRenderer labels = {text, NULL};
    LineRenderer lines = {line, NULL};
    HostBinding bindings[] = {
        MeasureGlyphWidthBinding(&fonts),
        MeasureGlyphLineHeightBinding(&fonts),
        RasterRoundedRectangleBinding(&shapes),
        RasterRoundedRectangleOutlineBinding(&shapes),
        RasterTextBinding(&labels),
        RasterTextClippedBinding(&labels),
        RasterLineBinding(&lines),
        RasterImageBinding(&unused_image_renderer),
    };
    char module_name[] = "font_metrics";
    char function_name[] = "MeasureGlyphWidth";
    bindings[0].module = module_name;
    bindings[0].function = function_name;
    BundleInstance *instance = BundleInstantiate(bundle, bindings, 8);
    assert(instance != NULL);
    module_name[0] = 'x';
    function_name[0] = 'x';
    long long value = 0;
    int has_value = 0;
    assert(BundleInstanceRun(instance, &value, &has_value));
    assert(has_value && value == 0 && draws == 0 &&
           labels_drawn == 0 && measured == 2);
    assert(BundleInstanceRun(instance, &value, &has_value));
    assert(has_value && value == 1 && draws == 3 &&
           labels_drawn == 1 && measured == 2);
    assert(BundleInstanceRun(instance, &value, &has_value));
    assert(has_value && value == 2 && draws == 6 &&
           labels_drawn == 2 && measured == 4);
    assert(BundleInstanceRun(instance, &value, &has_value));
    assert(has_value && value == 3 && draws == 6 &&
           labels_drawn == 2 && measured == 6);
    assert(BundleInstanceRun(instance, &value, &has_value));
    assert(has_value && value == 4 && draws == 6 &&
           labels_drawn == 2 && measured == 8);
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
