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
} IconRowItem;

typedef struct {
    int center_x;
    int view_width;
    int view_height;
    int count;
    const IconRowItem *items;
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
} IconRowResult;

typedef struct {
    int route;
    const char *label;
    Texture2D icon;
    int active;
    int disabled;
} BottomNavItem;

typedef struct {
    int view_width;
    int view_height;
    int count;
    const BottomNavItem *items;
    int height;
    int icon_size;
    int icon_padding;
    int side_margin;
    int bottom_margin;
    int max_button_width;
    Color icon_color; /* Transparent keeps the original texture colors. */
} BottomNavProps;

typedef struct {
    int clicked_index;
    int clicked_route;
    int y;
    int height;
} BottomNavResult;

typedef struct {
    int route;
    const char *label;
    Texture2D icon;
} BottomNavOption;

typedef struct {
    int id;
    const char *title;
    int *routes;
    int *route_count;
    int max_route_count;
    const char **slot_labels;
    const BottomNavOption *options;
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
} BottomNavConfigResult;

typedef struct {
    Texture2D icon;
    int disabled;
} ToolbarAction;

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
    const ToolbarAction *actions;
    int action_count;
    int action_icon_size;
    int action_icon_padding;
    int action_gap;
    int side_padding;
} ToolbarProps;

typedef struct {
    int selected_menu_item;
    int clicked_action;
} ToolbarResult;

typedef struct {
    ToolbarProps toolbar;
    Texture2D leading_icon;
    int leading_width;
    int leading_icon_size;
    int leading_icon_padding;
} ToolbarHeaderProps;

typedef struct {
    ToolbarResult toolbar;
    int leading_clicked;
} ToolbarHeaderResult;

typedef struct {
    Texture2D icon;
    int disabled;
} TopNavAction;

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
    const TopNavAction *actions;
    int action_count;
    int action_icon_size;
    int action_icon_padding;
    int action_gap;
    int side_padding;
} TopNavProps;

typedef struct {
    int selected_menu_item;
    int clicked_action;
} TopNavResult;

typedef struct {
    const char *label;
    Texture2D icon;
    int icon_size;
    int disabled;
    Color accent;
} Subtab;

typedef struct {
    Rectangle bounds;
    const Subtab *tabs;
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
} Tab;

typedef struct {
    Rectangle bounds;
    const Tab *tabs;
    int count;
    int selected_index;
    int font;
    int min_tab_width;
    int max_tab_width;
    int *scroll_offset;
    int focus_selected;
    int *closed_index;
    int *double_clicked_index;
    int *reordered_from_index;
    int *reordered_to_index;
    Rectangle *selected_tab_bounds;
    int *middle_clicked_index;
} TabBarProps;

typedef enum {
    PaneDropNone,
    PaneDropCenter,
    PaneDropLeft,
    PaneDropRight,
    PaneDropTop,
    PaneDropBottom
} PaneDropZone;

typedef struct {
    Rectangle bounds;
    const Tab *tabs;
    int count;
    int selected_index;
    int font;
    int min_tab_width;
    int max_tab_width;
    int *scroll_offset;
    int *dragged_index;
} PaneTabBar;

typedef struct {
    int clicked_index;
    int dragged_index;
} PaneTabBarResult;

PaneDropZone GetPaneDropZone(Rectangle bounds, Vector2 mouse);
int GetTabBarHeight(void);
int TabBarHeight(void);

#endif
