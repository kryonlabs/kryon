/*
 * Kryon Graphics Engine - /dev/cons Device
 * C89/C90 compliant
 *
 * Console I/O device for debug output
 */

#include "kryon.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

/*
 * Console buffer size
 */
#define CONSOLE_BUF_SIZE  4096

/*
 * Console device state
 */
typedef struct {
    char buffer[CONSOLE_BUF_SIZE];
    size_t len;
} ConsState;

/*
 * Global console state
 */
static ConsState g_cons_state = { "", 0 };

/*
 * Read from /dev/cons
 * Returns console buffer contents
 */
static ssize_t devcons_read(char *buf, size_t count, uint64_t offset,
                            void *data)
{
    ConsState *state = (ConsState *)data;
    size_t bytes_to_copy;

    if (state == NULL) {
        return 0;
    }

    if (offset >= state->len) {
        return 0;  /* EOF */
    }

    if (offset + count > state->len) {
        bytes_to_copy = state->len - offset;
    } else {
        bytes_to_copy = count;
    }

    memcpy(buf, state->buffer + offset, bytes_to_copy);

    return bytes_to_copy;
}

/*
 * Write to /dev/cons
 * Appends to console buffer (logs to stderr)
 */
static ssize_t devcons_write(const char *buf, size_t count, uint64_t offset,
                             void *data)
{
    ConsState *state = (ConsState *)data;
    size_t space_left;
    size_t to_copy;

    (void)offset;  /* Always append */

    if (state == NULL || buf == NULL || count == 0) {
        return -1;
    }

    /* Also log to stderr for debugging */
    fprintf(stderr, "[cons] %.*s", (int)count, buf);

    /* Append to buffer */
    space_left = CONSOLE_BUF_SIZE - state->len;
    if (space_left == 0) {
        /* Buffer full, shift contents */
        memmove(state->buffer, state->buffer + CONSOLE_BUF_SIZE / 2,
                state->len - CONSOLE_BUF_SIZE / 2);
        state->len -= CONSOLE_BUF_SIZE / 2;
        space_left = CONSOLE_BUF_SIZE - state->len;
    }

    to_copy = (count < space_left) ? count : space_left;
    memcpy(state->buffer + state->len, buf, to_copy);
    state->len += to_copy;

    return count;
}

/*
 * Initialize /dev/cons device
 */
int devcons_init(P9Node *dev_dir)
{
    P9Node *cons_node;

    if (dev_dir == NULL) {
        return -1;
    }

    /* Create /dev/cons file */
    cons_node = tree_create_file(dev_dir, "cons",
                                 &g_cons_state,
                                 (P9ReadFunc)devcons_read,
                                 (P9WriteFunc)devcons_write);
    if (cons_node == NULL) {
        fprintf(stderr, "devcons_init: cannot create cons node\n");
        return -1;
    }

    cons_node->length = CONSOLE_BUF_SIZE;

    fprintf(stderr, "devcons_init: initialized console device\n");

    return 0;
}

/*
 * Write to console (utility function)
 */
int devcons_printf(const char *fmt, ...)
{
    char buf[512];
    va_list args;
    int len;

    va_start(args, fmt);
    len = vsprintf(buf, fmt, args);
    va_end(args);

    if (len < 0 || (size_t)len >= sizeof(buf)) {
        return -1;
    }

    return devcons_write(buf, (size_t)len, 0, &g_cons_state);
}
