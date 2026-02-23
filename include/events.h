/*
 * Kryon Event System
 * C89/C90 compliant
 *
 * Routes user input to widgets and maintains event queues
 */

#ifndef EVENTS_H
#define EVENTS_H

#include <stddef.h>
#include <stdint.h>

/*
 * C89 compatibility
 */
#ifdef _WIN32
typedef long ssize_t;
#else
#include <sys/types.h>
#endif

#include "graphics.h"

/*
 * Forward declarations
 */
struct KryonWidget;  /* Forward declaration - don't typedef here */

/*
 * Event types
 */
typedef enum {
    EVENT_NONE = 0,
    EVENT_MOUSE_CLICK,
    EVENT_MOUSE_DBLCLICK,
    EVENT_MOUSE_HOVER,
    EVENT_KEY_PRESS,
    EVENT_KEY_RELEASE,
    EVENT_FOCUS_GAIN,
    EVENT_FOCUS_LOST
} EventType;

/*
 * Event structure
 */
typedef struct {
    EventType type;
    uint32_t widget_id;
    Point xy;            /* Mouse position */
    int button;          /* Mouse button (1-5) or key code */
    unsigned long msec;  /* Timestamp */
    char data[256];      /* Event-specific data */
} KryonEvent;

/*
 * Event queue (per-widget)
 */
typedef struct {
    KryonEvent *events;
    int nevents;
    int event_capacity;
    int read_pos;
    int write_pos;
    struct KryonWidget *widget;
} EventQueue;

/*
 * Event system initialization
 */
int event_system_init(void);
void event_system_cleanup(void);

/*
 * Event queue management
 */
EventQueue *event_queue_create(struct KryonWidget *widget);
void event_queue_destroy(EventQueue *eq);
int event_queue_push(EventQueue *eq, const KryonEvent *ev);
int event_queue_pop(EventQueue *eq, KryonEvent *ev);
int event_queue_peek(EventQueue *eq, KryonEvent *ev);

/*
 * Event file operations
 */
ssize_t event_file_read(char *buf, size_t count, uint64_t offset, EventQueue *eq);
ssize_t event_file_write(const char *buf, size_t count, uint64_t offset, EventQueue *eq);

/*
 * Hit-testing (find widget at point)
 */
struct KryonWidget *hit_test(Point xy);

/*
 * Generate mouse event
 */
int generate_mouse_event(Point xy, int buttons);

/*
 * Generate keyboard event
 */
int generate_key_event(int keycode, int pressed);

/*
 * Widget event queue lookup
 */
EventQueue *widget_event_queue(struct KryonWidget *widget);

#endif /* EVENTS_H */
