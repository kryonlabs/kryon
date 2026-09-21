#ifndef KRYON_NODE_H
#define KRYON_NODE_H

#include "kryon_property.h"
#include "ui_node_props.generated.h"

typedef struct KryonNodeEdit {
    const char *node_id;
    const char *property_id;
    PropertyValue value;
} KryonNodeEdit;

const char *KryonNodeKindName(KryonNodeKind kind);
void KryonNodeInit(KryonNode *node, const char *id, const char *type,
                   KryonNodeKind kind);

#endif /* KRYON_NODE_H */
