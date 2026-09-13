#ifndef KRYON_MENU_TYPES_H
#define KRYON_MENU_TYPES_H

#include "kryon_compat.generated.h"

typedef enum {
    MenuCommand,
    MenuCheck,
    MenuRadio,
    MenuSeparator,
    MenuSubmenu
} MenuItemKind;

typedef struct MenuItem {
    MenuItemKind kind;
    const char *label;
    const char *accelerator;
    int id;
    int disabled;
    int checked;
    const struct MenuItem *submenu;
    int submenu_count;
} MenuItem;

typedef struct {
    Rectangle bounds;
    const char *label;
    const MenuItem *items;
    int item_count;
} MenuGroup;

typedef enum {
    MenuModeBar = 0,
    MenuModePopup = 1,
    MenuModeContext = 2
} MenuMode;

typedef struct {
    int activated_id;
    int open_index;
} MenuResult;

#endif
