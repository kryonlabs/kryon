#include "ui_icons.h"
#include "kryon.h"
#include "kry_sw_png.h"
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
    case ICON_SHEET_CORE:
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

unsigned int
kry_icon_asset_count(void)
{
    return ui_icon_asset_count;
}

const IconAsset *
kry_icon_assets(void)
{
    return ui_icon_assets;
}

static Texture2D
LoadIconSheet(IconSheet sheet)
{
    const unsigned char *png;
    unsigned int png_size;

    if(!icon_sheet_png(sheet, &png, &png_size))
        return (Texture2D){0};
    return load_atlas(&icon_sheets[sheet], png, png_size);
}

Texture2D
kry_icon_sheet_texture(IconSheet sheet)
{
    return LoadIconSheet(sheet);
}
