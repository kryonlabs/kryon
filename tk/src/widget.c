/*
 * Widget Registry Implementation
 */

#include "widget.h"
#include "window.h"
#include "events.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

/*
 * snprintf prototype for C89 compatibility
 */
extern int snprintf(char *str, size_t size, const char *format, ...);

/*
 * External rendering functions
 */
extern void render_all(void);
extern void mark_dirty(struct KryonWindow *win);

/*
 * Forward declarations for property handlers
 */
static ssize_t prop_read_type(struct KryonWidget *w, char *buf, size_t count, uint64_t offset);
static ssize_t prop_read_name(struct KryonWidget *w, char *buf, size_t count, uint64_t offset);
static ssize_t prop_read_value(struct KryonWidget *w, char *buf, size_t count, uint64_t offset);
static ssize_t prop_write_value(struct KryonWidget *w, const char *buf, size_t count, uint64_t offset);
static ssize_t prop_read_text(struct KryonWidget *w, char *buf, size_t count, uint64_t offset);
static ssize_t prop_write_text(struct KryonWidget *w, const char *buf, size_t count, uint64_t offset);
static ssize_t prop_read_visible(struct KryonWidget *w, char *buf, size_t count, uint64_t offset);
static ssize_t prop_write_visible(struct KryonWidget *w, const char *buf, size_t count, uint64_t offset);
static ssize_t prop_read_enabled(struct KryonWidget *w, char *buf, size_t count, uint64_t offset);
static ssize_t prop_write_enabled(struct KryonWidget *w, const char *buf, size_t count, uint64_t offset);
static ssize_t prop_read_event(struct KryonWidget *w, char *buf, size_t count, uint64_t offset);
static ssize_t prop_read_bgcolor(struct KryonWidget *w, char *buf, size_t count, uint64_t offset);
static ssize_t prop_write_bgcolor(struct KryonWidget *w, const char *buf, size_t count, uint64_t offset);
static ssize_t prop_read_fgcolor(struct KryonWidget *w, char *buf, size_t count, uint64_t offset);
static ssize_t prop_write_fgcolor(struct KryonWidget *w, const char *buf, size_t count, uint64_t offset);

/*
 * Property file data structure
 */
typedef struct {
    struct KryonWidget *widget;
    ssize_t (*read_func)(struct KryonWidget *, char *, size_t, uint64_t);
    ssize_t (*write_func)(struct KryonWidget *, const char *, size_t, uint64_t);
} WidgetPropertyData;

/*
 * Global widget registry
 */
static struct KryonWidget **widgets = NULL;
static int nwidgets = 0;
static int widget_capacity = 0;
static uint32_t next_widget_id = 1;

/*
 * Initialize widget registry
 */
int widget_registry_init(void)
{
    if (widgets != NULL) {
        return -1;
    }

    widget_capacity = 256;
    widgets = (struct KryonWidget **)malloc(widget_capacity * sizeof(struct KryonWidget *));
    if (widgets == NULL) {
        return -1;
    }

    nwidgets = 0;
    return 0;
}

/*
 * Cleanup widget registry (destroys all widgets)
 */
void widget_registry_cleanup(void)
{
    int i;

    if (widgets == NULL) {
        return;
    }

    for (i = 0; i < nwidgets; i++) {
        widget_destroy(widgets[i]);
    }

    free(widgets);
    widgets = NULL;
    nwidgets = 0;
    widget_capacity = 0;
}

/*
 * Reset widget registry (frees array but does NOT destroy widgets)
 * Use this when widgets have already been destroyed elsewhere (e.g., by window_destroy)
 */
void widget_registry_reset(void)
{
    if (widgets == NULL) {
        return;
    }

    free(widgets);
    widgets = NULL;
    nwidgets = 0;
    widget_capacity = 0;
}

/*
 * Create a new widget
 */
struct KryonWidget *widget_create(WidgetType type, const char *name,
                                   struct KryonWindow *parent_window)
{
    struct KryonWidget *widget;
    char buf[64];

    /* Check capacity */
    if (nwidgets >= widget_capacity) {
        return NULL;
    }

    widget = (struct KryonWidget *)calloc(1, sizeof(struct KryonWidget));
    if (widget == NULL) {
        return NULL;
    }

    /* Initialize */
    widget->id = next_widget_id++;
    widget->type = type;
    widget->parent_window = parent_window;
    widget->parent = NULL;

    /* Set name */
    if (name != NULL) {
        widget->name = (char *)malloc(strlen(name) + 1);
        if (widget->name != NULL) {
            strcpy(widget->name, name);
        }
    }

    if (widget->name == NULL) {
        snprintf(buf, sizeof(buf), "widget_%u", widget->id);
        widget->name = (char *)malloc(strlen(buf) + 1);
        if (widget->name != NULL) {
            strcpy(widget->name, buf);
        }
    }

    /* Set default properties */
    widget->prop_visible = 1;
    widget->prop_enabled = 1;

    widget->prop_text = (char *)malloc(1);
    if (widget->prop_text != NULL) {
        widget->prop_text[0] = '\0';
    }

    widget->prop_value = (char *)malloc(2);
    if (widget->prop_value != NULL) {
        strcpy(widget->prop_value, "0");
    }

    widget->prop_placeholder = (char *)malloc(1);
    if (widget->prop_placeholder != NULL) {
        widget->prop_placeholder[0] = '\0';
    }

    widget->prop_color = (char *)malloc(1);
    if (widget->prop_color != NULL) {
        widget->prop_color[0] = '\0';
    }

    /* Initialize focus state */
    widget->prop_focused = 0;
    widget->prop_focusable = 0;

    /* Text inputs are focusable */
    switch (type) {
        case WIDGET_TEXT_INPUT:
        case WIDGET_PASSWORD_INPUT:
        case WIDGET_SEARCH_INPUT:
        case WIDGET_NUMBER_INPUT:
        case WIDGET_EMAIL_INPUT:
        case WIDGET_URL_INPUT:
        case WIDGET_TEL_INPUT:
        case WIDGET_TEXT_AREA:
            widget->prop_focusable = 1;
            break;
        default:
            widget->prop_focusable = 0;
            break;
    }

    widget->prop_rect = (char *)malloc(8);
    if (widget->prop_rect != NULL) {
        strcpy(widget->prop_rect, "0 0 0 0");
    }

    widget->layout_type = (char *)malloc(4);
    if (widget->layout_type != NULL) {
        strcpy(widget->layout_type, "row");
    }

    widget->layout_align = (char *)malloc(6);
    if (widget->layout_align != NULL) {
        strcpy(widget->layout_align, "start");
    }

    widget->layout_justify = (char *)malloc(6);
    if (widget->layout_justify != NULL) {
        strcpy(widget->layout_justify, "start");
    }

    widget->layout_gap = 0;
    widget->layout_padding = 0;

    /* Set type string */
    widget->prop_type = (char *)malloc(strlen(widget_type_to_string(type)) + 1);
    if (widget->prop_type != NULL) {
        strcpy(widget->prop_type, widget_type_to_string(type));
    }

    /* Allocate children array */
    widget->child_capacity = 64;
    widget->children = (struct KryonWidget **)malloc(widget->child_capacity * sizeof(struct KryonWidget *));
    widget->nchildren = 0;

    /* Add to registry */
    widgets[nwidgets++] = widget;

    return widget;
}

/*
 * Destroy a widget
 */
void widget_destroy(struct KryonWidget *widget)
{
    int i;

    if (widget == NULL) {
        return;
    }

    /* Destroy children */
    for (i = 0; i < widget->nchildren; i++) {
        widget_destroy(widget->children[i]);
    }

    if (widget->children != NULL) {
        free(widget->children);
    }

    if (widget->name != NULL) {
        free(widget->name);
    }

    if (widget->prop_type != NULL) {
        free(widget->prop_type);
    }

    if (widget->prop_rect != NULL) {
        free(widget->prop_rect);
    }

    if (widget->prop_text != NULL) {
        free(widget->prop_text);
    }

    if (widget->prop_value != NULL) {
        free(widget->prop_value);
    }

    if (widget->prop_placeholder != NULL) {
        free(widget->prop_placeholder);
    }

    if (widget->prop_color != NULL) {
        free(widget->prop_color);
    }

    if (widget->prop_bgcolor != NULL) {
        free(widget->prop_bgcolor);
    }

    if (widget->prop_fgcolor != NULL) {
        free(widget->prop_fgcolor);
    }

    if (widget->layout_type != NULL) {
        free(widget->layout_type);
    }

    if (widget->layout_align != NULL) {
        free(widget->layout_align);
    }

    if (widget->layout_justify != NULL) {
        free(widget->layout_justify);
    }

    /* Free dropdown internal data */
    if (widget->type == WIDGET_DROPDOWN && widget->internal_data != NULL) {
        /* Use external dropdown cleanup function */
        extern void dropdown_cleanup_widget_internal_data(void *data);
        dropdown_cleanup_widget_internal_data(widget->internal_data);
        widget->internal_data = NULL;
    }

    /* Free slider internal data */
    if ((widget->type == WIDGET_SLIDER || widget->type == WIDGET_RANGE_SLIDER) &&
        widget->internal_data != NULL) {
        extern void slider_cleanup_widget_internal_data(void *data);
        slider_cleanup_widget_internal_data(widget->internal_data);
        widget->internal_data = NULL;
    }

    free(widget);
}

/*
 * Get widget by ID
 */
struct KryonWidget *widget_get(uint32_t id)
{
    int i;

    for (i = 0; i < nwidgets; i++) {
        if (widgets[i]->id == id) {
            return widgets[i];
        }
    }

    return NULL;
}

/*
 * Get total number of widgets
 */
int widget_get_count(void)
{
    return nwidgets;
}

/*
 * Get widget by array index (not ID)
 * Returns NULL if index is out of bounds
 */
struct KryonWidget *widget_get_by_index(int index)
{
    if (index < 0 || index >= nwidgets) {
        return NULL;
    }
    return widgets[index];
}

/*
 * Get widget type as string
 */
const char *widget_type_to_string(WidgetType type)
{
    switch (type) {
        case WIDGET_BUTTON: return "button";
        case WIDGET_LABEL: return "label";
        case WIDGET_SWITCH: return "switch";
        case WIDGET_SLIDER: return "slider";
        case WIDGET_TEXT_INPUT: return "textinput";
        case WIDGET_CHECKBOX: return "checkbox";
        case WIDGET_RADIO_BUTTON: return "radiobutton";
        case WIDGET_DROPDOWN: return "dropdown";
        case WIDGET_HEADING: return "heading";
        case WIDGET_PARAGRAPH: return "paragraph";
        case WIDGET_CONTAINER: return "container";
        case WIDGET_PROGRESS_BAR: return "progressbar";
        case WIDGET_RANGE_SLIDER: return "rangeslider";
        default: return "unknown";
    }
}

/*
 * Get widget type from string
 */
WidgetType widget_type_from_string(const char *str)
{
    if (str == NULL) {
        return WIDGET_UNKNOWN;
    }

    if (strcmp(str, "button") == 0) return WIDGET_BUTTON;
    if (strcmp(str, "label") == 0) return WIDGET_LABEL;
    if (strcmp(str, "switch") == 0) return WIDGET_SWITCH;
    if (strcmp(str, "slider") == 0) return WIDGET_SLIDER;
    if (strcmp(str, "textinput") == 0) return WIDGET_TEXT_INPUT;
    if (strcmp(str, "checkbox") == 0) return WIDGET_CHECKBOX;
    if (strcmp(str, "radiobutton") == 0) return WIDGET_RADIO_BUTTON;
    if (strcmp(str, "dropdown") == 0) return WIDGET_DROPDOWN;
    if (strcmp(str, "heading") == 0) return WIDGET_HEADING;
    if (strcmp(str, "paragraph") == 0) return WIDGET_PARAGRAPH;
    if (strcmp(str, "container") == 0) return WIDGET_CONTAINER;
    if (strcmp(str, "progressbar") == 0) return WIDGET_PROGRESS_BAR;
    if (strcmp(str, "rangeslider") == 0) return WIDGET_RANGE_SLIDER;

    return WIDGET_UNKNOWN;
}

/*
 * Property file read wrapper
 */
static ssize_t widget_property_read(char *buf, size_t count, uint64_t offset,
                                     void *data)
{
    WidgetPropertyData *prop_data = (WidgetPropertyData *)data;
    if (prop_data->read_func == NULL) {
        return -1;
    }
    return prop_data->read_func(prop_data->widget, buf, count, offset);
}

/*
 * Property file write wrapper
 */
static ssize_t widget_property_write(const char *buf, size_t count, uint64_t offset,
                                      void *data)
{
    WidgetPropertyData *prop_data = (WidgetPropertyData *)data;
    if (prop_data->write_func == NULL) {
        return -1;
    }
    return prop_data->write_func(prop_data->widget, buf, count, offset);
}

/*
 * Read property: type
 */
static ssize_t prop_read_type(struct KryonWidget *w, char *buf, size_t count, uint64_t offset)
{
    const char *val;
    size_t len;

    if (w == NULL || w->prop_type == NULL) {
        return 0;
    }

    val = w->prop_type;
    len = strlen(val);

    if (offset >= len) {
        return 0;
    }
    if (offset + count > len) {
        count = len - offset;
    }

    memcpy(buf, val + offset, count);
    return count;
}

/*
 * Read property: name
 */
static ssize_t prop_read_name(struct KryonWidget *w, char *buf, size_t count, uint64_t offset)
{
    const char *val;
    size_t len;

    val = (w != NULL && w->name != NULL) ? w->name : "";
    len = strlen(val);

    if (offset >= len) {
        return 0;
    }
    if (offset + count > len) {
        count = len - offset;
    }

    memcpy(buf, val + offset, count);
    return count;
}

/*
 * Read property: value
 */
static ssize_t prop_read_value(struct KryonWidget *w, char *buf, size_t count, uint64_t offset)
{
    const char *val;
    size_t len;

    if (w == NULL || w->prop_value == NULL) {
        return 0;
    }

    val = w->prop_value;
    len = strlen(val);

    if (offset >= len) {
        return 0;
    }
    if (offset + count > len) {
        count = len - offset;
    }

    memcpy(buf, val + offset, count);
    return count;
}

/*
 * Write property: value
 */
static ssize_t prop_write_value(struct KryonWidget *w, const char *buf, size_t count, uint64_t offset)
{
    if (w == NULL || buf == NULL) {
        return -1;
    }

    if (offset > 0) {
        return -1;
    }

    free(w->prop_value);
    w->prop_value = (char *)malloc(count + 1);
    if (w->prop_value == NULL) {
        return -1;
    }

    memcpy(w->prop_value, buf, count);
    w->prop_value[count] = '\0';

    fprintf(stderr, "Widget %u value: %s\n", w->id, w->prop_value);

    /* Trigger re-render */
    mark_dirty(w->parent_window);
    render_all();

    return count;
}

/*
 * Read property: text
 */
static ssize_t prop_read_text(struct KryonWidget *w, char *buf, size_t count, uint64_t offset)
{
    const char *val;
    size_t len;

    if (w == NULL || w->prop_text == NULL) {
        return 0;
    }

    val = w->prop_text;
    len = strlen(val);

    if (offset >= len) {
        return 0;
    }
    if (offset + count > len) {
        count = len - offset;
    }

    memcpy(buf, val + offset, count);
    return count;
}

/*
 * Write property: text
 */
static ssize_t prop_write_text(struct KryonWidget *w, const char *buf, size_t count, uint64_t offset)
{
    if (w == NULL || buf == NULL) {
        return -1;
    }

    if (offset > 0) {
        return -1;
    }

    free(w->prop_text);
    w->prop_text = (char *)malloc(count + 1);
    if (w->prop_text == NULL) {
        return -1;
    }

    memcpy(w->prop_text, buf, count);
    w->prop_text[count] = '\0';

    fprintf(stderr, "Widget %u text: %s\n", w->id, w->prop_text);

    /* Trigger re-render */
    mark_dirty(w->parent_window);
    render_all();

    return count;
}

/*
 * Read property: visible
 */
static ssize_t prop_read_visible(struct KryonWidget *w, char *buf, size_t count, uint64_t offset)
{
    const char *val;

    if (w == NULL) {
        return 0;
    }

    val = w->prop_visible ? "1" : "0";

    if (offset >= 1) {
        return 0;
    }

    buf[0] = val[0];
    return 1;
}

/*
 * Write property: visible
 */
static ssize_t prop_write_visible(struct KryonWidget *w, const char *buf, size_t count, uint64_t offset)
{
    if (w == NULL || buf == NULL) {
        return -1;
    }

    if (count > 0 && buf[0] == '1') {
        w->prop_visible = 1;
    } else if (count > 0 && buf[0] == '0') {
        w->prop_visible = 0;
    }

    /* Trigger re-render */
    mark_dirty(w->parent_window);
    render_all();

    return count;
}

/*
 * Read property: enabled
 */
static ssize_t prop_read_enabled(struct KryonWidget *w, char *buf, size_t count, uint64_t offset)
{
    const char *val;

    if (w == NULL) {
        return 0;
    }

    val = w->prop_enabled ? "1" : "0";

    if (offset >= 1) {
        return 0;
    }

    buf[0] = val[0];
    return 1;
}

/*
 * Write property: enabled
 */
static ssize_t prop_write_enabled(struct KryonWidget *w, const char *buf, size_t count, uint64_t offset)
{
    if (w == NULL || buf == NULL) {
        return -1;
    }

    if (count > 0 && buf[0] == '1') {
        w->prop_enabled = 1;
    } else if (count > 0 && buf[0] == '0') {
        w->prop_enabled = 0;
    }

    return count;
}

/*
 * Read property: event
 */
static ssize_t prop_read_event(struct KryonWidget *w, char *buf, size_t count, uint64_t offset)
{
    EventQueue *eq;

    if (w == NULL) {
        return 0;
    }

    /* Get or create event queue for this widget */
    eq = widget_event_queue(w);
    if (eq == NULL) {
        eq = event_queue_create(w);
        if (eq == NULL) {
            return 0;
        }
    }

    return event_file_read(buf, count, offset, eq);
}

/*
 * Read property: bgcolor
 */
static ssize_t prop_read_bgcolor(struct KryonWidget *w, char *buf, size_t count, uint64_t offset)
{
    const char *val;
    size_t len;

    if (w == NULL) {
        return 0;
    }

    val = (w->prop_bgcolor != NULL) ? w->prop_bgcolor : "";
    len = strlen(val);

    if (offset >= len) {
        return 0;
    }
    if (offset + count > len) {
        count = len - offset;
    }

    memcpy(buf, val + offset, count);
    return count;
}

/*
 * Write property: bgcolor
 */
static ssize_t prop_write_bgcolor(struct KryonWidget *w, const char *buf, size_t count, uint64_t offset)
{
    if (w == NULL || buf == NULL) {
        return -1;
    }

    if (offset > 0) {
        return -1;
    }

    free(w->prop_bgcolor);
    w->prop_bgcolor = (char *)malloc(count + 1);
    if (w->prop_bgcolor == NULL) {
        return -1;
    }

    memcpy(w->prop_bgcolor, buf, count);
    w->prop_bgcolor[count] = '\0';

    mark_dirty(w->parent_window);
    render_all();

    return count;
}

/*
 * Read property: fgcolor
 */
static ssize_t prop_read_fgcolor(struct KryonWidget *w, char *buf, size_t count, uint64_t offset)
{
    const char *val;
    size_t len;

    if (w == NULL) {
        return 0;
    }

    val = (w->prop_fgcolor != NULL) ? w->prop_fgcolor : "";
    len = strlen(val);

    if (offset >= len) {
        return 0;
    }
    if (offset + count > len) {
        count = len - offset;
    }

    memcpy(buf, val + offset, count);
    return count;
}

/*
 * Write property: fgcolor
 */
static ssize_t prop_write_fgcolor(struct KryonWidget *w, const char *buf, size_t count, uint64_t offset)
{
    if (w == NULL || buf == NULL) {
        return -1;
    }

    if (offset > 0) {
        return -1;
    }

    free(w->prop_fgcolor);
    w->prop_fgcolor = (char *)malloc(count + 1);
    if (w->prop_fgcolor == NULL) {
        return -1;
    }

    memcpy(w->prop_fgcolor, buf, count);
    w->prop_fgcolor[count] = '\0';

    mark_dirty(w->parent_window);
    render_all();

    return count;
}

/*
 * Create 9P file hierarchy for a widget
 */
int widget_create_fs_entries(struct KryonWidget *widget, P9Node *widgets_dir)
{
    P9Node *widget_dir;
    P9Node *file;
    WidgetPropertyData *prop_data;
    char dirname[16];

    if (widget == NULL || widgets_dir == NULL) {
        return -1;
    }

    /* Create widget directory: use name if available, else numeric ID */
    if (widget->name != NULL && widget->name[0] != '\0') {
        widget_dir = tree_create_dir(widgets_dir, widget->name);
    } else {
        snprintf(dirname, sizeof(dirname), "%u", widget->id);
        widget_dir = tree_create_dir(widgets_dir, dirname);
    }
    if (widget_dir == NULL) {
        return -1;
    }

    widget->node = widget_dir;

    /* Create name file (read-only) */
    prop_data = (WidgetPropertyData *)malloc(sizeof(WidgetPropertyData));
    if (prop_data != NULL) {
        prop_data->widget = widget;
        prop_data->read_func = prop_read_name;
        prop_data->write_func = NULL;
        file = tree_create_file(widget_dir, "name", prop_data,
                                (P9ReadFunc)widget_property_read,
                                (P9WriteFunc)widget_property_write);
        if (file == NULL) {
            free(prop_data);
            return -1;
        }
    }

    /* Create type file (read-only) */
    prop_data = (WidgetPropertyData *)malloc(sizeof(WidgetPropertyData));
    if (prop_data != NULL) {
        prop_data->widget = widget;
        prop_data->read_func = prop_read_type;
        prop_data->write_func = NULL;
        file = tree_create_file(widget_dir, "type", prop_data,
                                (P9ReadFunc)widget_property_read,
                                (P9WriteFunc)widget_property_write);
        if (file == NULL) {
            free(prop_data);
            return -1;
        }
    }

    /* Create value file (read-write) */
    prop_data = (WidgetPropertyData *)malloc(sizeof(WidgetPropertyData));
    if (prop_data != NULL) {
        prop_data->widget = widget;
        prop_data->read_func = prop_read_value;
        prop_data->write_func = prop_write_value;
        file = tree_create_file(widget_dir, "value", prop_data,
                                (P9ReadFunc)widget_property_read,
                                (P9WriteFunc)widget_property_write);
        if (file == NULL) {
            free(prop_data);
            return -1;
        }
    }

    /* Create text file (read-write) */
    prop_data = (WidgetPropertyData *)malloc(sizeof(WidgetPropertyData));
    if (prop_data != NULL) {
        prop_data->widget = widget;
        prop_data->read_func = prop_read_text;
        prop_data->write_func = prop_write_text;
        file = tree_create_file(widget_dir, "text", prop_data,
                                (P9ReadFunc)widget_property_read,
                                (P9WriteFunc)widget_property_write);
        if (file == NULL) {
            free(prop_data);
            return -1;
        }
    }

    /* Create visible file (read-write) */
    prop_data = (WidgetPropertyData *)malloc(sizeof(WidgetPropertyData));
    if (prop_data != NULL) {
        prop_data->widget = widget;
        prop_data->read_func = prop_read_visible;
        prop_data->write_func = prop_write_visible;
        file = tree_create_file(widget_dir, "visible", prop_data,
                                (P9ReadFunc)widget_property_read,
                                (P9WriteFunc)widget_property_write);
        if (file == NULL) {
            free(prop_data);
            return -1;
        }
    }

    /* Create enabled file (read-write) */
    prop_data = (WidgetPropertyData *)malloc(sizeof(WidgetPropertyData));
    if (prop_data != NULL) {
        prop_data->widget = widget;
        prop_data->read_func = prop_read_enabled;
        prop_data->write_func = prop_write_enabled;
        file = tree_create_file(widget_dir, "enabled", prop_data,
                                (P9ReadFunc)widget_property_read,
                                (P9WriteFunc)widget_property_write);
        if (file == NULL) {
            free(prop_data);
            return -1;
        }
    }

    /* Create event file (read-only) */
    prop_data = (WidgetPropertyData *)malloc(sizeof(WidgetPropertyData));
    if (prop_data != NULL) {
        prop_data->widget = widget;
        prop_data->read_func = prop_read_event;
        prop_data->write_func = NULL;
        file = tree_create_file(widget_dir, "event", prop_data,
                                (P9ReadFunc)widget_property_read,
                                NULL);
        if (file == NULL) {
            free(prop_data);
            return -1;
        }
    }

    /* Create bgcolor file (read-write) */
    prop_data = (WidgetPropertyData *)malloc(sizeof(WidgetPropertyData));
    if (prop_data != NULL) {
        prop_data->widget = widget;
        prop_data->read_func = prop_read_bgcolor;
        prop_data->write_func = prop_write_bgcolor;
        file = tree_create_file(widget_dir, "bgcolor", prop_data,
                                (P9ReadFunc)widget_property_read,
                                (P9WriteFunc)widget_property_write);
        if (file == NULL) {
            free(prop_data);
            return -1;
        }
    }

    /* Create fgcolor file (read-write) */
    prop_data = (WidgetPropertyData *)malloc(sizeof(WidgetPropertyData));
    if (prop_data != NULL) {
        prop_data->widget = widget;
        prop_data->read_func = prop_read_fgcolor;
        prop_data->write_func = prop_write_fgcolor;
        file = tree_create_file(widget_dir, "fgcolor", prop_data,
                                (P9ReadFunc)widget_property_read,
                                (P9WriteFunc)widget_property_write);
        if (file == NULL) {
            free(prop_data);
            return -1;
        }
    }

    return 0;
}
