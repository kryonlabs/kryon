#include "kryon_portable_host.h"

#include <assert.h>

static int draws;

static int width(void *context, const char *text, size_t length, int font,
                 const char *typeface, size_t typeface_length)
{
    (void)context; (void)text; (void)length; (void)font;
    (void)typeface; (void)typeface_length;
    assert(0 && "unlabelled Progress measured text");
    return 0;
}

static int height(void *context, int font, const char *typeface,
                  size_t typeface_length)
{
    (void)context; (void)font; (void)typeface; (void)typeface_length;
    assert(0 && "unlabelled Progress measured a line");
    return 0;
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
    (void)context; (void)value; (void)length; (void)x; (void)y;
    (void)font; (void)r; (void)g; (void)b; (void)a;
    assert(0 && "unlabelled Progress painted text");
}

static void run(Bundle *bundle)
{
    FontMeasurer fonts = {width, height, NULL};
    RoundedRectangleRenderer shapes = {fill, outline, NULL};
    TextRenderer labels = {text, NULL};
    HostBinding bindings[] = {
        MeasureGlyphWidthBinding(&fonts),
        MeasureGlyphLineHeightBinding(&fonts),
        RasterRoundedRectangleBinding(&shapes),
        RasterRoundedRectangleOutlineBinding(&shapes),
        RasterTextBinding(&labels),
    };
    char module_name[] = "font_metrics";
    char function_name[] = "MeasureGlyphWidth";
    bindings[0].module = module_name;
    bindings[0].function = function_name;
    BundleInstance *instance = BundleInstantiate(bundle, bindings, 5);
    assert(instance != NULL);
    module_name[0] = 'x';
    function_name[0] = 'x';
    long long value = 0;
    int has_value = 0;
    assert(BundleInstanceRun(instance, &value, &has_value));
    assert(has_value && value == 1 && draws == 3);
    assert(BundleInstanceRun(instance, &value, &has_value));
    assert(has_value && value == 2 && draws == 6);
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
