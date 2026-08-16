/*
 * Registration of the built-in scene node kinds. Called once at startup so the
 * per-kind ops/destroy tables are populated before any Scene ticks.
 */

#include "scene_tree.h"
#include "scene_property.h"
#include "node2d_props.h"
#include <stdlib.h>

/* The Box2D-backed physics nodes (Body2D, CollisionShape2D, Area2D, plus the
 * physics world stepper) are optional. Build with -DKRYON_WITH_PHYSICS=0 and
 * omit the physics sources (see KRYON_PHYSICS_SRCS in mk/vendor.mk) for UI-only
 * apps that don't need 2D physics and don't want the box2d dependency. */
#ifndef KRYON_WITH_PHYSICS
#define KRYON_WITH_PHYSICS 1
#endif

/* per-kind register routines (one per node_*.c) */
void kry_register_node2d(void);
void kry_register_camera2d(void);
void kry_register_sprite2d(void);
#if KRYON_WITH_PHYSICS
void kry_register_body2d(void);
void kry_register_collision_shape2d(void);
void kry_register_area2d(void);
#endif
void kry_register_animation_player(void);
void kry_register_animated_sprite2d(void);
void kry_register_tilemap(void);
void kry_register_audio_source(void);

/* installed by physics_world.c; declared in scene_tree.c. The pointer itself
 * lives in scene_tree.c (always linked) and stays NULL when physics is off. */
extern void (*kry_scene_physics_step_fn)(Scene *scene, float dt);
#if KRYON_WITH_PHYSICS
void kry_physics_step_install(void);
#endif

void
SceneRegisterBuiltins(void)
{
    kry_register_node2d();
    kry_register_camera2d();
    kry_register_sprite2d();
#if KRYON_WITH_PHYSICS
    kry_register_body2d();
    kry_register_collision_shape2d();
    kry_register_area2d();
#endif
    kry_register_animation_player();
    kry_register_animated_sprite2d();
    kry_register_tilemap();
    kry_register_audio_source();
    SceneRegisterBuiltinProperties();
#if KRYON_WITH_PHYSICS
    kry_physics_step_install();
#endif
}

/*
 * Props allocation helpers for scene builders. The k2c-generated scene builder
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
