#ifndef NODE2D_PROPS_H
#define NODE2D_PROPS_H

/*
 * Kind-specific props for built-in scene nodes. These are attached to a
 * KryNode via its `props` pointer and read by the node's lifecycle hooks.
 * Each concrete kind owns its props struct; the scene tree treats props as
 * opaque (void *) and only the kind's registered ops/destroy touch it.
 */

#include "kryon_compat.generated.h"
#include "ui_picture.h" /* PictureProps / UIPictureFit, shared with the UI widget */

typedef struct Camera2DProps {
    float zoom;        /* 1.0 = default; >1 zooms in */
    float rotation;    /* extra camera rotation in radians, added to the node transform */
    int active;        /* nonzero = this camera drives BeginMode2D during KrySceneDraw */
} Camera2DProps;

typedef struct Sprite2DProps {
    const char *asset_path; /* texture path or embedded asset name */
    Rectangle source;       /* sub-rectangle of the texture; {0,0,0,0} = full texture */
    Vector2 size;           /* world-space size of the sprite (width/height) */
    Color tint;             /* .a == 0 -> WHITE */
    UIPictureFit fit;         /* STRETCH/CONTAIN/COVER within `size` */
} Sprite2DProps;

/*
 * Props allocation helpers for scene builders. Allocate zero-initialized
 * kind-specific props to attach to a node via KryNodeGet()->props. Returns
 * NULL on allocation failure. The caller transfers the pointer to the node;
 * the kind's destroy hook frees it.
 */
Camera2DProps *KryCamera2DPropsAlloc(float zoom, int active);
Sprite2DProps *KrySprite2DPropsAlloc(const char *asset_path, float w, float h);

#endif
