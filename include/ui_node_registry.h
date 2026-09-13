#ifndef NODE_REGISTRY_H
#define NODE_REGISTRY_H

typedef enum NodeTypeFlags {
    NODE_INSERTABLE = 1 << 0,
    NODE_SELECTABLE = 1 << 1,
    NODE_MOVABLE = 1 << 2,
    NODE_RESIZABLE = 1 << 3
} NodeTypeFlags;

typedef struct NodeType {
    const char *name;
    const char *label;
    const char *group;
    const char *base;
    const char *detail;
    unsigned flags;
} NodeType;

int NodeTypeCount(void);
const NodeType *NodeTypeAt(int index);
const char *NodeTypeName(int index);
const char *NodeTypeLabel(int index);
const char *NodeTypeGroup(int index);
const char *NodeTypeBase(int index);
const char *NodeTypeDetail(int index);
unsigned NodeTypeFlagsAt(int index);
int NodeTypeInsertable(int index);
int NodeTypeSnippet(int index, int x, int y, char *dst, int cap);

#endif /* NODE_REGISTRY_H */
