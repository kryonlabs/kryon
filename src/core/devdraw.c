/*
 * Kryon Graphics Engine - /dev/draw Device
 * C89/C90 compliant
 *
 * Processes binary drawing commands
 */

#include "kryon.h"
#include "graphics.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/*
 * Draw opcodes
 */
enum {
    D_alloc = 0,    /* Allocate image */
    D_free,         /* Free image */
    D_rect,         /* Fill rectangle */
    D_draw,         /* Bit blit */
    D_line,         /* Draw line */
    D_poly,         /* Draw polygon */
    D_text,         /* Draw text */
    D_set,          /* Set parameters */
};

/*
 * Draw device state
 */
typedef struct {
    Memimage *screen;       /* Main screen image */
    Memimage **images;      /* Image cache */
    int nimages;
    int image_capacity;
    int client_id;
    int refresh_needed;
} DrawState;

/*
 * Global draw state
 */
static DrawState *g_draw_state = NULL;

/*
 * Initialize draw state
 */
static DrawState *drawstate_create(Memimage *screen)
{
    DrawState *state;

    state = (DrawState *)malloc(sizeof(DrawState));
    if (state == NULL) {
        return NULL;
    }

    memset(state, 0, sizeof(DrawState));

    state->screen = screen;
    state->nimages = 0;
    state->image_capacity = 16;
    state->client_id = 0;
    state->refresh_needed = 0;

    state->images = (Memimage **)malloc(sizeof(Memimage *) * state->image_capacity);
    if (state->images == NULL) {
        free(state);
        return NULL;
    }

    /* Image 0 is always the screen */
    state->images[0] = screen;
    state->nimages = 1;

    return state;
}

/*
 * Clean up draw state
 */
static void drawstate_destroy(DrawState *state)
{
    int i;

    if (state == NULL) {
        return;
    }

    /* Free cached images (except screen which is owned by main) */
    for (i = 1; i < state->nimages; i++) {
        if (state->images[i] != NULL) {
            memimage_free(state->images[i]);
        }
    }

    if (state->images != NULL) {
        free(state->images);
    }

    /* Don't free screen - it's owned by main.c */

    free(state);
}

/*
 * Add image to cache
 */
static int drawstate_add_image(DrawState *state, Memimage *img)
{
    if (state->nimages >= state->image_capacity) {
        int new_capacity;
        Memimage **new_images;

        new_capacity = state->image_capacity * 2;
        new_images = (Memimage **)realloc(state->images,
                                          sizeof(Memimage *) * new_capacity);
        if (new_images == NULL) {
            return -1;
        }

        state->images = new_images;
        state->image_capacity = new_capacity;
    }

    state->images[state->nimages] = img;
    return state->nimages++;
}

/*
 * Process D_rect command
 * Format: [opcode][x0][y0][x1][y1][color]
 * All values are 32-bit integers
 */
static int process_drect(const uint8_t *buf, size_t len)
{
    Rectangle r;
    unsigned long color;
    uint32_t x0, y0, x1, y1, color_val;

    if (len < 20) {
        return -1;  /* Not enough data */
    }

    x0 = le_get32(buf + 4);
    y0 = le_get32(buf + 8);
    x1 = le_get32(buf + 12);
    y1 = le_get32(buf + 16);
    color_val = le_get32(buf + 20);

    r.min.x = (int)x0;
    r.min.y = (int)y0;
    r.max.x = (int)x1;
    r.max.y = (int)y1;
    color = color_val;

    if (g_draw_state == NULL || g_draw_state->screen == NULL) {
        return -1;
    }

    memfillcolor_rect(g_draw_state->screen, r, color);
    g_draw_state->refresh_needed = 1;

    return 0;
}

/*
 * Read from /dev/draw
 * Returns status information
 */
static ssize_t devdraw_read(char *buf, size_t count, uint64_t offset,
                            DrawState *state)
{
    char status[256];
    int len;

    (void)offset;

    if (state == NULL) {
        return 0;
    }

    /* Use sprintf instead of snprintf for C89 compatibility */
    len = sprintf(status,
                  "refresh=%d\nimages=%d\nclient=%d\n",
                  state->refresh_needed,
                  state->nimages,
                  state->client_id);

    if (len < 0 || (size_t)len > sizeof(status)) {
        return 0;
    }

    if (len > count) {
        len = count;
    }

    memcpy(buf, status, len);

    return len;
}

/*
 * Write to /dev/draw
 * Processes binary drawing commands
 */
static ssize_t devdraw_write(const char *buf, size_t count, uint64_t offset,
                             DrawState *state)
{
    const uint8_t *ubuf = (const uint8_t *)buf;
    uint8_t opcode;

    (void)offset;

    if (state == NULL || buf == NULL || count < 1) {
        return -1;
    }

    opcode = ubuf[0];

    switch (opcode) {
    case D_rect:
        if (process_drect(ubuf, count) < 0) {
            fprintf(stderr, "devdraw_write: D_rect failed\n");
            return -1;
        }
        break;

    case D_alloc:
    case D_free:
    case D_draw:
    case D_line:
    case D_poly:
    case D_text:
    case D_set:
        fprintf(stderr, "devdraw_write: opcode %d not yet implemented\n", opcode);
        break;

    default:
        fprintf(stderr, "devdraw_write: unknown opcode %d\n", opcode);
        return -1;
    }

    return count;
}

/*
 * Initialize /dev/draw device
 */
int devdraw_init(P9Node *dev_dir, Memimage *screen)
{
    P9Node *draw_node;

    if (dev_dir == NULL || screen == NULL) {
        return -1;
    }

    /* Create global state */
    if (g_draw_state == NULL) {
        g_draw_state = drawstate_create(screen);
        if (g_draw_state == NULL) {
            fprintf(stderr, "devdraw_init: cannot create state\n");
            return -1;
        }
    }

    /* Create /dev/draw file */
    draw_node = tree_create_file(dev_dir, "draw",
                                 g_draw_state,
                                 (P9ReadFunc)devdraw_read,
                                 (P9WriteFunc)devdraw_write);
    if (draw_node == NULL) {
        fprintf(stderr, "devdraw_init: cannot create draw node\n");
        drawstate_destroy(g_draw_state);
        g_draw_state = NULL;
        return -1;
    }

    fprintf(stderr, "devdraw_init: initialized draw device\n");

    return 0;
}

/*
 * Mark screen as needing refresh
 */
void devdraw_mark_dirty(void)
{
    if (g_draw_state != NULL) {
        g_draw_state->refresh_needed = 1;
    }
}

/*
 * Clear refresh flag
 */
void devdraw_clear_dirty(void)
{
    if (g_draw_state != NULL) {
        g_draw_state->refresh_needed = 0;
    }
}

/*
 * Check if refresh is needed
 */
int devdraw_is_dirty(void)
{
    if (g_draw_state != NULL) {
        return g_draw_state->refresh_needed;
    }
    return 0;
}

/*
 * Cleanup draw device
 */
void devdraw_cleanup(void)
{
    if (g_draw_state != NULL) {
        drawstate_destroy(g_draw_state);
        g_draw_state = NULL;
    }
}
