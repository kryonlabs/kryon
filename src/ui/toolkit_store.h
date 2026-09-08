#ifndef KRYON_TOOLKIT_STORE_H
#define KRYON_TOOLKIT_STORE_H

/* Private retained state shared by native toolkit widgets. Each render host
 * owns one store, so equal widget IDs cannot collide across windows. */
typedef struct ToolkitStore ToolkitStore;

ToolkitStore *toolkit_store_new(void);
void toolkit_store_free(ToolkitStore *store);
ToolkitStore *toolkit_store_swap(ToolkitStore *store);
ToolkitStore *toolkit_store_current(void);

#endif
