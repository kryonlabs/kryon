#ifndef KRYON_NODE_H
#define KRYON_NODE_H

#include "kryon_property.h"

#define KRYON_NODE_ID_MAX 96
#define KRYON_NODE_NAME_MAX 128
#define KRYON_NODE_TYPE_MAX 64
#define KRYON_NODE_PATH_MAX 512

typedef enum KryonNodeKind {
    KRYON_NODE_KIND_UNKNOWN,
    KRYON_NODE_KIND_UI,
    KRYON_NODE_KIND_2D,
    KRYON_NODE_KIND_DATA,
    KRYON_NODE_KIND_GROUP
} KryonNodeKind;

typedef enum KryonNodeFlags {
    KRYON_NODE_FLAG_SELECTABLE = 1 << 0,
    KRYON_NODE_FLAG_MOVABLE = 1 << 1,
    KRYON_NODE_FLAG_RESIZABLE = 1 << 2,
    KRYON_NODE_FLAG_DELETABLE = 1 << 3,
    KRYON_NODE_FLAG_INSERTABLE = 1 << 4,
    KRYON_NODE_FLAG_READONLY = 1 << 5,
    KRYON_NODE_FLAG_VISIBLE = 1 << 6,
    KRYON_NODE_FLAG_LOCKED = 1 << 7
} KryonNodeFlags;

typedef struct KryonNode {
    char id[KRYON_NODE_ID_MAX];
    char name[KRYON_NODE_NAME_MAX];
    char type[KRYON_NODE_TYPE_MAX];
    char base_type[KRYON_NODE_TYPE_MAX];
    char parent_id[KRYON_NODE_ID_MAX];
    char source_path[KRYON_NODE_PATH_MAX];
    KryonNodeKind kind;
    unsigned flags;
    Rectangle bounds;
    Vector2 position;
    Vector2 scale;
    Vector2 origin;
    float rotation;
    int layer;
    int order;
    int source_line;
    int source_start;
    int source_end;
} KryonNode;

typedef struct KryonNodeEdit {
    const char *node_id;
    const char *property_id;
    KryonPropertyValue value;
} KryonNodeEdit;

const char *KryonNodeKindName(KryonNodeKind kind);
void KryonNodeInit(KryonNode *node, const char *id, const char *type,
                   KryonNodeKind kind);

#endif /* KRYON_NODE_H */
