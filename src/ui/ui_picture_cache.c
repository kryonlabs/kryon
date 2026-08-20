/*
 * Shared image texture cache, used by the UI Image widget and the Sprite2D
 * scene node. Lifted out of ui_tree.c so both trees share one load path.
 */

#include "ui_internal.h"
#include "ui_picture.h"
#include "embedded_assets.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

typedef struct PictureCacheEntry {
    char path[512];
    Texture2D texture;
    int loaded;
} PictureCacheEntry;

static PictureCacheEntry picture_cache[KRY_PICTURE_CACHE_MAX];

static const char *
picture_file_ext(const char *path)
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
        if(picture_cache[i].loaded &&
           strcmp(picture_cache[i].path, path) == 0)
            return picture_cache[i].texture;
        if(!picture_cache[i].loaded && free_slot < 0)
            free_slot = i;
    }
    if(FileExists(path))
        texture = LoadTexture(path);
    else {
        asset = GetEmbeddedAsset(path);
        if(asset == NULL)
            return texture;
        image = LoadImageFromMemory(picture_file_ext(path), asset->data,
                                    (int)asset->size);
        if(image.data == NULL)
            return texture;
        texture = LoadTextureFromImage(image);
        UnloadImage(image);
    }
    if(texture.id != 0 && free_slot >= 0) {
        snprintf(picture_cache[free_slot].path,
                 sizeof(picture_cache[free_slot].path), "%s", path);
        picture_cache[free_slot].texture = texture;
        picture_cache[free_slot].loaded = 1;
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

static float
picture_radius_from_roundness(Rectangle bounds, float roundness)
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
picture_style_radius(Rectangle bounds, UIPictureStyle style)
{
    float min_side = bounds.width < bounds.height ? bounds.width : bounds.height;
    float radius = (float)style.radius_px;

    if(radius <= 0.0f && style.roundness > 0.0f)
        radius = picture_radius_from_roundness(bounds, style.roundness);
    if(min_side > 0.0f && radius > min_side * 0.5f)
        radius = min_side * 0.5f;
    return radius > 0.0f ? radius : 0.0f;
}

static float
picture_roundness_from_radius(Rectangle bounds, float radius)
{
    float min_side = bounds.width < bounds.height ? bounds.width : bounds.height;

    if(radius <= 0.0f || min_side <= 0.0f)
        return 0.0f;
    if(radius > min_side * 0.5f)
        radius = min_side * 0.5f;
    return (radius * 2.0f) / min_side;
}

static float
picture_row_inset(Rectangle bounds, float radius, float sample_y)
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
picture_source_for_strip(Texture2D texture, Rectangle dst, Rectangle strip)
{
    Rectangle source = {0.0f, 0.0f, 0.0f, 0.0f};

    if(dst.width == 0.0f || dst.height == 0.0f)
        return source;
    source.x = (strip.x - dst.x) * (float)texture.width / dst.width;
    source.y = (strip.y - dst.y) * (float)texture.height / dst.height;
    source.width = strip.width * (float)texture.width / dst.width;
    source.height = strip.height * (float)texture.height / dst.height;
    return source;
}

static void
picture_draw_texture_strip(Texture2D texture, Rectangle dst, Rectangle strip)
{
    Rectangle source;

    if(strip.width <= 0.0f || strip.height <= 0.0f)
        return;
    source = picture_source_for_strip(texture, dst, strip);
    DrawTexturePro(texture, source, strip, (Vector2){0.0f, 0.0f}, 0.0f,
                   WHITE);
}

static void
picture_draw_rounded_texture(Texture2D texture, Rectangle dst,
                             Rectangle bounds, float radius)
{
    int y_start;
    int y_end;

    if(radius <= 0.0f) {
        picture_draw_texture_strip(texture, dst, bounds);
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
        Rectangle strip;

        if(strip_h <= 0.0f)
            continue;
        inset = picture_row_inset(bounds, radius, sample_y);
        strip = (Rectangle){bounds.x + inset, strip_y,
                            bounds.width - inset * 2.0f, strip_h};
        picture_draw_texture_strip(texture, dst, strip);
    }
}

static void
picture_draw_rounded_solid(Rectangle bounds, float radius, Color color)
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
        inset = picture_row_inset(bounds, radius, sample_y);
        DrawRectangleRec((Rectangle){bounds.x + inset, strip_y,
                                     bounds.width - inset * 2.0f, strip_h},
                         color);
    }
}

static Color
picture_lerp_color(Color top, Color bottom, float t)
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
picture_draw_rounded_gradient(Rectangle bounds, float radius, Color top,
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
        inset = picture_row_inset(bounds, radius, sample_y);
        t = bounds.height > 0.0f ? (sample_y - bounds.y) / bounds.height
                                 : 0.0f;
        color = picture_lerp_color(top, bottom, t);
        DrawRectangleRec((Rectangle){bounds.x + inset, strip_y,
                                     bounds.width - inset * 2.0f, strip_h},
                         color);
    }
}

static void
picture_apply_style(Rectangle bounds, UIPictureStyle *style, float *radius,
                    float *roundness, int *segments, int *outline_px)
{
    ThemeStyle theme_style = GetEffectiveThemeStyle();
    UIStyleTokens tokens = GetUIStyleTokens();

    if(theme_style == THEME_STYLE_RETRO) {
        *radius = 0.0f;
        *roundness = 0.0f;
        *segments = 1;
        *outline_px = ScaleUIPx(2);
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

    if(theme_style == THEME_STYLE_MATERIAL) {
        UIMaterialScheme scheme = ui_material_scheme();

        if(*radius <= 0.0f)
            *radius = (float)ScaleUIPx((int)tokens.panel_radius);
        *roundness = picture_roundness_from_radius(bounds, *radius);
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
UIDrawStyledCoverPicture(Texture2D texture, Rectangle bounds,
                         UIPictureStyle style)
{
    PictureProps picture = {0};
    Rectangle dst;
    float radius;
    int segments;
    int outline_px;
    float roundness;
    ThemeStyle theme_style;

    if(texture.id == 0 || texture.width <= 0 || texture.height <= 0 ||
       bounds.width <= 0.0f || bounds.height <= 0.0f)
        return;

    picture.bounds = bounds;
    picture.fit = UI_PICTURE_FIT_COVER;
    dst = KryPictureFitRect(picture, texture);
    radius = picture_style_radius(bounds, style);
    segments = style.segments > 0 ? style.segments : 10;
    outline_px = style.outline_px > 0 ? style.outline_px : 1;
    roundness = style.roundness > 0.0f ? style.roundness : 0.0f;
    theme_style = GetEffectiveThemeStyle();
    picture_apply_style(bounds, &style, &radius, &roundness, &segments,
                        &outline_px);

    if(theme_style == THEME_STYLE_MATERIAL)
        ui_material_elevation(bounds, roundness, GetUIStyleTokens().shadow_offset_y);

    if(style.background.a > 0) {
        if(roundness > 0.0f)
            DrawRectangleRounded(bounds, roundness, segments, style.background);
        else
            DrawRectangleRec(bounds, style.background);
    }

    picture_draw_rounded_texture(texture, dst, bounds, radius);
    if(style.tonal_overlay.a > 0)
        picture_draw_rounded_solid(bounds, radius, style.tonal_overlay);
    if(style.surface_overlay.a > 0)
        picture_draw_rounded_solid(bounds, radius, style.surface_overlay);
    if(style.scrim_top.a > 0 || style.scrim_bottom.a > 0)
        picture_draw_rounded_gradient(bounds, radius, style.scrim_top,
                                      style.scrim_bottom);
    if(theme_style == THEME_STYLE_RETRO) {
        DrawUIBevel((int)bounds.x, (int)bounds.y, (int)bounds.width,
                    (int)bounds.height, LightenUIColor(GetThemeBackground(), 52),
                    DarkenUIColor(GetThemeBackground(), 50));
        if(style.outline.a > 0)
            DrawRectangleLinesEx(bounds, (float)outline_px, style.outline);
    } else if(roundness > 0.0f && style.outline.a > 0)
        DrawRectangleRoundedLinesEx(bounds, roundness, segments,
                                    (float)outline_px, style.outline);
    else if(style.outline.a > 0)
        DrawRectangleLinesEx(bounds, (float)outline_px, style.outline);
}
