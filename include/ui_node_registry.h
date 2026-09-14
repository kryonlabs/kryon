#ifndef NODE_REGISTRY_H
#define NODE_REGISTRY_H

#include "ui_node_registry_props.generated.h"

typedef struct NodeType {
    const char *name;
    const char *label;
    const char *group;
    const char *base;
    const char *detail;
    NodeTypeFlag flags;
} NodeType;

int NodeTypeCount(void);
const NodeType *NodeTypeAt(int index);
const char *NodeTypeName(int index);
const char *NodeTypeLabel(int index);
const char *NodeTypeGroup(int index);
const char *NodeTypeBase(int index);
const char *NodeTypeDetail(int index);
NodeTypeFlag NodeTypeFlagsAt(int index);
int NodeTypeInsertable(int index);
int NodeTypeSnippet(int index, int x, int y, char *dst, int cap);

#endif /* NODE_REGISTRY_H */
