#ifndef KRYON_ICONS_H
#define KRYON_ICONS_H

#include "kryon.h"
#include "ui_icon_types.h"

typedef enum IconSheet {
    ICON_SHEET_CORE,
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

/* Auto-generated icon names array (alphabetical order, matches IconType enum) */
extern const char *ui_icon_names[];

#endif // KRYON_ICONS_H
