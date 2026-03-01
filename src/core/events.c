/*
 * Kryon Event System Implementation
 * C89/C90 compliant
 */

#include "events.h"
#include "widget.h"
#include "window.h"
#include "graphics.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/time.h>
#endif

/* External function from window.c */
extern struct KryonWindow *window_get(uint32_t id);

/*
 * Default event queue capacity
 */
#define EVENT_QUEUE_CAPACITY 32

/*
 * Global event queue registry
 * Maps widget_id -> EventQueue
 */
typedef struct {
    uint32_t widget_id;
    EventQueue *queue;
} EventQueueEntry;

static EventQueueEntry *g_event_queues = NULL;
static int g_nqueues = 0;
static int g_queue_capacity = 0;
static int g_initialized = 0;

/*
 * Initialize event system
 */
int event_system_init(void)
{
    if (g_initialized) {
        return 0;
    }

    g_queue_capacity = 256;
    g_event_queues = (EventQueueEntry *)malloc(
        g_queue_capacity * sizeof(EventQueueEntry));
    if (g_event_queues == NULL) {
        return -1;
    }

    g_nqueues = 0;
    g_initialized = 1;

    return 0;
}

/*
 * Cleanup event system
 */
void event_system_cleanup(void)
{
    int i;

    if (!g_initialized) {
        return;
    }

    for (i = 0; i < g_nqueues; i++) {
        event_queue_destroy(g_event_queues[i].queue);
    }

    free(g_event_queues);
    g_event_queues = NULL;
    g_nqueues = 0;
    g_queue_capacity = 0;
    g_initialized = 0;
}

/*
 * Create event queue for a widget
 */
EventQueue *event_queue_create(struct KryonWidget *widget)
{
    EventQueue *eq;
    EventQueueEntry *entry;
    int new_capacity;

    /* Allocate queue */
    eq = (EventQueue *)calloc(1, sizeof(EventQueue));
    if (eq == NULL) {
        return NULL;
    }

    eq->event_capacity = EVENT_QUEUE_CAPACITY;
    eq->events = (KryonEvent *)malloc(
        eq->event_capacity * sizeof(KryonEvent));
    if (eq->events == NULL) {
        free(eq);
        return NULL;
    }

    eq->widget = widget;
    eq->nevents = 0;
    eq->read_pos = 0;
    eq->write_pos = 0;

    /* NULL widget = standalone queue, not registered globally */
    if (widget == NULL) {
        return eq;
    }

    /* Check if queue already exists */
    if (widget_event_queue(widget) != NULL) {
        free(eq->events);
        free(eq);
        return NULL;
    }

    /* Expand registry if needed */
    if (g_nqueues >= g_queue_capacity) {
        new_capacity = g_queue_capacity * 2;
        entry = (EventQueueEntry *)realloc(
            g_event_queues,
            new_capacity * sizeof(EventQueueEntry));
        if (entry == NULL) {
            free(eq->events);
            free(eq);
            return NULL;
        }
        g_event_queues = entry;
        g_queue_capacity = new_capacity;
    }

    /* Add to registry */
    g_event_queues[g_nqueues].widget_id = widget->id;
    g_event_queues[g_nqueues].queue = eq;
    g_nqueues++;

    return eq;
}

/*
 * Destroy event queue
 */
void event_queue_destroy(EventQueue *eq)
{
    int i;

    if (eq == NULL) {
        return;
    }

    /* Remove from registry */
    for (i = 0; i < g_nqueues; i++) {
        if (g_event_queues[i].queue == eq) {
            /* Move last entry into this slot */
            if (i < g_nqueues - 1) {
                g_event_queues[i] = g_event_queues[g_nqueues - 1];
            }
            g_nqueues--;
            break;
        }
    }

    if (eq->events != NULL) {
        free(eq->events);
    }

    free(eq);
}

/*
 * Push event to queue
 */
int event_queue_push(EventQueue *eq, const KryonEvent *ev)
{
    int next_pos;

    if (eq == NULL || ev == NULL) {
        return -1;
    }

    /* Check if queue is full */
    next_pos = (eq->write_pos + 1) % eq->event_capacity;
    if (next_pos == eq->read_pos) {
        /* Queue full - discard oldest event */
        eq->read_pos = (eq->read_pos + 1) % eq->event_capacity;
        eq->nevents--;
    }

    /* Add event */
    eq->events[eq->write_pos] = *ev;
    eq->write_pos = next_pos;
    eq->nevents++;

    return 0;
}

/*
 * Pop event from queue
 */
int event_queue_pop(EventQueue *eq, KryonEvent *ev)
{
    if (eq == NULL || ev == NULL) {
        return -1;
    }

    if (eq->read_pos == eq->write_pos) {
        /* Queue empty */
        return -1;
    }

    *ev = eq->events[eq->read_pos];
    eq->read_pos = (eq->read_pos + 1) % eq->event_capacity;
    eq->nevents--;

    return 0;
}

/*
 * Peek at next event without removing it
 */
int event_queue_peek(EventQueue *eq, KryonEvent *ev)
{
    if (eq == NULL || ev == NULL) {
        return -1;
    }

    if (eq->read_pos == eq->write_pos) {
        /* Queue empty */
        return -1;
    }

    *ev = eq->events[eq->read_pos];
    return 0;
}

/*
 * Get event queue for widget
 */
EventQueue *widget_event_queue(KryonWidget *widget)
{
    int i;

    if (widget == NULL) {
        return NULL;
    }

    for (i = 0; i < g_nqueues; i++) {
        if (g_event_queues[i].widget_id == widget->id) {
            return g_event_queues[i].queue;
        }
    }

    return NULL;
}

/*
 * Read from event file
 * Returns event data in text format
 */
ssize_t event_file_read(char *buf, size_t count, uint64_t offset, EventQueue *eq)
{
    KryonEvent ev;
    char msg[512];
    int len;
    const char *type_str;

    (void)offset;  /* Events are stream-oriented */

    if (eq == NULL || buf == NULL) {
        return -1;
    }

    /* Check if event available */
    if (event_queue_peek(eq, &ev) < 0) {
        /* No events available - would block in real implementation */
        return 0;
    }

    /* Format event */
    switch (ev.type) {
        case EVENT_MOUSE_CLICK:
            type_str = "click";
            break;
        case EVENT_MOUSE_DBLCLICK:
            type_str = "dblclick";
            break;
        case EVENT_MOUSE_HOVER:
            type_str = "hover";
            break;
        case EVENT_KEY_PRESS:
            type_str = "keypress";
            break;
        case EVENT_KEY_RELEASE:
            type_str = "keyrelease";
            break;
        case EVENT_FOCUS_GAIN:
            type_str = "focus";
            break;
        case EVENT_FOCUS_LOST:
            type_str = "blur";
            break;
        default:
            type_str = "unknown";
            break;
    }

    len = sprintf(msg,
                  "%s x=%d y=%d button=%d msec=%lu\n",
                  type_str,
                  ev.xy.x,
                  ev.xy.y,
                  ev.button,
                  ev.msec);

    if (len < 0 || len >= sizeof(msg)) {
        return -1;
    }

    /* Remove event from queue after reading */
    event_queue_pop(eq, &ev);

    if (len > count) {
        len = count;
    }

    memcpy(buf, msg, len);
    return len;
}

/*
 * Write to event file (reserved for synthetic events)
 */
ssize_t event_file_write(const char *buf, size_t count, uint64_t offset,
                        EventQueue *eq)
{
    (void)buf;
    (void)count;
    (void)offset;
    (void)eq;

    /* Reserved for future use */
    return -1;
}

/*
 * Hit-test: Find widget at point
 * Returns top-most visible widget at the given position
 */
struct KryonWidget *hit_test(Point xy)
{
    KryonWindow *win;
    struct KryonWidget *widget;
    struct KryonWidget *result;
    Rectangle win_rect;
    Rectangle widget_rect;
    int i;
    int max_z;

    /* Search all windows (TODO: track windows properly) */
    /* For now, just check window 1 */
    win = window_get(1);
    if (win == NULL) {
        return NULL;
    }

    if (!win->visible) {
        return NULL;
    }

    /* Parse window rect */
    if (parse_rect(win->rect, &win_rect) < 0) {
        return NULL;
    }

    /* Check if point is in window */
    if (!ptinrect(xy, win_rect)) {
        return NULL;
    }

    /* Search widgets in reverse order (top to bottom) */
    result = NULL;
    max_z = -1;

    for (i = 0; i < win->nwidgets; i++) {
        widget = win->widgets[i];

        if (!widget->prop_visible) {
            continue;
        }

        /* Parse widget rect */
        if (parse_rect(widget->prop_rect, &widget_rect) < 0) {
            continue;
        }

        /* Check if point is in widget */
        if (ptinrect(xy, widget_rect)) {
            /* Use i as a simple z-order proxy */
            if (i > max_z) {
                max_z = i;
                result = widget;
            }
        }
    }

    return result;
}

/*
 * Generate mouse event
 */
int generate_mouse_event(Point xy, int buttons)
{
    KryonWidget *target;
    EventQueue *eq;
    KryonEvent ev;
    struct timeval tv;

    target = hit_test(xy);
    if (target == NULL) {
        return 0;  /* No widget at position */
    }

    /* Get event queue for widget */
    eq = widget_event_queue(target);
    if (eq == NULL) {
        /* Create event queue on-demand */
        eq = event_queue_create(target);
        if (eq == NULL) {
            return -1;
        }
    }

    /* Get timestamp */
    {
#ifdef _WIN32
        ev.msec = (unsigned long)time(NULL) * 1000;
#else
        struct timeval tv;
        gettimeofday(&tv, NULL);
        ev.msec = (unsigned long)(tv.tv_sec * 1000 + tv.tv_usec / 1000);
#endif
    }

    /* Determine event type */
    if (buttons == 1) {
        ev.type = EVENT_MOUSE_CLICK;
    } else if (buttons == 2) {
        ev.type = EVENT_MOUSE_CLICK;
    } else if (buttons == 4) {
        ev.type = EVENT_MOUSE_CLICK;
    } else {
        ev.type = EVENT_MOUSE_HOVER;
    }

    ev.widget_id = target->id;
    ev.xy = xy;
    ev.button = buttons;
    ev.data[0] = '\0';

    /* Push to queue */
    event_queue_push(eq, &ev);

    fprintf(stderr, "Mouse event: widget=%u type=%d x=%d y=%d button=%d\n",
            ev.widget_id, ev.type, ev.xy.x, ev.xy.y, ev.button);

    return 0;
}

/*
 * Generate keyboard event
 */
int generate_key_event(int keycode, int pressed)
{
    KryonEvent ev;
    struct timeval tv;

    /* TODO: Implement focus tracking */
    /* For now, keyboard events go to the focused widget (if any) */

    /* Get timestamp */
    {
#ifdef _WIN32
        ev.msec = (unsigned long)time(NULL) * 1000;
#else
        struct timeval tv;
        gettimeofday(&tv, NULL);
        ev.msec = (unsigned long)(tv.tv_sec * 1000 + tv.tv_usec / 1000);
#endif
    }

    ev.type = pressed ? EVENT_KEY_PRESS : EVENT_KEY_RELEASE;
    ev.button = keycode;
    ev.xy.x = 0;
    ev.xy.y = 0;
    ev.data[0] = '\0';

    /* TODO: Route to focused widget */
    fprintf(stderr, "Key event: keycode=%d pressed=%d\n", keycode, pressed);

    return 0;
}
