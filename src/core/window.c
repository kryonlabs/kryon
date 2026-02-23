/*
 * Window Registry Implementation
 */

#include "window.h"
#include "widget.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

/*
 * snprintf prototype for C89 compatibility
 * (gcc provides this as an extension)
 */
extern int snprintf(char *str, size_t size, const char *format, ...);

/*
 * Forward declarations for property handlers
 */
static ssize_t prop_read_title(struct KryonWindow *w, char *buf, size_t count, uint64_t offset);
static ssize_t prop_write_title(struct KryonWindow *w, const char *buf, size_t count, uint64_t offset);
static ssize_t prop_read_rect(struct KryonWindow *w, char *buf, size_t count, uint64_t offset);
static ssize_t prop_write_rect(struct KryonWindow *w, const char *buf, size_t count, uint64_t offset);
static ssize_t prop_read_visible(struct KryonWindow *w, char *buf, size_t count, uint64_t offset);
static ssize_t prop_write_visible(struct KryonWindow *w, const char *buf, size_t count, uint64_t offset);
static ssize_t prop_read_event(struct KryonWindow *w, char *buf, size_t count, uint64_t offset);

/*
 * Property file data structure
 */
typedef struct {
    struct KryonWindow *window;
    ssize_t (*read_func)(struct KryonWindow *, char *, size_t, uint64_t);
    ssize_t (*write_func)(struct KryonWindow *, const char *, size_t, uint64_t);
} WindowPropertyData;

/*
 * Global window registry
 */
static struct KryonWindow **windows = NULL;
static int nwindows = 0;
static int window_capacity = 0;
static uint32_t next_window_id = 1;

/*
 * Initialize window registry
 */
int window_registry_init(void)
{
    if (windows != NULL) {
        return -1;  /* Already initialized */
    }

    window_capacity = 16;
    windows = (struct KryonWindow **)malloc(window_capacity * sizeof(struct KryonWindow *));
    if (windows == NULL) {
        return -1;
    }

    nwindows = 0;
    return 0;
}

/*
 * Cleanup window registry
 */
void window_registry_cleanup(void)
{
    int i;

    if (windows == NULL) {
        return;
    }

    for (i = 0; i < nwindows; i++) {
        window_destroy(windows[i]);
    }

    free(windows);
    windows = NULL;
    nwindows = 0;
    window_capacity = 0;
}

/*
 * Create a new window
 */
struct KryonWindow *window_create(const char *title, int width, int height)
{
    struct KryonWindow *window;
    char buf[64];

    /* Check capacity */
    if (nwindows >= window_capacity) {
        return NULL;
    }

    window = (struct KryonWindow *)calloc(1, sizeof(struct KryonWindow));
    if (window == NULL) {
        return NULL;
    }

    /* Initialize */
    window->id = next_window_id++;

    if (title != NULL) {
        window->title = (char *)malloc(strlen(title) + 1);
        if (window->title != NULL) {
            strcpy(window->title, title);
        }
    }

    if (window->title == NULL) {
        snprintf(buf, sizeof(buf), "Window %u", window->id);
        window->title = (char *)malloc(strlen(buf) + 1);
        if (window->title != NULL) {
            strcpy(window->title, buf);
        }
    }

    /* Default rect */
    snprintf(buf, sizeof(buf), "100 100 %d %d", width, height);
    window->rect = (char *)malloc(strlen(buf) + 1);
    if (window->rect != NULL) {
        strcpy(window->rect, buf);
    }

    window->visible = 1;
    window->widgets_node = NULL;

    /* Allocate widget array */
    window->widget_capacity = 256;
    window->widgets = (struct KryonWidget **)malloc(window->widget_capacity * sizeof(struct KryonWidget *));
    window->nwidgets = 0;

    /* Add to registry */
    windows[nwindows++] = window;

    return window;
}

/*
 * Destroy a window
 */
void window_destroy(struct KryonWindow *window)
{
    int i;

    if (window == NULL) {
        return;
    }

    /* Destroy all widgets in this window */
    for (i = 0; i < window->nwidgets; i++) {
        widget_destroy(window->widgets[i]);
    }

    if (window->widgets != NULL) {
        free(window->widgets);
    }

    if (window->title != NULL) {
        free(window->title);
    }

    if (window->rect != NULL) {
        free(window->rect);
    }

    free(window);
}

/*
 * Get window by ID
 */
struct KryonWindow *window_get(uint32_t id)
{
    int i;

    for (i = 0; i < nwindows; i++) {
        if (windows[i]->id == id) {
            return windows[i];
        }
    }

    return NULL;
}

/*
 * Set window title
 */
int window_set_title(struct KryonWindow *window, const char *title)
{
    if (window == NULL || title == NULL) {
        return -1;
    }

    free(window->title);
    window->title = (char *)malloc(strlen(title) + 1);
    if (window->title != NULL) {
        strcpy(window->title, title);
    }

    return 0;
}

/*
 * Set window rect
 */
int window_set_rect(struct KryonWindow *window, const char *rect)
{
    if (window == NULL || rect == NULL) {
        return -1;
    }

    free(window->rect);
    window->rect = (char *)malloc(strlen(rect) + 1);
    if (window->rect != NULL) {
        strcpy(window->rect, rect);
    }

    return 0;
}

/*
 * Set window visible
 */
int window_set_visible(struct KryonWindow *window, int visible)
{
    if (window == NULL) {
        return -1;
    }

    window->visible = visible;
    return 0;
}

/*
 * Add widget to window
 */
int window_add_widget(struct KryonWindow *window, struct KryonWidget *widget)
{
    if (window == NULL || widget == NULL) {
        return -1;
    }

    if (window->nwidgets >= window->widget_capacity) {
        return -1;
    }

    window->widgets[window->nwidgets++] = widget;
    widget->parent_window = window;
    return 0;
}

/*
 * Property file read wrapper
 */
static ssize_t window_property_read(char *buf, size_t count, uint64_t offset,
                                     void *data)
{
    WindowPropertyData *prop_data = (WindowPropertyData *)data;
    if (prop_data->read_func == NULL) {
        return -1;
    }
    return prop_data->read_func(prop_data->window, buf, count, offset);
}

/*
 * Property file write wrapper
 */
static ssize_t window_property_write(const char *buf, size_t count, uint64_t offset,
                                      void *data)
{
    WindowPropertyData *prop_data = (WindowPropertyData *)data;
    if (prop_data->write_func == NULL) {
        return -1;
    }
    return prop_data->write_func(prop_data->window, buf, count, offset);
}

/*
 * Read property: title
 */
static ssize_t prop_read_title(struct KryonWindow *w, char *buf, size_t count, uint64_t offset)
{
    const char *val;
    size_t len;

    if (w == NULL || w->title == NULL) {
        return 0;
    }

    val = w->title;
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
 * Write property: title
 */
static ssize_t prop_write_title(struct KryonWindow *w, const char *buf, size_t count, uint64_t offset)
{
    if (w == NULL || buf == NULL) {
        return -1;
    }

    if (offset > 0) {
        return -1;
    }

    free(w->title);
    w->title = (char *)malloc(count + 1);
    if (w->title == NULL) {
        return -1;
    }

    memcpy(w->title, buf, count);
    w->title[count] = '\0';

    fprintf(stderr, "Window %u title: %s\n", w->id, w->title);
    return count;
}

/*
 * Read property: rect
 */
static ssize_t prop_read_rect(struct KryonWindow *w, char *buf, size_t count, uint64_t offset)
{
    const char *val;
    size_t len;

    if (w == NULL || w->rect == NULL) {
        return 0;
    }

    val = w->rect;
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
 * Write property: rect
 */
static ssize_t prop_write_rect(struct KryonWindow *w, const char *buf, size_t count, uint64_t offset)
{
    if (w == NULL || buf == NULL) {
        return -1;
    }

    if (offset > 0) {
        return -1;
    }

    free(w->rect);
    w->rect = (char *)malloc(count + 1);
    if (w->rect == NULL) {
        return -1;
    }

    memcpy(w->rect, buf, count);
    w->rect[count] = '\0';

    return count;
}

/*
 * Read property: visible
 */
static ssize_t prop_read_visible(struct KryonWindow *w, char *buf, size_t count, uint64_t offset)
{
    const char *val;

    if (w == NULL) {
        return 0;
    }

    val = w->visible ? "1" : "0";

    if (offset >= 1) {
        return 0;
    }

    buf[0] = val[0];
    return 1;
}

/*
 * Write property: visible
 */
static ssize_t prop_write_visible(struct KryonWindow *w, const char *buf, size_t count, uint64_t offset)
{
    if (w == NULL || buf == NULL) {
        return -1;
    }

    if (count > 0 && buf[0] == '1') {
        w->visible = 1;
    } else if (count > 0 && buf[0] == '0') {
        w->visible = 0;
    }

    return count;
}

/*
 * Read property: event (placeholder for Phase 3)
 */
static ssize_t prop_read_event(struct KryonWindow *w, char *buf, size_t count, uint64_t offset)
{
    /* Phase 2: No events yet */
    return 0;
}

/*
 * Create 9P file hierarchy for a window
 */
int window_create_fs_entries(struct KryonWindow *window, P9Node *windows_root)
{
    P9Node *window_dir;
    P9Node *widgets_dir;
    P9Node *file;
    WindowPropertyData *prop_data;
    char dirname[16];

    if (window == NULL || windows_root == NULL) {
        return -1;
    }

    /* Create window directory: /windows/{id} */
    snprintf(dirname, sizeof(dirname), "%u", window->id);
    window_dir = tree_create_dir(windows_root, dirname);
    if (window_dir == NULL) {
        return -1;
    }

    window->node = window_dir;

    /* Create title file */
    prop_data = (WindowPropertyData *)malloc(sizeof(WindowPropertyData));
    if (prop_data != NULL) {
        prop_data->window = window;
        prop_data->read_func = prop_read_title;
        prop_data->write_func = prop_write_title;
        file = tree_create_file(window_dir, "title", prop_data,
                                (P9ReadFunc)window_property_read,
                                (P9WriteFunc)window_property_write);
        if (file == NULL) {
            free(prop_data);
            return -1;
        }
    }

    /* Create rect file */
    prop_data = (WindowPropertyData *)malloc(sizeof(WindowPropertyData));
    if (prop_data != NULL) {
        prop_data->window = window;
        prop_data->read_func = prop_read_rect;
        prop_data->write_func = prop_write_rect;
        file = tree_create_file(window_dir, "rect", prop_data,
                                (P9ReadFunc)window_property_read,
                                (P9WriteFunc)window_property_write);
        if (file == NULL) {
            free(prop_data);
            return -1;
        }
    }

    /* Create visible file */
    prop_data = (WindowPropertyData *)malloc(sizeof(WindowPropertyData));
    if (prop_data != NULL) {
        prop_data->window = window;
        prop_data->read_func = prop_read_visible;
        prop_data->write_func = prop_write_visible;
        file = tree_create_file(window_dir, "visible", prop_data,
                                (P9ReadFunc)window_property_read,
                                (P9WriteFunc)window_property_write);
        if (file == NULL) {
            free(prop_data);
            return -1;
        }
    }

    /* Create widgets subdirectory */
    widgets_dir = tree_create_dir(window_dir, "widgets");
    if (widgets_dir == NULL) {
        return -1;
    }

    window->widgets_node = widgets_dir;

    /* Create event file (read-only for now) */
    prop_data = (WindowPropertyData *)malloc(sizeof(WindowPropertyData));
    if (prop_data != NULL) {
        prop_data->window = window;
        prop_data->read_func = prop_read_event;
        prop_data->write_func = NULL;
        file = tree_create_file(window_dir, "event", prop_data,
                                (P9ReadFunc)window_property_read,
                                NULL);
        if (file == NULL) {
            free(prop_data);
            return -1;
        }
    }

    return 0;
}
