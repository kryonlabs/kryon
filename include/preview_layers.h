#ifndef PREVIEW_LAYERS_H
#define PREVIEW_LAYERS_H

#include "kryon_compat.generated.h"

#define PREVIEW_LAYER_NAME_MAX 64

typedef enum PreviewLayerKind {
    PREVIEW_LAYER_RECTS,
    PREVIEW_LAYER_LINES,
    PREVIEW_LAYER_POINTS,
    PREVIEW_LAYER_OBJECTS
} PreviewLayerKind;

typedef struct PreviewLine {
    Vector2 a;
    Vector2 b;
} PreviewLine;

typedef struct PreviewObject {
    int type;
    Rectangle bounds;
} PreviewObject;

typedef struct PreviewLayerInfo {
    const char *id;
    const char *label;
    PreviewLayerKind kind;
    Color color;
    int visible;
    int editable;
} PreviewLayerInfo;

#endif
