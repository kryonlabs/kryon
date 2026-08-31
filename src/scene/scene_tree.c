/*
 * Kryon retained scene tree implementation.
 *
 * Mirrors the arena pattern from ui_tree.c: a fixed array of nodes, integer
 * index links, rebuilt links on add/remove. World transforms are recomputed
 * top-down each tick before _process fires, so reads during process see the
 * parents updated world transform.
 */

#include "scene_tree.h"
#include "kry_signal.h"
#include "node2d_props.h"
#include <string.h>
#include <stdio.h>

/* Default to physics enabled when the build does not override it. The Box2D
 * world teardown is compiled out below when KRYON_WITH_PHYSICS=0, so builds
 * that filter the physics sources (e.g. UI-only apps) link cleanly without
 * box2d. */
#ifndef KRYON_WITH_PHYSICS
#define KRYON_WITH_PHYSICS 1
#endif

#define KRY_KIND_MAX 64

static NodeOps g_kry_node_ops[KRY_KIND_MAX];
static NodeDestroyFn g_kry_node_destroy[KRY_KIND_MAX];
static char g_kry_kind_names[KRY_KIND_MAX][SCENE_NAME_MAX];
static int g_kry_kind_count = NODE_CUSTOM + 1;

int
NodeKindCount(void)
{
    return g_kry_kind_count;
}

const char *
NodeKindName(NodeKind kind)
{
    int kind_id = (int)kind;

    if(kind_id < 0 || kind_id >= g_kry_kind_count)
        return NULL;
    return g_kry_kind_names[kind];
}

/*
 * Register a new application-defined kind beyond the built-ins. The id is
 * valid with NodeCreate/NodeRegisterOps/SceneRegisterProperties
 * exactly like a builtin kind. Returns the kind id, or -1 when the table is
 * full or the name is missing.
 */
NodeKind
NodeRegisterCustomKind(const char *name)
{
    if(name == NULL || name[0] == '\0')
        return -1;
    if(g_kry_kind_count >= KRY_KIND_MAX)
        return -1;
    snprintf(g_kry_kind_names[g_kry_kind_count],
             sizeof(g_kry_kind_names[0]), "%s", name);
    return (NodeKind)g_kry_kind_count++;
}

/* Set by physics_world.c during SceneRegisterBuiltins. Keeps scene_tree.c
 * free of a box2d.h dependency while letting ScenePhysicsTick step the world. */
void (*kry_scene_physics_step_fn)(Scene *scene, float dt);

static int
kry_node_is_valid(Scene *scene, NodeId node)
{
    return scene != NULL && node >= 0 && node < scene->count &&
           (scene->nodes[node].flags & NODE_FLAG_ALIVE);
}

void
SceneInit(Scene *scene)
{
    Node *root;
    if(scene == NULL)
        return;
    memset(scene, 0, sizeof(*scene));
    scene->root = -1;
    scene->free_head = -1;
    scene->active_camera = -1;
    scene->time_scale = 1.0f;
    root = &scene->nodes[0];
    memset(root, 0, sizeof(*root));
    root->id = 0;
    root->parent = -1;
    root->first_child = -1;
    root->next_sibling = -1;
    root->kind = NODE_ROOT;
    root->flags = NODE_FLAG_ALIVE;
    strncpy(root->name, "root", sizeof(root->name) - 1);
    root->local = Transform2DIdentity();
    root->world = Transform2DIdentity();
    scene->count = 1;
    scene->root = 0;
}

void
SceneDestroy(Scene *scene)
{
    int i;
    NodeDestroyFn destroy;
    if(scene == NULL)
        return;
    for(i = 0; i < scene->count; i++) {
        if(!(scene->nodes[i].flags & NODE_FLAG_ALIVE))
            continue;
        destroy = g_kry_node_destroy[scene->nodes[i].kind];
        if(destroy != NULL)
            destroy(scene, &scene->nodes[i]);
    }
#if KRYON_WITH_PHYSICS
    ScenePhysicsDestroy(scene);
#endif
    memset(scene, 0, sizeof(*scene));
    scene->root = -1;
    scene->free_head = -1;
    scene->active_camera = -1;
}

static void
kry_node_detach(Scene *scene, Node *node)
{
    Node *parent;
    NodeId *slot;
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

NodeId
NodeCreate(Scene *scene, NodeId parent, NodeKind kind,
              const char *name)
{
    int index;
    Node *node;
    Node *parent_node;
    NodeId *slot;

    if(scene == NULL || scene->count >= SCENE_MAX_NODES)
        return -1;
    if(parent < 0 || parent >= scene->count ||
       !(scene->nodes[parent].flags & NODE_FLAG_ALIVE))
        return -1;
    if(scene->free_head >= 0) {
        index = scene->free_head;
        scene->free_head = scene->nodes[index].next_sibling;
    } else {
        index = scene->count++;
    }
    node = &scene->nodes[index];
    memset(node, 0, sizeof(*node));
    node->id = index;
    node->parent = -1;
    node->first_child = -1;
    node->next_sibling = -1;
    node->kind = kind;
    node->flags = NODE_FLAG_ALIVE | NODE_FLAG_DIRTY;
    if(name != NULL)
        strncpy(node->name, name, sizeof(node->name) - 1);
    node->local = Transform2DIdentity();
    node->world = Transform2DIdentity();

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
kry_node_free_recursive(Scene *scene, NodeId node)
{
    Node *n;
    NodeId child;
    NodeId next;
    NodeDestroyFn destroy;

    if(node < 0 || node >= scene->count)
        return;
    n = &scene->nodes[node];
    if(!(n->flags & NODE_FLAG_ALIVE))
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
    n->flags &= ~NODE_FLAG_ALIVE;
    n->first_child = -1;
    n->parent = -1;
    /* recycle: chain the dead slot onto the freelist (next_sibling is free
     * once the node is detached and children reset) */
    n->next_sibling = scene->free_head;
    scene->free_head = node;
}

void
NodeRemove(Scene *scene, NodeId node)
{
    Node *n;
    if(scene == NULL || !kry_node_is_valid(scene, node))
        return;
    SignalDisconnectNode(scene, node);
    n = &scene->nodes[node];
    kry_node_detach(scene, n);
    kry_node_free_recursive(scene, node);
}

NodeId
NodeFindChild(Scene *scene, NodeId parent, const char *name)
{
    Node *p;
    NodeId child;
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

Node *
NodeGet(Scene *scene, NodeId node)
{
    if(scene == NULL || node < 0 || node >= scene->count)
        return NULL;
    return &scene->nodes[node];
}

void
NodeSetPosition(Scene *scene, NodeId node, float x, float y)
{
    Node *n = NodeGet(scene, node);
    if(n != NULL) {
        n->local.position = (Vector2){x, y};
        n->flags |= NODE_FLAG_DIRTY;
    }
}

void
NodeSetRotation(Scene *scene, NodeId node, float radians)
{
    Node *n = NodeGet(scene, node);
    if(n != NULL) {
        n->local.rotation = radians;
        n->flags |= NODE_FLAG_DIRTY;
    }
}

void
NodeSetScale(Scene *scene, NodeId node, float sx, float sy)
{
    Node *n = NodeGet(scene, node);
    if(n != NULL) {
        n->local.scale = (Vector2){sx, sy};
        n->flags |= NODE_FLAG_DIRTY;
    }
}

void
NodeSetProps(Scene *scene, NodeId node, void *props)
{
    Node *n = NodeGet(scene, node);
    if(n != NULL)
        n->props = props;
}

void
NodeRegisterOps(NodeKind kind, const NodeOps *ops)
{
    int kind_id = (int)kind;

    if(kind_id < 0 || kind_id >= g_kry_kind_count)
        return;
    if(ops != NULL)
        g_kry_node_ops[kind] = *ops;
    else
        memset(&g_kry_node_ops[kind], 0, sizeof(g_kry_node_ops[kind]));
}

const NodeOps *
NodeOpsFor(NodeKind kind)
{
    int kind_id = (int)kind;

    if(kind_id < 0 || kind_id >= g_kry_kind_count)
        return NULL;
    return &g_kry_node_ops[kind];
}

void
NodeRegisterDestroy(NodeKind kind, NodeDestroyFn destroy)
{
    int kind_id = (int)kind;

    if(kind_id < 0 || kind_id >= g_kry_kind_count)
        return;
    g_kry_node_destroy[kind] = destroy;
}

static void
kry_node_update_world(Scene *scene, NodeId node)
{
    Node *n = &scene->nodes[node];
    NodeId child;

    if(n->parent < 0)
        n->world = n->local;
    else
        n->world = Transform2DCompose(scene->nodes[n->parent].world, n->local);
    n->flags &= ~NODE_FLAG_DIRTY;

    child = n->first_child;
    while(child >= 0) {
        kry_node_update_world(scene, child);
        child = scene->nodes[child].next_sibling;
    }
}

static void
kry_node_fire_ready(Scene *scene, NodeId node)
{
    Node *n = &scene->nodes[node];
    const NodeOps *ops;
    NodeId child;

    if(!(n->flags & NODE_FLAG_ALIVE))
        return;
    if(!(n->flags & NODE_FLAG_READY)) {
        ops = NodeOpsFor(n->kind);
        if(ops != NULL && ops->ready != NULL)
            ops->ready(scene, node);
        n->flags |= NODE_FLAG_READY;
    }
    child = n->first_child;
    while(child >= 0) {
        kry_node_fire_ready(scene, child);
        child = scene->nodes[child].next_sibling;
    }
}

void
SceneTick(Scene *scene, float dt)
{
    int i;
    Node *n;
    const NodeOps *ops;
    float scaled_dt;

    if(scene == NULL || scene->root < 0)
        return;
    kry_node_fire_ready(scene, scene->root);
    kry_node_update_world(scene, scene->root);

    scaled_dt = dt * scene->time_scale;
    for(i = 0; i < scene->count; i++) {
        n = &scene->nodes[i];
        if(!(n->flags & NODE_FLAG_ALIVE))
            continue;
        ops = NodeOpsFor(n->kind);
        if(ops != NULL && ops->process != NULL)
            ops->process(scene, i, scaled_dt);
    }
}

void
ScenePhysicsTick(Scene *scene, float dt)
{
    int i;
    Node *n;
    const NodeOps *ops;
    float scaled_dt;

    if(scene == NULL || scene->root < 0)
        return;
    scaled_dt = dt * scene->time_scale;
    /* physics_process hooks run BEFORE the world step (so bodies see the
     * pre-step state); the step happens centrally via kry_scene_physics_step
     * declared in scene_physics_internal.h; scene_tree.c does not include
     * box2d.h, so the step is dispatched through a function pointer set by
     * physics_world.c at registration time. */
    for(i = 0; i < scene->count; i++) {
        n = &scene->nodes[i];
        if(!(n->flags & NODE_FLAG_ALIVE))
            continue;
        ops = NodeOpsFor(n->kind);
        if(ops != NULL && ops->physics_process != NULL)
            ops->physics_process(scene, i, scaled_dt);
    }
    if(scene->physics_enabled && kry_scene_physics_step_fn != NULL)
        kry_scene_physics_step_fn(scene, scaled_dt);
}

void
SceneDraw(Scene *scene)
{
    int i;
    Node *n;
    const NodeOps *ops;
    Camera2D camera;
    int used_camera = 0;
    int screen_width;
    int screen_height;
    Vector2 screen_size;

    if(scene == NULL || scene->root < 0)
        return;
    screen_width = GetScreenWidth();
    screen_height = GetScreenHeight();
    if(screen_width <= 0 || screen_height <= 0)
        return;
    screen_size = (Vector2){(float)screen_width, (float)screen_height};

    if(scene->active_camera >= 0 && scene->active_camera < scene->count) {
        n = &scene->nodes[scene->active_camera];
        if((n->flags & NODE_FLAG_ALIVE) && n->kind == NODE_CAMERA2D) {
            float zoom = 1.0f;
            Camera2DProps *cp = (Camera2DProps *)n->props;
            if(cp != NULL)
                zoom = cp->zoom;
            camera = Camera2DFromTransform(n->world, screen_size, zoom);
            BeginMode2D(camera);
            used_camera = 1;
        }
    }

    for(i = 0; i < scene->count; i++) {
        n = &scene->nodes[i];
        if(!(n->flags & NODE_FLAG_ALIVE))
            continue;
        ops = NodeOpsFor(n->kind);
        if(ops != NULL && ops->draw != NULL)
            ops->draw(scene, i);
    }

    if(used_camera)
        EndMode2D();
}
