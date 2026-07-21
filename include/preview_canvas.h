#ifndef PREVIEW_CANVAS_H
#define PREVIEW_CANVAS_H

#include "kryon_compat.generated.h"

typedef enum PreviewScaleMode {
    PREVIEW_SCALE_FIT,
    PREVIEW_SCALE_100,
    PREVIEW_SCALE_75,
    PREVIEW_SCALE_50
} PreviewScaleMode;

Rectangle PreviewFitRect(Rectangle bounds, int virtual_width,
                         int virtual_height, PreviewScaleMode mode);
Camera2D PreviewCanvasCamera(Rectangle device, int virtual_width,
                             int virtual_height);
Vector2 PreviewScreenToWorld(Rectangle device, int virtual_width,
                             int virtual_height, Vector2 screen);
Vector2 PreviewWorldToScreen(Rectangle device, int virtual_width,
                             int virtual_height, Vector2 world);

#endif
