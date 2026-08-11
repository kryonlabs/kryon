/*
 * Camera2D: defines the view into the scene. On ready, if it is marked active
 * it becomes the scene's active camera; KrySceneDraw then wraps the world draw
 * in raylib BeginMode2D using this node's world transform and zoom.
 *
 * The node owns a Camera2DProps (zoom/rotation/active) allocated in the node's
 * props slot by whoever creates it (the kc-generated builder or app code).
 */

#include "scene_tree.h"
#include "node2d_props.h"
#include <stdlib.h>

static void
kry_camera2d_ready(KryScene *scene, KryNodeId node)
{
    KryNode *n = KryNodeGet(scene, node);
    Camera2DProps *props;
    if(n == NULL)
        return;
    props = (Camera2DProps *)n->props;
    if(props != NULL && props->active && scene->active_camera < 0)
        scene->active_camera = node;
}

static void
kry_camera2d_destroy(KryScene *scene, KryNode *node)
{
    (void)scene;
    if(node->props != NULL) {
        free(node->props);
        node->props = NULL;
    }
}

static const KryNodeOps kry_camera2d_ops = {
    kry_camera2d_ready,
    NULL, /* process */
    NULL, /* physics_process */
    NULL  /* draw: handled centrally by KrySceneDraw */
};

void
kry_register_camera2d(void)
{
    KryNodeRegisterOps(KRY_NODE_CAMERA2D, &kry_camera2d_ops);
    KryNodeRegisterDestroy(KRY_NODE_CAMERA2D, kry_camera2d_destroy);
}
