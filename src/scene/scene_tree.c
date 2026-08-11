/*
 * Kryon retained scene tree implementation.
 *
 * Mirrors the arena pattern from ui_tree.c: a fixed array of nodes, integer
 * index links, rebuilt links on add/remove. World transforms are recomputed
 * top-down each tick before _process fires, so reads during process see the
 * parent's updated world transform.
 */

#include "scene_tree.h"
#include "kry_signal.h"
#include "node2d_props.h"
#include <string.h>

static KryNodeOps g_kry_node_ops[KRY_NODE_CUSTOM + 1];
static KryNodeDestroyFn g_kry_node_destroy[KRY_NODE_CUSTOM + 1];

static int
kry_node_is_valid(KryScene *scene, KryNodeId node)
{
    return scene != NULL && node >= 0 && node < scene->count &&
           (scene->nodes[node].flags & KRY_NODE_FLAG_ALIVE);
}

void
KrySceneInit(KryScene *scene)
{
    KryNode *root;
    if(scene == NULL)
        return;
    memset(scene, 0, sizeof(*scene));
    scene->root = -1;
    scene->active_camera = -1;
    scene->time_scale = 1.0f;
    root = &scene->nodes[0];
    memset(root, 0, sizeof(*root));
    root->id = 0;
    root->parent = -1;
    root->first_child = -1;
    root->next_sibling = -1;
    root->kind = KRY_NODE_ROOT;
    root->flags = KRY_NODE_FLAG_ALIVE;
    strncpy(root->name, "root", sizeof(root->name) - 1);
    root->local = KryTransform2DIdentity();
    root->world = KryTransform2DIdentity();
    scene->count = 1;
    scene->root = 0;
}

void
KrySceneDestroy(KryScene *scene)
{
    int i;
    KryNodeDestroyFn destroy;
    if(scene == NULL)
        return;
    for(i = 0; i < scene->count; i++) {
        if(!(scene->nodes[i].flags & KRY_NODE_FLAG_ALIVE))
            continue;
        destroy = g_kry_node_destroy[scene->nodes[i].kind];
        if(destroy != NULL)
            destroy(scene, &scene->nodes[i]);
    }
    memset(scene, 0, sizeof(*scene));
    scene->root = -1;
    scene->active_camera = -1;
}

static void
kry_node_detach(KryScene *scene, KryNode *node)
{
    KryNode *parent;
    KryNodeId *slot;
    if(node->parent < 0)
        return;
    parent = &scene->nodes[node->parent];
    if(parent->child_count > 0)
        parent->child_count--;
    slot = &parent->first_child;
    while(*slot >= 0 && *slot != node->id)
        slot = &scene->nodes[*slot].next_sibling;
    if(*slot == node->id)
        *slot = node->next_sibling;
    node->parent = -1;
    node->next_sibling = -1;
}

KryNodeId
KryNodeCreate(KryScene *scene, KryNodeId parent, KryNodeKind kind,
              const char *name)
{
    int index;
    KryNode *node;
    KryNode *parent_node;
    KryNodeId *slot;

    if(scene == NULL || scene->count >= KRY_SCENE_MAX_NODES)
        return -1;
    if(parent < 0 || parent >= scene->count ||
       !(scene->nodes[parent].flags & KRY_NODE_FLAG_ALIVE))
        return -1;
    index = scene->count++;
    node = &scene->nodes[index];
    memset(node, 0, sizeof(*node));
    node->id = index;
    node->parent = -1;
    node->first_child = -1;
    node->next_sibling = -1;
    node->kind = kind;
    node->flags = KRY_NODE_FLAG_ALIVE | KRY_NODE_FLAG_DIRTY;
    if(name != NULL)
        strncpy(node->name, name, sizeof(node->name) - 1);
    node->local = KryTransform2DIdentity();
    node->world = KryTransform2DIdentity();

    parent_node = &scene->nodes[parent];
    parent_node->child_count++;
    slot = &parent_node->first_child;
    while(*slot >= 0)
        slot = &scene->nodes[*slot].next_sibling;
    *slot = index;
    node->parent = parent;
    return index;
}

static void
kry_node_free_recursive(KryScene *scene, KryNodeId node)
{
    KryNode *n;
    KryNodeId child;
    KryNodeId next;
    KryNodeDestroyFn destroy;

    if(node < 0 || node >= scene->count)
        return;
    n = &scene->nodes[node];
    if(!(n->flags & KRY_NODE_FLAG_ALIVE))
        return;
    child = n->first_child;
    while(child >= 0) {
        next = scene->nodes[child].next_sibling;
        kry_node_free_recursive(scene, child);
        child = next;
    }
    destroy = g_kry_node_destroy[n->kind];
    if(destroy != NULL)
        destroy(scene, n);
    n->flags &= ~KRY_NODE_FLAG_ALIVE;
    n->first_child = -1;
    n->next_sibling = -1;
    n->parent = -1;
}

void
KryNodeRemove(KryScene *scene, KryNodeId node)
{
    KryNode *n;
    if(scene == NULL || !kry_node_is_valid(scene, node))
        return;
    KrySignalDisconnectNode(scene, node);
    n = &scene->nodes[node];
    kry_node_detach(scene, n);
    kry_node_free_recursive(scene, node);
}

KryNodeId
KryNodeFindChild(KryScene *scene, KryNodeId parent, const char *name)
{
    KryNode *p;
    KryNodeId child;
    if(scene == NULL || name == NULL || !kry_node_is_valid(scene, parent))
        return -1;
    p = &scene->nodes[parent];
    child = p->first_child;
    while(child >= 0) {
        if(strcmp(scene->nodes[child].name, name) == 0)
            return child;
        child = scene->nodes[child].next_sibling;
    }
    return -1;
}

KryNode *
KryNodeGet(KryScene *scene, KryNodeId node)
{
    if(scene == NULL || node < 0 || node >= scene->count)
        return NULL;
    return &scene->nodes[node];
}

void
KryNodeSetPosition(KryScene *scene, KryNodeId node, float x, float y)
{
    KryNode *n = KryNodeGet(scene, node);
    if(n != NULL) {
        n->local.position = (Vector2){x, y};
        n->flags |= KRY_NODE_FLAG_DIRTY;
    }
}

void
KryNodeSetRotation(KryScene *scene, KryNodeId node, float radians)
{
    KryNode *n = KryNodeGet(scene, node);
    if(n != NULL) {
        n->local.rotation = radians;
        n->flags |= KRY_NODE_FLAG_DIRTY;
    }
}

void
KryNodeSetScale(KryScene *scene, KryNodeId node, float sx, float sy)
{
    KryNode *n = KryNodeGet(scene, node);
    if(n != NULL) {
        n->local.scale = (Vector2){sx, sy};
        n->flags |= KRY_NODE_FLAG_DIRTY;
    }
}

void
KryNodeSetProps(KryScene *scene, KryNodeId node, void *props)
{
    KryNode *n = KryNodeGet(scene, node);
    if(n != NULL)
        n->props = props;
}

void
KryNodeRegisterOps(KryNodeKind kind, const KryNodeOps *ops)
{
    if(kind < 0 || kind > KRY_NODE_CUSTOM)
        return;
    if(ops != NULL)
        g_kry_node_ops[kind] = *ops;
    else
        memset(&g_kry_node_ops[kind], 0, sizeof(g_kry_node_ops[kind]));
}

const KryNodeOps *
KryNodeOpsFor(KryNodeKind kind)
{
    if(kind < 0 || kind > KRY_NODE_CUSTOM)
        return NULL;
    return &g_kry_node_ops[kind];
}

void
KryNodeRegisterDestroy(KryNodeKind kind, KryNodeDestroyFn destroy)
{
    if(kind < 0 || kind > KRY_NODE_CUSTOM)
        return;
    g_kry_node_destroy[kind] = destroy;
}

static void
kry_node_update_world(KryScene *scene, KryNodeId node)
{
    KryNode *n = &scene->nodes[node];
    KryNodeId child;

    if(n->parent < 0)
        n->world = n->local;
    else
        n->world = KryTransform2DCompose(scene->nodes[n->parent].world, n->local);
    n->flags &= ~KRY_NODE_FLAG_DIRTY;

    child = n->first_child;
    while(child >= 0) {
        kry_node_update_world(scene, child);
        child = scene->nodes[child].next_sibling;
    }
}

static void
kry_node_fire_ready(KryScene *scene, KryNodeId node)
{
    KryNode *n = &scene->nodes[node];
    const KryNodeOps *ops;
    KryNodeId child;

    if(!(n->flags & KRY_NODE_FLAG_ALIVE))
        return;
    if(!(n->flags & KRY_NODE_FLAG_READY)) {
        ops = KryNodeOpsFor(n->kind);
        if(ops != NULL && ops->ready != NULL)
            ops->ready(scene, node);
        n->flags |= KRY_NODE_FLAG_READY;
    }
    child = n->first_child;
    while(child >= 0) {
        kry_node_fire_ready(scene, child);
        child = scene->nodes[child].next_sibling;
    }
}

void
KrySceneTick(KryScene *scene, float dt)
{
    int i;
    KryNode *n;
    const KryNodeOps *ops;
    float scaled_dt;

    if(scene == NULL || scene->root < 0)
        return;
    kry_node_fire_ready(scene, scene->root);
    kry_node_update_world(scene, scene->root);

    scaled_dt = dt * scene->time_scale;
    for(i = 0; i < scene->count; i++) {
        n = &scene->nodes[i];
        if(!(n->flags & KRY_NODE_FLAG_ALIVE))
            continue;
        ops = KryNodeOpsFor(n->kind);
        if(ops != NULL && ops->process != NULL)
            ops->process(scene, i, scaled_dt);
    }
}

void
KryScenePhysicsTick(KryScene *scene, float dt)
{
    int i;
    KryNode *n;
    const KryNodeOps *ops;
    float scaled_dt;

    if(scene == NULL || scene->root < 0)
        return;
    scaled_dt = dt * scene->time_scale;
    for(i = 0; i < scene->count; i++) {
        n = &scene->nodes[i];
        if(!(n->flags & KRY_NODE_FLAG_ALIVE))
            continue;
        ops = KryNodeOpsFor(n->kind);
        if(ops != NULL && ops->physics_process != NULL)
            ops->physics_process(scene, i, scaled_dt);
    }
}

void
KrySceneDraw(KryScene *scene)
{
    int i;
    KryNode *n;
    const KryNodeOps *ops;
    Camera2D camera;
    int used_camera = 0;
    Vector2 screen_size;

    if(scene == NULL || scene->root < 0)
        return;
    screen_size = (Vector2){(float)GetScreenWidth(), (float)GetScreenHeight()};

    if(scene->active_camera >= 0 && scene->active_camera < scene->count) {
        n = &scene->nodes[scene->active_camera];
        if((n->flags & KRY_NODE_FLAG_ALIVE) && n->kind == KRY_NODE_CAMERA2D) {
            float zoom = 1.0f;
            Camera2DProps *cp = (Camera2DProps *)n->props;
            if(cp != NULL)
                zoom = cp->zoom;
            camera = KryCamera2DFromTransform(n->world, screen_size, zoom);
            BeginMode2D(camera);
            used_camera = 1;
        }
    }

    for(i = 0; i < scene->count; i++) {
        n = &scene->nodes[i];
        if(!(n->flags & KRY_NODE_FLAG_ALIVE))
            continue;
        ops = KryNodeOpsFor(n->kind);
        if(ops != NULL && ops->draw != NULL)
            ops->draw(scene, i);
    }

    if(used_camera)
        EndMode2D();
}
