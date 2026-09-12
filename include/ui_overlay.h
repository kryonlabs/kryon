#ifndef UI_OVERLAY_H
#define UI_OVERLAY_H

#include "kryon_compat.generated.h"

typedef struct {
    Rectangle bounds;
    int view_width;
    int view_height;
    Color scrim;
    int dismiss_disabled;
} DismissibleOverlayProps;

typedef struct {
    int closed;
    int outside_released;
    int release_consumed;
} DismissibleOverlayResult;

DismissibleOverlayResult DismissibleOverlay(DismissibleOverlayProps overlay);

#endif
