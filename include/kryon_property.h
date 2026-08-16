#ifndef KRYON_PROPERTY_H
#define KRYON_PROPERTY_H

#include "kryon_compat.generated.h"

#define PROPERTY_ID_MAX 64
#define PROPERTY_LABEL_MAX 96
#define PROPERTY_VALUE_MAX 512
#define PROPERTY_ENUM_MAX 16

typedef enum PropertyKind {
    PROPERTY_BOOL,
    PROPERTY_INT,
    PROPERTY_FLOAT,
    PROPERTY_STRING,
    PROPERTY_COLOR,
    PROPERTY_ENUM,
    PROPERTY_VECTOR2,
    PROPERTY_RECTANGLE,
    PROPERTY_ASSET_PATH
} PropertyKind;

typedef enum PropertyFlags {
    PROPERTY_READONLY = 1 << 0,
    PROPERTY_ADVANCED = 1 << 1
} PropertyFlags;

typedef struct PropertySpec {
    char id[PROPERTY_ID_MAX];
    char label[PROPERTY_LABEL_MAX];
    char section[PROPERTY_LABEL_MAX];
    PropertyKind kind;
    unsigned flags;
    float min_value;
    float max_value;
    float step;
    int enum_count;
    const char *enum_values[PROPERTY_ENUM_MAX];
} PropertySpec;

typedef struct PropertyValue {
    PropertyKind kind;
    union {
        int bool_value;
        int int_value;
        float float_value;
        char string_value[PROPERTY_VALUE_MAX];
        Color color_value;
        int enum_index;
        Vector2 vector2_value;
        Rectangle rectangle_value;
    } as;
} PropertyValue;

const char *PropertyKindName(PropertyKind kind);
PropertyValue PropertyBool(int value);
PropertyValue PropertyInt(int value);
PropertyValue PropertyFloat(float value);
PropertyValue PropertyString(const char *value);
PropertyValue PropertyColor(Color value);
PropertyValue PropertyEnum(int index);
PropertyValue PropertyVector2(Vector2 value);
PropertyValue PropertyRectangle(Rectangle value);

#endif /* KRYON_PROPERTY_H */
