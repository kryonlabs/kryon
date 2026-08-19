#ifndef UI_NAV_H
#define UI_NAV_H

#include "kryon_compat.generated.h"
#include "ui_controls.h"
#include "ui_icon_types.h"

typedef struct {
    int id;
    int x;
    int y;
    int icon_size;
    int icon_padding;
    Texture2D icon;
    int *open;
    int *value;
    int min;
    int max;
    int popup_width;
    int popup_height;
} IconSliderPopupProps;

typedef struct {
    Texture2D icon;
    int disabled;
} UIIconRowItem;

typedef struct {
    int center_x;
    int view_width;
    int view_height;
    int count;
    const UIIconRowItem *items;
    int icon_size;
    int icon_padding;
    int gap;
    int side_margin;
    int bottom_margin;
    int max_button_width;
    int min_icon_size;
    int min_icon_padding;
    int min_gap;
} BottomIconRowProps;

typedef struct {
    int clicked_index;
    int y;
    int button_width;
} UIIconRowResult;

typedef struct {
    int route;
    const char *label;
    Texture2D icon;
    int active;
    int disabled;
} UIBottomNavItem;

typedef struct {
    int view_width;
    int view_height;
    int count;
    const UIBottomNavItem *items;
    int height;
    int icon_size;
    int icon_padding;
    int side_margin;
    int bottom_margin;
    int max_button_width;
} BottomNavProps;

typedef struct {
    int clicked_index;
    int clicked_route;
    int y;
    int height;
} UIBottomNavResult;

typedef struct {
    int route;
    const char *label;
    Texture2D icon;
} UIBottomNavOption;

typedef struct {
    int id;
    const char *title;
    int *routes;
    int *route_count;
    int max_route_count;
    const char **slot_labels;
    const UIBottomNavOption *options;
    int option_count;
    const char *add_label;
    const char *cancel_label;
    const char *save_label;
    const char *reset_label;
    Texture2D close_icon;
} BottomNavConfigProps;

typedef struct {
    int action;
    int changed;
} UIBottomNavConfigResult;

typedef struct {
    Texture2D icon;
    int disabled;
} UIToolbarAction;

typedef struct {
    int id;
    int x;
    int y;
    int width;
    int height;
    int draw_menu;
    const char **options;
    int option_count;
    int *selected_index;
    int dropdown_min_width;
    int dropdown_max_width;
    int dropdown_height;
    const UIToolbarAction *actions;
    int action_count;
    int action_icon_size;
    int action_icon_padding;
    int action_gap;
    int side_padding;
} ToolbarProps;

typedef struct {
    int selected_menu_item;
    int clicked_action;
} UIToolbarResult;

typedef struct {
    ToolbarProps toolbar;
    Texture2D leading_icon;
    int leading_width;
    int leading_icon_size;
    int leading_icon_padding;
} ToolbarHeaderProps;

typedef struct {
    UIToolbarResult toolbar;
    int leading_clicked;
} UIToolbarHeaderResult;

typedef struct {
    Texture2D icon;
    int disabled;
} UITopNavAction;

typedef struct {
    int id;
    int x;
    int y;
    int width;
    int height;
    const char *title;
    const char **options;
    int option_count;
    int *selected_index;
    int disabled;
    int dropdown_min_width;
    int dropdown_height;
    const UITopNavAction *actions;
    int action_count;
    int action_icon_size;
    int action_icon_padding;
    int action_gap;
    int side_padding;
} TopNavProps;

typedef struct {
    int selected_menu_item;
    int clicked_action;
} UITopNavResult;

typedef struct {
    const char *label;
    Texture2D icon;
    int icon_size;
    int disabled;
    Color accent;
} UISubtab;

typedef struct {
    Rectangle bounds;
    const UISubtab *tabs;
    int count;
    int selected_index;
    int font;
} SubtabBarProps;

typedef struct {
    const char *label;
    Texture2D icon;
    int icon_size;
    int disabled;
    Color accent;
    int italic;
    int closeable;
} UITab;

typedef struct {
    Rectangle bounds;
    const UITab *tabs;
    int count;
    int selected_index;
    int font;
    int min_tab_width;
    int max_tab_width;
    int *scroll_offset;
    int focus_selected;
    int *closed_index;
    int *double_clicked_index;
} TabBarProps;

typedef enum {
    UI_PANE_DROP_NONE,
    UI_PANE_DROP_CENTER,
    UI_PANE_DROP_LEFT,
    UI_PANE_DROP_RIGHT,
    UI_PANE_DROP_TOP,
    UI_PANE_DROP_BOTTOM
} UIPaneDropZone;

typedef struct {
    Rectangle bounds;
    const UITab *tabs;
    int count;
    int selected_index;
    int font;
    int min_tab_width;
    int max_tab_width;
    int *scroll_offset;
    int *dragged_index;
} UIPaneTabBar;

typedef struct {
    int clicked_index;
    int dragged_index;
} UIPaneTabBarResult;

int DrawUITabBar(TabBarProps bar);
UIPaneTabBarResult DrawUIPaneTabBar(UIPaneTabBar bar);
UIPaneDropZone GetUIPaneDropZone(Rectangle bounds, Vector2 mouse);
void DrawUIPaneDropPreview(Rectangle bounds, UIPaneDropZone zone);
int GetUITabBarHeight(void);
int TabBarHeight(void);

#endif
