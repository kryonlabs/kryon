/*
 * Registration of the built-in scene node kinds. Called once at startup so the
 * per-kind ops/destroy tables are populated before any KryScene ticks.
 */

#include "scene_tree.h"
#include "scene_property.h"
#include "node2d_props.h"
#include <stdlib.h>

/* per-kind register routines (one per node_*.c) */
void kry_register_node2d(void);
void kry_register_camera2d(void);
void kry_register_sprite2d(void);
void kry_register_body2d(void);
void kry_register_collision_shape2d(void);
void kry_register_area2d(void);

/* installed by physics_world.c; declared in scene_tree.c */
extern void (*kry_scene_physics_step_fn)(KryScene *scene, float dt);
void kry_physics_step_install(void);

void
KrySceneRegisterBuiltins(void)
{
    kry_register_node2d();
    kry_register_camera2d();
    kry_register_sprite2d();
    kry_register_body2d();
    kry_register_collision_shape2d();
    kry_register_area2d();
    KrySceneRegisterBuiltinProperties();
    kry_physics_step_install();
}

/*
 * Props allocation helpers for scene builders. The kc-generated scene builder
 * and app code call these to attach kind-specific props to a freshly created
 * node. They allocate zero-initialized memory so optional fields (tint, fit)
 * get sane defaults handled by the draw ops.
 */
Camera2DProps *
KryCamera2DPropsAlloc(float zoom, int active)
{
    Camera2DProps *p = calloc(1, sizeof(*p));
    if(p != NULL) {
        p->zoom = zoom;
        p->active = active;
    }
    return p;
}

Sprite2DProps *
KrySprite2DPropsAlloc(const char *asset_path, float w, float h)
{
    Sprite2DProps *p = calloc(1, sizeof(*p));
    if(p != NULL) {
        p->asset_path = asset_path;
        p->size = (Vector2){w, h};
    }
    return p;
}
