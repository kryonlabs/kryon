#ifndef UI_ICONS_H
#define UI_ICONS_H

#include "kryon.h"
#include "ui_icon_types.h"

typedef enum UIIconSheet {
    UI_ICON_SHEET_UI,
    UI_ICON_SHEET_PFP,
    UI_ICON_SHEET_PLATFORMS,
    UI_ICON_SHEET_PAYMENTS,
    UI_ICON_SHEET_LANGUAGE,
    UI_ICON_SHEET_TILES,
    UI_ICON_SHEET_LOGOS,
    UI_ICON_SHEET_COUNT
} UIIconSheet;

typedef struct UIIconAsset {
    UIIconType type;
    const char *name;
    UIIconSheet sheet;
    Rectangle source;
} UIIconAsset;

const UIIconAsset *GetUIIconAsset(UIIconType type);
const UIIconAsset *GetUIIconAssetByName(const char *name);
Texture2D LoadIconSheet(UIIconSheet sheet);
void UnloadIconSheets(void);
void DrawIcon(UIIconType type, Rectangle bounds, Color tint);
void DrawIconByName(const char *name, Rectangle bounds, Color tint);

/* Transitional texture API for controls that do not yet accept UIIconType. */
Texture2D LoadUIIconTexture(UIIconType type);
Texture2D LoadUIIconTextureByName(const char *name);
void LoadAllUIIconTextures(Texture2D *icons);
void UnloadAllUIIconTextures(Texture2D *icons);

/* Auto-generated icon names array (alphabetical order, matches UIIconType enum) */
extern const char *ui_icon_names[];

#endif // UI_ICONS_H
