/*
 * Kryon Graphics Engine - /dev/screen Device
 * C89/C90 compliant
 *
 * Exports the screen framebuffer as a read-only file
 */

#include "kryon.h"
#include "graphics.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/*
 * Screen device state
 */
typedef struct {
    Memimage *screen;    /* Screen framebuffer */
} ScreenState;

/*
 * Global screen state
 */
static ScreenState *g_screen_state = NULL;

/*
 * Read from /dev/screen
 * Returns raw pixel data in RGBA32 format
 */
static ssize_t devscreen_read(char *buf, size_t count, uint64_t offset,
                              ScreenState *state)
{
    unsigned char *pixel_data;
    size_t total_size;
    size_t bytes_to_copy;

    if (state == NULL || state->screen == NULL) {
        return 0;
    }

    pixel_data = state->screen->data->bdata;
    total_size = Dx(state->screen->r) * Dy(state->screen->r) * 4;

    if (offset >= total_size) {
        return 0;  /* EOF */
    }

    if (offset + count > total_size) {
        bytes_to_copy = total_size - offset;
    } else {
        bytes_to_copy = count;
    }

    memcpy(buf, pixel_data + offset, bytes_to_copy);

    return bytes_to_copy;
}

/*
 * Write to /dev/screen (not supported)
 */
static ssize_t devscreen_write(const char *buf, size_t count, uint64_t offset,
                               ScreenState *state)
{
    (void)buf;
    (void)offset;
    (void)state;

    /* Screen is read-only from userspace */
    return -1;  /* Error */
}

/*
 * Initialize /dev/screen device
 */
int devscreen_init(P9Node *dev_dir, Memimage *screen)
{
    P9Node *screen_node;
    ScreenState *state;

    if (dev_dir == NULL || screen == NULL) {
        return -1;
    }

    /* Allocate state */
    state = (ScreenState *)malloc(sizeof(ScreenState));
    if (state == NULL) {
        fprintf(stderr, "devscreen_init: cannot allocate state\n");
        return -1;
    }

    state->screen = screen;
    g_screen_state = state;

    /* Create /dev/screen file */
    screen_node = tree_create_file(dev_dir, "screen",
                                   state,
                                   (P9ReadFunc)devscreen_read,
                                   (P9WriteFunc)devscreen_write);
    if (screen_node == NULL) {
        fprintf(stderr, "devscreen_init: cannot create screen node\n");
        free(state);
        return -1;
    }

    /* Set file size */
    screen_node->length = Dx(screen->r) * Dy(screen->r) * 4;

    fprintf(stderr, "devscreen_init: initialized %dx%d screen (%lu bytes)\n",
            Dx(screen->r), Dy(screen->r), screen_node->length);

    return 0;
}

/*
 * Get global screen state (for other devices)
 */
ScreenState *devscreen_get_state(void)
{
    return g_screen_state;
}

/*
 * Update screen reference (e.g., if screen is resized)
 */
int devscreen_set_screen(Memimage *screen)
{
    if (g_screen_state == NULL) {
        return -1;
    }

    g_screen_state->screen = screen;
    return 0;
}

/*
 * Cleanup screen device
 */
void devscreen_cleanup(void)
{
    if (g_screen_state != NULL) {
        /* Don't free the screen itself - it's owned by main.c */
        free(g_screen_state);
        g_screen_state = NULL;
    }
}
