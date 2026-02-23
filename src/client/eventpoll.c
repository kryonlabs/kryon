/*
 * Kryon Event Polling Utility
 * C89/C90 compliant
 */

#include "p9client.h"
#include "events.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

/*
 * snprintf prototype for C89 compatibility
 */
extern int snprintf(char *str, size_t size, const char *format, ...);

/*
 * Parse event from text format
 */
static int parse_event(const char *text, KryonEvent *ev)
{
    char type_str[32];
    char x_str[16];
    char y_str[16];
    char button_str[16];
    char msec_str[16];
    int n;

    if (text == NULL || ev == NULL) {
        return -1;
    }

    /* Format: "type x=# y=# button=# msec=#" */
    n = sscanf(text, "%31s x=%[^ ] y=%[^ ] button=%[^ ] msec=%[^ ]",
               type_str, x_str, y_str, button_str, msec_str);

    if (n < 5) {
        return -1;
    }

    /* Parse type */
    if (strcmp(type_str, "click") == 0) {
        ev->type = EVENT_MOUSE_CLICK;
    } else if (strcmp(type_str, "dblclick") == 0) {
        ev->type = EVENT_MOUSE_DBLCLICK;
    } else if (strcmp(type_str, "hover") == 0) {
        ev->type = EVENT_MOUSE_HOVER;
    } else if (strcmp(type_str, "keypress") == 0) {
        ev->type = EVENT_KEY_PRESS;
    } else if (strcmp(type_str, "keyrelease") == 0) {
        ev->type = EVENT_KEY_RELEASE;
    } else if (strcmp(type_str, "focus") == 0) {
        ev->type = EVENT_FOCUS_GAIN;
    } else if (strcmp(type_str, "blur") == 0) {
        ev->type = EVENT_FOCUS_LOST;
    } else {
        ev->type = EVENT_NONE;
    }

    ev->xy.x = atoi(x_str);
    ev->xy.y = atoi(y_str);
    ev->button = atoi(button_str);
    ev->msec = atol(msec_str);

    return 0;
}

/*
 * Poll for widget event
 * Returns 1 if event available, 0 if no event, -1 on error
 */
int poll_widget_event(P9Client *p9, uint32_t widget_id, KryonEvent *ev)
{
    char event_path[128];
    int event_fd;
    char *buf;
    ssize_t nread;
    int result;

    if (p9 == NULL || ev == NULL) {
        return -1;
    }

    /* Build event path */
    snprintf(event_path, sizeof(event_path), "/windows/1/widgets/%u/event", widget_id);

    /* Open event file */
    event_fd = p9_client_open_path(p9, event_path, P9_OREAD);
    if (event_fd < 0) {
        return -1;
    }

    /* Try to read event (non-blocking) */
    buf = (char *)malloc(512);
    if (buf == NULL) {
        p9_client_clunk(p9, event_fd);
        return -1;
    }

    nread = p9_client_read(p9, event_fd, buf, 512);

    /* Clean up */
    p9_client_clunk(p9, event_fd);

    if (nread <= 0) {
        free(buf);
        return 0;
    }

    buf[nread] = '\0';

    /* Parse event */
    result = parse_event(buf, ev);
    free(buf);

    if (result < 0) {
        return -1;
    }

    return 1;
}

/*
 * Read all pending events from a widget
 */
int poll_widget_events_all(P9Client *p9, uint32_t widget_id,
                           KryonEvent *events, int max_events)
{
    int count = 0;
    int result;

    while (count < max_events) {
        result = poll_widget_event(p9, widget_id, &events[count]);
        if (result <= 0) {
            break;
        }
        count++;
    }

    return count;
}

/*
 * Utility: Format event as string
 */
int format_event(char *buf, size_t bufsize, const KryonEvent *ev)
{
    const char *type_str;
    int len;

    if (buf == NULL || ev == NULL) {
        return -1;
    }

    switch (ev->type) {
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

    len = snprintf(buf, bufsize,
                   "%s x=%d y=%d button=%d msec=%lu\n",
                   type_str,
                   ev->xy.x,
                   ev->xy.y,
                   ev->button,
                   ev->msec);

    if (len < 0 || (size_t)len >= bufsize) {
        return -1;
    }

    return len;
}
