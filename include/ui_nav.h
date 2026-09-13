#ifndef KRYON_NAV_H
#define KRYON_NAV_H

#include "kryon_compat.generated.h"
#include "ui_controls.h"
#include "ui_icon_types.h"
#include "ui_navigation_bar_props.generated.h"
#include "ui_tab_bar_props.generated.h"
#include "ui_toolbar_props.generated.h"

typedef enum {
    PaneDropNone,
    PaneDropCenter,
    PaneDropLeft,
    PaneDropRight,
    PaneDropTop,
    PaneDropBottom
} PaneDropZone;

PaneDropZone GetPaneDropZone(Rectangle bounds, Vector2 mouse);
int GetTabBarHeight(void);
int TabBarHeight(void);

#endif
