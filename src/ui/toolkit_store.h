#ifndef KRYON_TOOLKIT_STORE_H
#define KRYON_TOOLKIT_STORE_H

/* Private retained state shared by native toolkit widgets. Each render host
 * owns one store, so equal widget IDs cannot collide across windows. */
#include <stddef.h>
#include <stdint.h>

typedef struct ToolkitStore ToolkitStore;

ToolkitStore *toolkit_store_new(void);
void toolkit_store_free(ToolkitStore *store);
ToolkitStore *toolkit_store_swap(ToolkitStore *store);
ToolkitStore *toolkit_store_current(void);
void toolkit_store_frame(ToolkitStore *store);
/* Borrowed until a later frame sweep or store destruction. Live entries are
 * stable across insertions and are never displaced by other widget IDs. */
/* Type names have static lifetime and identify a declaration's state shape.
 * Equal keys in different types or render hosts refer to different instances. */
void *toolkit_instance(const char *type, uint64_t key, size_t size);
#define instance_state(type, key) ((type *)toolkit_instance(#type, (key), sizeof(type)))

#endif
