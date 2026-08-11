#ifndef SCENE_TREE_H
#define SCENE_TREE_H

/*
 * Kryon retained scene tree.
 *
 * A parallel tree to the immediate-mode UI tree (ui_tree.h). The UI tree is
 * rebuilt every frame for app widgets; this scene tree persists across frames
 * for game nodes with transforms, lifecycle hooks, and (later) physics.
 *
 * Nodes are stored in an array-indexed arena, mirroring the proven ui_tree.c
 * layout: parent/first_child/next_sibling are indices, -1 means "none".
 */

#include "kryon_compat.generated.h"
#include "kry_math.h"

#define KRY_SCENE_MAX_NODES 4096
#define KRY_SCENE_NAME_MAX 64

typedef int KryNodeId;

/* Forward declarations so the KryNodeOps vtable can reference KryScene. */
typedef struct KryScene KryScene;
typedef struct KryNode KryNode;

typedef enum KryNodeKind {
    KRY_NODE_ROOT,
    KRY_NODE_NODE2D,
    KRY_NODE_CAMERA2D,
    KRY_NODE_SPRITE2D,
    KRY_NODE_ANIMATED_SPRITE2D,
    KRY_NODE_TILEMAP,
    KRY_NODE_COLLISION_SHAPE2D,
    KRY_NODE_AREA2D,
    KRY_NODE_BODY2D,
    KRY_NODE_TIMER,
    KRY_NODE_AUDIO_SOURCE,
    KRY_NODE_CUSTOM
} KryNodeKind;

typedef enum KryNodeFlags {
    KRY_NODE_FLAG_ALIVE = 1 << 0,
    KRY_NODE_FLAG_READY = 1 << 1,   /* _ready has been called */
    KRY_NODE_FLAG_DIRTY = 1 << 2    /* world transform needs recomputation */
} KryNodeFlags;

/*
 * Per-kind lifecycle vtable. Any slot may be NULL. The runtime calls these
 * while walking the tree each frame; `node` is the node index.
 */
typedef struct KryNodeOps {
    void (*ready)(struct KryScene *scene, KryNodeId node);
    void (*process)(struct KryScene *scene, KryNodeId node, float dt);
    void (*physics_process)(struct KryScene *scene, KryNodeId node, float dt);
    void (*draw)(struct KryScene *scene, KryNodeId node);
} KryNodeOps;

typedef struct KryNode {
    KryNodeId id;
    int parent;
    int first_child;
    int next_sibling;
    int child_count;
    KryNodeKind kind;
    unsigned flags;
    char name[KRY_SCENE_NAME_MAX];
    KryTransform2D local;
    KryTransform2D world;
    void *props; /* kind-specific data (Sprite2DProps, Camera2DProps, ...) */
    void *state; /* per-instance runtime state (allocations owned by the kind) */
} KryNode;

typedef struct KryScene {
    KryNode nodes[KRY_SCENE_MAX_NODES];
    int count;
    KryNodeId root;
    float time_scale;
    KryNodeId active_camera; /* first Camera2D that wants to be active; -1 if none */
} KryScene;

/* --- scene lifecycle --- */
void KrySceneInit(KryScene *scene);
void KrySceneDestroy(KryScene *scene);

/* --- tree mutation --- */
/*
 * Create a node of `kind` as a child of `parent`. `name` may be NULL. The
 * node's props/state pointers are left NULL; the caller or the kind's ready
 * hook is responsible for initializing kind-specific data. Returns the new
 * node id, or -1 if the arena is full or parent is invalid.
 */
KryNodeId KryNodeCreate(KryScene *scene, KryNodeId parent, KryNodeKind kind,
                        const char *name);

/*
 * Mark a node (and its subtree) for removal. The node is detached from its
 * parent and its `state`/`props` are released via the kind's registered
 * destroyer if any. The node slot stays in the array but is no longer ALIVE.
 */
void KryNodeRemove(KryScene *scene, KryNodeId node);

KryNodeId KryNodeFindChild(KryScene *scene, KryNodeId parent, const char *name);
KryNode *KryNodeGet(KryScene *scene, KryNodeId node);

/* --- per-frame driver --- */
/*
 * Advance the simulation: fire _ready on newly-created nodes (one-shot), then
 * _process on all alive nodes with `dt` scaled by scene->time_scale. World
 * transforms are recomputed top-down before process so reads see fresh data.
 */
void KrySceneTick(KryScene *scene, float dt);

/*
 * Advance physics: fire _physics_process on all alive nodes. Fixed timestep
 * stepping is the caller's responsibility (kc-generated main() does it).
 */
void KryScenePhysicsTick(KryScene *scene, float dt);

/*
 * Render the tree. If a Camera2D is active, wraps the world draw in raylib
 * BeginMode2D/EndMode2D. Nodes without a draw op are skipped.
 */
void KrySceneDraw(KryScene *scene);

/* --- kind registration --- */
/*
 * Register the ops vtable for a kind. Each kind has at most one vtable,
 * installed once at startup (node_2d.c, node_sprite2d.c, etc. register their
 * own kinds during KrySceneRegisterBuiltins). NULL slots are treated as no-ops.
 */
void KryNodeRegisterOps(KryNodeKind kind, const KryNodeOps *ops);
const KryNodeOps *KryNodeOpsFor(KryNodeKind kind);

/* Register the built-in node kinds (Node2D, Camera2D, Sprite2D, ...). */
void KrySceneRegisterBuiltins(void);

/* Optional per-kind teardown hook for freeing props/state allocations. */
typedef void (*KryNodeDestroyFn)(KryScene *scene, KryNode *node);
void KryNodeRegisterDestroy(KryNodeKind kind, KryNodeDestroyFn destroy);

#endif
