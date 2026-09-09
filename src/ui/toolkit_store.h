#ifndef KRYON_TOOLKIT_STORE_H
#define KRYON_TOOLKIT_STORE_H

/* Private retained state shared by native toolkit widgets. Each render host
 * owns one store, so equal widget IDs cannot collide across windows. */
#include "runtime/surface.h"

typedef struct ToolkitStore ToolkitStore;

ToolkitStore *toolkit_store_new(void);
void toolkit_store_free(ToolkitStore *store);
ToolkitStore *toolkit_store_swap(ToolkitStore *store);
ToolkitStore *toolkit_store_current(void);
void toolkit_store_frame(ToolkitStore *store);
/* Borrowed until a later frame sweep or store destruction. Live entries are
 * stable across insertions and are never displaced by other widget IDs. */
InteractionMotion *toolkit_button_motion(unsigned int key);

#endif
