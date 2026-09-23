#include "kryon_portable_host.h"

#include <assert.h>
#include <stdint.h>

static int fills;

static void fill(void *context, float x, float y, float width,
                 float height, float radius, int segments,
                 uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    (void)context;
    assert(x == 2.0f && y == 3.0f && width == 40.0f &&
           height == 20.0f && radius == 0.0f && segments == 0);
    assert(r == 10 && g == 20 && b == 30);
    assert(a == (fills == 0 ? 99 : 200));
    fills++;
}

static void outline(void *context, float x, float y, float width,
                    float height, float radius, int segments,
                    float line_width, uint8_t r, uint8_t g,
                    uint8_t b, uint8_t a)
{
    (void)context; (void)x; (void)y; (void)width; (void)height;
    (void)radius; (void)segments; (void)line_width;
    (void)r; (void)g; (void)b; (void)a;
    assert(0);
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    Bundle *bundle = BundleOpen(argv[1]);
    assert(bundle != NULL);
    RoundedRectangleRenderer shape = {fill, outline, NULL};
    HostBinding binding = RasterRoundedRectangleBinding(&shape);
    BundleInstance *instance = BundleInstantiate(bundle, &binding, 1);
    assert(instance != NULL);
    fills = 0;
    for (int phase = 0; phase < 4; phase++) {
        long long value = -1;
        int has_value = 0;
        assert(BundleInstanceRun(instance, &value, &has_value));
        assert(has_value && value == phase);
        assert(fills == (phase == 0 ? 0 : phase == 1 ? 1 : 2));
    }
    BundleInstanceClose(instance);
    BundleClose(bundle);
    return 0;
}
