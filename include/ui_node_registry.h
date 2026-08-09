#ifndef UI_NODE_REGISTRY_H
#define UI_NODE_REGISTRY_H

typedef enum KryonNodeTypeFlags {
    KRYON_NODE_INSERTABLE = 1 << 0,
    KRYON_NODE_SELECTABLE = 1 << 1,
    KRYON_NODE_MOVABLE = 1 << 2,
    KRYON_NODE_RESIZABLE = 1 << 3
} KryonNodeTypeFlags;

typedef struct KryonNodeType {
    const char *name;
    const char *label;
    const char *group;
    const char *base;
    const char *detail;
    unsigned flags;
} KryonNodeType;

int KryonNodeTypeCount(void);
const KryonNodeType *KryonNodeTypeAt(int index);
const char *KryonNodeTypeName(int index);
const char *KryonNodeTypeLabel(int index);
const char *KryonNodeTypeGroup(int index);
const char *KryonNodeTypeBase(int index);
const char *KryonNodeTypeDetail(int index);
unsigned KryonNodeTypeFlagsAt(int index);
int KryonNodeTypeInsertable(int index);
int KryonNodeTypeSnippet(int index, int x, int y, char *dst, int cap);

#endif /* UI_NODE_REGISTRY_H */
