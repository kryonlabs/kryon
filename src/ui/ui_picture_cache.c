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

static void
picture_mask_rounded_corners_bleed(Rectangle bounds, int radius, int bleed,
                                    Color color)
{
    int left = (int)bounds.x;
    int top = (int)bounds.y;
    int right = (int)(bounds.x + bounds.width);
    int bottom = (int)(bounds.y + bounds.height);
    int effective_radius = radius + bleed;
    int radius_sq = effective_radius * effective_radius;
    int py;
    int px;

    if(radius <= 0 || bounds.width <= 0.0f || bounds.height <= 0.0f)
        return;
    if(bleed < 0)
        bleed = 0;

    for(py = 0; py < radius; py++) {
        int dy = radius - py;
        for(px = 0; px < radius; px++) {
            int dx = radius - px;

            if(dx * dx + dy * dy <= radius_sq)
                continue;
            DrawRectangle(left + px, top + py, 1, 1, color);
            DrawRectangle(right - px - 1, top + py, 1, 1, color);
            DrawRectangle(left + px, bottom - py - 1, 1, 1, color);
            DrawRectangle(right - px - 1, bottom - py - 1, 1, 1, color);
        }
    }
}

static int
picture_style_radius(Rectangle bounds, UIPictureStyle style)
{
    float min_side = bounds.width < bounds.height ? bounds.width : bounds.height;
    int radius = style.radius_px;

    if(radius <= 0 && style.roundness > 0.0f)
        radius = (int)(min_side * style.roundness);
    return radius > 0 ? radius : 0;
}

static float
picture_roundness_from_radius(Rectangle bounds, int radius)
{
    float min_side = bounds.width < bounds.height ? bounds.width : bounds.height;

    if(radius <= 0 || min_side <= 0.0f)
        return 0.0f;
    if((float)radius > min_side * 0.5f)
        radius = (int)(min_side * 0.5f);
    return (float)radius / min_side;
}

static void
picture_apply_style(Rectangle bounds, UIPictureStyle *style, int *radius,
                    float *roundness, int *segments, int *outline_px)
{
    ThemeStyle theme_style = GetEffectiveThemeStyle();
    UIStyleTokens tokens = GetUIStyleTokens();

    if(theme_style == THEME_STYLE_RETRO) {
        *radius = 0;
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

        if(*radius <= 0)
            *radius = ScaleUIPx((int)tokens.panel_radius);
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

    if(theme_style == THEME_STYLE_AERO) {
        UIAeroScheme scheme = ui_aero_scheme();

        if(*radius <= 0)
            *radius = ScaleUIPx((int)tokens.panel_radius);
        *roundness = picture_roundness_from_radius(bounds, *radius);
        *segments = *segments < 12 ? 12 : *segments;
        style->background = scheme.glass;
        style->outline = scheme.border;
        if(style->tonal_overlay.a < 22)
            style->tonal_overlay.a = 22;
        if(style->surface_overlay.a < 34)
            style->surface_overlay.a = 34;
        if(style->scrim_top.a < 18)
            style->scrim_top.a = 18;
        if(style->scrim_bottom.a < 46)
            style->scrim_bottom.a = 46;
    }
}

void
UIDrawStyledCoverPicture(Texture2D texture, Rectangle bounds,
                         UIPictureStyle style)
{
    PictureProps picture = {0};
    Rectangle dst;
    int radius;
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

    BeginScissorMode((int)bounds.x, (int)bounds.y, (int)bounds.width,
                     (int)bounds.height);
    DrawTexturePro(texture, (Rectangle){0.0f, 0.0f, (float)texture.width,
                                        (float)texture.height},
                   dst, (Vector2){0.0f, 0.0f}, 0.0f, WHITE);
    if(style.tonal_overlay.a > 0)
        DrawRectangleRec(bounds, style.tonal_overlay);
    if(style.surface_overlay.a > 0)
        DrawRectangleRec(bounds, style.surface_overlay);
    if(style.scrim_top.a > 0 || style.scrim_bottom.a > 0)
        DrawRectangleGradientV((int)bounds.x, (int)bounds.y,
                               (int)bounds.width, (int)bounds.height,
                               style.scrim_top, style.scrim_bottom);
    EndScissorMode();

    if(roundness > 0.0f && radius > 0 && style.background.a > 0)
        picture_mask_rounded_corners_bleed(bounds, radius,
                                           outline_px + ScaleUIPx(1),
                                           style.background);
    if(theme_style == THEME_STYLE_AERO && roundness > 0.0f) {
        Color shine = WHITE;
        Color inner = WHITE;

        shine.a = GetUIStyleTokens().shine_alpha;
        if(shine.a > 0) {
            BeginScissorMode((int)bounds.x, (int)bounds.y, (int)bounds.width,
                             (int)(bounds.height * 0.42f));
            DrawRectangleRounded(bounds, roundness, segments, shine);
            EndScissorMode();
        }
        inner.a = GetEffectiveThemeDarkMode() ? 44 : 82;
        if(bounds.width > 4.0f && bounds.height > 4.0f)
            DrawRectangleRoundedLines((Rectangle){bounds.x + 1.0f,
                                                  bounds.y + 1.0f,
                                                  bounds.width - 2.0f,
                                                  bounds.height - 2.0f},
                                      roundness, segments, inner);
    }
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
