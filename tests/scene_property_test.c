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
static NodeId g_last_target = -1;
static NodeId g_last_emitter = -1;
static char g_last_handler[64];
static int g_signal_fired = 0;

static void
test_signal_handler(Scene *scene, NodeId target, NodeId emitter,
                    const char *handler, PropertyValue arg)
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
    Scene scene;
    NodeId node_a;
    NodeId node_b;
    NodeId sprite;
    NodeId light;
    PropertyValue v;
    const PropertySpec *specs;
    int spec_count;

    SceneRegisterBuiltins();
    SceneInit(&scene);

    /* --- property model: Node2D transform fields --- */
    node_a = NodeCreate(&scene, scene.root, NODE_NODE2D, "a");
    specs = ScenePropertySpecs(NODE_NODE2D, &spec_count);
    check_int("Node2D property count", spec_count, 3);
    check_int("first Node2D property is position",
              strcmp(specs[0].id, "position"), 0);

    SceneNodeSetPropertyByName(&scene, node_a, "position",
                                  PropertyVector2((Vector2){50, 60}));
    v = SceneNodeGetPropertyByName(&scene, node_a, "position");
    check_int("position property kind", v.kind, PROPERTY_VECTOR2);
    check_float("position x round-trips", v.as.vector2_value.x, 50.0f, 0.001f);
    check_float("position y round-trips", v.as.vector2_value.y, 60.0f, 0.001f);

    /* index-based access matches name-based */
    v = SceneNodeGetProperty(&scene, node_a, 1); /* rotation */
    check_int("rotation property kind", v.kind, PROPERTY_FLOAT);
    SceneNodeSetProperty(&scene, node_a, 1, PropertyFloat(1.5f));
    v = SceneNodeGetProperty(&scene, node_a, 1);
    check_float("rotation set/get by index", v.as.float_value, 1.5f, 0.001f);

    /* dirty flag is set when transform props change */
    check_int("transform set marks node dirty",
              (NodeGet(&scene, node_a)->flags & NODE_FLAG_DIRTY) != 0, 1);

    /* --- property model: Sprite2D kind-specific props --- */
    sprite = NodeCreate(&scene, scene.root, NODE_SPRITE2D, "s");
    NodeSetProps(&scene, sprite, KrySprite2DPropsAlloc("tiles/tile.png", 96.0f, 96.0f));
    specs = ScenePropertySpecs(NODE_SPRITE2D, &spec_count);
    check_int("Sprite2D property count", spec_count, 6);
    v = SceneNodeGetPropertyByName(&scene, sprite, "asset_path");
    check_int("asset_path property kind", v.kind, PROPERTY_ASSET_PATH);
    check_int("asset_path value", strcmp(v.as.string_value, "tiles/tile.png"), 0);

    /* --- property model: Light2D rendering props --- */
    light = NodeCreate(&scene, scene.root, NODE_LIGHT2D, "lamp");
    NodeSetProps(&scene, light,
                 KryLight2DPropsAlloc(96.0f, (Color){120, 80, 220, 180}, 0.75f));
    specs = ScenePropertySpecs(NODE_LIGHT2D, &spec_count);
    check_int("Light2D property count", spec_count, 7);
    v = SceneNodeGetPropertyByName(&scene, light, "radius");
    check_float("Light2D radius", v.as.float_value, 96.0f, 0.001f);
    check_int("set Light2D enabled", SceneNodeSetPropertyByName(
                  &scene, light, "enabled", PropertyBool(0)), 1);
    v = SceneNodeGetPropertyByName(&scene, light, "enabled");
    check_int("Light2D enabled round-trips", v.as.bool_value, 0);

    /* --- signals: connect + emit + dispatch --- */
    node_b = NodeCreate(&scene, scene.root, NODE_NODE2D, "b");
    NodeKindRegisterSignalHandler(NODE_NODE2D, test_signal_handler);

    check_int("connect returns 1",
              SignalConnect(&scene, node_a, "hit", node_b, "on_hit"), 1);
    check_int("duplicate connect still returns 1 (idempotent)",
              SignalConnect(&scene, node_a, "hit", node_b, "on_hit"), 1);

    g_signal_fired = 0;
    check_int("emit fires one handler",
              SignalEmit(&scene, node_a, "hit", PropertyInt(0)), 1);
    check_int("signal fired count", g_signal_fired, 1);
    check_int("signal target", g_last_target, node_b);
    check_int("signal emitter", g_last_emitter, node_a);
    check_int("signal handler name", strcmp(g_last_handler, "on_hit"), 0);

    /* emit with no connection fires nothing */
    check_int("emit unconnected signal fires 0",
              SignalEmit(&scene, node_a, "miss", PropertyInt(0)), 0);

    /* removing a node disconnects its signals */
    NodeRemove(&scene, node_b);
    g_signal_fired = 0;
    check_int("emit after target removed fires 0",
              SignalEmit(&scene, node_a, "hit", PropertyInt(0)), 0);

    SceneDestroy(&scene);
    return failures == 0 ? 0 : 1;
}
