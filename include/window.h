/*
 * Kryon Window Management
 * C89/C90 compliant
 */

#ifndef KRYON_WINDOW_H
#define KRYON_WINDOW_H

#include "kryon.h"

/*
 * Forward declarations
 */
struct KryonWidget;
struct KryonWindow;
struct Memimage;

/*
 * Virtual Devices structure (v0.4.0)
 */
typedef struct VirtualDevices {
    struct Memimage *draw_buffer;      /* Virtual framebuffer */
    char *cons_buffer;                  /* Console buffer (character grid) */
    int cons_width;                     /* Console width (chars) */
    int cons_height;                    /* Console height (chars) */
    int cons_cursor_x;                  /* Console cursor X */
    int cons_cursor_y;                  /* Console cursor Y */
} VirtualDevices;

/*
 * Window state structure
 */
typedef struct KryonWindow {
    uint32_t id;                    /* Unique window ID */
    char *title;                    /* Window title */
    char *rect;                     /* "x y w h" */
    int visible;                    /* 0 or 1 */

    /* v0.4.0: Recursive window support */
    struct KryonWindow *parent;     /* Parent window (NULL for top-level) */
    struct KryonWindow **children;  /* Child windows */
    int nchildren;                  /* Number of child windows */
    int child_capacity;             /* Child array capacity */

    /* Widget tree */
    struct KryonWidget **widgets;   /* Widgets in this window */
    int nwidgets;
    int widget_capacity;

    /* 9P tree nodes */
    P9Node *node;                   /* /win/{id} */
    P9Node *widgets_node;           /* /win/{id}/widgets */
    P9Node *nested_win_dir;         /* /win/{id}/win/ (for nesting) */

    /* Graphics */
    struct Memimage *backing_store; /* Window backing store (optional) */

    /* v0.4.0: Virtual devices */
    VirtualDevices *vdev;           /* Virtual devices (NULL if not allocated) */

    /* Internal state */
    void *internal_data;            /* Platform-specific data */
} KryonWindow;

/*
 * Window registry
 */
int window_registry_init(void);
void window_registry_cleanup(void);

struct KryonWindow *window_create(const char *title, int width, int height);
void window_destroy(struct KryonWindow *window);
struct KryonWindow *window_get(uint32_t id);

/*
 * v0.4.0: Recursive window support
 */
struct KryonWindow *window_create_ex(const char *title, int width, int height,
                                     struct KryonWindow *parent);
int window_add_child(struct KryonWindow *parent, struct KryonWindow *child);
int window_remove_child(struct KryonWindow *parent, struct KryonWindow *child);
struct KryonWindow *window_resolve_path(const char *path);
struct KryonWindow *window_find_at_point(struct KryonWindow *root, int x, int y);

/*
 * Window properties
 */
int window_set_title(struct KryonWindow *window, const char *title);
int window_set_rect(struct KryonWindow *window, const char *rect);
int window_set_visible(struct KryonWindow *window, int visible);

/*
 * Widget management
 */
int window_add_widget(struct KryonWindow *window, struct KryonWidget *widget);

/*
 * File system integration
 */
int window_create_fs_entries(struct KryonWindow *window, P9Node *windows_root);
int window_create_fs_entries_ex(struct KryonWindow *window, P9Node *parent_dir);

#endif /* KRYON_WINDOW_H */
