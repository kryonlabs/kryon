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

    BeginUIClip((int)screen.x, (int)screen.y,
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

Rectangle
ImageFitRect(ImageProps image, Texture2D texture)
{
    Rectangle dst = image.bounds;
    float src_w = image.source.width != 0.0f ? fabsf(image.source.width)
                                               : (float)texture.width;
    float src_h = image.source.height != 0.0f ? fabsf(image.source.height)
                                                : (float)texture.height;
    float sx;
    float sy;
    float scale;

    if(image.fit == IMAGE_FIT_CONTAIN || image.fit == IMAGE_FIT_COVER) {
        if(src_w == 0.0f || src_h == 0.0f)
            return dst;
        sx = dst.width / src_w;
        sy = dst.height / src_h;
        scale = image.fit == IMAGE_FIT_COVER
                    ? (sx > sy ? sx : sy)
                    : (sx < sy ? sx : sy);
        dst.width = src_w * scale;
        dst.height = src_h * scale;
        dst.x = image.bounds.x + (image.bounds.width - dst.width) * 0.5f;
        dst.y = image.bounds.y + (image.bounds.height - dst.height) * 0.5f;
    }
    return dst;
}

static float
image_radius_from_roundness(Rectangle bounds, float roundness)
{
    float min_side;

    if(roundness <= 0.0f || bounds.width <= 0.0f || bounds.height <= 0.0f)
        return 0.0f;
    if(roundness > 1.0f)
        roundness = 1.0f;
    min_side = bounds.width < bounds.height ? bounds.width : bounds.height;
    return min_side * roundness * 0.5f;
}

static float
image_style_radius(Rectangle bounds, ImageStyle style)
{
    float min_side = bounds.width < bounds.height ? bounds.width : bounds.height;
    float radius = (float)style.radius_px;

    if(radius <= 0.0f && style.roundness > 0.0f)
        radius = image_radius_from_roundness(bounds, style.roundness);
    if(min_side > 0.0f && radius > min_side * 0.5f)
        radius = min_side * 0.5f;
    return radius > 0.0f ? radius : 0.0f;
}

static float
image_roundness_from_radius(Rectangle bounds, float radius)
{
    float min_side = bounds.width < bounds.height ? bounds.width : bounds.height;

    if(radius <= 0.0f || min_side <= 0.0f)
        return 0.0f;
    if(radius > min_side * 0.5f)
        radius = min_side * 0.5f;
    return (radius * 2.0f) / min_side;
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
image_default_source(Texture2D texture, Rectangle source)
{
    if(source.width == 0.0f || source.height == 0.0f)
        return (Rectangle){0.0f, 0.0f, (float)texture.width,
                           (float)texture.height};
    return source;
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

static void
image_draw_rounded_texture(Texture2D texture, Rectangle source,
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

static void
image_draw_rounded_solid(Rectangle bounds, float radius, Color color)
{
    int y_start;
    int y_end;

    if(color.a == 0)
        return;
    if(radius <= 0.0f) {
        DrawRectangleRec(bounds, color);
        return;
    }

    y_start = (int)floorf(bounds.y);
    y_end = (int)ceilf(bounds.y + bounds.height);
    for(int y = y_start; y < y_end; y++) {
        float row_y = (float)y;
        float row_bottom = row_y + 1.0f;
        float strip_y = row_y < bounds.y ? bounds.y : row_y;
        float strip_bottom = row_bottom > bounds.y + bounds.height
                                 ? bounds.y + bounds.height
                                 : row_bottom;
        float strip_h = strip_bottom - strip_y;
        float sample_y = strip_y + strip_h * 0.5f;
        float inset;

        if(strip_h <= 0.0f)
            continue;
        inset = image_row_inset(bounds, radius, sample_y);
        DrawRectangleRec((Rectangle){bounds.x + inset, strip_y,
                                     bounds.width - inset * 2.0f, strip_h},
                         color);
    }
}

static Color
image_lerp_color(Color top, Color bottom, float t)
{
    if(t < 0.0f)
        t = 0.0f;
    if(t > 1.0f)
        t = 1.0f;
    return (Color){
        (unsigned char)((float)top.r + ((float)bottom.r - (float)top.r) * t),
        (unsigned char)((float)top.g + ((float)bottom.g - (float)top.g) * t),
        (unsigned char)((float)top.b + ((float)bottom.b - (float)top.b) * t),
        (unsigned char)((float)top.a + ((float)bottom.a - (float)top.a) * t)
    };
}

static void
image_draw_rounded_gradient(Rectangle bounds, float radius, Color top,
                              Color bottom)
{
    int y_start;
    int y_end;

    if(top.a == 0 && bottom.a == 0)
        return;
    if(radius <= 0.0f) {
        DrawRectangleGradientV((int)bounds.x, (int)bounds.y,
                               (int)bounds.width, (int)bounds.height, top,
                               bottom);
        return;
    }

    y_start = (int)floorf(bounds.y);
    y_end = (int)ceilf(bounds.y + bounds.height);
    for(int y = y_start; y < y_end; y++) {
        float row_y = (float)y;
        float row_bottom = row_y + 1.0f;
        float strip_y = row_y < bounds.y ? bounds.y : row_y;
        float strip_bottom = row_bottom > bounds.y + bounds.height
                                 ? bounds.y + bounds.height
                                 : row_bottom;
        float strip_h = strip_bottom - strip_y;
        float sample_y = strip_y + strip_h * 0.5f;
        float inset;
        float t;
        Color color;

        if(strip_h <= 0.0f)
            continue;
        inset = image_row_inset(bounds, radius, sample_y);
        t = bounds.height > 0.0f ? (sample_y - bounds.y) / bounds.height
                                 : 0.0f;
        color = image_lerp_color(top, bottom, t);
        DrawRectangleRec((Rectangle){bounds.x + inset, strip_y,
                                     bounds.width - inset * 2.0f, strip_h},
                         color);
    }
}

static void
image_apply_style(Rectangle bounds, ImageStyle *style, float *radius,
                    float *roundness, int *segments, int *outline_px)
{
    ThemeStyle theme_style = GetEffectiveThemeStyle();
    ThemeMetrics tokens = GetThemeMetrics();

    if(theme_style == THEME_STYLE_CLASSIC) {
        *radius = 0.0f;
        *roundness = 0.0f;
        *segments = 1;
        *outline_px = Scale(2);
        style->surface_overlay.a = 0;
        style->scrim_top.a = 0;
        if(style->scrim_bottom.a > 30)
            style->scrim_bottom.a = 30;
        if(style->tonal_overlay.a > 24)
            style->tonal_overlay.a = 24;
        style->outline = DarkenUIColor(GetThemeBackground(), 44);
        style->outline.a = 255;
        return;
    }

    if(theme_style == THEME_STYLE_DEFAULT) {
        ThemeScheme scheme = ui_default_scheme();

        if(*radius <= 0.0f)
            *radius = (float)Scale((int)tokens.panel_radius);
        *roundness = image_roundness_from_radius(bounds, *radius);
        *segments = *segments < 12 ? 12 : *segments;
        style->background = scheme.surface_container;
        style->outline = scheme.outline;
        if(style->tonal_overlay.a > 30)
            style->tonal_overlay.a = 30;
        if(style->surface_overlay.a > 18)
            style->surface_overlay.a = 18;
        if(style->scrim_top.a > 8)
            style->scrim_top.a = 8;
        if(style->scrim_bottom.a > 42)
            style->scrim_bottom.a = 42;
        return;
    }

}

void
ImageTexture(Texture2D texture, ImageProps image)
{
    Rectangle source;
    Rectangle dst;
    float radius;
    int segments;
    int outline_px;
    float roundness;
    ThemeStyle theme_style;

    if(texture.id == 0 || texture.width <= 0 || texture.height <= 0 ||
       image.bounds.width <= 0.0f || image.bounds.height <= 0.0f)
        return;

    source = image_default_source(texture, image.source);
    dst = ImageFitRect(image, texture);
    image.tint = image.tint.a == 0 ? WHITE : image.tint;

    if(!image.style.enabled) {
        image_begin_bounds_clip(image.bounds);
        DrawTexturePro(texture, source, dst, image.origin, image.rotation,
                       image.tint);
        EndUIClip();
        return;
    }

    radius = image_style_radius(image.bounds, image.style);
    segments = image.style.segments > 0 ? image.style.segments : 10;
    outline_px = image.style.outline_px > 0 ? image.style.outline_px : 1;
    roundness = image.style.roundness > 0.0f ? image.style.roundness : 0.0f;
    theme_style = GetEffectiveThemeStyle();
    image_apply_style(image.bounds, &image.style, &radius, &roundness, &segments,
                        &outline_px);

    if(theme_style == THEME_STYLE_DEFAULT)
        ui_default_elevation(image.bounds, roundness,
                              GetThemeMetrics().shadow_offset_y);

    if(image.style.background.a > 0) {
        if(roundness > 0.0f)
            DrawRectangleRounded(image.bounds, roundness, segments,
                                 image.style.background);
        else
            DrawRectangleRec(image.bounds, image.style.background);
    }

    image_draw_rounded_texture(texture, source, dst, image.bounds, radius,
                                 image.tint);
    if(image.style.tonal_overlay.a > 0)
        image_draw_rounded_solid(image.bounds, radius,
                                   image.style.tonal_overlay);
    if(image.style.surface_overlay.a > 0)
        image_draw_rounded_solid(image.bounds, radius,
                                   image.style.surface_overlay);
    if(image.style.scrim_top.a > 0 || image.style.scrim_bottom.a > 0)
        image_draw_rounded_gradient(image.bounds, radius,
                                      image.style.scrim_top,
                                      image.style.scrim_bottom);
    if(theme_style == THEME_STYLE_CLASSIC) {
        RenderBevel((int)image.bounds.x, (int)image.bounds.y,
                    (int)image.bounds.width, (int)image.bounds.height,
                    LightenUIColor(GetThemeBackground(), 52),
                    DarkenUIColor(GetThemeBackground(), 50));
        if(image.style.outline.a > 0)
            DrawRectangleLinesEx(image.bounds, (float)outline_px,
                                 image.style.outline);
    } else if(roundness > 0.0f && image.style.outline.a > 0)
        DrawRectangleRoundedLinesEx(image.bounds, roundness, segments,
                                    (float)outline_px, image.style.outline);
    else if(image.style.outline.a > 0)
        DrawRectangleLinesEx(image.bounds, (float)outline_px,
                             image.style.outline);
}
