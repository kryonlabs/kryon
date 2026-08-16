/*
 * Property registry for the runtime scene tree. Maps the editor-facing
 * PropertySpec/Value model onto live Node fields so the Inspector can
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
#define KRY_PROPERTY_KIND_MAX 64

typedef struct KryPropertyTable {
    const PropertySpec *specs;
    int count;
    ScenePropertyGetFn get;
    ScenePropertySetFn set;
} KryPropertyTable;

static KryPropertyTable g_property_tables[KRY_PROPERTY_KIND_MAX];

/*
 * Register an application-defined node kind (id beyond the builtins, from
 * NodeRegisterCustomKind) together with its property spec table and
 * getter/setter callbacks. The callbacks receive the node id and the spec
 * index; they read/write the kind's props struct, whose pointer is stored on
 * the node via NodeSetProps. Returns 1 on success.
 */
int
SceneRegisterCustomKind(NodeKind kind, const PropertySpec *specs,
                           int count, ScenePropertyGetFn get,
                           ScenePropertySetFn set)
{
    if(kind <= NODE_CUSTOM || kind >= KRY_PROPERTY_KIND_MAX)
        return 0;
    g_property_tables[kind].specs = specs;
    g_property_tables[kind].count = count;
    g_property_tables[kind].get = get;
    g_property_tables[kind].set = set;
    return 1;
}

void
SceneRegisterProperties(NodeKind kind, const PropertySpec *specs,
                           int count)
{
    if(kind < 0 || kind >= NodeKindCount() ||
       kind >= KRY_PROPERTY_KIND_MAX)
        return;
    g_property_tables[kind].specs = specs;
    g_property_tables[kind].count = count;
}

const PropertySpec *
ScenePropertySpecs(NodeKind kind, int *out_count)
{
    if(kind < 0 || kind >= NodeKindCount() ||
       kind >= KRY_PROPERTY_KIND_MAX)
        return NULL;
    if(out_count != NULL)
        *out_count = g_property_tables[kind].count;
    return g_property_tables[kind].specs;
}

static int
property_index_by_id(NodeKind kind, const char *property_id)
{
    int count;
    int i;
    const PropertySpec *specs = ScenePropertySpecs(kind, &count);
    if(specs == NULL || property_id == NULL)
        return -1;
    for(i = 0; i < count; i++) {
        if(strcmp(specs[i].id, property_id) == 0)
            return i;
    }
    return -1;
}

PropertyValue
SceneNodeGetProperty(Scene *scene, NodeId node, int index)
{
    Node *n;
    const PropertySpec *specs;
    int count;
    PropertyValue out = {0};

    n = NodeGet(scene, node);
    if(n == NULL)
        return out;
    specs = ScenePropertySpecs(n->kind, &count);
    if(specs == NULL || index < 0 || index >= count)
        return out;

    if(n->kind > NODE_CUSTOM) {
        KryPropertyTable *t = &g_property_tables[n->kind];

        /* indices 0..2 are shared transform fields on every kind */
        if(index == 0)
            return PropertyVector2(n->local.position);
        if(index == 1)
            return PropertyFloat(n->local.rotation);
        if(index == 2)
            return PropertyVector2(n->local.scale);
        if(t->get != NULL)
            return t->get(scene, node, index);
        return out;
    }

    switch(n->kind) {
    case NODE_NODE2D:
    case NODE_CAMERA2D:
    case NODE_SPRITE2D:
        /* indices 0..2 are shared transform fields on every Node2D-base kind */
        if(index == 0)
            return PropertyVector2(n->local.position);
        if(index == 1)
            return PropertyFloat(n->local.rotation);
        if(index == 2)
            return PropertyVector2(n->local.scale);
        break;
    default:
        break;
    }

    if(n->kind == NODE_CAMERA2D && n->props != NULL) {
        Camera2DProps *p = (Camera2DProps *)n->props;
        if(index == 3)
            return PropertyFloat(p->zoom);
        if(index == 4)
            return PropertyBool(p->active);
    }

    if(n->kind == NODE_SPRITE2D && n->props != NULL) {
        Sprite2DProps *p = (Sprite2DProps *)n->props;
        if(index == 3)
            return PropertyString(p->asset_path != NULL ? p->asset_path : "");
        if(index == 4)
            return PropertyVector2(p->size);
        if(index == 5)
            return PropertyColor(p->tint);
    }

    return out;
}

int
SceneNodeSetProperty(Scene *scene, NodeId node, int index,
                        PropertyValue value)
{
    Node *n;
    const PropertySpec *specs;
    int count;

    n = NodeGet(scene, node);
    if(n == NULL)
        return 0;
    specs = ScenePropertySpecs(n->kind, &count);
    if(specs == NULL || index < 0 || index >= count)
        return 0;
    if(specs[index].kind != value.kind)
        return 0;

    if(n->kind > NODE_CUSTOM) {
        KryPropertyTable *t = &g_property_tables[n->kind];

        if(index == 0) {
            n->local.position = value.as.vector2_value;
            n->flags |= NODE_FLAG_DIRTY;
            return 1;
        }
        if(index == 1) {
            n->local.rotation = value.as.float_value;
            n->flags |= NODE_FLAG_DIRTY;
            return 1;
        }
        if(index == 2) {
            n->local.scale = value.as.vector2_value;
            n->flags |= NODE_FLAG_DIRTY;
            return 1;
        }
        if(t->set != NULL) {
            if(t->set(scene, node, index, value)) {
                n->flags |= NODE_FLAG_DIRTY;
                return 1;
            }
        }
        return 0;
    }

    switch(n->kind) {
    case NODE_NODE2D:
    case NODE_CAMERA2D:
    case NODE_SPRITE2D:
        if(index == 0) {
            n->local.position = value.as.vector2_value;
            n->flags |= NODE_FLAG_DIRTY;
            return 1;
        }
        if(index == 1) {
            n->local.rotation = value.as.float_value;
            n->flags |= NODE_FLAG_DIRTY;
            return 1;
        }
        if(index == 2) {
            n->local.scale = value.as.vector2_value;
            n->flags |= NODE_FLAG_DIRTY;
            return 1;
        }
        break;
    default:
        break;
    }

    if(n->kind == NODE_CAMERA2D && n->props != NULL) {
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

    if(n->kind == NODE_SPRITE2D && n->props != NULL) {
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

PropertyValue
SceneNodeGetPropertyByName(Scene *scene, NodeId node,
                              const char *property_id)
{
    Node *n = NodeGet(scene, node);
    int index;
    if(n == NULL) {
        PropertyValue out = {0};
        return out;
    }
    index = property_index_by_id(n->kind, property_id);
    if(index < 0) {
        PropertyValue out = {0};
        return out;
    }
    return SceneNodeGetProperty(scene, node, index);
}

int
SceneNodeSetPropertyByName(Scene *scene, NodeId node,
                              const char *property_id, PropertyValue value)
{
    Node *n = NodeGet(scene, node);
    int index;
    if(n == NULL)
        return 0;
    index = property_index_by_id(n->kind, property_id);
    if(index < 0)
        return 0;
    return SceneNodeSetProperty(scene, node, index, value);
}

/* ---- built-in property spec tables ---- */

static const PropertySpec kry_node2d_props[] = {
    {"position", "Position", "Transform", PROPERTY_VECTOR2, 0, 0, 0, 1, 0, {0}},
    {"rotation", "Rotation", "Transform", PROPERTY_FLOAT, 0, -6.2832f, 6.2832f, 0.01f, 0, {0}},
    {"scale", "Scale", "Transform", PROPERTY_VECTOR2, 0, 0, 0, 1, 0, {0}},
};

static const PropertySpec kry_camera2d_props[] = {
    {"position", "Position", "Transform", PROPERTY_VECTOR2, 0, 0, 0, 1, 0, {0}},
    {"rotation", "Rotation", "Transform", PROPERTY_FLOAT, 0, -6.2832f, 6.2832f, 0.01f, 0, {0}},
    {"scale", "Scale", "Transform", PROPERTY_VECTOR2, 0, 0, 0, 1, 0, {0}},
    {"zoom", "Zoom", "Camera", PROPERTY_FLOAT, 0, 0.1f, 16.0f, 0.1f, 0, {0}},
    {"active", "Active", "Camera", PROPERTY_BOOL, 0, 0, 0, 1, 0, {0}},
};

static const PropertySpec kry_sprite2d_props[] = {
    {"position", "Position", "Transform", PROPERTY_VECTOR2, 0, 0, 0, 1, 0, {0}},
    {"rotation", "Rotation", "Transform", PROPERTY_FLOAT, 0, -6.2832f, 6.2832f, 0.01f, 0, {0}},
    {"scale", "Scale", "Transform", PROPERTY_VECTOR2, 0, 0, 0, 1, 0, {0}},
    {"asset_path", "Asset", "Sprite", PROPERTY_ASSET_PATH, 0, 0, 0, 1, 0, {0}},
    {"size", "Size", "Sprite", PROPERTY_VECTOR2, 0, 0, 0, 1, 0, {0}},
    {"tint", "Tint", "Sprite", PROPERTY_COLOR, 0, 0, 0, 1, 0, {0}},
};

void
SceneRegisterBuiltinProperties(void)
{
    SceneRegisterProperties(NODE_NODE2D, kry_node2d_props,
                               (int)(sizeof(kry_node2d_props) / sizeof(kry_node2d_props[0])));
    SceneRegisterProperties(NODE_CAMERA2D, kry_camera2d_props,
                               (int)(sizeof(kry_camera2d_props) / sizeof(kry_camera2d_props[0])));
    SceneRegisterProperties(NODE_SPRITE2D, kry_sprite2d_props,
                               (int)(sizeof(kry_sprite2d_props) / sizeof(kry_sprite2d_props[0])));
}
