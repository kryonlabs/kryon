/*
 * Kryon /dev/kbd Implementation
 * C89/C90 compliant
 *
 * Implements the Plan 9 keyboard device
 */

#include "kryon.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/*
 * Keyboard state
 */
static struct {
    char buffer[256];
    int buffer_len;
    int buffer_pos;
} kbd_state = { {0}, 0, 0 };

/*
 * Read from /dev/kbd
 * Returns keyboard events in "rX" or "RX" format
 */
static ssize_t devkbd_read(char *buf, size_t count, uint64_t offset, void *data)
{
    size_t to_copy;

    (void)data;
    (void)offset;

    /* For now, return no events (non-blocking) */
    /* In real implementation, this would block for events */

    to_copy = 0;

    if (to_copy > count) {
        to_copy = count;
    }

    return to_copy;
}

/*
 * Write to /dev/kbd
 * Accepts keyboard events in "rX" or "RX" format
 */
static ssize_t devkbd_write(const char *buf, size_t count, uint64_t offset, void *data)
{
    (void)offset;
    (void)data;

    if (buf == NULL || count < 2) {
        return -1;
    }

    /* Parse format: "rX" or "RX" where X is UTF-8 character */
    if (buf[0] == 'r' || buf[0] == 'R') {
        fprintf(stderr, "devkbd_write: %c event: '%c' (0x%02X)\n",
                buf[0], buf[1] >= 32 ? buf[1] : '?', buf[1]);
        /* In real implementation, this would be queued for clients */
    }

    return count;
}

/*
 * Initialize /dev/kbd device
 */
int devkbd_init(P9Node *dev_dir)
{
    P9Node *kbd_node;

    if (dev_dir == NULL) {
        return -1;
    }

    /* Create /dev/kbd file */
    kbd_node = tree_create_file(dev_dir, "kbd", NULL,
                                (P9ReadFunc)devkbd_read,
                                (P9WriteFunc)devkbd_write);
    if (kbd_node == NULL) {
        fprintf(stderr, "devkbd_init: cannot create kbd node\n");
        return -1;
    }


    return 0;
}
