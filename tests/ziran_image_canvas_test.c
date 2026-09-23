#include "image_canvas.h"

#include <assert.h>
#include <string.h>

static uint8_t framebuffer[4 * 4 * 4];
static const uint8_t checker[2 * 2 * 4] = {
    255, 0, 0, 255,     0, 255, 0, 255,
    0, 0, 255, 255,     255, 255, 255, 255,
};
static const ImageAsset assets[] = {
    {"checker", 77, checker, 2, 2, 8},
};

static void pixel(int x, int y, uint8_t r, uint8_t g,
                  uint8_t b, uint8_t a)
{
    const uint8_t *p = framebuffer + (y * 4 + x) * 4;
    assert(p[0] == r && p[1] == g && p[2] == b && p[3] == a);
}

static void unexpected_line(void *context, float x1, float y1,
                            float x2, float y2, uint8_t r, uint8_t g,
                            uint8_t b, uint8_t a)
{
    (void)context; (void)x1; (void)y1; (void)x2; (void)y2;
    (void)r; (void)g; (void)b; (void)a;
    assert(0 && "Image should not paint a line");
}

static void unexpected_shape(void *context, float x, float y, float w,
                             float h, float radius, int segments,
                             uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    (void)context; (void)x; (void)y; (void)w; (void)h;
    (void)radius; (void)segments; (void)r; (void)g; (void)b; (void)a;
    assert(0 && "Image should not paint a shape");
}

static void unexpected_outline(void *context, float x, float y,
                               float w, float h, float radius,
                               int segments, float line_width,
                               uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    (void)context; (void)x; (void)y; (void)w; (void)h;
    (void)radius; (void)segments; (void)line_width;
    (void)r; (void)g; (void)b; (void)a;
    assert(0 && "Image should not paint an outline");
}

static void unexpected_text(void *context, const char *value, size_t length,
                            int x, int y, int font, uint8_t r, uint8_t g,
                            uint8_t b, uint8_t a)
{
    (void)context; (void)value; (void)length; (void)x; (void)y;
    (void)font; (void)r; (void)g; (void)b; (void)a;
    assert(0 && "available Image should not paint fallback text");
}

static int unexpected_width(void *context, const char *text,
                            size_t length, int font,
                            const char *typeface, size_t typeface_length)
{
    (void)context; (void)text; (void)length; (void)font;
    (void)typeface; (void)typeface_length;
    assert(0 && "available Image should not measure fallback text");
    return 0;
}

static int unexpected_height(void *context, int font,
                             const char *typeface, size_t typeface_length)
{
    (void)context; (void)font; (void)typeface; (void)typeface_length;
    assert(0 && "available Image should not measure fallback height");
    return 0;
}

static void raw_raster_checks(ImageRasterizer *renderer)
{
    const float source[4] = {0, 0, 2, 2};
    const float destination[4] = {0, 0, 4, 4};
    const float clip[4] = {1, 1, 2, 2};
    const float full[4] = {0, 0, 4, 4};
    const float origin[2] = {0, 0};
    const uint8_t white[4] = {255, 255, 255, 255};
    const uint8_t half_red[4] = {255, 128, 128, 128};
    memset(framebuffer, 0, sizeof(framebuffer));
    renderer->draw(renderer->context, "checker", 7, 0, source,
                   destination, clip, origin, 0, 0, white);
    pixel(0, 0, 0, 0, 0, 0);
    pixel(1, 1, 255, 0, 0, 255);
    pixel(2, 2, 255, 255, 255, 255);
    pixel(3, 3, 0, 0, 0, 0);

    memset(framebuffer, 0, sizeof(framebuffer));
    renderer->draw(renderer->context, "ignored", 7, 77, source,
                   destination, full, origin, 0, 2, white);
    pixel(0, 0, 0, 0, 0, 0);
    pixel(1, 1, 255, 0, 0, 255);
    pixel(3, 3, 0, 0, 0, 0);

    memset(framebuffer, 0, sizeof(framebuffer));
    {
        const float rotated[4] = {2, 0, 2, 2};
        renderer->draw(renderer->context, "checker", 7, 0, source,
                       rotated, full, origin, 90, 0, white);
    }
    pixel(1, 0, 255, 0, 0, 255);
    pixel(0, 0, 0, 0, 255, 255);

    memset(framebuffer, 0, sizeof(framebuffer));
    renderer->draw(renderer->context, "checker", 7, 0, source,
                   destination, full, origin, 0, 0, half_red);
    pixel(0, 0, 255, 0, 0, 128);

    for(size_t i = 0; i < sizeof(framebuffer); i += 4) {
        framebuffer[i] = framebuffer[i + 1] = framebuffer[i + 2] = 100;
        framebuffer[i + 3] = 255;
    }
    renderer->draw(renderer->context, "checker", 7, 0, source,
                   destination, full, origin, 0, 0, half_red);
    pixel(0, 0, 177, 49, 49, 255);
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    Bundle *bundle = BundleOpen(argv[1]);
    assert(bundle != NULL);
    ImageCanvas canvas = {framebuffer, 4, 4, 16, assets, 1};
    ImageRasterizer renderer = ImageCanvasRasterizer(&canvas);
    LineRenderer line = {unexpected_line, NULL};
    RoundedRectangleRenderer shape = {
        unexpected_shape, unexpected_outline, NULL};
    TextRenderer text = {unexpected_text, NULL};
    FontMeasurer fonts = {unexpected_width, unexpected_height, NULL};
    HostBinding bindings[] = {
        ImageWidthBinding(&renderer),
        ImageHeightBinding(&renderer),
        RasterImageBinding(&renderer),
        RasterLineBinding(&line),
        RasterRoundedRectangleBinding(&shape),
        RasterRoundedRectangleOutlineBinding(&shape),
        RasterTextBinding(&text),
        MeasureGlyphWidthBinding(&fonts),
        MeasureGlyphLineHeightBinding(&fonts),
    };
    long long result = 0;
    int has_result = 0;
    assert(BundleRun(bundle, bindings, 9, &result, &has_result));
    assert(has_result && result == 42);
    pixel(0, 0, 255, 0, 0, 255);
    pixel(3, 0, 0, 255, 0, 255);
    pixel(0, 3, 0, 0, 255, 255);
    pixel(3, 3, 255, 255, 255, 255);
    raw_raster_checks(&renderer);
    BundleClose(bundle);
    return 0;
}
