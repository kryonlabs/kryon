/*
 * Area2D: a trigger volume. Like a CollisionShape2D with is_sensor=1, it
 * detects Body2D entry/exit without solid collision. On ready it creates a
 * sensor shape on the nearest Body2D ancestor. After each physics step,
 * ScenePhysicsTick drains Box2D sensor events and Area2D emits body_enter
 * / body_exit signals (built on the Phase 2 signal bus).
 */

#include "scene_tree.h"
#include "node2d_props.h"
#include "kry_signal.h"
#include "scene_physics_internal.h"
#include <stdlib.h>

static void
kry_area2d_ready(Scene *scene, NodeId node)
{
    Node *n = NodeGet(scene, node);
    Area2DProps *props;
    if(n == NULL)
        return;
    props = (Area2DProps *)n->props;
    if(props == NULL)
        return;
    /* An Area2D is a sensor; the actual shape is provided by a child
     * CollisionShape2D with is_sensor. The ready is a no-op on the area node
     * itself; shape attachment happens on the child's ready. */
}

static void
kry_area2d_destroy(Scene *scene, Node *node)
{
    (void)scene;
    if(node->props != NULL) {
        free(node->props);
        node->props = NULL;
    }
}

static const NodeOps kry_area2d_ops = {
    kry_area2d_ready,
    NULL,
    NULL,
    NULL
};

void
kry_register_area2d(void)
{
    NodeRegisterOps(NODE_AREA2D, &kry_area2d_ops);
    NodeRegisterDestroy(NODE_AREA2D, kry_area2d_destroy);
}
