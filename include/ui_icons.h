#ifndef UI_ICONS_H
#define UI_ICONS_H

#include "flint.h"
#include "ui_icon_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct UIIconAsset {
    UIIconType type;
    const char *name;
    const unsigned char *data;
    unsigned int size;
} UIIconAsset;

const UIIconAsset *GetUIIconAsset(UIIconType type);
const UIIconAsset *GetUIIconAssetByName(const char *name);
Texture2D LoadUIIconTexture(UIIconType type);
Texture2D LoadUIIconTextureByName(const char *name);
void LoadAllUIIconTextures(Texture2D *icons);
void UnloadAllUIIconTextures(Texture2D *icons);

/* Auto-generated icon names array (alphabetical order, matches UIIconType enum) */
extern const char *ui_icon_names[];

#ifdef __cplusplus
}
#endif

#endif // UI_ICONS_H
