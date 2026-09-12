#ifndef KRYON_DROPDOWN_STORE_H
#define KRYON_DROPDOWN_STORE_H

/* Private retained state for the canonical Dropdown implementation.
 * Each render host owns one store, so equal widget IDs in different windows
 * cannot collide. */
typedef struct DropdownStore DropdownStore;

DropdownStore *dropdown_store_new(void);
void dropdown_store_free(DropdownStore *store);
DropdownStore *dropdown_store_swap(DropdownStore *store);
DropdownStore *dropdown_store_current(void);
void dropdown_store_clip(int top, int bottom);

#endif
