#include "ui_icons.h"
#include "ui_internal.h"
#include "kryon.h"
#include "runtime/surface.h"
#include <math.h>
#include "../backend/kry_sw_png.h"
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

static Texture2D
LoadIconSheet(IconSheet sheet)
{
    const unsigned char *png;
    unsigned int png_size;

    if(!icon_sheet_png(sheet, &png, &png_size))
        return (Texture2D){0};
    return load_atlas(&icon_sheets[sheet], png, png_size);
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
DrawProfileImageIcon(IconType type, Rectangle bounds, int dark_mode)
{
    const IconAsset *asset = GetIconAsset(type);

    (void)dark_mode;
    if(asset == NULL || bounds.width <= 0 || bounds.height <= 0)
        return;
    DrawIcon(type, bounds, WHITE);
}
