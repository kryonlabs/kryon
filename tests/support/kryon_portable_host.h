/* Native host callback ABI. Widget and tree behavior is implemented in Ziran. */
#ifndef ZI_KRYON_PORTABLE_HOST_H
#define ZI_KRYON_PORTABLE_HOST_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "ziran_host.h"

#ifdef __cplusplus
extern "C" {
#endif
typedef void (*LineDrawFn)(void*, float, float, float, float, uint8_t, uint8_t, uint8_t, uint8_t);
typedef void (*RoundedRectangleFillFn)(void*, float, float, float, float, float, int32_t, uint8_t, uint8_t, uint8_t, uint8_t);
typedef void (*RoundedRectangleOutlineFn)(void*, float, float, float, float, float, int32_t, float, uint8_t, uint8_t, uint8_t, uint8_t);
typedef void (*TextDrawFn)(void*, uint8_t*, size_t, int32_t, int32_t, int32_t, uint8_t, uint8_t, uint8_t, uint8_t);
typedef void (*TextDrawClippedFn)(void*, uint8_t*, size_t, int32_t, int32_t, int32_t, float*, uint8_t, uint8_t, uint8_t, uint8_t);
typedef int32_t (*GlyphWidthFn)(void*, uint8_t*, size_t, int32_t, uint8_t*, size_t);
typedef int32_t (*GlyphLineHeightFn)(void*, int32_t, uint8_t*, size_t);
typedef int32_t (*ImageSizeFn)(void*, uint8_t*, size_t, int32_t*, int32_t*);
typedef void (*ImageDrawFn)(void*, uint8_t*, size_t, uint32_t, float*, float*, float*, float*, float, float, uint8_t*);
typedef struct LineRenderer LineRenderer;
typedef struct RoundedRectangleRenderer RoundedRectangleRenderer;
typedef struct TextRenderer TextRenderer;
typedef struct FontMeasurer FontMeasurer;
typedef struct ImageRasterizer ImageRasterizer;

struct LineRenderer {
    LineDrawFn draw;
    void* context;
};

struct RoundedRectangleRenderer {
    RoundedRectangleFillFn fill;
    RoundedRectangleOutlineFn outline;
    void* context;
};

struct TextRenderer {
    TextDrawFn draw;
    void* context;
    TextDrawClippedFn draw_clipped;
};

struct FontMeasurer {
    GlyphWidthFn width;
    GlyphLineHeightFn line_height;
    void* context;
};

struct ImageRasterizer {
    ImageSizeFn size;
    ImageDrawFn draw;
    void* context;
};
HostBinding RasterLineBinding(LineRenderer* renderer);
HostBinding RasterRoundedRectangleBinding(RoundedRectangleRenderer* renderer);
HostBinding RasterRoundedRectangleOutlineBinding(RoundedRectangleRenderer* renderer);
HostBinding RasterTextBinding(TextRenderer* renderer);
HostBinding RasterTextClippedBinding(TextRenderer* renderer);
HostBinding MeasureGlyphWidthBinding(FontMeasurer* measurer);
HostBinding MeasureGlyphLineHeightBinding(FontMeasurer* measurer);
HostBinding ImageWidthBinding(ImageRasterizer* renderer);
HostBinding ImageHeightBinding(ImageRasterizer* renderer);
HostBinding RasterImageBinding(ImageRasterizer* renderer);

#ifdef __cplusplus
}
#endif

#endif /* ZI_KRYON_PORTABLE_HOST_H */
