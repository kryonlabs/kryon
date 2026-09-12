#include "ui_icons.h"
#include "ui_internal.h"
#include "kryon.h"
#include "runtime/surface.h"
#include <math.h>
#include "../backend/kry_sw_png.h"
#include <stdlib.h>
#include <string.h>

extern const IconAsset ui_icon_assets[];
extern const unsigned int ui_icon_asset_count;
extern const unsigned char ui_icon_atlas_png[];
extern const unsigned int ui_icon_atlas_png_size;
extern const unsigned char ui_pfp_atlas_png[];
extern const unsigned int ui_pfp_atlas_png_size;
extern const unsigned char ui_platforms_atlas_png[];
extern const unsigned int ui_platforms_atlas_png_size;
extern const unsigned char ui_payments_atlas_png[];
extern const unsigned int ui_payments_atlas_png_size;
extern const unsigned char ui_language_atlas_png[];
extern const unsigned int ui_language_atlas_png_size;
extern const unsigned char ui_tiles_atlas_png[];
extern const unsigned int ui_tiles_atlas_png_size;
extern const unsigned char ui_logos_atlas_png[];
extern const unsigned int ui_logos_atlas_png_size;

static Texture2D icon_sheets[ICON_SHEET_COUNT];

static int
icon_sheet_png(IconSheet sheet, const unsigned char **png,
               unsigned int *png_size)
{
    if(png == NULL || png_size == NULL)
        return 0;

    switch(sheet) {
    case ICON_SHEET_UI:
        *png = ui_icon_atlas_png;
        *png_size = ui_icon_atlas_png_size;
        return 1;
    case ICON_SHEET_PFP:
        *png = ui_pfp_atlas_png;
        *png_size = ui_pfp_atlas_png_size;
        return 1;
    case ICON_SHEET_PLATFORMS:
        *png = ui_platforms_atlas_png;
        *png_size = ui_platforms_atlas_png_size;
        return 1;
    case ICON_SHEET_PAYMENTS:
        *png = ui_payments_atlas_png;
        *png_size = ui_payments_atlas_png_size;
        return 1;
    case ICON_SHEET_LANGUAGE:
        *png = ui_language_atlas_png;
        *png_size = ui_language_atlas_png_size;
        return 1;
    case ICON_SHEET_TILES:
        *png = ui_tiles_atlas_png;
        *png_size = ui_tiles_atlas_png_size;
        return 1;
    case ICON_SHEET_LOGOS:
        *png = ui_logos_atlas_png;
        *png_size = ui_logos_atlas_png_size;
        return 1;
    default:
        return 0;
    }
}

static Texture2D
load_atlas(Texture2D *atlas, const unsigned char *png, unsigned int png_size)
{
    Image image;

    if(atlas->id != 0)
        return *atlas;

    memset(&image, 0, sizeof(image));
    image.data = kry_sw_png_rgba(png, (size_t)png_size,
                                 &image.width, &image.height);
    if(image.data == NULL)
        return *atlas;
    image.mipmaps = 1;
    image.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;

    *atlas = LoadTextureFromImage(image);
    UnloadImage(image);
    if(atlas->id != 0)
        SetTextureFilter(*atlas, TEXTURE_FILTER_BILINEAR);
    return *atlas;
}

const IconAsset *
GetIconAsset(IconType type)
{
    for(unsigned int i = 0; i < ui_icon_asset_count; i++) {
        if(ui_icon_assets[i].type == type)
            return &ui_icon_assets[i];
    }
    return NULL;
}

const IconAsset *
GetIconAssetByName(const char *name)
{
    if(name == NULL)
        return NULL;

    for(unsigned int i = 0; i < ui_icon_asset_count; i++) {
        if(strcmp(ui_icon_assets[i].name, name) == 0)
            return &ui_icon_assets[i];
    }
    return NULL;
}

Texture2D
LoadIconSheet(IconSheet sheet)
{
    const unsigned char *png;
    unsigned int png_size;

    if(!icon_sheet_png(sheet, &png, &png_size))
        return (Texture2D){0};
    return load_atlas(&icon_sheets[sheet], png, png_size);
}

void
UnloadIconSheets(void)
{
    for(int sheet = 0; sheet < ICON_SHEET_COUNT; sheet++) {
        if(icon_sheets[sheet].id != 0)
            UnloadTexture(icon_sheets[sheet]);
    }
    memset(icon_sheets, 0, sizeof(icon_sheets));
}

static void
draw_icon_asset(const IconAsset *asset, Rectangle bounds, Color tint)
{
    Texture2D atlas;

    if(asset == NULL || bounds.width <= 0 || bounds.height <= 0 || tint.a == 0)
        return;
    atlas = LoadIconSheet(asset->sheet);
    if(atlas.id == 0)
        return;
    if(asset->sheet != ICON_SHEET_UI)
        tint = (Color){255, 255, 255, tint.a};
    DrawTexturePro(atlas, asset->source, bounds, (Vector2){0}, 0.0f, tint);
}

void
DrawIcon(IconType type, Rectangle bounds, Color tint)
{
    int shape = 0;
    if(type == ICON_PLUS) shape = 1;
    if(type == ICON_PLAY) shape = 2;
    if(type == ICON_TRASH) shape = 3;
    if(type == ICON_SAVE) shape = 4;
    if(shape != 0) {
        for(int y = (int)floorf(bounds.y); y < (int)ceilf(bounds.y + bounds.height); y++) {
            for(int x = (int)floorf(bounds.x); x < (int)ceilf(bounds.x + bounds.width); x++) {
                float coverage = IconCoverage(shape, x - bounds.x, y - bounds.y,
                    bounds.width, bounds.height);
                if(coverage > 0)
                    DrawRectangle(x, y, 1, 1, GetColor(Opacity(ColorToInt(tint), coverage)));
            }
        }
        return;
    }
    draw_icon_asset(GetIconAsset(type), bounds, tint);
}

void
DrawIconByName(const char *name, Rectangle bounds, Color tint)
{
    const IconAsset *asset = GetIconAssetByName(name);
    if(asset != NULL)
        DrawIcon(asset->type, bounds, tint);
}

void
DrawProfilePictureIcon(IconType type, Rectangle bounds, int dark_mode)
{
    const IconAsset *asset = GetIconAsset(type);

    (void)dark_mode;
    if(asset == NULL || bounds.width <= 0 || bounds.height <= 0)
        return;
    DrawIcon(type, bounds, WHITE);
}

static Texture2D
load_icon_asset_texture(const IconAsset *asset)
{
    const unsigned char *png;
    unsigned int png_size;
    unsigned char *sheet_pixels;
    unsigned char *icon_pixels;
    Texture2D texture = {0};
    Image image = {0};
    int sheet_width;
    int sheet_height;
    int x;
    int y;
    int width;
    int height;

    if(asset == NULL ||
       !icon_sheet_png(asset->sheet, &png, &png_size))
        return texture;

    sheet_pixels = kry_sw_png_rgba(png, (size_t)png_size,
                                   &sheet_width, &sheet_height);
    if(sheet_pixels == NULL)
        return texture;

    x = (int)asset->source.x;
    y = (int)asset->source.y;
    width = (int)asset->source.width;
    height = (int)asset->source.height;
    if(x < 0 || y < 0 || width <= 0 || height <= 0 ||
       x + width > sheet_width || y + height > sheet_height) {
        free(sheet_pixels);
        return texture;
    }

    icon_pixels = malloc((size_t)width * (size_t)height * 4u);
    if(icon_pixels == NULL) {
        free(sheet_pixels);
        return texture;
    }
    for(int row = 0; row < height; row++) {
        memcpy(icon_pixels + (size_t)row * (size_t)width * 4u,
               sheet_pixels + ((size_t)(y + row) * (size_t)sheet_width +
                               (size_t)x) * 4u,
               (size_t)width * 4u);
    }
    free(sheet_pixels);

    image.data = icon_pixels;
    image.width = width;
    image.height = height;
    image.mipmaps = 1;
    image.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
    texture = LoadTextureFromImage(image);
    UnloadImage(image);
    if(texture.id != 0)
        SetTextureFilter(texture, TEXTURE_FILTER_BILINEAR);
    return texture;
}

Texture2D
LoadIconTexture(IconType type)
{
    return load_icon_asset_texture(GetIconAsset(type));
}

Texture2D
LoadIconTextureByName(const char *name)
{
    return load_icon_asset_texture(GetIconAssetByName(name));
}

void
LoadAllIconTextures(Texture2D *icons)
{
    if(icons == NULL)
        return;
    for(int i = 1; i < ICON_COUNT; i++)
        if(icons[i].id == 0)
            icons[i] = LoadIconTexture((IconType)i);
}

void
UnloadAllIconTextures(Texture2D *icons)
{
    if(icons == NULL)
        return;
    for(int i = 0; i < ICON_COUNT; i++) {
        if(icons[i].id != 0)
            UnloadTexture(icons[i]);
        memset(&icons[i], 0, sizeof(icons[i]));
    }
}
