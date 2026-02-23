/*
 * Kryon Graphics Engine - /dev/mouse Device
 * C89/C90 compliant
 *
 * Handles mouse input and hit-testing for widgets
 */

#include "kryon.h"
#include "graphics.h"
#include "widget.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

/*
 * Mouse device state
 */
typedef struct {
    Point xy;            /* Current mouse position */
    int buttons;         /* Button state (bitmask) */
    unsigned long msec;  /* Timestamp in milliseconds */
    unsigned long counter; /* Event counter */
} MouseState;

/*
 * Global mouse state
 */
static MouseState g_mouse_state = {
    {0, 0},
    0,
    0,
    0
};

/*
 * Read from /dev/mouse
 * Returns mouse state in Plan 9 format:
 * "m %11d %11d %11d %11d "
 * (resized, x, y, buttons, msec)
 */
static ssize_t devmouse_read(char *buf, size_t count, uint64_t offset,
                             MouseState *state)
{
    char msg[64];
    int len;

    (void)offset;  /* Mouse is stream-oriented */

    if (state == NULL) {
        return 0;
    }

    /* Format: m x y buttons msec */
    len = sprintf(msg,
                  "m %11d %11d %11d %11lu ",
                  0,  /* resized flag (0 = no resize) */
                  state->xy.x,
                  state->xy.y,
                  state->msec);

    if (len < 0 || (size_t)len > sizeof(msg)) {
        return 0;
    }

    if (len > count) {
        len = count;
    }

    memcpy(buf, msg, len);

    return len;
}

/*
 * Write to /dev/mouse
 * Accepts mouse input in format:
 * "m %11d %11d %11d %11d"
 * (resized, x, y, buttons, msec)
 */
static ssize_t devmouse_write(const char *buf, size_t count, uint64_t offset,
                              MouseState *state)
{
    char resized_str[16];
    char x_str[16];
    char y_str[16];
    char buttons_str[16];
    int x, y;
    int buttons;

    (void)offset;

    if (state == NULL || buf == NULL) {
        return -1;
    }

    /* Parse mouse input */
    if (sscanf(buf, "m %11s %11s %11s %11s",
               resized_str, x_str, y_str, buttons_str) != 4) {
        fprintf(stderr, "devmouse_write: invalid format\n");
        return -1;
    }

    x = atoi(x_str);
    y = atoi(y_str);
    buttons = atoi(buttons_str);

    /* Update state */
    state->xy.x = x;
    state->xy.y = y;
    state->buttons = buttons;
    state->msec = (unsigned long)time(NULL) * 1000;
    state->counter++;

    /* TODO: Trigger hit-test for widgets */
    /* This will be implemented in Phase 4 when we have event handling */

    return count;
}

/*
 * Initialize /dev/mouse device
 */
int devmouse_init(P9Node *dev_dir)
{
    P9Node *mouse_node;

    if (dev_dir == NULL) {
        return -1;
    }

    /* Create /dev/mouse file */
    mouse_node = tree_create_file(dev_dir, "mouse",
                                  &g_mouse_state,
                                  (P9ReadFunc)devmouse_read,
                                  (P9WriteFunc)devmouse_write);
    if (mouse_node == NULL) {
        fprintf(stderr, "devmouse_init: cannot create mouse node\n");
        return -1;
    }

    fprintf(stderr, "devmouse_init: initialized mouse device\n");

    return 0;
}

/*
 * Get current mouse position
 */
Point devmouse_get_pos(void)
{
    return g_mouse_state.xy;
}

/*
 * Get current mouse button state
 */
int devmouse_get_buttons(void)
{
    return g_mouse_state.buttons;
}
