#ifndef IMAGE_CANVAS_H
#define IMAGE_CANVAS_H

#include "kryon_portable_host.h"

/* Straight-alpha RGBA8 pixels and assets are borrowed for the bindings' lifetime. */
typedef struct ImageAsset {
    const char *path;
    uint32_t texture_id;
    const uint8_t *pixels;
    int width;
    int height;
    int stride;
} ImageAsset;

typedef struct ImageCanvas {
    uint8_t *pixels;
    int width;
    int height;
    int stride;
    const ImageAsset *assets;
    size_t asset_count;
} ImageCanvas;

/* A headless rasterizer; link with libkryon_host.a and -lm. */
ImageRasterizer ImageCanvasRasterizer(ImageCanvas *canvas);

#endif
