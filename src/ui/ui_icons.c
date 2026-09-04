#include "ui_icons.h"
#include "ui_internal.h"
#include "kryon.h"
#include "../backend/kry_sw_png.h"
#include <stdlib.h>
#include <string.h>

extern const UIIconAsset ui_icon_assets[];
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

static Texture2D icon_sheets[UI_ICON_SHEET_COUNT];

static int
icon_sheet_png(UIIconSheet sheet, const unsigned char **png,
               unsigned int *png_size)
{
    if(png == NULL || png_size == NULL)
        return 0;

    switch(sheet) {
    case UI_ICON_SHEET_UI:
        *png = ui_icon_atlas_png;
        *png_size = ui_icon_atlas_png_size;
        return 1;
    case UI_ICON_SHEET_PFP:
        *png = ui_pfp_atlas_png;
        *png_size = ui_pfp_atlas_png_size;
        return 1;
    case UI_ICON_SHEET_PLATFORMS:
        *png = ui_platforms_atlas_png;
        *png_size = ui_platforms_atlas_png_size;
        return 1;
    case UI_ICON_SHEET_PAYMENTS:
        *png = ui_payments_atlas_png;
        *png_size = ui_payments_atlas_png_size;
        return 1;
    case UI_ICON_SHEET_LANGUAGE:
        *png = ui_language_atlas_png;
        *png_size = ui_language_atlas_png_size;
        return 1;
    case UI_ICON_SHEET_TILES:
        *png = ui_tiles_atlas_png;
        *png_size = ui_tiles_atlas_png_size;
        return 1;
    case UI_ICON_SHEET_LOGOS:
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
        SetTextureFilter(*atlas, TEXTURE_FILTER_POINT);
    return *atlas;
}

const UIIconAsset *
GetUIIconAsset(UIIconType type)
{
    for(unsigned int i = 0; i < ui_icon_asset_count; i++) {
        if(ui_icon_assets[i].type == type)
            return &ui_icon_assets[i];
    }
    return NULL;
}

const UIIconAsset *
GetUIIconAssetByName(const char *name)
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
LoadIconSheet(UIIconSheet sheet)
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
    for(int sheet = 0; sheet < UI_ICON_SHEET_COUNT; sheet++) {
        if(icon_sheets[sheet].id != 0)
            UnloadTexture(icon_sheets[sheet]);
    }
    memset(icon_sheets, 0, sizeof(icon_sheets));
}

static void
draw_icon_asset(const UIIconAsset *asset, Rectangle bounds, Color tint)
{
    Texture2D atlas;

    if(asset == NULL || bounds.width <= 0 || bounds.height <= 0)
        return;
    atlas = LoadIconSheet(asset->sheet);
    if(atlas.id == 0)
        return;
    if(tint.a == 0)
        tint = WHITE;
    if(asset->sheet != UI_ICON_SHEET_UI)
        tint = (Color){255, 255, 255, tint.a};
    DrawTexturePro(atlas, asset->source, bounds, (Vector2){0}, 0.0f, tint);
}

void
DrawIcon(UIIconType type, Rectangle bounds, Color tint)
{
    draw_icon_asset(GetUIIconAsset(type), bounds, tint);
}

void
DrawIconByName(const char *name, Rectangle bounds, Color tint)
{
    draw_icon_asset(GetUIIconAssetByName(name), bounds, tint);
}

static Texture2D
load_icon_asset_texture(const UIIconAsset *asset)
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
        SetTextureFilter(texture, TEXTURE_FILTER_POINT);
    return texture;
}

Texture2D
LoadUIIconTexture(UIIconType type)
{
    return load_icon_asset_texture(GetUIIconAsset(type));
}

Texture2D
LoadUIIconTextureByName(const char *name)
{
    return load_icon_asset_texture(GetUIIconAssetByName(name));
}

void
LoadAllUIIconTextures(Texture2D *icons)
{
    if(icons == NULL)
        return;
    for(int i = 1; i < UI_ICON_TYPE_COUNT; i++)
        if(icons[i].id == 0)
            icons[i] = LoadUIIconTexture((UIIconType)i);
}

void
UnloadAllUIIconTextures(Texture2D *icons)
{
    if(icons == NULL)
        return;
    for(int i = 0; i < UI_ICON_TYPE_COUNT; i++) {
        if(icons[i].id != 0)
            UnloadTexture(icons[i]);
        memset(&icons[i], 0, sizeof(icons[i]));
    }
}
