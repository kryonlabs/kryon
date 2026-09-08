#ifndef KRYON_TAB_BAR_STORE_H
#define KRYON_TAB_BAR_STORE_H

typedef struct TabBarStore TabBarStore;

TabBarStore *tab_bar_store_new(void);
void tab_bar_store_free(TabBarStore *store);
TabBarStore *tab_bar_store_swap(TabBarStore *store);
TabBarStore *tab_bar_store_current(void);

#endif
