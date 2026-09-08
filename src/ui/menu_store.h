#ifndef KRYON_MENU_STORE_H
#define KRYON_MENU_STORE_H

/* Private retained state for menu widgets. Each render host owns one store,
 * so menu IDs and deferred overlays cannot cross window boundaries. */
typedef struct MenuStore MenuStore;

MenuStore *menu_store_new(void);
void menu_store_free(MenuStore *store);
MenuStore *menu_store_swap(MenuStore *store);
MenuStore *menu_store_current(void);

#endif
