#ifndef KRYON_PORTABLE_HOST_H
#define KRYON_PORTABLE_HOST_H

#include "ziran_host.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Bind Kryon's frame pacing effect to the platform's SetTargetFPS. */
HostBinding FramePacingBinding(void);

/* Byte-range slicing for KSS source text; the result borrows the source. */
HostBinding KssStringSliceBinding(void);

typedef struct LineRenderer {
    void (*draw)(void *context, float x1, float y1, float x2, float y2,
                 uint8_t r, uint8_t g, uint8_t b, uint8_t a);
    void *context;
} LineRenderer;

/* The renderer and its context must remain valid through BundleRun. */
HostBinding RasterLineBinding(LineRenderer *renderer);

typedef struct RoundedRectangleRenderer {
    void (*fill)(void *context, float x, float y, float width, float height,
                 float radius, int segments,
                 uint8_t r, uint8_t g, uint8_t b, uint8_t a);
    void (*outline)(void *context, float x, float y, float width, float height,
                    float radius, int segments, float line_width,
                    uint8_t r, uint8_t g, uint8_t b, uint8_t a);
    void *context;
} RoundedRectangleRenderer;

typedef struct TextRenderer {
    void (*draw)(void *context, const char *text, size_t length,
                 int x, int y, int font,
                 uint8_t r, uint8_t g, uint8_t b, uint8_t a);
    void *context;
} TextRenderer;

/* Renderers and their contexts must remain valid through BundleRun. */
HostBinding RasterRoundedRectangleBinding(RoundedRectangleRenderer *renderer);
HostBinding RasterRoundedRectangleOutlineBinding(RoundedRectangleRenderer *renderer);
HostBinding RasterTextBinding(TextRenderer *renderer);

typedef struct FontMeasurer {
    int (*width)(void *context, const char *text, size_t text_length,
                 int font, const char *typeface, size_t typeface_length);
    int (*line_height)(void *context, int font,
                       const char *typeface, size_t typeface_length);
    void *context;
} FontMeasurer;

/* Text and typeface byte spans are borrowed for the duration of each call. */
HostBinding MeasureGlyphWidthBinding(FontMeasurer *measurer);
HostBinding MeasureGlyphLineHeightBinding(FontMeasurer *measurer);

typedef struct ImageRasterizer {
    /* Return nonzero when an asset exists; missing assets leave dimensions 0. */
    int (*size)(void *context, const char *path, size_t path_length,
                int *width, int *height);
    /* Clip and radius are raw raster parameters selected by Kryon. */
    void (*draw)(void *context, const char *path, size_t path_length,
                 uint32_t texture_id, const float source[4],
                 const float destination[4], const float clip[4],
                 const float origin[2], float rotation, float radius,
                 const uint8_t tint[4]);
    void *context;
} ImageRasterizer;

HostBinding ImageWidthBinding(ImageRasterizer *renderer);
HostBinding ImageHeightBinding(ImageRasterizer *renderer);
HostBinding RasterImageBinding(ImageRasterizer *renderer);

#ifdef __cplusplus
}
#endif

#endif
