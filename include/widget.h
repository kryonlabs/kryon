/*
 * Kryon Widget Management
 * C89/C90 compliant
 */

#ifndef KRYON_WIDGET_H
#define KRYON_WIDGET_H

#include "kryon.h"

/*
 * Forward declarations
 */
struct KryonWindow;

/*
 * Widget type enumeration
 */
typedef enum {
    WIDGET_UNKNOWN = 0,

    /* Input widgets (24) */
    WIDGET_TEXT_INPUT,
    WIDGET_PASSWORD_INPUT,
    WIDGET_TEXT_AREA,
    WIDGET_SEARCH_INPUT,
    WIDGET_NUMBER_INPUT,
    WIDGET_EMAIL_INPUT,
    WIDGET_URL_INPUT,
    WIDGET_TEL_INPUT,
    WIDGET_CHECKBOX,
    WIDGET_RADIO_BUTTON,
    WIDGET_DROPDOWN,
    WIDGET_MULTI_SELECT,
    WIDGET_LIST_BOX,
    WIDGET_SWITCH,
    WIDGET_SLIDER,
    WIDGET_RANGE_SLIDER,
    WIDGET_RATING,
    WIDGET_SPINBOX,
    WIDGET_DATE_INPUT,
    WIDGET_TIME_INPUT,
    WIDGET_DATE_TIME_INPUT,
    WIDGET_MONTH_INPUT,
    WIDGET_WEEK_INPUT,
    WIDGET_FILE_INPUT,

    /* Buttons (8) */
    WIDGET_BUTTON,
    WIDGET_SUBMIT_BUTTON,
    WIDGET_RESET_BUTTON,
    WIDGET_ICON_BUTTON,
    WIDGET_FAB_BUTTON,
    WIDGET_DROPDOWN_BUTTON,
    WIDGET_SEGMENTED_CONTROL,

    /* Display (11) */
    WIDGET_LABEL,
    WIDGET_HEADING,
    WIDGET_PARAGRAPH,
    WIDGET_RICH_TEXT,
    WIDGET_CODE,
    WIDGET_IMAGE,
    WIDGET_PROGRESS_BAR,
    WIDGET_ACTIVITY_INDICATOR,
    WIDGET_BADGE,
    WIDGET_DIVIDER,
    WIDGET_TOOLTIP,

    /* Containers (11) */
    WIDGET_CONTAINER,
    WIDGET_BOX,
    WIDGET_FRAME,
    WIDGET_CARD,
    WIDGET_SCROLL_AREA,
    WIDGET_SPLIT_PANE,
    WIDGET_TAB_CONTAINER,
    WIDGET_DIALOG,
    WIDGET_MODAL,
    WIDGET_ACTION_SHEET,
    WIDGET_DRAWER,

    /* Lists (8) */
    WIDGET_LIST_ITEM,
    WIDGET_LIST_TILE,
    WIDGET_LIST_VIEW,
    WIDGET_GRID_VIEW,
    WIDGET_DATA_TABLE,
    WIDGET_TREE_VIEW,
    WIDGET_CHIP,
    WIDGET_TAG,

    /* Navigation (8) */
    WIDGET_MENU_BAR,
    WIDGET_TOOL_BAR,
    WIDGET_NAV_BAR,
    WIDGET_NAV_RAIL,
    WIDGET_BREADCRUMB,
    WIDGET_PAGINATION,
    WIDGET_STEPPER,
    WIDGET_PAGE_VIEW,

    /* Menus (4) */
    WIDGET_DROPDOWN_MENU,
    WIDGET_CONTEXT_MENU,
    WIDGET_CASCADING_MENU,
    WIDGET_POPUP_MENU,

    /* Forms (4) */
    WIDGET_FORM,
    WIDGET_FORM_GROUP,
    WIDGET_VALIDATION_MESSAGE,
    WIDGET_INPUT_GROUP,

    /* Windows (4) */
    WIDGET_WINDOW,
    WIDGET_DIALOG_WINDOW,
    WIDGET_PAGE_SCAFFOLD,
    WIDGET_FULLSCREEN,

    /* Specialized (8) */
    WIDGET_CANVAS,
    WIDGET_SVG,
    WIDGET_CHART,
    WIDGET_COLOR_PICKER,
    WIDGET_SIGNATURE_PAD,
    WIDGET_RESIZABLE,
    WIDGET_CALENDAR,
    WIDGET_TABLE_VIEW,

    /* Alias for Phase 1 compatibility */
    WIDGET_TOGGLE = WIDGET_SWITCH
} WidgetType;

/*
 * Property types
 */
typedef enum {
    PROP_TYPE_STRING,
    PROP_TYPE_INTEGER,
    PROP_TYPE_BOOLEAN,
    PROP_TYPE_RECT,
    PROP_TYPE_ENUM,
    PROP_TYPE_CALLBACK
} PropertyType;

/*
 * Widget state structure
 */
typedef struct KryonWidget {
    uint32_t id;                    /* Unique widget ID */
    WidgetType type;                /* Widget type */
    char *name;                     /* Widget name */
    struct KryonWindow *parent_window;  /* Parent window */
    struct KryonWidget *parent;     /* Parent widget (if container) */

    /* Common properties (mandatory) */
    char *prop_type;                /* Widget type as string */
    char *prop_rect;                /* "x y w h" */
    int prop_visible;               /* 0 or 1 */
    int prop_enabled;               /* 0 or 1 */

    /* Widget-specific properties */
    char *prop_text;                /* Label/caption */
    char *prop_value;               /* State/value */
    char *prop_placeholder;         /* Hint text */

    /* Internal state */
    P9Node *node;                   /* Corresponding tree node */
    void *internal_data;            /* Type-specific data */

    /* Layout properties (for containers) */
    char *layout_type;              /* row, column, grid, absolute */
    int layout_gap;
    int layout_padding;
    char *layout_align;             /* start, center, end, stretch */
    char *layout_justify;           /* start, center, end, space-between */

    /* Children (for container widgets) */
    struct KryonWidget **children;
    int nchildren;
    int child_capacity;
} KryonWidget;

/*
 * Widget registry
 */
int widget_registry_init(void);
void widget_registry_cleanup(void);

struct KryonWidget *widget_create(WidgetType type, const char *name,
                                   struct KryonWindow *parent_window);
void widget_destroy(struct KryonWidget *widget);
struct KryonWidget *widget_get(uint32_t id);

/*
 * Widget type utilities
 */
const char *widget_type_to_string(WidgetType type);
WidgetType widget_type_from_string(const char *str);

/*
 * File system integration
 */
int widget_create_fs_entries(struct KryonWidget *widget, P9Node *widgets_dir);

#endif /* KRYON_WIDGET_H */
