#ifndef KRYON_ICONS_H
#define KRYON_ICONS_H

#include "kryon.h"
#include "ui_icon_types.h"
#include "ui_icon_sheet_props.generated.h"

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
