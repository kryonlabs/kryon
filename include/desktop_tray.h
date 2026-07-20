#ifndef KRYON_DESKTOP_TRAY_H
#define KRYON_DESKTOP_TRAY_H

typedef enum DesktopTrayMenuItemKind {
    DESKTOP_TRAY_MENU_ITEM_ACTION = 0,
    DESKTOP_TRAY_MENU_ITEM_SUBMENU,
    DESKTOP_TRAY_MENU_ITEM_SEPARATOR
} DesktopTrayMenuItemKind;

typedef struct DesktopTrayMenuItem DesktopTrayMenuItem;

struct DesktopTrayMenuItem {
    DesktopTrayMenuItemKind kind;
    const char *label;
    int action;
    int enabled;
    const DesktopTrayMenuItem *children;
    int child_count;
};

typedef struct DesktopTraySpec {
    const char *id;
    const char *title;
    const char *icon_name;
    const char *const *icon_paths;
    int close_action;
    int activate_action;
    const DesktopTrayMenuItem *menu_items;
    int menu_item_count;
} DesktopTraySpec;

int InitDesktopTray(const DesktopTraySpec *spec);
void ShutdownDesktopTray(void);
int PollDesktopTrayAction(void);
void SetDesktopTrayStatus(const char *text);
void SetDesktopTrayMenu(const DesktopTrayMenuItem *items, int count);
void SetDesktopTrayActivateAction(int action);

#endif
