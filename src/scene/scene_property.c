/*
 * Property registry for the runtime scene tree. Maps the editor-facing
 * KryonPropertySpec/Value model onto live KryNode fields so the Inspector can
 * read and write every node property generically.
 *
 * Each kind publishes an ordered spec table at startup; the getter/setter use
 * the index into that table to decide which field to touch. The mapping is
 * fixed per kind (a property's index is its identity).
 */

#include "scene_property.h"
#include "node2d_props.h"
#include <string.h>

#define KRY_PROPERTY_TABLE_MAX 16

typedef struct KryPropertyTable {
    const KryonPropertySpec *specs;
    int count;
} KryPropertyTable;

static KryPropertyTable g_property_tables[KRY_NODE_CUSTOM + 1];

void
KrySceneRegisterProperties(KryNodeKind kind, const KryonPropertySpec *specs,
                           int count)
{
    if(kind < 0 || kind > KRY_NODE_CUSTOM)
        return;
    g_property_tables[kind].specs = specs;
    g_property_tables[kind].count = count;
}

const KryonPropertySpec *
KryScenePropertySpecs(KryNodeKind kind, int *out_count)
{
    if(kind < 0 || kind > KRY_NODE_CUSTOM)
        return NULL;
    if(out_count != NULL)
        *out_count = g_property_tables[kind].count;
    return g_property_tables[kind].specs;
}

static int
property_index_by_id(KryNodeKind kind, const char *property_id)
{
    int count;
    int i;
    const KryonPropertySpec *specs = KryScenePropertySpecs(kind, &count);
    if(specs == NULL || property_id == NULL)
        return -1;
    for(i = 0; i < count; i++) {
        if(strcmp(specs[i].id, property_id) == 0)
            return i;
    }
    return -1;
}

KryonPropertyValue
KrySceneNodeGetProperty(KryScene *scene, KryNodeId node, int index)
{
    KryNode *n;
    const KryonPropertySpec *specs;
    int count;
    KryonPropertyValue out = {0};

    n = KryNodeGet(scene, node);
    if(n == NULL)
        return out;
    specs = KryScenePropertySpecs(n->kind, &count);
    if(specs == NULL || index < 0 || index >= count)
        return out;

    switch(n->kind) {
    case KRY_NODE_NODE2D:
    case KRY_NODE_CAMERA2D:
    case KRY_NODE_SPRITE2D:
        /* indices 0..2 are shared transform fields on every Node2D-base kind */
        if(index == 0)
            return KryonPropertyVector2(n->local.position);
        if(index == 1)
            return KryonPropertyFloat(n->local.rotation);
        if(index == 2)
            return KryonPropertyVector2(n->local.scale);
        break;
    default:
        break;
    }

    if(n->kind == KRY_NODE_CAMERA2D && n->props != NULL) {
        Camera2DProps *p = (Camera2DProps *)n->props;
        if(index == 3)
            return KryonPropertyFloat(p->zoom);
        if(index == 4)
            return KryonPropertyBool(p->active);
    }

    if(n->kind == KRY_NODE_SPRITE2D && n->props != NULL) {
        Sprite2DProps *p = (Sprite2DProps *)n->props;
        if(index == 3)
            return KryonPropertyString(p->asset_path != NULL ? p->asset_path : "");
        if(index == 4)
            return KryonPropertyVector2(p->size);
        if(index == 5)
            return KryonPropertyColor(p->tint);
    }

    return out;
}

int
KrySceneNodeSetProperty(KryScene *scene, KryNodeId node, int index,
                        KryonPropertyValue value)
{
    KryNode *n;
    const KryonPropertySpec *specs;
    int count;

    n = KryNodeGet(scene, node);
    if(n == NULL)
        return 0;
    specs = KryScenePropertySpecs(n->kind, &count);
    if(specs == NULL || index < 0 || index >= count)
        return 0;
    if(specs[index].kind != value.kind)
        return 0;

    switch(n->kind) {
    case KRY_NODE_NODE2D:
    case KRY_NODE_CAMERA2D:
    case KRY_NODE_SPRITE2D:
        if(index == 0) {
            n->local.position = value.as.vector2_value;
            n->flags |= KRY_NODE_FLAG_DIRTY;
            return 1;
        }
        if(index == 1) {
            n->local.rotation = value.as.float_value;
            n->flags |= KRY_NODE_FLAG_DIRTY;
            return 1;
        }
        if(index == 2) {
            n->local.scale = value.as.vector2_value;
            n->flags |= KRY_NODE_FLAG_DIRTY;
            return 1;
        }
        break;
    default:
        break;
    }

    if(n->kind == KRY_NODE_CAMERA2D && n->props != NULL) {
        Camera2DProps *p = (Camera2DProps *)n->props;
        if(index == 3) {
            p->zoom = value.as.float_value;
            return 1;
        }
        if(index == 4) {
            p->active = value.as.bool_value;
            return 1;
        }
    }

    if(n->kind == KRY_NODE_SPRITE2D && n->props != NULL) {
        Sprite2DProps *p = (Sprite2DProps *)n->props;
        if(index == 3) {
            /* asset_path is owned by the builder; the editor copies into the
             * props struct's fixed buffer to avoid dangling pointers */
            p->asset_path = strdup(value.as.string_value);
            return 1;
        }
        if(index == 4) {
            p->size = value.as.vector2_value;
            return 1;
        }
        if(index == 5) {
            p->tint = value.as.color_value;
            return 1;
        }
    }

    return 0;
}

KryonPropertyValue
KrySceneNodeGetPropertyByName(KryScene *scene, KryNodeId node,
                              const char *property_id)
{
    KryNode *n = KryNodeGet(scene, node);
    int index;
    if(n == NULL) {
        KryonPropertyValue out = {0};
        return out;
    }
    index = property_index_by_id(n->kind, property_id);
    if(index < 0) {
        KryonPropertyValue out = {0};
        return out;
    }
    return KrySceneNodeGetProperty(scene, node, index);
}

int
KrySceneNodeSetPropertyByName(KryScene *scene, KryNodeId node,
                              const char *property_id, KryonPropertyValue value)
{
    KryNode *n = KryNodeGet(scene, node);
    int index;
    if(n == NULL)
        return 0;
    index = property_index_by_id(n->kind, property_id);
    if(index < 0)
        return 0;
    return KrySceneNodeSetProperty(scene, node, index, value);
}

/* ---- built-in property spec tables ---- */

static const KryonPropertySpec kry_node2d_props[] = {
    {"position", "Position", "Transform", KRYON_PROPERTY_VECTOR2, 0, 0, 0, 1, 0, {0}},
    {"rotation", "Rotation", "Transform", KRYON_PROPERTY_FLOAT, 0, -6.2832f, 6.2832f, 0.01f, 0, {0}},
    {"scale", "Scale", "Transform", KRYON_PROPERTY_VECTOR2, 0, 0, 0, 1, 0, {0}},
};

static const KryonPropertySpec kry_camera2d_props[] = {
    {"position", "Position", "Transform", KRYON_PROPERTY_VECTOR2, 0, 0, 0, 1, 0, {0}},
    {"rotation", "Rotation", "Transform", KRYON_PROPERTY_FLOAT, 0, -6.2832f, 6.2832f, 0.01f, 0, {0}},
    {"scale", "Scale", "Transform", KRYON_PROPERTY_VECTOR2, 0, 0, 0, 1, 0, {0}},
    {"zoom", "Zoom", "Camera", KRYON_PROPERTY_FLOAT, 0, 0.1f, 16.0f, 0.1f, 0, {0}},
    {"active", "Active", "Camera", KRYON_PROPERTY_BOOL, 0, 0, 0, 1, 0, {0}},
};

static const KryonPropertySpec kry_sprite2d_props[] = {
    {"position", "Position", "Transform", KRYON_PROPERTY_VECTOR2, 0, 0, 0, 1, 0, {0}},
    {"rotation", "Rotation", "Transform", KRYON_PROPERTY_FLOAT, 0, -6.2832f, 6.2832f, 0.01f, 0, {0}},
    {"scale", "Scale", "Transform", KRYON_PROPERTY_VECTOR2, 0, 0, 0, 1, 0, {0}},
    {"asset_path", "Asset", "Sprite", KRYON_PROPERTY_ASSET_PATH, 0, 0, 0, 1, 0, {0}},
    {"size", "Size", "Sprite", KRYON_PROPERTY_VECTOR2, 0, 0, 0, 1, 0, {0}},
    {"tint", "Tint", "Sprite", KRYON_PROPERTY_COLOR, 0, 0, 0, 1, 0, {0}},
};

void
KrySceneRegisterBuiltinProperties(void)
{
    KrySceneRegisterProperties(KRY_NODE_NODE2D, kry_node2d_props,
                               (int)(sizeof(kry_node2d_props) / sizeof(kry_node2d_props[0])));
    KrySceneRegisterProperties(KRY_NODE_CAMERA2D, kry_camera2d_props,
                               (int)(sizeof(kry_camera2d_props) / sizeof(kry_camera2d_props[0])));
    KrySceneRegisterProperties(KRY_NODE_SPRITE2D, kry_sprite2d_props,
                               (int)(sizeof(kry_sprite2d_props) / sizeof(kry_sprite2d_props[0])));
}
