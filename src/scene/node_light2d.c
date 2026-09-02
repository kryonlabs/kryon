/* Light2D: a backend-portable soft point light. */

#include "scene_tree.h"
#include "node2d_props.h"
#include <stdlib.h>

static unsigned char
kry_light_alpha(unsigned char alpha, float energy)
{
    float value = (float)alpha * energy;
    if(value < 0.0f)
        value = 0.0f;
    if(value > 255.0f)
        value = 255.0f;
    return (unsigned char)value;
}

static void
kry_light2d_draw(Scene *scene, NodeId node)
{
    Node *n = NodeGet(scene, node);
    Light2DProps *props;
    Color inner;
    Color outer;
    float scale;

    if(n == NULL || n->props == NULL)
        return;
    props = (Light2DProps *)n->props;
    if(!props->enabled || props->radius <= 0.0f || props->energy <= 0.0f)
        return;
    scale = (n->world.scale.x + n->world.scale.y) * 0.5f;
    if(scale < 0.0f)
        scale = -scale;
    inner = props->color;
    inner.a = kry_light_alpha(inner.a, props->energy);
    outer = props->color;
    outer.a = 0;
    BeginBlendMode(BLEND_ADDITIVE);
    DrawCircleGradient(n->world.position, props->radius * scale, inner, outer);
    EndBlendMode();
}

static void
kry_light2d_destroy(Scene *scene, Node *node)
{
    (void)scene;
    free(node->props);
    node->props = NULL;
}

static const NodeOps kry_light2d_ops = {
    NULL,
    NULL,
    NULL,
    kry_light2d_draw
};

void
kry_register_light2d(void)
{
    NodeRegisterOps(NODE_LIGHT2D, &kry_light2d_ops);
    NodeRegisterDestroy(NODE_LIGHT2D, kry_light2d_destroy);
}
