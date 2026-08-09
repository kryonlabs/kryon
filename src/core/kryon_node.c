#include "kryon_node.h"

#include <stdio.h>
#include <string.h>

const char *
KryonPropertyKindName(KryonPropertyKind kind)
{
    switch(kind) {
    case KRYON_PROPERTY_BOOL: return "bool";
    case KRYON_PROPERTY_INT: return "int";
    case KRYON_PROPERTY_FLOAT: return "float";
    case KRYON_PROPERTY_STRING: return "string";
    case KRYON_PROPERTY_COLOR: return "color";
    case KRYON_PROPERTY_ENUM: return "enum";
    case KRYON_PROPERTY_VECTOR2: return "vector2";
    case KRYON_PROPERTY_RECTANGLE: return "rectangle";
    case KRYON_PROPERTY_ASSET_PATH: return "asset_path";
    }
    return "unknown";
}

KryonPropertyValue
KryonPropertyBool(int value)
{
    KryonPropertyValue prop = {0};
    prop.kind = KRYON_PROPERTY_BOOL;
    prop.as.bool_value = value != 0;
    return prop;
}

KryonPropertyValue
KryonPropertyInt(int value)
{
    KryonPropertyValue prop = {0};
    prop.kind = KRYON_PROPERTY_INT;
    prop.as.int_value = value;
    return prop;
}

KryonPropertyValue
KryonPropertyFloat(float value)
{
    KryonPropertyValue prop = {0};
    prop.kind = KRYON_PROPERTY_FLOAT;
    prop.as.float_value = value;
    return prop;
}

KryonPropertyValue
KryonPropertyString(const char *value)
{
    KryonPropertyValue prop = {0};
    prop.kind = KRYON_PROPERTY_STRING;
    snprintf(prop.as.string_value, sizeof(prop.as.string_value), "%s",
             value != NULL ? value : "");
    return prop;
}

KryonPropertyValue
KryonPropertyColor(Color value)
{
    KryonPropertyValue prop = {0};
    prop.kind = KRYON_PROPERTY_COLOR;
    prop.as.color_value = value;
    return prop;
}

KryonPropertyValue
KryonPropertyEnum(int index)
{
    KryonPropertyValue prop = {0};
    prop.kind = KRYON_PROPERTY_ENUM;
    prop.as.enum_index = index;
    return prop;
}

KryonPropertyValue
KryonPropertyVector2(Vector2 value)
{
    KryonPropertyValue prop = {0};
    prop.kind = KRYON_PROPERTY_VECTOR2;
    prop.as.vector2_value = value;
    return prop;
}

KryonPropertyValue
KryonPropertyRectangle(Rectangle value)
{
    KryonPropertyValue prop = {0};
    prop.kind = KRYON_PROPERTY_RECTANGLE;
    prop.as.rectangle_value = value;
    return prop;
}

const char *
KryonNodeKindName(KryonNodeKind kind)
{
    switch(kind) {
    case KRYON_NODE_KIND_UI: return "ui";
    case KRYON_NODE_KIND_2D: return "2d";
    case KRYON_NODE_KIND_DATA: return "data";
    case KRYON_NODE_KIND_GROUP: return "group";
    case KRYON_NODE_KIND_UNKNOWN: break;
    }
    return "unknown";
}

void
KryonNodeInit(KryonNode *node, const char *id, const char *type,
              KryonNodeKind kind)
{
    if(node == NULL)
        return;
    memset(node, 0, sizeof(*node));
    snprintf(node->id, sizeof(node->id), "%s", id != NULL ? id : "");
    snprintf(node->name, sizeof(node->name), "%s",
             type != NULL ? type : "Node");
    snprintf(node->type, sizeof(node->type), "%s",
             type != NULL ? type : "Node");
    node->kind = kind;
    node->flags = KRYON_NODE_FLAG_VISIBLE;
    node->scale = (Vector2){1.0f, 1.0f};
}
