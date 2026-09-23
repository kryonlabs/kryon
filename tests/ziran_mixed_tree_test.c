#include "kryon_portable_host.h"

#include <assert.h>
#include <string.h>
#include "unused_image_host.h"

static char sequence[16];
static int count;

static void record(char event)
{
    assert(count < (int)sizeof(sequence) - 1);
    sequence[count++] = event;
}

static void line(void *context, float x1, float y1, float x2, float y2,
                 uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    (void)context; (void)r; (void)g; (void)b; (void)a;
    assert(x1 == 30 && y1 == 50 && x2 == 30 && y2 == 90);
    record('L');
}

static void fill(void *context, float x, float y, float width,
                 float height, float radius, int segments,
                 uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    (void)context; (void)x; (void)y; (void)width; (void)height;
    (void)radius; (void)segments; (void)r; (void)g; (void)b; (void)a;
    record('F');
}

static void outline(void *context, float x, float y, float width,
                    float height, float radius, int segments,
                    float line_width, uint8_t r, uint8_t g,
                    uint8_t b, uint8_t a)
{
    (void)context; (void)x; (void)y; (void)width; (void)height;
    (void)radius; (void)segments; (void)line_width;
    (void)r; (void)g; (void)b; (void)a;
    record('O');
}

static void text(void *context, const char *value, size_t length,
                 int x, int y, int font, uint8_t r, uint8_t g,
                 uint8_t b, uint8_t a)
{
    (void)context; (void)value; (void)length; (void)x; (void)y;
    (void)font; (void)r; (void)g; (void)b; (void)a;
    assert(0 && "unlabelled widgets should not draw text");
}

static int width(void *context, const char *value, size_t length,
                 int font, const char *typeface, size_t typeface_length)
{
    (void)context; (void)value; (void)length; (void)font;
    (void)typeface; (void)typeface_length;
    assert(0 && "unlabelled widgets should not measure text");
    return 0;
}

static int height(void *context, int font, const char *typeface,
                  size_t typeface_length)
{
    (void)context; (void)font; (void)typeface; (void)typeface_length;
    assert(0 && "unlabelled widgets should not measure height");
    return 0;
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    Bundle *bundle = BundleOpen(argv[1]);
    assert(bundle != NULL);
    LineRenderer lines = {line, NULL};
    RoundedRectangleRenderer shapes = {fill, outline, NULL};
    TextRenderer texts = {text, NULL};
    FontMeasurer fonts = {width, height, NULL};
    HostBinding bindings[] = {
        RasterLineBinding(&lines),
        RasterRoundedRectangleBinding(&shapes),
        RasterRoundedRectangleOutlineBinding(&shapes),
        RasterTextBinding(&texts),
        MeasureGlyphWidthBinding(&fonts),
        MeasureGlyphLineHeightBinding(&fonts),
        RasterImageBinding(&unused_image_renderer),
    };
    long long value = 0;
    int has_value = 0;
    assert(BundleRun(bundle, bindings, 7, &value, &has_value));
    assert(has_value && value == 4);
    sequence[count] = '\0';
    assert(strcmp(sequence, "FFOLFFO") == 0);
    BundleClose(bundle);
    return 0;
}
