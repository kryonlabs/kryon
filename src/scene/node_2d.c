/*
 * Node2D: the base 2D transform node. It has no draw or update behavior of its
 * own -- it exists to group children under a transform. Concrete visual nodes
 * (Sprite2D, Camera2D, ...) are separate kinds that compose a Node2D-style
 * transform. This file keeps the kind registered with an empty vtable so
 * NodeOpsFor(NODE_NODE2D) returns a valid (no-op) ops table.
 */

#include "scene_tree.h"
#include <stddef.h>

static const NodeOps kry_node2d_ops = {
    NULL, /* ready */
    NULL, /* process */
    NULL, /* physics_process */
    NULL  /* draw */
};

void
kry_register_node2d(void)
{
    NodeRegisterOps(NODE_NODE2D, &kry_node2d_ops);
}
