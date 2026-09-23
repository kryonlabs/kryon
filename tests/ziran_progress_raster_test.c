#include "kryon_portable_host.h"

#include <assert.h>
#include <string.h>

static int calls;

static void
fill(void *context, float x, float y, float width, float height,
     float radius, int segments, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    (void)context;
    assert(segments == 12 && radius == 0.25f);
    if(calls == 0 || calls == 4) {
        assert(x == 10 && y == 20 && width == 100 && height == 20);
        assert(r == 0x11 && g == 0x22 && b == 0x33 && a == 0x44);
    } else {
        assert(calls == 1);
        assert(x == 10 && y == 20 && width == 25 && height == 20);
        assert(r == 0x55 && g == 0x66 && b == 0x77 && a == 0x88);
    }
    calls++;
}

static void
outline(void *context, float x, float y, float width, float height,
        float radius, int segments, float line_width,
        uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    (void)context;
    assert(calls == 2 && x == 10 && y == 20 && width == 100 && height == 20);
    assert(radius == 0.25f && segments == 12 && line_width == 2.0f);
    assert(r == 0x99 && g == 0xaa && b == 0xbb && a == 0xcc);
    calls++;
}

static void
draw_text(void *context, const char *text, size_t length,
          int x, int y, int font, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    (void)context;
    assert(calls == 3 && length == 3 && memcmp(text, "50%", 3) == 0);
    assert(x == 40 && y == 25 && font == 14);
    assert(r == 0x10 && g == 0x20 && b == 0x30 && a == 0x40);
    calls++;
}

int
main(int argc, char **argv)
{
    assert(argc == 2);
    Bundle *bundle = BundleOpen(argv[1]);
    assert(bundle != NULL);
    assert(BundleCapabilityCount(bundle) == 3);
    RoundedRectangleRenderer shapes = {fill, outline, NULL};
    TextRenderer text = {draw_text, NULL};
    HostBinding bindings[] = {
        RasterRoundedRectangleBinding(&shapes),
        RasterRoundedRectangleOutlineBinding(&shapes),
        RasterTextBinding(&text),
    };
    long long result = 0;
    int has_result = 0;
    assert(!BundleRun(bundle, bindings, 2, &result, &has_result));
    assert(calls == 0);
    assert(BundleRun(bundle, bindings, 3, &result, &has_result));
    assert(has_result && result == 42 && calls == 5);
    BundleClose(bundle);
    return 0;
}
