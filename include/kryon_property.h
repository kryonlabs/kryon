#ifndef KRYON_PROPERTY_H
#define KRYON_PROPERTY_H

#include "kryon_compat.generated.h"

#define KRYON_PROPERTY_ID_MAX 64
#define KRYON_PROPERTY_LABEL_MAX 96
#define KRYON_PROPERTY_VALUE_MAX 512
#define KRYON_PROPERTY_ENUM_MAX 16

typedef enum KryonPropertyKind {
    KRYON_PROPERTY_BOOL,
    KRYON_PROPERTY_INT,
    KRYON_PROPERTY_FLOAT,
    KRYON_PROPERTY_STRING,
    KRYON_PROPERTY_COLOR,
    KRYON_PROPERTY_ENUM,
    KRYON_PROPERTY_VECTOR2,
    KRYON_PROPERTY_RECTANGLE,
    KRYON_PROPERTY_ASSET_PATH
} KryonPropertyKind;

typedef enum KryonPropertyFlags {
    KRYON_PROPERTY_READONLY = 1 << 0,
    KRYON_PROPERTY_ADVANCED = 1 << 1
} KryonPropertyFlags;

typedef struct KryonPropertySpec {
    char id[KRYON_PROPERTY_ID_MAX];
    char label[KRYON_PROPERTY_LABEL_MAX];
    char section[KRYON_PROPERTY_LABEL_MAX];
    KryonPropertyKind kind;
    unsigned flags;
    float min_value;
    float max_value;
    float step;
    int enum_count;
    const char *enum_values[KRYON_PROPERTY_ENUM_MAX];
} KryonPropertySpec;

typedef struct KryonPropertyValue {
    KryonPropertyKind kind;
    union {
        int bool_value;
        int int_value;
        float float_value;
        char string_value[KRYON_PROPERTY_VALUE_MAX];
        Color color_value;
        int enum_index;
        Vector2 vector2_value;
        Rectangle rectangle_value;
    } as;
} KryonPropertyValue;

const char *KryonPropertyKindName(KryonPropertyKind kind);
KryonPropertyValue KryonPropertyBool(int value);
KryonPropertyValue KryonPropertyInt(int value);
KryonPropertyValue KryonPropertyFloat(float value);
KryonPropertyValue KryonPropertyString(const char *value);
KryonPropertyValue KryonPropertyColor(Color value);
KryonPropertyValue KryonPropertyEnum(int index);
KryonPropertyValue KryonPropertyVector2(Vector2 value);
KryonPropertyValue KryonPropertyRectangle(Rectangle value);

#endif /* KRYON_PROPERTY_H */
