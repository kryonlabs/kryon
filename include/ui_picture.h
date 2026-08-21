#ifndef UI_PICTURE_H
#define UI_PICTURE_H

/*
 * UI picture widget + shared picture texture cache.
 *
 * The immediate-mode UI Picture widget (Picture/PictureProps) and the retained
 * Sprite2D scene node share the same texture cache so a path is loaded at most
 * once regardless of which tree draws it. Named "Picture" rather than "Image"
 * because raylib already owns `Image` as a decoded-image-in-memory struct type.
 */

#include "kryon_compat.generated.h"

#define KRY_PICTURE_CACHE_MAX 128

typedef enum PictureFit {
    PICTURE_FIT_STRETCH,
    PICTURE_FIT_CONTAIN,
    PICTURE_FIT_COVER
} PictureFit;

typedef struct PictureStyle {
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
} PictureStyle;

typedef struct PictureProps {
    const char *asset_path;
    Rectangle bounds;
    Rectangle source;
    Vector2 origin;
    float rotation;
    Color tint;
    PictureFit fit;
    PictureStyle style;
} PictureProps;

#endif
