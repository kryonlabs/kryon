/*
 * Virtual Device API
 * C89/C90 compliant
 */

#ifndef KRYON_VDEV_H
#define KRYON_VDEV_H

#include "kryon.h"

/*
 * Forward declaration
 */
struct KryonWindow;

/*
 * Virtual /dev/draw functions
 */

/*
 * Allocate virtual framebuffer for window
 *
 * Parameters:
 *   win - Window to allocate framebuffer for
 *   w - Width in pixels
 *   h - Height in pixels
 *
 * Returns 0 on success, -1 on error
 */
int vdev_alloc_draw(struct KryonWindow *win, int w, int h);

/*
 * Free virtual framebuffer
 *
 * Parameters:
 *   win - Window to free framebuffer for
 *
 * Returns 0 on success, -1 on error
 */
int vdev_free_draw(struct KryonWindow *win);

/*
 * Get virtual framebuffer
 *
 * Parameters:
 *   win - Window to get framebuffer from
 *
 * Returns Memimage pointer, or NULL on error
 */
struct Memimage *vdev_get_draw_buffer(struct KryonWindow *win);

/*
 * Compose virtual framebuffer to parent
 *
 * Parameters:
 *   win - Window to compose
 *
 * Returns 0 on success, -1 on error
 */
int vdev_compose_to_parent(struct KryonWindow *win);

/*
 * Virtual /dev/cons functions
 */

/*
 * Allocate console buffer
 *
 * Parameters:
 *   win - Window to allocate console for
 *   w - Width in characters
 *   h - Height in characters
 *
 * Returns 0 on success, -1 on error
 */
int vdev_alloc_cons(struct KryonWindow *win, int w, int h);

/*
 * Write to console (9P handler)
 *
 * Parameters:
 *   buf - Buffer to write
 *   count - Number of bytes to write
 *   offset - Offset in file (ignored)
 *   data - Window pointer
 *
 * Returns number of bytes written, or -1 on error
 */
ssize_t vdev_cons_write(const char *buf, size_t count, uint64_t offset,
                       void *data);

/*
 * Read from console (9P handler)
 *
 * Parameters:
 *   buf - Buffer to read into
 *   count - Number of bytes to read
 *   offset - Offset in file
 *   data - Window pointer
 *
 * Returns number of bytes read, or -1 on error
 */
ssize_t vdev_cons_read(char *buf, size_t count, uint64_t offset,
                      void *data);

/*
 * Virtual /dev/mouse functions
 */

/*
 * Write to mouse device (9P handler)
 *
 * Parameters:
 *   buf - Buffer containing mouse event data
 *   count - Number of bytes to write
 *   offset - Offset in file (ignored)
 *   data - Window pointer
 *
 * Returns number of bytes written, or -1 on error
 */
ssize_t vdev_mouse_write(const char *buf, size_t count, uint64_t offset,
                        void *data);

/*
 * Read from mouse device (9P handler)
 */
ssize_t vdev_mouse_read(char *buf, size_t count, uint64_t offset,
                       void *data);

/*
 * Virtual /dev/kbd functions
 */

/*
 * Write to keyboard device (9P handler)
 */
ssize_t vdev_kbd_write(const char *buf, size_t count, uint64_t offset,
                      void *data);

/*
 * Read from keyboard device (9P handler)
 */
ssize_t vdev_kbd_read(char *buf, size_t count, uint64_t offset,
                     void *data);

#endif /* KRYON_VDEV_H */
