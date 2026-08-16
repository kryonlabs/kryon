#include "kryon_node.h"

#include <stdio.h>
#include <string.h>

const char *
PropertyKindName(PropertyKind kind)
{
    switch(kind) {
    case PROPERTY_BOOL: return "bool";
    case PROPERTY_INT: return "int";
    case PROPERTY_FLOAT: return "float";
    case PROPERTY_STRING: return "string";
    case PROPERTY_COLOR: return "color";
    case PROPERTY_ENUM: return "enum";
    case PROPERTY_VECTOR2: return "vector2";
    case PROPERTY_RECTANGLE: return "rectangle";
    case PROPERTY_ASSET_PATH: return "asset_path";
    }
    return "unknown";
}

PropertyValue
PropertyBool(int value)
{
    PropertyValue prop = {0};
    prop.kind = PROPERTY_BOOL;
    prop.as.bool_value = value != 0;
    return prop;
}

PropertyValue
PropertyInt(int value)
{
    PropertyValue prop = {0};
    prop.kind = PROPERTY_INT;
    prop.as.int_value = value;
    return prop;
}

PropertyValue
PropertyFloat(float value)
{
    PropertyValue prop = {0};
    prop.kind = PROPERTY_FLOAT;
    prop.as.float_value = value;
    return prop;
}

PropertyValue
PropertyString(const char *value)
{
    PropertyValue prop = {0};
    prop.kind = PROPERTY_STRING;
    snprintf(prop.as.string_value, sizeof(prop.as.string_value), "%s",
             value != NULL ? value : "");
    return prop;
}

PropertyValue
PropertyColor(Color value)
{
    PropertyValue prop = {0};
    prop.kind = PROPERTY_COLOR;
    prop.as.color_value = value;
    return prop;
}

PropertyValue
PropertyEnum(int index)
{
    PropertyValue prop = {0};
    prop.kind = PROPERTY_ENUM;
    prop.as.enum_index = index;
    return prop;
}

PropertyValue
PropertyVector2(Vector2 value)
{
    PropertyValue prop = {0};
    prop.kind = PROPERTY_VECTOR2;
    prop.as.vector2_value = value;
    return prop;
}

PropertyValue
PropertyRectangle(Rectangle value)
{
    PropertyValue prop = {0};
    prop.kind = PROPERTY_RECTANGLE;
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
