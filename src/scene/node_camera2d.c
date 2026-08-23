/*
 * Camera2D: defines the view into the scene. On ready, if it is marked active
 * it becomes the scenes active camera; SceneDraw then wraps the world draw
 * in raylib BeginMode2D using this nodes world transform and zoom.
 *
 * The node owns a Camera2DProps (zoom/rotation/active) allocated in the nodes
 * props slot by whoever creates it (the k2c-generated builder or app code).
 */

#include "scene_tree.h"
#include "node2d_props.h"
#include <stdlib.h>

static void
kry_camera2d_ready(Scene *scene, NodeId node)
{
    Node *n = NodeGet(scene, node);
    Camera2DProps *props;
    if(n == NULL)
        return;
    props = (Camera2DProps *)n->props;
    if(props != NULL && props->active && scene->active_camera < 0)
        scene->active_camera = node;
}

static void
kry_camera2d_destroy(Scene *scene, Node *node)
{
    (void)scene;
    if(node->props != NULL) {
        free(node->props);
        node->props = NULL;
    }
}

static const NodeOps kry_camera2d_ops = {
    kry_camera2d_ready,
    NULL, /* process */
    NULL, /* physics_process */
    NULL  /* draw: handled centrally by SceneDraw */
};

void
kry_register_camera2d(void)
{
    NodeRegisterOps(NODE_CAMERA2D, &kry_camera2d_ops);
    NodeRegisterDestroy(NODE_CAMERA2D, kry_camera2d_destroy);
}
