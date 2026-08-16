/*
 * CollisionShape2D: attaches a Box2D shape (box or circle) to the nearest
 * ancestor Body2D. On ready it walks up the tree to find a Body2D parent and
 * calls kry_collision_shape2d_attach. Drawn as a debug outline.
 */

#include "scene_tree.h"
#include "node2d_props.h"
#include "scene_physics_internal.h"
#include <stdlib.h>

static NodeId
kry_find_body_ancestor(Scene *scene, NodeId node)
{
    Node *n;
    int parent;
    n = NodeGet(scene, node);
    if(n == NULL)
        return -1;
    parent = n->parent;
    while(parent >= 0) {
        n = NodeGet(scene, parent);
        if(n == NULL)
            return -1;
        if(n->kind == NODE_BODY2D && n->props != NULL)
            return parent;
        parent = n->parent;
    }
    return -1;
}

static void
kry_collision_shape2d_ready(Scene *scene, NodeId node)
{
    Node *n = NodeGet(scene, node);
    CollisionShape2DProps *props;
    NodeId body_id;
    Node *body;

    if(n == NULL)
        return;
    props = (CollisionShape2DProps *)n->props;
    if(props == NULL)
        return;
    body_id = kry_find_body_ancestor(scene, node);
    if(body_id < 0)
        return;
    body = NodeGet(scene, body_id);
    kry_collision_shape2d_attach(scene, node, props,
                                 (Body2DProps *)body->props);
}

static void
kry_collision_shape2d_draw(Scene *scene, NodeId node)
{
    Node *n = NodeGet(scene, node);
    CollisionShape2DProps *props;
    if(n == NULL)
        return;
    props = (CollisionShape2DProps *)n->props;
    if(props == NULL)
        return;
    if(props->shape_kind == KRY_SHAPE2D_CIRCLE) {
        DrawCircleLines((int)n->world.position.x, (int)n->world.position.y,
                        (int)(props->size.x * 0.5f),
                        (Color){140, 255, 140, 180});
    } else {
        DrawRectangleLines((int)(n->world.position.x - props->size.x * 0.5f),
                           (int)(n->world.position.y - props->size.y * 0.5f),
                           (int)props->size.x, (int)props->size.y,
                           (Color){140, 255, 140, 180});
    }
}

static void
kry_collision_shape2d_destroy(Scene *scene, Node *node)
{
    (void)scene;
    if(node->props != NULL) {
        free(node->props);
        node->props = NULL;
    }
}

static const NodeOps kry_collision_shape2d_ops = {
    kry_collision_shape2d_ready,
    NULL,
    NULL,
    kry_collision_shape2d_draw
};

void
kry_register_collision_shape2d(void)
{
    NodeRegisterOps(NODE_COLLISION_SHAPE2D, &kry_collision_shape2d_ops);
    NodeRegisterDestroy(NODE_COLLISION_SHAPE2D,
                           kry_collision_shape2d_destroy);
}
