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
    D_ellipse = 8,  /* Draw ellipse/arc */
    D_bezier,       /* Draw bezier curve */
    D_image,        /* Load image */
    D_flush,        /* Flush operations */
    D_query,        /* Query capabilities */
    D_batch,        /* Batch operations */
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
    Rectangle clip_rect;    /* Current clipping rectangle */
    int draw_op;            /* Current draw operation */
    unsigned long color;    /* Current drawing color */
    int font_id;            /* Current font ID */
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
    state->draw_op = SoverD;  /* Default alpha blending */
    state->color = 0xFFFFFFFF;  /* Default white */
    state->font_id = 0;  /* Default font */

    /* Initialize clipping rectangle to screen bounds */
    if (screen != NULL) {
        state->clip_rect = screen->r;
    }

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
 * Validate image ID
 */
static int validate_image_id(DrawState *state, int image_id)
{
    if (state == NULL || image_id < 0 || image_id >= state->nimages) {
        return 0;
    }
    if (state->images[image_id] == NULL) {
        return 0;
    }
    return 1;
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
 * Process D_alloc command
 * Format: [opcode:1][image_id:4][x:4][y:4][width:4][height:4][chan:4]
 * Returns: Slot number in image cache
 */
static int process_dalloc(const uint8_t *buf, size_t len)
{
    Rectangle r;
    uint32_t image_id, x, y, width, height, chan;
    Memimage *img;
    int slot;

    if (len < 25) {
        fprintf(stderr, "process_dalloc: insufficient data (need 25 bytes, got %lu)\n",
                (unsigned long)len);
        return -1;
    }

    if (g_draw_state == NULL) {
        fprintf(stderr, "process_dalloc: no draw state\n");
        return -1;
    }

    /* Parse parameters */
    image_id = le_get32(buf + 1);
    x = le_get32(buf + 5);
    y = le_get32(buf + 9);
    width = le_get32(buf + 13);
    height = le_get32(buf + 17);
    chan = le_get32(buf + 21);

    /* Validate dimensions */
    if (width == 0 || height == 0) {
        fprintf(stderr, "process_dalloc: invalid dimensions %dx%d\n", width, height);
        return -1;
    }

    /* Create rectangle */
    r.min.x = (int)x;
    r.min.y = (int)y;
    r.max.x = (int)x + (int)width;
    r.max.y = (int)y + (int)height;

    /* Allocate image */
    img = memimage_alloc(r, chan);
    if (img == NULL) {
        fprintf(stderr, "process_dalloc: failed to allocate image\n");
        return -1;
    }

    /* Add to cache */
    slot = drawstate_add_image(g_draw_state, img);
    if (slot < 0) {
        fprintf(stderr, "process_dalloc: failed to add image to cache\n");
        memimage_free(img);
        return -1;
    }

    fprintf(stderr, "process_dalloc: allocated image %d (%dx%d)\n", slot, width, height);
    return slot;
}

/*
 * Process D_free command
 * Format: [opcode:1][image_id:4]
 */
static int process_dfree(const uint8_t *buf, size_t len)
{
    uint32_t image_id;

    if (len < 5) {
        fprintf(stderr, "process_dfree: insufficient data (need 5 bytes, got %lu)\n",
                (unsigned long)len);
        return -1;
    }

    if (g_draw_state == NULL) {
        fprintf(stderr, "process_dfree: no draw state\n");
        return -1;
    }

    image_id = le_get32(buf + 1);

    /* Cannot free screen (image 0) */
    if (image_id == 0) {
        fprintf(stderr, "process_dfree: cannot free screen image (id 0)\n");
        return -1;
    }

    /* Validate image ID */
    if (!validate_image_id(g_draw_state, (int)image_id)) {
        fprintf(stderr, "process_dfree: invalid image id %lu\n", (unsigned long)image_id);
        return -1;
    }

    /* Free image */
    if (g_draw_state->images[image_id] != NULL) {
        memimage_free(g_draw_state->images[image_id]);
        g_draw_state->images[image_id] = NULL;
        fprintf(stderr, "process_dfree: freed image %lu\n", (unsigned long)image_id);
    }

    return 0;
}

/*
 * Process D_draw command (bit blit)
 * Format: [opcode:1][dst_id:4][dst_x:4][dst_y:4][dst_w:4][dst_h:4]
 *         [src_id:4][src_x:4][src_y:4][mask_id:4][mask_x:4][mask_y:4][op:4]
 */
static int process_ddraw(const uint8_t *buf, size_t len)
{
    Rectangle dst_rect;
    Point src_pt, mask_pt;
    uint32_t dst_id, dst_x, dst_y, dst_w, dst_h;
    uint32_t src_id, src_x, src_y;
    uint32_t mask_id, mask_x, mask_y;
    uint32_t op;
    Memimage *dst, *src, *mask;

    if (len < 41) {
        fprintf(stderr, "process_ddraw: insufficient data (need 41 bytes, got %lu)\n",
                (unsigned long)len);
        return -1;
    }

    if (g_draw_state == NULL) {
        fprintf(stderr, "process_ddraw: no draw state\n");
        return -1;
    }

    /* Parse destination parameters */
    dst_id = le_get32(buf + 1);
    dst_x = le_get32(buf + 5);
    dst_y = le_get32(buf + 9);
    dst_w = le_get32(buf + 13);
    dst_h = le_get32(buf + 17);

    /* Parse source parameters */
    src_id = le_get32(buf + 21);
    src_x = le_get32(buf + 25);
    src_y = le_get32(buf + 29);

    /* Parse mask parameters */
    mask_id = le_get32(buf + 33);
    mask_x = le_get32(buf + 37);
    /* mask_y and op are at buf + 41 and buf + 45, but we need to check buffer */
    if (len < 45) {
        fprintf(stderr, "process_ddraw: insufficient data for mask and op\n");
        return -1;
    }
    mask_y = le_get32(buf + 41);
    op = le_get32(buf + 45);

    /* Validate image IDs */
    if (!validate_image_id(g_draw_state, (int)dst_id)) {
        fprintf(stderr, "process_ddraw: invalid dst_id %lu\n", (unsigned long)dst_id);
        return -1;
    }
    if (!validate_image_id(g_draw_state, (int)src_id)) {
        fprintf(stderr, "process_ddraw: invalid src_id %lu\n", (unsigned long)src_id);
        return -1;
    }

    dst = g_draw_state->images[dst_id];
    src = g_draw_state->images[src_id];

    /* Get mask image (if mask_id is 0xFFFFFFFF, no mask) */
    mask = NULL;
    if (mask_id != 0xFFFFFFFF) {
        if (!validate_image_id(g_draw_state, (int)mask_id)) {
            fprintf(stderr, "process_ddraw: invalid mask_id %lu\n", (unsigned long)mask_id);
            return -1;
        }
        mask = g_draw_state->images[mask_id];
    }

    /* Build destination rectangle */
    dst_rect.min.x = (int)dst_x;
    dst_rect.min.y = (int)dst_y;
    dst_rect.max.x = (int)dst_x + (int)dst_w;
    dst_rect.max.y = (int)dst_y + (int)dst_h;

    /* Build source point */
    src_pt.x = (int)src_x;
    src_pt.y = (int)src_y;

    /* Build mask point */
    mask_pt.x = (int)mask_x;
    mask_pt.y = (int)mask_y;

    /* Perform bit blit */
    memdraw(dst, dst_rect, src, src_pt, mask, mask_pt, (int)op);

    /* Mark screen as dirty if destination is screen */
    if (dst_id == 0) {
        g_draw_state->refresh_needed = 1;
    }

    return 0;
}

/*
 * Process D_line command
 * Format: [opcode:1][x0:4][y0:4][x1:4][y1:4][color:4][thickness:4]
 */
static int process_dline(const uint8_t *buf, size_t len)
{
    Point p0, p1;
    uint32_t x0, y0, x1, y1, color, thickness;

    if (len < 21) {
        fprintf(stderr, "process_dline: insufficient data (need 21 bytes, got %lu)\n",
                (unsigned long)len);
        return -1;
    }

    if (g_draw_state == NULL || g_draw_state->screen == NULL) {
        fprintf(stderr, "process_dline: no draw state or screen\n");
        return -1;
    }

    /* Parse parameters */
    x0 = le_get32(buf + 1);
    y0 = le_get32(buf + 5);
    x1 = le_get32(buf + 9);
    y1 = le_get32(buf + 13);
    color = le_get32(buf + 17);
    thickness = le_get32(buf + 21);

    /* Build points */
    p0.x = (int)x0;
    p0.y = (int)y0;
    p1.x = (int)x1;
    p1.y = (int)y1;

    /* Draw line to screen */
    memdraw_line(g_draw_state->screen, p0, p1, color, (int)thickness);
    g_draw_state->refresh_needed = 1;

    return 0;
}

/*
 * Process D_poly command
 * Format: [opcode:1][npoints:4][color:4][fill:4][x0:4][y0:4][x1:4][y1:4]...
 */
static int process_dpoly(const uint8_t *buf, size_t len)
{
    Point *points;
    uint32_t npoints, color, fill;
    uint32_t x, y;
    int i;
    int expected_len;

    if (len < 13) {
        fprintf(stderr, "process_dpoly: insufficient data (need at least 13 bytes, got %lu)\n",
                (unsigned long)len);
        return -1;
    }

    if (g_draw_state == NULL || g_draw_state->screen == NULL) {
        fprintf(stderr, "process_dpoly: no draw state or screen\n");
        return -1;
    }

    /* Parse parameters */
    npoints = le_get32(buf + 1);
    color = le_get32(buf + 5);
    fill = le_get32(buf + 9);

    /* Validate number of points */
    if (npoints < 3) {
        fprintf(stderr, "process_dpoly: need at least 3 points, got %lu\n",
                (unsigned long)npoints);
        return -1;
    }

    /* Check buffer length */
    expected_len = 13 + npoints * 8;  /* header + x,y pairs */
    if (len < expected_len) {
        fprintf(stderr, "process_dpoly: insufficient data for %lu points (need %d bytes, got %lu)\n",
                (unsigned long)npoints, expected_len, (unsigned long)len);
        return -1;
    }

    /* Allocate points array */
    points = (Point *)malloc(sizeof(Point) * npoints);
    if (points == NULL) {
        fprintf(stderr, "process_dpoly: cannot allocate points array\n");
        return -1;
    }

    /* Parse points */
    for (i = 0; i < npoints; i++) {
        x = le_get32(buf + 13 + i * 8);
        y = le_get32(buf + 13 + i * 8 + 4);
        points[i].x = (int)x;
        points[i].y = (int)y;
    }

    /* Draw polygon */
    memdraw_poly(g_draw_state->screen, points, (int)npoints, color, (int)fill);
    g_draw_state->refresh_needed = 1;

    free(points);
    return 0;
}

/*
 * Process D_text command
 * Format: [opcode:1][x:4][y:4][color:4][length:4][font_id:4][string...]
 */
static int process_dtext(const uint8_t *buf, size_t len)
{
    Point p;
    uint32_t x, y, color, length, font_id;
    char *str;
    int i;

    if (len < 21) {
        fprintf(stderr, "process_dtext: insufficient data (need at least 21 bytes, got %lu)\n",
                (unsigned long)len);
        return -1;
    }

    if (g_draw_state == NULL || g_draw_state->screen == NULL) {
        fprintf(stderr, "process_dtext: no draw state or screen\n");
        return -1;
    }

    /* Parse parameters */
    x = le_get32(buf + 1);
    y = le_get32(buf + 5);
    color = le_get32(buf + 9);
    length = le_get32(buf + 13);
    font_id = le_get32(buf + 17);

    /* Validate font ID (only 0 is supported for now) */
    if (font_id != 0) {
        fprintf(stderr, "process_dtext: unsupported font id %lu\n", (unsigned long)font_id);
        return -1;
    }

    /* Check buffer length for string */
    if (len < 21 + length) {
        fprintf(stderr, "process_dtext: insufficient data for string (need %lu bytes, got %lu)\n",
                (unsigned long)(21 + length), (unsigned long)len);
        return -1;
    }

    /* Allocate string buffer */
    str = (char *)malloc(length + 1);
    if (str == NULL) {
        fprintf(stderr, "process_dtext: cannot allocate string buffer\n");
        return -1;
    }

    /* Copy string */
    for (i = 0; i < length; i++) {
        str[i] = (char)buf[21 + i];
    }
    str[length] = '\0';

    /* Build point */
    p.x = (int)x;
    p.y = (int)y;

    /* Draw text */
    memdraw_text(g_draw_state->screen, p, str, color);
    g_draw_state->refresh_needed = 1;

    free(str);
    return 0;
}

/*
 * Process D_set command
 * Format: [opcode:1][param_type:4][param_length:4][data...]
 *
 * Parameter types:
 *  0: Clipping rectangle [x:4][y:4][w:4][h:4]
 *  1: Draw operation [op:4]
 *  2: Current color [color:4]
 *  3: Load font (future)
 */
static int process_dset(const uint8_t *buf, size_t len)
{
    uint32_t param_type, param_length;

    if (len < 9) {
        fprintf(stderr, "process_dset: insufficient data (need at least 9 bytes, got %lu)\n",
                (unsigned long)len);
        return -1;
    }

    if (g_draw_state == NULL) {
        fprintf(stderr, "process_dset: no draw state\n");
        return -1;
    }

    /* Parse parameters */
    param_type = le_get32(buf + 1);
    param_length = le_get32(buf + 5);

    /* Check buffer length */
    if (len < 9 + param_length) {
        fprintf(stderr, "process_dset: insufficient data for parameter (need %lu bytes, got %lu)\n",
                (unsigned long)(9 + param_length), (unsigned long)len);
        return -1;
    }

    switch (param_type) {
    case 0:  /* Clipping rectangle */
        {
            uint32_t x, y, w, h;
            if (param_length < 16) {
                fprintf(stderr, "process_dset: insufficient data for clip rect\n");
                return -1;
            }
            x = le_get32(buf + 9);
            y = le_get32(buf + 13);
            w = le_get32(buf + 17);
            h = le_get32(buf + 21);

            g_draw_state->clip_rect.min.x = (int)x;
            g_draw_state->clip_rect.min.y = (int)y;
            g_draw_state->clip_rect.max.x = (int)x + (int)w;
            g_draw_state->clip_rect.max.y = (int)y + (int)h;

            fprintf(stderr, "process_dset: clip rect set to (%d,%d)-(%d,%d)\n",
                    g_draw_state->clip_rect.min.x, g_draw_state->clip_rect.min.y,
                    g_draw_state->clip_rect.max.x, g_draw_state->clip_rect.max.y);
        }
        break;

    case 1:  /* Draw operation */
        {
            uint32_t op;
            if (param_length < 4) {
                fprintf(stderr, "process_dset: insufficient data for draw op\n");
                return -1;
            }
            op = le_get32(buf + 9);
            g_draw_state->draw_op = (int)op;
            fprintf(stderr, "process_dset: draw op set to %d\n", g_draw_state->draw_op);
        }
        break;

    case 2:  /* Current color */
        {
            uint32_t color;
            if (param_length < 4) {
                fprintf(stderr, "process_dset: insufficient data for color\n");
                return -1;
            }
            color = le_get32(buf + 9);
            g_draw_state->color = color;
            fprintf(stderr, "process_dset: color set to 0x%08lX\n", (unsigned long)color);
        }
        break;

    case 3:  /* Load font (not yet implemented) */
        fprintf(stderr, "process_dset: font loading not yet implemented\n");
        break;

    default:
        fprintf(stderr, "process_dset: unknown parameter type %lu\n", (unsigned long)param_type);
        return -1;
    }

    return 0;
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
    case D_alloc:
        if (process_dalloc(ubuf, count) < 0) {
            fprintf(stderr, "devdraw_write: D_alloc failed\n");
            return -1;
        }
        break;

    case D_free:
        if (process_dfree(ubuf, count) < 0) {
            fprintf(stderr, "devdraw_write: D_free failed\n");
            return -1;
        }
        break;

    case D_rect:
        if (process_drect(ubuf, count) < 0) {
            fprintf(stderr, "devdraw_write: D_rect failed\n");
            return -1;
        }
        break;

    case D_draw:
        if (process_ddraw(ubuf, count) < 0) {
            fprintf(stderr, "devdraw_write: D_draw failed\n");
            return -1;
        }
        break;

    case D_line:
        if (process_dline(ubuf, count) < 0) {
            fprintf(stderr, "devdraw_write: D_line failed\n");
            return -1;
        }
        break;

    case D_poly:
        if (process_dpoly(ubuf, count) < 0) {
            fprintf(stderr, "devdraw_write: D_poly failed\n");
            return -1;
        }
        break;

    case D_text:
        if (process_dtext(ubuf, count) < 0) {
            fprintf(stderr, "devdraw_write: D_text failed\n");
            return -1;
        }
        break;

    case D_set:
        if (process_dset(ubuf, count) < 0) {
            fprintf(stderr, "devdraw_write: D_set failed\n");
            return -1;
        }
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
