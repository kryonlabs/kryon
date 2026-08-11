/*
 * Property model + signal bus unit tests. Exercises the generic property
 * getters/setters (which back the editor Inspector) and signal connect/emit
 * dispatch, without requiring a raylib context.
 */

#include "kryon.h"
#include <stdio.h>
#include <string.h>

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
    if(got - want <= epsilon && want - got <= epsilon)
        return;
    fprintf(stderr, "FAIL: %s got %f want %f\n", name, got, want);
    failures++;
}

/* signal handler recording the last dispatch for assertion */
static KryNodeId g_last_target = -1;
static KryNodeId g_last_emitter = -1;
static char g_last_handler[64];
static int g_signal_fired = 0;

static void
test_signal_handler(KryScene *scene, KryNodeId target, KryNodeId emitter,
                    const char *handler, KryonPropertyValue arg)
{
    (void)scene;
    (void)arg;
    g_last_target = target;
    g_last_emitter = emitter;
    snprintf(g_last_handler, sizeof(g_last_handler), "%s", handler);
    g_signal_fired++;
}

int
main(void)
{
    KryScene scene;
    KryNodeId node_a;
    KryNodeId node_b;
    KryNodeId sprite;
    KryonPropertyValue v;
    const KryonPropertySpec *specs;
    int spec_count;

    KrySceneRegisterBuiltins();
    KrySceneInit(&scene);

    /* --- property model: Node2D transform fields --- */
    node_a = KryNodeCreate(&scene, scene.root, KRY_NODE_NODE2D, "a");
    specs = KryScenePropertySpecs(KRY_NODE_NODE2D, &spec_count);
    check_int("Node2D property count", spec_count, 3);
    check_int("first Node2D property is position",
              strcmp(specs[0].id, "position"), 0);

    KrySceneNodeSetPropertyByName(&scene, node_a, "position",
                                  KryonPropertyVector2((Vector2){50, 60}));
    v = KrySceneNodeGetPropertyByName(&scene, node_a, "position");
    check_int("position property kind", v.kind, KRYON_PROPERTY_VECTOR2);
    check_float("position x round-trips", v.as.vector2_value.x, 50.0f, 0.001f);
    check_float("position y round-trips", v.as.vector2_value.y, 60.0f, 0.001f);

    /* index-based access matches name-based */
    v = KrySceneNodeGetProperty(&scene, node_a, 1); /* rotation */
    check_int("rotation property kind", v.kind, KRYON_PROPERTY_FLOAT);
    KrySceneNodeSetProperty(&scene, node_a, 1, KryonPropertyFloat(1.5f));
    v = KrySceneNodeGetProperty(&scene, node_a, 1);
    check_float("rotation set/get by index", v.as.float_value, 1.5f, 0.001f);

    /* dirty flag is set when transform props change */
    check_int("transform set marks node dirty",
              (KryNodeGet(&scene, node_a)->flags & KRY_NODE_FLAG_DIRTY) != 0, 1);

    /* --- property model: Sprite2D kind-specific props --- */
    sprite = KryNodeCreate(&scene, scene.root, KRY_NODE_SPRITE2D, "s");
    KryNodeSetProps(&scene, sprite, KrySprite2DPropsAlloc("tiles/tile.png", 96.0f, 96.0f));
    specs = KryScenePropertySpecs(KRY_NODE_SPRITE2D, &spec_count);
    check_int("Sprite2D property count", spec_count, 6);
    v = KrySceneNodeGetPropertyByName(&scene, sprite, "asset_path");
    check_int("asset_path property kind", v.kind, KRYON_PROPERTY_ASSET_PATH);
    check_int("asset_path value", strcmp(v.as.string_value, "tiles/tile.png"), 0);

    /* --- signals: connect + emit + dispatch --- */
    node_b = KryNodeCreate(&scene, scene.root, KRY_NODE_NODE2D, "b");
    KryNodeKindRegisterSignalHandler(KRY_NODE_NODE2D, test_signal_handler);

    check_int("connect returns 1",
              KrySignalConnect(&scene, node_a, "hit", node_b, "on_hit"), 1);
    check_int("duplicate connect still returns 1 (idempotent)",
              KrySignalConnect(&scene, node_a, "hit", node_b, "on_hit"), 1);

    g_signal_fired = 0;
    check_int("emit fires one handler",
              KrySignalEmit(&scene, node_a, "hit", KryonPropertyInt(0)), 1);
    check_int("signal fired count", g_signal_fired, 1);
    check_int("signal target", g_last_target, node_b);
    check_int("signal emitter", g_last_emitter, node_a);
    check_int("signal handler name", strcmp(g_last_handler, "on_hit"), 0);

    /* emit with no connection fires nothing */
    check_int("emit unconnected signal fires 0",
              KrySignalEmit(&scene, node_a, "miss", KryonPropertyInt(0)), 0);

    /* removing a node disconnects its signals */
    KryNodeRemove(&scene, node_b);
    g_signal_fired = 0;
    check_int("emit after target removed fires 0",
              KrySignalEmit(&scene, node_a, "hit", KryonPropertyInt(0)), 0);

    KrySceneDestroy(&scene);
    return failures == 0 ? 0 : 1;
}
