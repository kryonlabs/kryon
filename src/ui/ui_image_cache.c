/*
 * Shared image texture cache, used by the UI Image widget and the Sprite2D
 * scene node. Lifted out of ui_tree.c so both trees share one load path.
 */

#include "ui_internal.h"
#include "ui_image.h"
#include "ui_image_internal.h"
#include "embedded_assets.h"
#include <math.h>
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

static float
image_row_inset(Rectangle bounds, float radius, float sample_y)
{
    float top_center;
    float bottom_center;
    float dy = 0.0f;
    float inside;

    if(radius <= 0.0f)
        return 0.0f;

    top_center = bounds.y + radius;
    bottom_center = bounds.y + bounds.height - radius;
    if(sample_y < top_center)
        dy = top_center - sample_y;
    else if(sample_y > bottom_center)
        dy = sample_y - bottom_center;
    if(dy <= 0.0f)
        return 0.0f;
    if(dy >= radius)
        return radius;

    inside = radius * radius - dy * dy;
    return radius - sqrtf(inside > 0.0f ? inside : 0.0f);
}

static Rectangle
image_source_for_strip(Rectangle source_base, Rectangle dst, Rectangle strip)
{
    Rectangle source = {0.0f, 0.0f, 0.0f, 0.0f};

    if(dst.width == 0.0f || dst.height == 0.0f)
        return source;
    source.x = source_base.x + (strip.x - dst.x) * source_base.width / dst.width;
    source.y = source_base.y + (strip.y - dst.y) * source_base.height / dst.height;
    source.width = strip.width * source_base.width / dst.width;
    source.height = strip.height * source_base.height / dst.height;
    return source;
}

static void
image_draw_texture_strip(Texture2D texture, Rectangle source_base,
                           Rectangle dst, Rectangle strip, Color tint)
{
    Rectangle source;

    if(strip.width <= 0.0f || strip.height <= 0.0f)
        return;
    source = image_source_for_strip(source_base, dst, strip);
    DrawTexturePro(texture, source, strip, (Vector2){0.0f, 0.0f}, 0.0f,
                   tint);
}

void
ui_image_draw_rounded_texture(Texture2D texture, Rectangle source,
                             Rectangle dst, Rectangle bounds, float radius,
                             Color tint)
{
    int y_start;
    int y_end;

    if(radius <= 0.0f) {
        image_draw_texture_strip(texture, source, dst, bounds, tint);
        return;
    }

    y_start = (int)ceilf(bounds.y);
    y_end = (int)floorf(bounds.y + bounds.height);
    for(int y = y_start; y < y_end; y++) {
        float sample_y = (float)y + 0.5f;
        float inset;
        float left;
        float right;
        Rectangle strip;

        inset = ceilf(image_row_inset(bounds, radius, sample_y));
        left = ceilf(bounds.x + inset);
        right = floorf(bounds.x + bounds.width - inset);
        if(right <= left)
            continue;
        strip = (Rectangle){left, (float)y, right - left, 1.0f};
        image_draw_texture_strip(texture, source, dst, strip, tint);
    }
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
