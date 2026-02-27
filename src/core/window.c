/*
 * Window Registry Implementation
 */

#include "window.h"
#include "widget.h"
#include "namespace.h"
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

    /* v0.4.0: Initialize parent/child fields */
    window->parent = NULL;
    window->children = NULL;
    window->nchildren = 0;
    window->child_capacity = 0;

    /* Allocate children array */
    window->child_capacity = 16;
    window->children = (struct KryonWindow **)malloc(
        window->child_capacity * sizeof(struct KryonWindow *));
    if (window->children == NULL) {
        /* Continue without children support */
        window->child_capacity = 0;
    }

    /* Allocate widget array */
    window->widget_capacity = 256;
    window->widgets = (struct KryonWidget **)malloc(window->widget_capacity * sizeof(struct KryonWidget *));
    window->nwidgets = 0;

    /* v0.4.0: Initialize virtual devices to NULL */
    window->vdev = NULL;
    window->nested_win_dir = NULL;

    /* v0.5.0: Initialize namespace */
    window_namespace_init(window);

    /* Add to registry */
    windows[nwindows++] = window;

    return window;
}

/*
 * v0.4.0: Create a new window with parent
 */
struct KryonWindow *window_create_ex(const char *title, int width, int height,
                                     struct KryonWindow *parent)
{
    struct KryonWindow *window;

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
        char buf[64];
        snprintf(buf, sizeof(buf), "Window %u", window->id);
        window->title = (char *)malloc(strlen(buf) + 1);
        if (window->title != NULL) {
            strcpy(window->title, buf);
        }
    }

    /* Default rect */
    {
        char buf[64];
        snprintf(buf, sizeof(buf), "100 100 %d %d", width, height);
        window->rect = (char *)malloc(strlen(buf) + 1);
        if (window->rect != NULL) {
            strcpy(window->rect, buf);
        }
    }

    window->visible = 1;
    window->widgets_node = NULL;

    /* Initialize parent/child fields */
    window->parent = parent;
    window->children = NULL;
    window->nchildren = 0;
    window->child_capacity = 0;

    /* Allocate children array */
    window->child_capacity = 16;
    window->children = (struct KryonWindow **)malloc(
        window->child_capacity * sizeof(struct KryonWindow *));
    if (window->children == NULL) {
        window->child_capacity = 0;
    }

    /* Allocate widget array */
    window->widget_capacity = 256;
    window->widgets = (struct KryonWidget **)malloc(window->widget_capacity * sizeof(struct KryonWidget *));
    window->nwidgets = 0;

    /* Initialize virtual devices to NULL */
    window->vdev = NULL;
    window->nested_win_dir = NULL;

    /* v0.5.0: Initialize namespace */
    window_namespace_init(window);

    /* Add to registry */
    windows[nwindows++] = window;

    /* Add to parent's children list */
    if (parent != NULL) {
        window_add_child(parent, window);
    }

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

    /* v0.4.0: Destroy all child windows first */
    for (i = 0; i < window->nchildren; i++) {
        window_destroy(window->children[i]);
    }

    if (window->children != NULL) {
        free(window->children);
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

    /* v0.4.0: Clean up virtual devices */
    if (window->vdev != NULL) {
        /* Free console buffer */
        if (window->vdev->cons_buffer != NULL) {
            free(window->vdev->cons_buffer);
        }
        /* Note: draw_buffer is a Memimage, should be freed by memimage_free */
        /* if (window->vdev->draw_buffer != NULL) {
             memimage_free(window->vdev->draw_buffer);
         } */
        free(window->vdev);
    }

    /* v0.5.0: Clean up namespace */
    window_namespace_unmount(window);

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

/*
 * ========== v0.4.0: Recursive Window Support ==========
 */

/*
 * Add child window to parent
 */
int window_add_child(struct KryonWindow *parent, struct KryonWindow *child)
{
    if (parent == NULL || child == NULL) {
        return -1;
    }

    /* Check capacity */
    if (parent->nchildren >= parent->child_capacity) {
        /* Expand array */
        int new_capacity = parent->child_capacity * 2;
        struct KryonWindow **new_children =
            (struct KryonWindow **)realloc(parent->children,
                new_capacity * sizeof(struct KryonWindow *));
        if (new_children == NULL) {
            return -1;
        }
        parent->children = new_children;
        parent->child_capacity = new_capacity;
    }

    /* Add child */
    parent->children[parent->nchildren++] = child;
    child->parent = parent;

    return 0;
}

/*
 * Remove child window from parent
 */
int window_remove_child(struct KryonWindow *parent, struct KryonWindow *child)
{
    int i;

    if (parent == NULL || child == NULL) {
        return -1;
    }

    /* Find and remove child */
    for (i = 0; i < parent->nchildren; i++) {
        if (parent->children[i] == child) {
            /* Shift remaining children */
            for (; i < parent->nchildren - 1; i++) {
                parent->children[i] = parent->children[i + 1];
            }
            parent->nchildren--;
            child->parent = NULL;
            return 0;
        }
    }

    return -1;
}

/*
 * Resolve window by path (e.g., "/win/1/win/2/win/3")
 */
struct KryonWindow *window_resolve_path(const char *path)
{
    char path_copy[256];
    char *token;
    struct KryonWindow *current = NULL;
    uint32_t id;

    if (path == NULL || strlen(path) == 0) {
        return NULL;
    }

    /* Copy path (strtok modifies input) */
    strncpy(path_copy, path, sizeof(path_copy) - 1);
    path_copy[sizeof(path_copy) - 1] = '\0';

    /* Skip leading "/" */
    if (path_copy[0] == '/') {
        token = strtok(path_copy + 1, "/");
    } else {
        token = strtok(path_copy, "/");
    }

    /* First token should be "win" */
    if (token == NULL || strcmp(token, "win") != 0) {
        return NULL;
    }

    /* Parse window IDs */
    while ((token = strtok(NULL, "/")) != NULL) {
        id = (uint32_t)atoi(token);

        /* Find window by ID */
        if (current == NULL) {
            /* Top-level window */
            current = window_get(id);
        } else {
            /* Nested window - search in children */
            int found = 0;
            int i;
            for (i = 0; i < current->nchildren; i++) {
                if (current->children[i]->id == id) {
                    current = current->children[i];
                    found = 1;
                    break;
                }
            }
            if (!found) {
                return NULL;  /* Child not found */
            }
        }

        if (current == NULL) {
            return NULL;  /* Window not found */
        }
    }

    return current;
}

/*
 * Find window at point (recursive)
 */
struct KryonWindow *window_find_at_point(struct KryonWindow *root, int x, int y)
{
    int i;
    struct KryonWindow *found;

    if (root == NULL) {
        return NULL;
    }

    /* Check if point is in this window */
    /* TODO: Parse rect and check bounds */
    /* For now, just check children */

    /* Search children (reverse order - topmost first) */
    for (i = root->nchildren - 1; i >= 0; i--) {
        found = window_find_at_point(root->children[i], x, y);
        if (found != NULL) {
            return found;
        }
    }

    /* Check this window */
    /* TODO: Implement bounds checking */
    return root;
}

