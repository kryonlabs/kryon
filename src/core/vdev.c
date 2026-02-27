/*
 * Virtual Device Implementation
 * C89/C90 compliant
 *
 * Provides per-window virtualized devices:
 * - /dev/draw (virtual framebuffer)
 * - /dev/cons (virtual console)
 * - /dev/mouse (virtual mouse input)
 * - /dev/kbd (virtual keyboard input)
 */

#include "window.h"
#include "graphics.h"
#include "kryon.h"
#include "vdev.h"
#include "namespace.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

/*
 * ========== Virtual /dev/draw Implementation ==========
 */

/*
 * Allocate virtual framebuffer for window
 */
int vdev_alloc_draw(struct KryonWindow *win, int w, int h)
{
    Rectangle screen_rect;

    if (win == NULL || w <= 0 || h <= 0) {
        return -1;
    }

    /* Allocate virtual devices structure if not exists */
    if (win->vdev == NULL) {
        win->vdev = (VirtualDevices *)calloc(1, sizeof(VirtualDevices));
        if (win->vdev == NULL) {
            return -1;
        }
        /* Initialize Marrow FDs */
        win->vdev->marrow_screen_fd = -1;
        win->vdev->marrow_cons_fd = -1;
        win->vdev->marrow_mouse_fd = -1;
        win->vdev->marrow_kbd_fd = -1;
    }

    /* Free existing buffer if any */
    if (win->vdev->draw_buffer != NULL) {
        /* Note: memimage_free should be called here */
        /* For now, just free the pointer */
        win->vdev->draw_buffer = NULL;
    }

    /* Allocate framebuffer */
    screen_rect.min.x = 0;
    screen_rect.min.y = 0;
    screen_rect.max.x = w;
    screen_rect.max.y = h;

    win->vdev->draw_buffer = memimage_alloc(screen_rect, RGBA32);
    if (win->vdev->draw_buffer == NULL) {
        return -1;
    }

    fprintf(stderr, "vdev_alloc_draw: allocated %dx%d framebuffer for window %u\n",
            w, h, win->id);
    return 0;
}

/*
 * Free virtual framebuffer
 */
int vdev_free_draw(struct KryonWindow *win)
{
    if (win == NULL || win->vdev == NULL) {
        return -1;
    }

    if (win->vdev->draw_buffer != NULL) {
        /* Note: memimage_free should be called here */
        win->vdev->draw_buffer = NULL;
    }

    return 0;
}

/*
 * Get virtual framebuffer
 */
Memimage *vdev_get_draw_buffer(struct KryonWindow *win)
{
    if (win == NULL || win->vdev == NULL) {
        return NULL;
    }
    return win->vdev->draw_buffer;
}

/*
 * Compose virtual framebuffer to parent
 */
int vdev_compose_to_parent(struct KryonWindow *win)
{
    (void)win;
    /* TODO: Implement composition */
    /* This requires access to parent's buffer or screen */
    return 0;
}

/*
 * ========== Virtual /dev/cons Implementation ==========
 */

/*
 * Allocate console buffer
 */
int vdev_alloc_cons(struct KryonWindow *win, int w, int h)
{
    if (win == NULL || w <= 0 || h <= 0) {
        return -1;
    }

    if (win->vdev == NULL) {
        win->vdev = (VirtualDevices *)calloc(1, sizeof(VirtualDevices));
        if (win->vdev == NULL) {
            return -1;
        }
    }

    /* Free existing buffer if any */
    if (win->vdev->cons_buffer != NULL) {
        free(win->vdev->cons_buffer);
    }

    /* Allocate character grid */
    win->vdev->cons_width = w;
    win->vdev->cons_height = h;
    win->vdev->cons_buffer = (char *)calloc(w * h, sizeof(char));
    win->vdev->cons_cursor_x = 0;
    win->vdev->cons_cursor_y = 0;

    if (win->vdev->cons_buffer == NULL) {
        return -1;
    }

    fprintf(stderr, "vdev_alloc_cons: allocated %dx%d console for window %u\n",
            w, h, win->id);
    return 0;
}

/*
 * Write to console
 */
ssize_t vdev_cons_write(const char *buf, size_t count, uint64_t offset,
                       void *data)
{
    struct KryonWindow *win = (struct KryonWindow *)data;
    VirtualDevices *vdev;
    size_t i;

    (void)offset;

    if (win == NULL || win->vdev == NULL) {
        return -1;
    }

    vdev = win->vdev;

    /* Allocate console buffer if not exists */
    if (vdev->cons_buffer == NULL) {
        if (vdev_alloc_cons(win, 80, 24) < 0) {
            return -1;
        }
    }

    /* Write characters to buffer */
    for (i = 0; i < count; i++) {
        char c = buf[i];

        if (c == '\n') {
            /* Newline */
            vdev->cons_cursor_x = 0;
            vdev->cons_cursor_y++;
        } else if (c == '\r') {
            /* Carriage return */
            vdev->cons_cursor_x = 0;
        } else if (c >= ' ' && c <= '~') {
            /* Printable character */
            int pos = vdev->cons_cursor_y * vdev->cons_width + vdev->cons_cursor_x;
            if (pos < vdev->cons_width * vdev->cons_height) {
                vdev->cons_buffer[pos] = c;
            }
            vdev->cons_cursor_x++;
        }

        /* Scroll if needed */
        if (vdev->cons_cursor_x >= vdev->cons_width) {
            vdev->cons_cursor_x = 0;
            vdev->cons_cursor_y++;
        }
        if (vdev->cons_cursor_y >= vdev->cons_height) {
            /* Scroll up (move all lines up) */
            memmove(vdev->cons_buffer,
                    vdev->cons_buffer + vdev->cons_width,
                    (vdev->cons_height - 1) * vdev->cons_width);
            /* Clear last line */
            memset(vdev->cons_buffer +
                   (vdev->cons_height - 1) * vdev->cons_width,
                   ' ', vdev->cons_width);
            vdev->cons_cursor_y--;
        }
    }

    return count;
}

/*
 * Read from console
 */
ssize_t vdev_cons_read(char *buf, size_t count, uint64_t offset,
                      void *data)
{
    struct KryonWindow *win = (struct KryonWindow *)data;
    VirtualDevices *vdev;
    char *str;
    size_t len;
    size_t to_copy;

    if (win == NULL || win->vdev == NULL) {
        return -1;
    }

    vdev = win->vdev;

    /* Allocate console buffer if not exists */
    if (vdev->cons_buffer == NULL) {
        if (vdev_alloc_cons(win, 80, 24) < 0) {
            return -1;
        }
    }

    /* For now, return console as string */
    str = vdev->cons_buffer;
    len = vdev->cons_width * vdev->cons_height;

    if (offset >= len) {
        return 0;
    }

    to_copy = len - offset;
    if (to_copy > count) {
        to_copy = count;
    }

    memcpy(buf, str + offset, to_copy);
    return to_copy;
}

/*
 * ========== Virtual /dev/mouse Implementation ==========
 */

/*
 * Virtual /dev/mouse write handler
 * Writes mouse events - forwards to nested WM if present
 */
ssize_t vdev_mouse_write(const char *buf, size_t count, uint64_t offset,
                        void *data)
{
    struct KryonWindow *win = (struct KryonWindow *)data;

    (void)offset;

    if (win == NULL || buf == NULL || count == 0) {
        return -1;
    }

    /* If nested WM is running, forward mouse events via pipe */
    if (win->ns_type == NS_NESTED_WM && win->nested_fd_out >= 0) {
        /* Write mouse event to nested WM */
        ssize_t nwritten = write(win->nested_fd_out, buf, count);
        (void)nwritten;  /* Ignore write errors for pipe */
        return count;
    }

    /* Otherwise handle locally or consume */
    return count;
}

/*
 * Virtual /dev/mouse read handler
 */
ssize_t vdev_mouse_read(char *buf, size_t count, uint64_t offset,
                       void *data)
{
    (void)buf;
    (void)count;
    (void)offset;
    (void)data;
    /* TODO: Implement mouse event queue */
    return 0;
}

/*
 * ========== Virtual /dev/kbd Implementation ==========
 */

/*
 * Virtual /dev/kbd write handler
 * Writes keyboard events - forwards to nested WM if present
 */
ssize_t vdev_kbd_write(const char *buf, size_t count, uint64_t offset,
                      void *data)
{
    struct KryonWindow *win = (struct KryonWindow *)data;

    (void)offset;

    if (win == NULL || buf == NULL || count == 0) {
        return -1;
    }

    /* If nested WM is running, forward keyboard events via pipe */
    if (win->ns_type == NS_NESTED_WM && win->nested_fd_out >= 0) {
        /* Write keyboard event to nested WM */
        ssize_t nwritten = write(win->nested_fd_out, buf, count);
        (void)nwritten;  /* Ignore write errors for pipe */
        return count;
    }

    /* Otherwise handle locally or consume */
    return count;
}

/*
 * Virtual /dev/kbd read handler
 */
ssize_t vdev_kbd_read(char *buf, size_t count, uint64_t offset,
                     void *data)
{
    (void)buf;
    (void)count;
    (void)offset;
    (void)data;
    /* TODO: Implement keyboard event queue */
    return 0;
}
