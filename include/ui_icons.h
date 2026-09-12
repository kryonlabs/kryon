#ifndef UI_ICONS_H
#define UI_ICONS_H

#include "kryon.h"
#include "ui_icon_types.h"

typedef enum IconSheet {
    ICON_SHEET_UI,
    ICON_SHEET_PFP,
    ICON_SHEET_PLATFORMS,
    ICON_SHEET_PAYMENTS,
    ICON_SHEET_LANGUAGE,
    ICON_SHEET_TILES,
    ICON_SHEET_LOGOS,
    ICON_SHEET_COUNT
} IconSheet;

typedef struct IconAsset {
    IconType type;
    const char *name;
    IconSheet sheet;
    Rectangle source;
} IconAsset;

const IconAsset *GetIconAsset(IconType type);
const IconAsset *GetIconAssetByName(const char *name);
Texture2D LoadIconSheet(IconSheet sheet);
void UnloadIconSheets(void);
void DrawIcon(IconType type, Rectangle bounds, Color tint);
void DrawIconByName(const char *name, Rectangle bounds, Color tint);
void DrawProfilePictureIcon(IconType type, Rectangle bounds, int dark_mode);

/* Transitional texture API for controls that do not yet accept IconType. */
Texture2D LoadIconTexture(IconType type);
Texture2D LoadIconTextureByName(const char *name);
void LoadAllIconTextures(Texture2D *icons);
void UnloadAllIconTextures(Texture2D *icons);

/* Auto-generated icon names array (alphabetical order, matches IconType enum) */
extern const char *ui_icon_names[];

#endif // UI_ICONS_H
