/*
 * Shared image texture cache, used by the UI Image widget and the Sprite2D
 * scene node. Lifted out of ui_tree.c so both trees share one load path.
 */

#include "ui_picture.h"
#include "embedded_assets.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

typedef struct KryPictureCacheEntry {
    char path[512];
    Texture2D texture;
    int loaded;
} KryPictureCacheEntry;

static KryPictureCacheEntry kry_picture_cache[KRY_PICTURE_CACHE_MAX];

static const char *
kry_picture_file_ext(const char *path)
{
    const char *dot;
    if(path == NULL)
        return "";
    dot = strrchr(path, '.');
    return dot != NULL ? dot : "";
}

Texture2D
KryLoadPictureTexture(const char *path)
{
    const EmbeddedAsset *asset;
    Image image;
    Texture2D texture = {0};
    int free_slot = -1;
    int i;

    if(path == NULL || path[0] == '\0')
        return texture;
    for(i = 0; i < KRY_PICTURE_CACHE_MAX; i++) {
        if(kry_picture_cache[i].loaded &&
           strcmp(kry_picture_cache[i].path, path) == 0)
            return kry_picture_cache[i].texture;
        if(!kry_picture_cache[i].loaded && free_slot < 0)
            free_slot = i;
    }
    if(FileExists(path))
        texture = LoadTexture(path);
    else {
        asset = GetEmbeddedAsset(path);
        if(asset == NULL)
            return texture;
        image = LoadImageFromMemory(kry_picture_file_ext(path), asset->data,
                                    (int)asset->size);
        if(image.data == NULL)
            return texture;
        texture = LoadTextureFromImage(image);
        UnloadImage(image);
    }
    if(texture.id != 0 && free_slot >= 0) {
        snprintf(kry_picture_cache[free_slot].path,
                 sizeof(kry_picture_cache[free_slot].path), "%s", path);
        kry_picture_cache[free_slot].texture = texture;
        kry_picture_cache[free_slot].loaded = 1;
    }
    return texture;
}

Rectangle
KryPictureFitRect(PictureProps picture, Texture2D texture)
{
    Rectangle dst = picture.bounds;
    float src_w = picture.source.width != 0.0f ? fabsf(picture.source.width)
                                               : (float)texture.width;
    float src_h = picture.source.height != 0.0f ? fabsf(picture.source.height)
                                                : (float)texture.height;
    float sx;
    float sy;
    float scale;

    if(picture.fit == UI_PICTURE_FIT_CONTAIN || picture.fit == UI_PICTURE_FIT_COVER) {
        if(src_w == 0.0f || src_h == 0.0f)
            return dst;
        sx = dst.width / src_w;
        sy = dst.height / src_h;
        scale = picture.fit == UI_PICTURE_FIT_COVER
                    ? (sx > sy ? sx : sy)
                    : (sx < sy ? sx : sy);
        dst.width = src_w * scale;
        dst.height = src_h * scale;
        dst.x = picture.bounds.x + (picture.bounds.width - dst.width) * 0.5f;
        dst.y = picture.bounds.y + (picture.bounds.height - dst.height) * 0.5f;
    }
    return dst;
}
