#ifndef UI_PICTURE_H
#define UI_PICTURE_H

/*
 * UI picture widget + shared picture texture cache.
 *
 * The immediate-mode UI Picture widget (Picture/PictureProps) and the retained
 * Sprite2D scene node both load textures through KryLoadPictureTexture so a
 * path is loaded at most once regardless of which tree draws it. Named
 * "Picture" rather than "Image" because raylib already owns `Image` as a
 * decoded-image-in-memory struct type.
 */

#include "kryon_compat.generated.h"

#define KRY_PICTURE_CACHE_MAX 128

typedef enum UIPictureFit {
    UI_PICTURE_FIT_STRETCH,
    UI_PICTURE_FIT_CONTAIN,
    UI_PICTURE_FIT_COVER
} UIPictureFit;

typedef struct PictureProps {
    const char *asset_path;
    Rectangle bounds;
    Rectangle source;
    Vector2 origin;
    float rotation;
    Color tint;
    UIPictureFit fit;
} PictureProps;

/*
 * Load (or return cached) texture for `path`. The path may be a runtime file or
 * an embedded asset name. Returns a zero-id texture on failure. The returned
 * texture is owned by the cache; callers must not UnloadTexture it.
 */
Texture2D KryLoadPictureTexture(const char *path);

/*
 * Compute the destination rectangle for a picture given its fit mode and the
 * source texture dimensions. STRETCH/CONTAIN/COVER behavior is shared by the
 * UI Picture widget and the Sprite2D scene node so both draw identically.
 */
Rectangle KryPictureFitRect(PictureProps picture, Texture2D texture);

#endif
