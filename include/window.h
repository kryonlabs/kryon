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
struct Memimage;

/*
 * Window state structure
 */
typedef struct KryonWindow {
    uint32_t id;                    /* Unique window ID */
    char *title;                    /* Window title */
    char *rect;                     /* "x y w h" */
    int visible;                    /* 0 or 1 */

    /* Widget tree */
    struct KryonWidget **widgets;   /* Widgets in this window */
    int nwidgets;
    int widget_capacity;

    /* 9P tree nodes */
    P9Node *node;                   /* /windows/{id} */
    P9Node *widgets_node;           /* /windows/{id}/widgets */

    /* Graphics */
    struct Memimage *backing_store; /* Window backing store (optional) */

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

#endif /* KRYON_WINDOW_H */
