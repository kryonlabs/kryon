/*
 * Shared image texture cache, used by the UI Image widget and the Sprite2D
 * scene node. Lifted out of ui_tree.c so both trees share one load path.
 */

#include "ui_internal.h"
#include "ui_image_props.generated.h"
#include "ui_image_internal.h"
#include "embedded_assets.h"
#include <stdio.h>
#include <string.h>

typedef struct ImageCacheEntry {
    char path[512];
    Texture2D texture;
    int loaded;
} ImageCacheEntry;

static ImageCacheEntry image_cache[KRY_IMAGE_CACHE_MAX];

static Rectangle
image_world_rect_to_screen(Rectangle rect)
{
    return (Rectangle){
        g_ui_camera.offset.x + rect.x * g_ui_camera.zoom,
        g_ui_camera.offset.y + rect.y * g_ui_camera.zoom,
        rect.width * g_ui_camera.zoom,
        rect.height * g_ui_camera.zoom
    };
}

static void
image_begin_bounds_clip(Rectangle bounds)
{
    Rectangle screen = image_world_rect_to_screen(bounds);

    BeginClip((int)screen.x, (int)screen.y,
                (int)screen.width, (int)screen.height);
}

static const char *
image_file_ext(const char *path)
{
    const char *dot;
    if(path == NULL)
        return "";
    dot = strrchr(path, '.');
    return dot != NULL ? dot : "";
}

Texture2D
LoadImageTexture(const char *path)
{
    const EmbeddedAsset *asset;
    Image image;
    Texture2D texture = {0};
    int free_slot = -1;
    int i;

    if(path == NULL || path[0] == '\0')
        return texture;
    for(i = 0; i < KRY_IMAGE_CACHE_MAX; i++) {
        if(image_cache[i].loaded &&
           strcmp(image_cache[i].path, path) == 0)
            return image_cache[i].texture;
        if(!image_cache[i].loaded && free_slot < 0)
            free_slot = i;
    }
    if(FileExists(path))
        texture = LoadTexture(path);
    else {
        asset = GetEmbeddedAsset(path);
        if(asset == NULL)
            return texture;
        image = LoadImageFromMemory(image_file_ext(path), asset->data,
                                    (int)asset->size);
        if(image.data == NULL)
            return texture;
        texture = LoadTextureFromImage(image);
        UnloadImage(image);
    }
    if(texture.id != 0 && free_slot >= 0) {
        snprintf(image_cache[free_slot].path,
                 sizeof(image_cache[free_slot].path), "%s", path);
        image_cache[free_slot].texture = texture;
        image_cache[free_slot].loaded = 1;
    }
    return texture;
}

void
ui_image_draw_texture_strip(Texture2D texture, Rectangle source,
                            Rectangle strip, Color tint)
{
    DrawTexturePro(texture, source, strip, (Vector2){0.0f, 0.0f}, 0.0f,
                   tint);
}

void
ImageTextureTintedRaw(Texture2D texture, Rectangle bounds, Color tint)
{
    if(texture.id == 0 || texture.width <= 0 || texture.height <= 0 ||
       bounds.width <= 0.0f || bounds.height <= 0.0f)
        return;
    DrawTexturePro(texture,
                   (Rectangle){0, 0, (float)texture.width, (float)texture.height},
                   bounds, (Vector2){0}, 0.0f, tint);
}

void
ui_image_draw_clipped_texture(Texture2D texture, Rectangle source,
                              Rectangle dst, Rectangle bounds,
                              Vector2 origin, float rotation, Color tint)
{
    image_begin_bounds_clip(bounds);
    DrawTexturePro(texture, source, dst, origin, rotation, tint);
    EndClip();
}
