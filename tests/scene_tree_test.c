/*
 * Scene tree unit tests. Exercises KrySceneInit, node creation, parent/child
 * linking, transform composition, and the lifecycle/draw driver, without
 * requiring a raylib context (the draw op is a no-op when no window exists,
 * but the tree structure and transform math are fully testable).
 */

#include "kryon.h"
#include <stdio.h>
#include <math.h>

static int failures;

static void
check_int(const char *name, int got, int want)
{
    if(got == want)
        return;
    fprintf(stderr, "FAIL: %s got %d want %d\n", name, got, want);
    failures++;
}

static void
check_float(const char *name, float got, float want, float epsilon)
{
    if(fabsf(got - want) <= epsilon)
        return;
    fprintf(stderr, "FAIL: %s got %f want %f\n", name, got, want);
    failures++;
}

int
main(void)
{
    KryScene scene;
    KryNodeId root;
    KryNodeId parent;
    KryNodeId child_a;
    KryNodeId child_b;
    KryNode *n;

    KrySceneRegisterBuiltins();
    KrySceneInit(&scene);

    check_int("scene root exists", scene.root, 0);
    check_int("scene starts with one node (the root)", scene.count, 1);

    root = scene.root;
    parent = KryNodeCreate(&scene, root, KRY_NODE_NODE2D, "parent");
    check_int("parent created", parent, 1);
    check_int("parent is child of root", scene.nodes[parent].parent, root);
    check_int("root first child is parent", scene.nodes[root].first_child, parent);
    check_int("root child count", scene.nodes[root].child_count, 1);

    child_a = KryNodeCreate(&scene, parent, KRY_NODE_NODE2D, "a");
    child_b = KryNodeCreate(&scene, parent, KRY_NODE_NODE2D, "b");
    check_int("child_a is child of parent", scene.nodes[child_a].parent, parent);
    check_int("child_b is child of parent", scene.nodes[child_b].parent, parent);
    check_int("parent first child is a", scene.nodes[parent].first_child, child_a);
    check_int("child_a sibling is b", scene.nodes[child_a].next_sibling, child_b);
    check_int("parent child count after two adds", scene.nodes[parent].child_count, 2);
    check_int("findChild locates b", KryNodeFindChild(&scene, parent, "b"), child_b);
    check_int("findChild misses unknown", KryNodeFindChild(&scene, parent, "zzz"), -1);

    /* transform composition: parent at (100,100), child at (10,20) -> world (110,120) */
    KryNodeSetPosition(&scene, parent, 100.0f, 100.0f);
    KryNodeSetPosition(&scene, child_a, 10.0f, 20.0f);
    KrySceneTick(&scene, 0.016f); /* recomputes world transforms */
    n = KryNodeGet(&scene, child_a);
    check_float("child world x composes parent", n->world.position.x, 110.0f, 0.001f);
    check_float("child world y composes parent", n->world.position.y, 120.0f, 0.001f);

    /* rotation in the parent propagates to children */
    KryNodeSetRotation(&scene, parent, 3.14159265f / 2.0f); /* 90 deg */
    KryNodeSetPosition(&scene, child_a, 0.0f, 0.0f);
    KrySceneTick(&scene, 0.016f);
    n = KryNodeGet(&scene, child_a);
    /* (0,0) local under 90deg parent at (100,100) -> world stays (100,100) */
    check_float("rotated child world x", n->world.position.x, 100.0f, 0.001f);
    check_float("rotated child world y", n->world.position.y, 100.0f, 0.001f);

    /* remove a node: it detaches and frees its subtree */
    KryNodeRemove(&scene, child_b);
    check_int("removed node is not alive",
              (scene.nodes[child_b].flags & KRY_NODE_FLAG_ALIVE) == 0, 1);
    check_int("parent child count after remove", scene.nodes[parent].child_count, 1);

    /* active camera selection: a Camera2D with active=1 claims the scene */
    {
        KryNodeId cam = KryNodeCreate(&scene, root, KRY_NODE_CAMERA2D, "cam");
        Camera2DProps *props = KryCamera2DPropsAlloc(2.0f, 1);
        KryNodeSetProps(&scene, cam, props);
        KrySceneTick(&scene, 0.016f); /* fires ready on cam -> claims active_camera */
        check_int("active camera set", scene.active_camera, cam);
    }

    /* tick/draw on a scene with no window should not crash */
    KrySceneTick(&scene, 0.016f);
    KryScenePhysicsTick(&scene, 0.016f);
    KrySceneDraw(&scene);

    KrySceneDestroy(&scene);
    return failures == 0 ? 0 : 1;
}
