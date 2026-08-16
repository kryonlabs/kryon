/*
 * Body2D: a physics-driven 2D body. On ready it creates a Box2D body in the
 * scene's world (Body2DProps carries the body type: static/kinematic/dynamic).
 * On physics_process it copies the body's transform back into the node so the
 * rest of the scene tree (and SceneDraw) sees the simulated position.
 */

#include "scene_tree.h"
#include "node2d_props.h"
#include "scene_physics_internal.h"
#include <stdlib.h>

static void
kry_body2d_ready(Scene *scene, NodeId node)
{
    Node *n = NodeGet(scene, node);
    Body2DProps *props;
    if(n == NULL)
        return;
    props = (Body2DProps *)n->props;
    if(props == NULL)
        return;
    kry_body2d_create(scene, node, props);
}

static void
kry_body2d_physics_process(Scene *scene, NodeId node, float dt)
{
    Node *n = NodeGet(scene, node);
    Body2DProps *props;
    (void)dt;
    if(n == NULL)
        return;
    props = (Body2DProps *)n->props;
    if(props == NULL || props->body_id_index == 0)
        return;
    kry_body2d_sync(scene, node, props);
}

static void
kry_body2d_draw(Scene *scene, NodeId node)
{
    Node *n = NodeGet(scene, node);
    Body2DProps *props;
    if(n == NULL)
        return;
    props = (Body2DProps *)n->props;
    /* draw a thin outline so the body is visible even without a sprite child */
    DrawRectangleLines((int)(n->world.position.x - 8),
                       (int)(n->world.position.y - 8), 16, 16,
                       (Color){120, 200, 255, 180});
    (void)props;
}

static void
kry_body2d_destroy(Scene *scene, Node *node)
{
    (void)scene;
    if(node->props != NULL) {
        free(node->props);
        node->props = NULL;
    }
}

static const NodeOps kry_body2d_ops = {
    kry_body2d_ready,
    NULL,
    kry_body2d_physics_process,
    kry_body2d_draw
};

void
kry_register_body2d(void)
{
    NodeRegisterOps(NODE_BODY2D, &kry_body2d_ops);
    NodeRegisterDestroy(NODE_BODY2D, kry_body2d_destroy);
}
