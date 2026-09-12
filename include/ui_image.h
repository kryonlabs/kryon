#ifndef KRYON_IMAGE_H
#define KRYON_IMAGE_H

/*
 * UI image widget + shared image texture cache.
 *
 * The public widget is Image(ImageProps). Host drawing support is internal
 * because raylib already owns Image as a decoded-image-in-memory type.
 */

#include "kryon_compat.generated.h"

#define KRY_IMAGE_CACHE_MAX 128

typedef enum ImageFit {
    IMAGE_FIT_STRETCH,
    IMAGE_FIT_CONTAIN,
    IMAGE_FIT_COVER
} ImageFit;

typedef struct ImageStyle {
    int enabled;
    Color background;
    Color tonal_overlay;
    Color surface_overlay;
    Color scrim_top;
    Color scrim_bottom;
    Color outline;
    float roundness;
    int radius_px;
    int segments;
    int outline_px;
} ImageStyle;

typedef struct ImageProps {
    const char *asset_path;
    const char *alt_text;
    Rectangle bounds;
    Rectangle source;
    Vector2 origin;
    float rotation;
    Color tint;
    ImageFit fit;
    ImageStyle style;
} ImageProps;

#endif
