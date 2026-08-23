#ifndef SCENE_PHYSICS_INTERNAL_H
#define SCENE_PHYSICS_INTERNAL_H

/*
 * Private bridge between the scene tree and Box2D. Declares the helpers that
 * node_body2d.c / node_collision_shape2d.c / node_area2d.c call to create and
 * sync physics bodies. Kept out of the public headers because the signatures
 * use Box2D types (b2WorldId / b2BodyId) which would force every consumer to
 * include box2d.h.
 *
 * The whole bridge only exists when KRYON_WITH_PHYSICS is set; builds
 * without it (native Plan 9) neither ship nor include Box2D.
 */

#if KRYON_WITH_PHYSICS

#include "scene_tree.h"
#include "node2d_props.h"
#include <box2d/box2d.h>

b2WorldId kry_scene_b2_world(Scene *scene);
b2BodyId kry_body2d_create(Scene *scene, NodeId node, Body2DProps *props);
void kry_body2d_sync(Scene *scene, NodeId node, Body2DProps *props);
void kry_collision_shape2d_attach(Scene *scene, NodeId node,
                                  CollisionShape2DProps *props,
                                  Body2DProps *parent_body);

#endif /* KRYON_WITH_PHYSICS */

#endif
