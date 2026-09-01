/*
 * Scene tree unit tests. Exercises SceneInit, node creation, parent/child
 * linking, transform composition, and the lifecycle/draw driver, without
 * requiring a raylib context (the draw op is a no-op when no window exists,
 * but the tree structure and transform math are fully testable).
 */

#include "kryon.h"
#include <stdio.h>
#include <math.h>

static int failures;

/* Signal handler for the custom-kind dispatch test below. */
static int g_sig_probe_fired;

static void
sig_probe_handler(Scene *scene, NodeId target, NodeId emitter,
                    const char *handler, PropertyValue arg)
{
    (void)scene;
    (void)target;
    (void)emitter;
    (void)handler;
    g_sig_probe_fired = arg.as.int_value;
}

/* Captures a node's world position when its ready hook fires, mirroring
 * Body2D which creates its b2 body at n->world during ready. */
static Vector2 g_ready_probe_world;
static int g_ready_probe_fired;

static void
ready_probe_ready(Scene *scene, NodeId node)
{
    Node *n = NodeGet(scene, node);

    g_ready_probe_fired = 1;
    if(n != NULL)
        g_ready_probe_world = n->world.position;
}


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
    Scene scene;
    NodeId root;
    NodeId parent;
    NodeId child_a;
    NodeId child_b;
    Node *n;

    SceneRegisterBuiltins();
    SceneInit(&scene);

    check_int("scene root exists", scene.root, 0);
    check_int("scene starts with one node (the root)", scene.count, 1);

    root = scene.root;
    parent = NodeCreate(&scene, root, NODE_NODE2D, "parent");
    check_int("parent created", parent, 1);
    check_int("parent is child of root", scene.nodes[parent].parent, root);
    check_int("root first child is parent", scene.nodes[root].first_child, parent);
    check_int("root child count", scene.nodes[root].child_count, 1);

    child_a = NodeCreate(&scene, parent, NODE_NODE2D, "a");
    child_b = NodeCreate(&scene, parent, NODE_NODE2D, "b");
    check_int("child_a is child of parent", scene.nodes[child_a].parent, parent);
    check_int("child_b is child of parent", scene.nodes[child_b].parent, parent);
    check_int("parent first child is a", scene.nodes[parent].first_child, child_a);
    check_int("child_a sibling is b", scene.nodes[child_a].next_sibling, child_b);
    check_int("parent child count after two adds", scene.nodes[parent].child_count, 2);
    check_int("findChild locates b", NodeFindChild(&scene, parent, "b"), child_b);
    check_int("findChild misses unknown", NodeFindChild(&scene, parent, "zzz"), -1);

    /* transform composition: parent at (100,100), child at (10,20) -> world (110,120) */
    NodeSetPosition(&scene, parent, 100.0f, 100.0f);
    NodeSetPosition(&scene, child_a, 10.0f, 20.0f);
    SceneTick(&scene, 0.016f); /* recomputes world transforms */
    n = NodeGet(&scene, child_a);
    check_float("child world x composes parent", n->world.position.x, 110.0f, 0.001f);
    check_float("child world y composes parent", n->world.position.y, 120.0f, 0.001f);

    /* rotation in the parent propagates to children */
    NodeSetRotation(&scene, parent, 3.14159265f / 2.0f); /* 90 deg */
    NodeSetPosition(&scene, child_a, 0.0f, 0.0f);
    SceneTick(&scene, 0.016f);
    n = NodeGet(&scene, child_a);
    /* (0,0) local under 90deg parent at (100,100) -> world stays (100,100) */
    check_float("rotated child world x", n->world.position.x, 100.0f, 0.001f);
    check_float("rotated child world y", n->world.position.y, 100.0f, 0.001f);

    /* remove a node: it detaches and frees its subtree */
    NodeRemove(&scene, child_b);
    check_int("removed node is not alive",
              (scene.nodes[child_b].flags & NODE_FLAG_ALIVE) == 0, 1);
    check_int("parent child count after remove", scene.nodes[parent].child_count, 1);

    /* active camera selection: a Camera2D with active=1 claims the scene */
    {
        NodeId cam = NodeCreate(&scene, root, NODE_CAMERA2D, "cam");
        Camera2DProps *props = KryCamera2DPropsAlloc(2.0f, 1);
        NodeSetProps(&scene, cam, props);
        SceneTick(&scene, 0.016f); /* fires ready on cam -> claims active_camera */
        check_int("active camera set", scene.active_camera, cam);
    }

    /* ready hooks must see computed world transforms (Body2D creates its
     * b2 body at n->world during ready, so a stale identity transform put
     * every body at the origin) */
    {
        static const NodeOps probe_ops = {
            ready_probe_ready, NULL, NULL, NULL
        };
        NodeKind custom = NodeRegisterCustomKind("ready_world_probe");
        NodeId parent2 = NodeCreate(&scene, root, NODE_NODE2D, "p2");
        NodeId probe;

        check_int("custom kind registered", custom >= 0, 1);
        check_int("probe parent created", parent2 >= 0, 1);
        if(custom >= 0) {
            NodeRegisterOps(custom, &probe_ops);
            probe = NodeCreate(&scene, parent2, custom, "probe");
            check_int("probe created", probe >= 0, 1);
            NodeSetPosition(&scene, parent2, 100.0f, 100.0f);
            NodeSetPosition(&scene, probe, 10.0f, 20.0f);
            g_ready_probe_fired = 0;
            SceneTick(&scene, 0.0f);
            check_int("probe ready fired", g_ready_probe_fired, 1);
            check_float("ready sees composed world x",
                        g_ready_probe_world.x, 110.0f, 0.001f);
            check_float("ready sees composed world y",
                        g_ready_probe_world.y, 120.0f, 0.001f);
        }
    }

    /* Area2D sensor events: a monitoring area with a sensor shape records
     * the dynamic body that enters it (the sensor event drain in the
     * physics step emits body_enter signals). A sensor shape attaches to
     * the nearest Body2D ancestor, so the area rides a static body. */
    {
        NodeId anchor = NodeCreate(&scene, root, NODE_BODY2D, "anchor");
        NodeId area = NodeCreate(&scene, anchor, NODE_AREA2D, "trigger");
        NodeId area_shape;
        NodeId faller = NodeCreate(&scene, root, NODE_BODY2D, "faller");
        NodeId faller_shape;
        Area2DProps *ap;
        Body2DProps *bp;
        CollisionShape2DProps *sp;
        int i;

        bp = KryBody2DPropsAlloc(KRY_BODY2D_STATIC);
        NodeSetProps(&scene, anchor, bp);
        NodeSetPosition(&scene, anchor, 0.0f, 60.0f); /* +Y is down */
        ap = KryArea2DPropsAlloc();
        ap->monitoring = 1;
        NodeSetProps(&scene, area, ap);
        area_shape = NodeCreate(&scene, area, NODE_COLLISION_SHAPE2D, "s1");
        sp = KryCollisionShape2DPropsAlloc(KRY_SHAPE2D_BOX, 40.0f, 40.0f);
        sp->is_sensor = 1;
        NodeSetProps(&scene, area_shape, sp);

        bp = KryBody2DPropsAlloc(KRY_BODY2D_DYNAMIC);
        NodeSetProps(&scene, faller, bp);
        faller_shape = NodeCreate(&scene, faller, NODE_COLLISION_SHAPE2D, "s2");
        NodeSetProps(&scene, faller_shape, KryCollisionShape2DPropsAlloc(
                         KRY_SHAPE2D_BOX, 10.0f, 10.0f));
        NodeSetPosition(&scene, faller, 0.0f, 0.0f);

        check_int("physics world", ScenePhysicsCreate(&scene, 0.0f, 200.0f), 1);
        SceneTick(&scene, 0.0f); /* ready hooks create the bodies */
        for(i = 0; i < 120; i++) {
            ScenePhysicsTick(&scene, 1.0f / 60.0f);
            SceneTick(&scene, 1.0f / 60.0f);
        }
        check_int("sensor enter recorded", ap->last_enter_body, faller);

        /* direct body control: velocity steering moves the body */
        {
            float fx, fy;

            KryBody2DGetVelocity(&scene, faller, &fx, &fy);
            KryBody2DSetVelocity(&scene, faller, 50.0f, 0.0f);
            for(i = 0; i < 30; i++)
                ScenePhysicsTick(&scene, 1.0f / 60.0f);
            KryBody2DGetVelocity(&scene, faller, &fx, &fy);
            check_float("velocity read back", fx, 50.0f, 0.01f);
            n = NodeGet(&scene, faller);
            check_float("steered body moved right",
                        n->local.position.x > 20.0f, 1.0f, 0.0f);
        }
    }

    /* signal handlers must work for application-defined kinds: custom
     * kind ids sit beyond the builtins in the handler table */
    {
        NodeKind k = NodeRegisterCustomKind("sig_target");
        NodeId emitter = NodeCreate(&scene, root, NODE_NODE2D, "emitter2");
        NodeId target;

        check_int("sig custom kind registered", k >= 0, 1);
        if(k >= 0 && emitter >= 0) {
            target = NodeCreate(&scene, root, k, "target2");
            NodeKindRegisterSignalHandler(k, sig_probe_handler);
            check_int("sig connect", SignalConnect(&scene, emitter, "ping",
                                                   target, "on_ping"), 1);
            g_sig_probe_fired = 0;
            check_int("sig emit fires handler",
                      SignalEmit(&scene, emitter, "ping", PropertyInt(42)), 1);
            check_int("sig handler ran", g_sig_probe_fired, 42);
        }
    }

    /* tick/draw on a scene with no window should not crash */
    SceneTick(&scene, 0.016f);
    ScenePhysicsTick(&scene, 0.016f);
    SceneDraw(&scene);

    SceneDestroy(&scene);
    return failures == 0 ? 0 : 1;
}
