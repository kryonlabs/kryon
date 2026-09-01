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

#define SCENE_MAX_NODES 4096
#define SCENE_NAME_MAX 64

typedef int NodeId;

/* Forward declarations so the NodeOps vtable can reference Scene. */
typedef struct Scene Scene;
typedef struct Node Node;

typedef enum NodeKind {
    NODE_ROOT,
    NODE_NODE2D,
    NODE_CAMERA2D,
    NODE_SPRITE2D,
    NODE_ANIMATED_SPRITE2D,
    NODE_TILEMAP,
    NODE_COLLISION_SHAPE2D,
    NODE_AREA2D,
    NODE_BODY2D,
    NODE_TIMER,
    NODE_AUDIO_SOURCE,
    NODE_CUSTOM
} NodeKind;

typedef enum NodeFlags {
    NODE_FLAG_ALIVE = 1 << 0,
    NODE_FLAG_READY = 1 << 1,   /* _ready has been called */
    NODE_FLAG_DIRTY = 1 << 2    /* world transform needs recomputation */
} NodeFlags;

/*
 * Per-kind lifecycle vtable. Any slot may be NULL. The runtime calls these
 * while walking the tree each frame; `node` is the node index.
 */
typedef struct NodeOps {
    void (*ready)(struct Scene *scene, NodeId node);
    void (*process)(struct Scene *scene, NodeId node, float dt);
    void (*physics_process)(struct Scene *scene, NodeId node, float dt);
    void (*draw)(struct Scene *scene, NodeId node);
} NodeOps;

typedef struct Node {
    NodeId id;
    int parent;
    int first_child;
    int next_sibling;
    int child_count;
    NodeKind kind;
    unsigned flags;
    char name[SCENE_NAME_MAX];
    Transform2D local;
    Transform2D world;
    void *props; /* kind-specific data (Sprite2DProps, Camera2DProps, ...) */
    void *state; /* per-instance runtime state (allocations owned by the kind) */
} Node;

typedef struct Scene {
    Node nodes[SCENE_MAX_NODES];
    int count;
    /* Freelist of removed slots (chained through next_sibling); create pops
     * from here before growing count, so churn (respawning enemies, ...) does
     * not exhaust the arena. -1 when empty. */
    int free_head;
    NodeId root;
    float time_scale;
    NodeId active_camera; /* first Camera2D that wants to be active; -1 if none */
    /* Box2D physics world id. Opaque layout (mirrors b2WorldId {uint16,uint16})
     * so this header does not need to include box2d.h. {0,0} means no world. */
    unsigned short physics_world_index;
    unsigned short physics_world_gen;
    int physics_enabled; /* when nonzero, ScenePhysicsTick steps the world */
} Scene;

/* --- scene lifecycle --- */
void SceneInit(Scene *scene);
void SceneDestroy(Scene *scene);

/* --- tree mutation --- */
/*
 * Create a node of `kind` as a child of `parent`. `name` may be NULL. The
 * nodes props/state pointers are left NULL; the caller or the kinds ready
 * hook is responsible for initializing kind-specific data. Returns the new
 * node id, or -1 if the arena is full or parent is invalid.
 */
NodeId NodeCreate(Scene *scene, NodeId parent, NodeKind kind,
                        const char *name);

/*
 * Mark a node (and its subtree) for removal. The node is detached from its
 * parent and its `state`/`props` are released via the kinds registered
 * destroyer if any. The node slot stays in the array but is no longer ALIVE.
 */
void NodeRemove(Scene *scene, NodeId node);

NodeId NodeFindChild(Scene *scene, NodeId parent, const char *name);
Node *NodeGet(Scene *scene, NodeId node);

/* --- property setters (builders mutate nodes via these, since NodeGet
 * returns a pointer only into the arena; these are the canonical accessors) --- */
void NodeSetPosition(Scene *scene, NodeId node, float x, float y);
void NodeSetRotation(Scene *scene, NodeId node, float radians);
void NodeSetScale(Scene *scene, NodeId node, float sx, float sy);
void NodeSetProps(Scene *scene, NodeId node, void *props);

/* --- per-frame driver --- */
/*
 * Advance the simulation: fire _ready on newly-created nodes (one-shot), then
 * _process on all alive nodes with `dt` scaled by scene->time_scale. World
 * transforms are recomputed top-down before process so reads see fresh data.
 */
void SceneTick(Scene *scene, float dt);

/*
 * Advance physics: fire _physics_process on all alive nodes. Fixed timestep
 * stepping is the callers responsibility (k2c-generated main() does it).
 */
void ScenePhysicsTick(Scene *scene, float dt);

/*
 * Render the tree. If a Camera2D is active, wraps the world draw in raylib
 * BeginMode2D/EndMode2D. Nodes without a draw op are skipped.
 */
void SceneDraw(Scene *scene);

/* --- kind registration --- */
/*
 * Register the ops vtable for a kind. Each kind has at most one vtable,
 * installed once at startup (node_2d.c, node_sprite2d.c, etc. register their
 * own kinds during SceneRegisterBuiltins). NULL slots are treated as no-ops.
 */
void NodeRegisterOps(NodeKind kind, const NodeOps *ops);
const NodeOps *NodeOpsFor(NodeKind kind);

/* Register the built-in node kinds (Node2D, Camera2D, Sprite2D, ...). */
void SceneRegisterBuiltins(void);

/* --- application-defined kinds ---
 * Kinds beyond the built-in enum are allocated at runtime so games can put
 * their own entity types into the tree (and the editor property model)
 * without forking NodeKind. Ops/destroyers/properties register against
 * the returned id exactly like a builtin kind. */
int NodeKindCount(void);
const char *NodeKindName(NodeKind kind);
NodeKind NodeRegisterCustomKind(const char *name);

/* --- physics --- */
/*
 * Create the Box2D world for this scene (gravity defaults to {0, 9.8}). Once
 * created, ScenePhysicsTick steps it at a fixed sub-step count and Body2D /
 * Area2D / CollisionShape2D nodes sync their transforms from the world after
 * each step. Returns 1 on success, 0 if Box2D is unavailable.
 */
int ScenePhysicsCreate(Scene *scene, float gravity_x, float gravity_y);
void ScenePhysicsDestroy(Scene *scene);

/* --- direct Body2D control (players, kinematic platforms) --- */
/* Teleport a body; the node transform follows on the next tick. */
void KryBody2DSetTransform(Scene *scene, NodeId node, float x, float y);
/* Set/read linear velocity in scene units per second. */
void KryBody2DSetVelocity(Scene *scene, NodeId node, float vx, float vy);
void KryBody2DGetVelocity(Scene *scene, NodeId node, float *vx, float *vy);

/* Optional per-kind teardown hook for freeing props/state allocations. */
typedef void (*NodeDestroyFn)(Scene *scene, Node *node);
void NodeRegisterDestroy(NodeKind kind, NodeDestroyFn destroy);

#endif
