/*
 * Box2D v3 physics world bridge. Owns the only #include of <box2d/box2d.h> in
 * the scene tree; the public scene_tree.h stores the b2WorldId as two opaque
 * unsigned shorts so consumers dont pull in Box2D.
 *
 * ScenePhysicsCreate builds a world; ScenePhysicsTick steps it; Body2D
 * and CollisionShape2D nodes (in node_body2d.c / node_collision_shape2d.c)
 * create Box2D bodies/shapes via the helpers here and sync their Node
 * transforms from the world after each step.
 */

#include "scene_tree.h"
#include "node2d_props.h"
#include "kry_signal.h"
#include <box2d/box2d.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* zero constants: the native Plan 9 compiler rejects short
 * compound literals like (Type){0}, and a copy of a zero
 * object is equivalent on every platform. */
static const b2BodyId kryon_zero_b2bodyid;


/* Convert between the opaque Scene fields and a real b2WorldId. */
static b2WorldId
scene_world_id(Scene *scene)
{
    b2WorldId id;
    id.index1 = (uint16_t)scene->physics_world_index;
    id.generation = (uint16_t)scene->physics_world_gen;
    return id;
}

static void
set_scene_world_id(Scene *scene, b2WorldId id)
{
    scene->physics_world_index = id.index1;
    scene->physics_world_gen = id.generation;
}

int
ScenePhysicsCreate(Scene *scene, float gravity_x, float gravity_y)
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
ScenePhysicsDestroy(Scene *scene)
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
kry_scene_b2_world(Scene *scene)
{
    return scene_world_id(scene);
}

b2BodyId
kry_body2d_create(Scene *scene, NodeId node, Body2DProps *props)
{
    Node *n;
    b2BodyDef bd;
    b2BodyId bid;

    n = NodeGet(scene, node);
    if(n == NULL || props == NULL || !scene->physics_enabled)
        return kryon_zero_b2bodyid;
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
    bd.userData = (void *)(intptr_t)node;
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
kry_body2d_sync(Scene *scene, NodeId node, Body2DProps *props)
{
    Node *n;
    b2BodyId bid;
    b2Transform xf;

    n = NodeGet(scene, node);
    if(n == NULL || props == NULL || props->body_id_index == 0)
        return;
    bid = body2d_id(props);
    if(!b2Body_IsValid(bid))
        return;
    xf = b2Body_GetTransform(bid);
    n->local.position = (Vector2){xf.p.x, xf.p.y};
    n->local.rotation = b2Rot_GetAngle(xf.q);
    n->flags |= NODE_FLAG_DIRTY;
}

void
kry_collision_shape2d_attach(Scene *scene, NodeId node,
                             CollisionShape2DProps *props, Body2DProps *parent_body)
{
    Node *n;
    b2BodyId bid;
    b2ShapeDef sd;

    n = NodeGet(scene, node);
    if(n == NULL || props == NULL || parent_body == NULL ||
       parent_body->body_id_index == 0)
        return;
    bid = body2d_id(parent_body);
    if(!b2Body_IsValid(bid))
        return;
    sd = b2DefaultShapeDef();
    sd.isSensor = props->is_sensor != 0;
    /* Box2D disables sensor events on every shape by default (even
     * sensors); without this flag no overlap events are ever reported. */
    sd.enableSensorEvents = true;
    sd.userData = (void *)(intptr_t)node;
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

/* Direct body control for kinematic/dynamic steering (players, platforms).
 * The transform setter also marks the node dirty so the next tick
 * recomputes its world transform. */
void
KryBody2DSetTransform(Scene *scene, NodeId node, float x, float y)
{
    Node *n = NodeGet(scene, node);
    Body2DProps *props;
    b2BodyId bid;

    if(n == NULL || n->kind != NODE_BODY2D)
        return;
    props = (Body2DProps *)n->props;
    if(props == NULL || props->body_id_index == 0)
        return;
    bid = body2d_id(props);
    if(!b2Body_IsValid(bid))
        return;
    b2Body_SetTransform(bid, (b2Vec2){x, y}, b2Body_GetRotation(bid));
    n->flags |= NODE_FLAG_DIRTY;
}

void
KryBody2DSetVelocity(Scene *scene, NodeId node, float vx, float vy)
{
    Node *n = NodeGet(scene, node);
    Body2DProps *props;
    b2BodyId bid;

    if(n == NULL || n->kind != NODE_BODY2D)
        return;
    props = (Body2DProps *)n->props;
    if(props == NULL || props->body_id_index == 0)
        return;
    bid = body2d_id(props);
    if(!b2Body_IsValid(bid))
        return;
    b2Body_SetLinearVelocity(bid, (b2Vec2){vx, vy});
}

void
KryBody2DGetVelocity(Scene *scene, NodeId node, float *vx, float *vy)
{
    Node *n = NodeGet(scene, node);
    Body2DProps *props;
    b2BodyId bid;
    b2Vec2 v;

    if(vx != NULL)
        *vx = 0.0f;
    if(vy != NULL)
        *vy = 0.0f;
    if(n == NULL || n->kind != NODE_BODY2D)
        return;
    props = (Body2DProps *)n->props;
    if(props == NULL || props->body_id_index == 0)
        return;
    bid = body2d_id(props);
    if(!b2Body_IsValid(bid))
        return;
    v = b2Body_GetLinearVelocity(bid);
    if(vx != NULL)
        *vx = v.x;
    if(vy != NULL)
        *vy = v.y;
}

/* --- world stepping --- */

extern void (*kry_scene_physics_step_fn)(Scene *scene, float dt);

/* The sensor shape's CollisionShape2D node walks up to its Area2D; the
 * visitor shape's node walks up to its Body2D. Returns -1 when the chain
 * does not lead to the wanted kind. */
static NodeId
kry_sensor_owner(Scene *scene, b2ShapeId shape, NodeKind wanted)
{
    Node *n;

    if(!b2Shape_IsValid(shape))
        return -1;
    n = NodeGet(scene, (NodeId)(intptr_t)b2Shape_GetUserData(shape));
    while(n != NULL && (n->flags & NODE_FLAG_ALIVE) != 0) {
        if(n->kind == wanted)
            return n->id;
        if(n->parent < 0)
            break;
        n = &scene->nodes[n->parent];
    }
    return -1;
}

static void
kry_sensor_signal(Scene *scene, b2ShapeId sensor_shape, b2ShapeId visitor_shape,
                  int begin)
{
    NodeId area = kry_sensor_owner(scene, sensor_shape, NODE_AREA2D);
    NodeId body = kry_sensor_owner(scene, visitor_shape, NODE_BODY2D);
    Node *area_node;
    Area2DProps *props;

    if(area < 0 || body < 0)
        return;
    area_node = NodeGet(scene, area);
    if(area_node == NULL)
        return;
    props = (Area2DProps *)area_node->props;
    if(props == NULL || !props->monitoring)
        return;
    if(begin) {
        props->last_enter_body = body;
        SignalEmit(scene, area, "body_enter", PropertyInt(body));
    } else {
        props->last_exit_body = body;
        SignalEmit(scene, area, "body_exit", PropertyInt(body));
    }
}

/* Drain Box2D sensor overlap events accumulated by the last step and
 * surface them as Area2D body_enter/body_exit signals. */
static void
kry_scene_drain_sensor_events(Scene *scene)
{
    b2SensorEvents events;
    int i;

    events = b2World_GetSensorEvents(scene_world_id(scene));
    for(i = 0; i < events.beginCount; i++) {
        kry_sensor_signal(scene, events.beginEvents[i].sensorShapeId,
                          events.beginEvents[i].visitorShapeId, 1);
    }
    for(i = 0; i < events.endCount; i++) {
        /* End events may reference shapes destroyed this step; the signal
         * only fires when both nodes are still resolvable. */
        kry_sensor_signal(scene, events.endEvents[i].sensorShapeId,
                          events.endEvents[i].visitorShapeId, 0);
    }
}

static void
kry_scene_physics_step(Scene *scene, float dt)
{
    if(scene == NULL || !scene->physics_enabled)
        return;
    /* fixed-substep integration; 4 sub-steps is Box2Ds recommended default */
    b2World_Step(scene_world_id(scene), dt, 4);
    kry_scene_drain_sensor_events(scene);
}

void
kry_physics_step_install(void)
{
    kry_scene_physics_step_fn = kry_scene_physics_step;
}
