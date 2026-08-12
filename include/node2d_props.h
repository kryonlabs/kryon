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

typedef enum KryBody2DType {
    KRY_BODY2D_STATIC,
    KRY_BODY2D_KINEMATIC,
    KRY_BODY2D_DYNAMIC
} KryBody2DType;

typedef struct Body2DProps {
    KryBody2DType body_type;
    int fixed_rotation; /* nonzero prevents the body from rotating */
    float gravity_scale; /* 1.0 = normal; 0.0 = weightless */
    /* b2BodyId is {int32 index1, int16 world0, int16 generation}. Stored as
     * raw ints so this header does not need box2d.h. 0 = uncreated body. */
    int body_id_index;
    short body_id_world;
    short body_id_gen;
} Body2DProps;

typedef enum KryShape2DKind {
    KRY_SHAPE2D_BOX,
    KRY_SHAPE2D_CIRCLE
} KryShape2DKind;

typedef struct CollisionShape2DProps {
    KryShape2DKind shape_kind;
    Vector2 size;    /* full width/height (box) or diameter (circle) */
    int is_sensor;   /* nonzero = trigger volume (Area2D), no solid collision */
} CollisionShape2DProps;

typedef struct Area2DProps {
    int monitoring; /* nonzero collects body_enter/body_exit signals */
    /* last body that entered/left, for signal dispatch */
    int last_enter_body;
    int last_exit_body;
} Area2DProps;

Body2DProps *KryBody2DPropsAlloc(KryBody2DType type);
CollisionShape2DProps *KryCollisionShape2DPropsAlloc(KryShape2DKind kind, float w, float h);
Area2DProps *KryArea2DPropsAlloc(void);

#endif
