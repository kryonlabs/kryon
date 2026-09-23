#include "kryon_portable_host.h"

#include <assert.h>
#include <string.h>

typedef struct LineCall {
    float x1, y1, x2, y2;
    uint8_t r, g, b, a;
} LineCall;

static LineCall calls[6];
static int call_count;

static void
draw(void *context, float x1, float y1, float x2, float y2,
     uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    (void)context;
    assert(call_count < 6);
    calls[call_count++] = (LineCall){x1, y1, x2, y2, r, g, b, a};
}

int
main(int argc, char **argv)
{
    assert(argc == 2);
    Bundle *bundle = BundleOpen(argv[1]);
    assert(bundle != NULL);
    assert(BundleCapabilityCount(bundle) == 1);
    assert(strcmp(BundleCapabilityModule(bundle, 0), "raster") == 0);
    assert(strcmp(BundleCapabilityFunction(bundle, 0), "RasterLine") == 0);
    LineRenderer renderer = {draw, NULL};
    HostBinding binding = RasterLineBinding(&renderer);
    long long result = 0;
    int has_result = 0;
    assert(!BundleRun(bundle, NULL, 0, &result, &has_result));
    assert(call_count == 0);
    assert(BundleRun(bundle, &binding, 1, &result, &has_result));
    assert(has_result && result == 42 && call_count == 6);
    const LineCall expected[] = {
        {10, 20, 14, 20, 10, 20, 30, 40},
        {10, 20, 10, 23, 10, 20, 30, 40},
        {10, 23, 14, 23, 50, 60, 70, 80},
        {14, 20, 14, 23, 50, 60, 70, 80},
        {7, 3, 7, 9, 0x11, 0x22, 0x33, 0x44},
        {2, 6, 12, 6, 0x55, 0x66, 0x77, 0x88},
    };
    assert(memcmp(calls, expected, sizeof(expected)) == 0);
    BundleClose(bundle);
    return 0;
}
