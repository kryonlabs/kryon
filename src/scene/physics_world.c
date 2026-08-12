/*
 * Box2D v3 physics world bridge. Owns the only #include of <box2d/box2d.h> in
 * the scene tree; the public scene_tree.h stores the b2WorldId as two opaque
 * unsigned shorts so consumers don't pull in Box2D.
 *
 * KryScenePhysicsCreate builds a world; KryScenePhysicsTick steps it; Body2D
 * and CollisionShape2D nodes (in node_body2d.c / node_collision_shape2d.c)
 * create Box2D bodies/shapes via the helpers here and sync their KryNode
 * transforms from the world after each step.
 */

#include "scene_tree.h"
#include "node2d_props.h"
#include <box2d/box2d.h>
#include <stdlib.h>
#include <string.h>

/* Convert between the opaque KryScene fields and a real b2WorldId. */
static b2WorldId
scene_world_id(KryScene *scene)
{
    b2WorldId id;
    id.index1 = (uint16_t)scene->physics_world_index;
    id.generation = (uint16_t)scene->physics_world_gen;
    return id;
}

static void
set_scene_world_id(KryScene *scene, b2WorldId id)
{
    scene->physics_world_index = id.index1;
    scene->physics_world_gen = id.generation;
}

int
KryScenePhysicsCreate(KryScene *scene, float gravity_x, float gravity_y)
{
    b2WorldDef def;
    b2WorldId id;

    if(scene == NULL)
        return 0;
    def = b2DefaultWorldDef();
    def.gravity.x = gravity_x;
    def.gravity.y = gravity_y;
    id = b2CreateWorld(&def);
    set_scene_world_id(scene, id);
    scene->physics_enabled = 1;
    return 1;
}

void
KryScenePhysicsDestroy(KryScene *scene)
{
    if(scene == NULL || !scene->physics_enabled)
        return;
    b2DestroyWorld(scene_world_id(scene));
    scene->physics_world_index = 0;
    scene->physics_world_gen = 0;
    scene->physics_enabled = 0;
}

/* --- helpers used by Body2D / CollisionShape2D nodes --- */

b2WorldId
kry_scene_b2_world(KryScene *scene)
{
    return scene_world_id(scene);
}

b2BodyId
kry_body2d_create(KryScene *scene, KryNodeId node, Body2DProps *props)
{
    KryNode *n;
    b2BodyDef bd;
    b2BodyId bid;

    n = KryNodeGet(scene, node);
    if(n == NULL || props == NULL || !scene->physics_enabled)
        return (b2BodyId){0};
    bd = b2DefaultBodyDef();
    switch(props->body_type) {
    case KRY_BODY2D_STATIC: bd.type = b2_staticBody; break;
    case KRY_BODY2D_KINEMATIC: bd.type = b2_kinematicBody; break;
    case KRY_BODY2D_DYNAMIC: bd.type = b2_dynamicBody; break;
    }
    bd.position.x = n->world.position.x;
    bd.position.y = n->world.position.y;
    bd.rotation = b2MakeRot(n->world.rotation);
    bd.fixedRotation = props->fixed_rotation != 0;
    bd.gravityScale = props->gravity_scale != 0.0f ? props->gravity_scale : 1.0f;
    bid = b2CreateBody(scene_world_id(scene), &bd);
    props->body_id_index = bid.index1;
    props->body_id_world = bid.world0;
    props->body_id_gen = bid.generation;
    return bid;
}

static b2BodyId
body2d_id(Body2DProps *props)
{
    b2BodyId id;
    id.index1 = props->body_id_index;
    id.world0 = props->body_id_world;
    id.generation = props->body_id_gen;
    return id;
}

void
kry_body2d_sync(KryScene *scene, KryNodeId node, Body2DProps *props)
{
    KryNode *n;
    b2BodyId bid;
    b2Transform xf;

    n = KryNodeGet(scene, node);
    if(n == NULL || props == NULL || props->body_id_index == 0)
        return;
    bid = body2d_id(props);
    if(!b2Body_IsValid(bid))
        return;
    xf = b2Body_GetTransform(bid);
    n->local.position = (Vector2){xf.p.x, xf.p.y};
    n->local.rotation = b2Rot_GetAngle(xf.q);
    n->flags |= KRY_NODE_FLAG_DIRTY;
}

void
kry_collision_shape2d_attach(KryScene *scene, KryNodeId node,
                             CollisionShape2DProps *props, Body2DProps *parent_body)
{
    KryNode *n;
    b2BodyId bid;
    b2ShapeDef sd;

    n = KryNodeGet(scene, node);
    if(n == NULL || props == NULL || parent_body == NULL ||
       parent_body->body_id_index == 0)
        return;
    bid = body2d_id(parent_body);
    if(!b2Body_IsValid(bid))
        return;
    sd = b2DefaultShapeDef();
    sd.isSensor = props->is_sensor != 0;
    if(props->shape_kind == KRY_SHAPE2D_CIRCLE) {
        b2Circle circle;
        circle.center.x = 0.0f;
        circle.center.y = 0.0f;
        circle.radius = props->size.x * 0.5f;
        b2CreateCircleShape(bid, &sd, &circle);
    } else {
        b2Polygon box;
        box = b2MakeBox(props->size.x * 0.5f, props->size.y * 0.5f);
        b2CreatePolygonShape(bid, &sd, &box);
    }
}

Body2DProps *
KryBody2DPropsAlloc(KryBody2DType type)
{
    Body2DProps *p = calloc(1, sizeof(*p));
    if(p != NULL) {
        p->body_type = type;
        p->gravity_scale = 1.0f;
    }
    return p;
}

CollisionShape2DProps *
KryCollisionShape2DPropsAlloc(KryShape2DKind kind, float w, float h)
{
    CollisionShape2DProps *p = calloc(1, sizeof(*p));
    if(p != NULL) {
        p->shape_kind = kind;
        p->size = (Vector2){w, h};
    }
    return p;
}

Area2DProps *
KryArea2DPropsAlloc(void)
{
    Area2DProps *p = calloc(1, sizeof(*p));
    return p;
}

/* --- world stepping --- */

extern void (*kry_scene_physics_step_fn)(KryScene *scene, float dt);

static void
kry_scene_physics_step(KryScene *scene, float dt)
{
    if(scene == NULL || !scene->physics_enabled)
        return;
    /* fixed-substep integration; 4 sub-steps is Box2D's recommended default */
    b2World_Step(scene_world_id(scene), dt, 4);
}

void
kry_physics_step_install(void)
{
    kry_scene_physics_step_fn = kry_scene_physics_step;
}
