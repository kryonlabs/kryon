#ifndef FLINT_ICONS_H
#define FLINT_ICONS_H

#include "raylib.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    FLINT_ICON_TYPE_NONE,
    FLINT_ICON_TYPE_GEAR,
    FLINT_ICON_TYPE_X,
    FLINT_ICON_TYPE_MANUAL,
    FLINT_ICON_TYPE_RETURN,
    FLINT_ICON_TYPE_BACKWARD,
    FLINT_ICON_TYPE_FORWARD,
    FLINT_ICON_TYPE_PLAY,
    FLINT_ICON_TYPE_PAUSE,
    FLINT_ICON_TYPE_STAT,
    FLINT_ICON_TYPE_TELEGRAM,
    FLINT_ICON_TYPE_GLOBE,
    FLINT_ICON_TYPE_STRIPE,
    FLINT_ICON_TYPE_MONERO,
    FLINT_ICON_TYPE_HOME,
    FLINT_ICON_TYPE_TRASH,
    FLINT_ICON_TYPE_PENCIL,
    FLINT_ICON_TYPE_SAVE
} FlintIconType;

// Draw icon as fallback when no texture is available
void flint_draw_icon_fallback(FlintIconType type, int x, int y, int size, Color color);

#ifdef __cplusplus
}
#endif

#endif // FLINT_ICONS_H
