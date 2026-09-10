#include "ui_internal.h"
#include "ui_paint_internal.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#if defined(KRYON_BACKEND_RAYLIB)
/* Cache rasterized material layers, not widget state or composed backdrops.
 * Transparent texels retain straight alpha, so clipping, blending, placement,
 * and overlays still run in the caller's current rendering context. */
#define SURFACE_CACHE_COUNT 128
#define SURFACE_CACHE_BYTES (16U * 1024U * 1024U)
#define SURFACE_ENTRY_BYTES (1024U * 1024U)

typedef struct {
    SurfaceDrawing command;
    Texture2D texture;
    size_t bytes;
    unsigned long used;
} SurfaceCacheEntry;

static SurfaceCacheEntry surface_cache[SURFACE_CACHE_COUNT];
static size_t surface_cache_bytes;
static unsigned long surface_cache_clock;

static int
same_rectangle(Rectangle a, Rectangle b)
{
    return a.x == b.x && a.y == b.y &&
        a.width == b.width && a.height == b.height;
}

static int
same_surface(SurfaceDrawing a, SurfaceDrawing b)
{
    SurfaceLayer x = a.layer, y = b.layer;
    return a.scale == b.scale && same_rectangle(a.bounds, b.bounds) &&
        same_rectangle(a.area, b.area) && same_rectangle(a.surface, b.surface) &&
        same_rectangle(a.segment, b.segment) &&
        x.width == y.width && x.height == y.height && x.radius == y.radius &&
        x.stroke == y.stroke && x.blur == y.blur && x.inner_blur == y.inner_blur &&
        x.outside_only == y.outside_only && x.color == y.color &&
        x.end_color == y.end_color && x.gradient == y.gradient &&
        x.gradient_bias == y.gradient_bias;
}

static void
evict_surface(int index)
{
    SurfaceCacheEntry *entry = &surface_cache[index];
    if(entry->texture.id)
        UnloadTexture(entry->texture);
    surface_cache_bytes -= entry->bytes;
    memset(entry, 0, sizeof(*entry));
}

static int
oldest_surface(void)
{
    int oldest = 0;
    for(int i = 0; i < SURFACE_CACHE_COUNT; i++) {
        if(!surface_cache[i].texture.id)
            return i;
        if(surface_cache[i].used < surface_cache[oldest].used)
            oldest = i;
    }
    return oldest;
}
#endif

void
ui_surface_cache_shutdown(void)
{
#if defined(KRYON_BACKEND_RAYLIB)
    for(int i = 0; i < SURFACE_CACHE_COUNT; i++)
        evict_surface(i);
    surface_cache_clock = 0;
#endif
}

int
ui_draw_surface_cached(SurfaceDrawing command)
{
#if defined(KRYON_BACKEND_RAYLIB)
    if(!IsWindowReady() || !command.visible || !isfinite(command.area.x) ||
       !isfinite(command.area.y) || !isfinite(command.area.width) ||
       !isfinite(command.area.height))
        return 0;
    float left = floorf(command.area.x);
    float top = floorf(command.area.y);
    float width = ceilf(command.area.x + command.area.width) - left;
    float height = ceilf(command.area.y + command.area.height) - top;
    if(width <= 0 || height <= 0 || width > 4096 || height > 4096 ||
       width * height > SURFACE_ENTRY_BYTES / 4)
        return 0;
    size_t bytes = (size_t)width * (size_t)height * 4;
    /* Keep fractional placement in the key: subpixel coverage must not change
     * when an otherwise identical widget moves by a fraction of a pixel. */
    command.bounds.x -= left;
    command.bounds.y -= top;
    command.area.x -= left;
    command.area.y -= top;
    command.surface.x -= left;
    command.surface.y -= top;
    command.segment.x -= left;
    command.segment.y -= top;
    int index = -1;
    for(int i = 0; i < SURFACE_CACHE_COUNT; i++) {
        if(surface_cache[i].texture.id && same_surface(surface_cache[i].command, command)) {
            index = i;
            break;
        }
    }
    if(index < 0) {
        Color *pixels = malloc(bytes);
        if(pixels == NULL)
            return 0;
        for(int y = 0; y < (int)height; y++) {
            unsigned int shade = SampleColor(command.layer,
                ((float)y + 0.5f - command.bounds.y) / command.bounds.height);
            for(int x = 0; x < (int)width; x++) {
                float coverage = SampleCoverage(command.layer,
                    (float)x - command.bounds.x, (float)y - command.bounds.y, command.scale);
                coverage *= SegmentCoverage((float)x - command.surface.x,
                    command.segment.x - command.surface.x,
                    command.segment.width, command.surface.width);
                pixels[y * (int)width + x] = GetColor(Opacity(shade, coverage));
            }
        }
        Image raster = {.data = pixels, .width = (int)width, .height = (int)height,
            .mipmaps = 1, .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
        Texture2D texture = LoadTextureFromImage(raster);
        free(pixels);
        if(!texture.id)
            return 0;
        SetTextureFilter(texture, TEXTURE_FILTER_POINT);
        while(surface_cache_bytes + bytes > SURFACE_CACHE_BYTES) {
            /* The budget loop must evict an occupied entry, even when there
             * are still unused slots in the count-limited table. */
            int oldest = -1;
            for(int i = 0; i < SURFACE_CACHE_COUNT; i++) {
                if(surface_cache[i].texture.id && (oldest < 0 ||
                   surface_cache[i].used < surface_cache[oldest].used))
                    oldest = i;
            }
            if(oldest < 0)
                break;
            evict_surface(oldest);
        }
        index = oldest_surface();
        evict_surface(index);
        surface_cache[index].command = command;
        surface_cache[index].texture = texture;
        surface_cache[index].bytes = bytes;
        surface_cache_bytes += bytes;
    }
    surface_cache[index].used = ++surface_cache_clock;
    DrawTexturePro(surface_cache[index].texture, (Rectangle){0, 0, width, height},
        (Rectangle){left, top, width, height}, (Vector2){0}, 0, WHITE);
    return 1;
#else
    (void)command;
    return 0;
#endif
}
