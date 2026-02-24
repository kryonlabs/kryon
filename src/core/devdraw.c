/*
 * Kryon /dev/draw Device - Plan 9 Compliant Implementation
 * C89/C90 compliant
 *
 * Implements the full Plan 9 /dev/draw protocol for drawterm compatibility
 * File structure: /dev/draw/new, /dev/draw/[n]/{ctl,data,refresh}
 */

#include "kryon.h"
#include "graphics.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

/* ========== Connection Management ========== */

static DrawConnection *g_connections[MAX_DRAW_CONNECTIONS];
static struct Memimage *g_screen = NULL;

int drawconn_init(struct Memimage *screen)
{
    int i;

    if (screen == NULL) {
        fprintf(stderr, "drawconn_init: screen is NULL\n");
        return -1;
    }

    g_screen = screen;

    for (i = 0; i < MAX_DRAW_CONNECTIONS; i++) {
        g_connections[i] = NULL;
    }

    fprintf(stderr, "drawconn_init: initialized with screen\n");
    return 0;
}

void drawconn_cleanup(void)
{
    int i;

    for (i = 0; i < MAX_DRAW_CONNECTIONS; i++) {
        if (g_connections[i] != NULL) {
            drawconn_delete(g_connections[i]->id);
        }
    }

    g_screen = NULL;
}

int drawconn_next_id(void)
{
    int id;

    for (id = 0; id < MAX_DRAW_CONNECTIONS; id++) {
        if (g_connections[id] == NULL) {
            return id;
        }
    }

    return -1;
}

DrawConnection *drawconn_new(void)
{
    DrawConnection *conn;
    int id;

    id = drawconn_next_id();
    if (id < 0) {
        fprintf(stderr, "drawconn_new: no free connection slots\n");
        return NULL;
    }

    conn = (DrawConnection *)malloc(sizeof(DrawConnection));
    if (conn == NULL) {
        fprintf(stderr, "drawconn_new: malloc failed\n");
        return NULL;
    }

    memset(conn, 0, sizeof(DrawConnection));

    conn->id = id;
    conn->screen = g_screen;
    conn->screen_id = 0;
    conn->fillimage_id = 1;
    conn->next_image_id = 2;
    conn->refresh_enabled = 1;  /* Enable refresh by default */
    conn->screen_dirty = 1;    /* Mark as dirty initially */
    conn->nimages = 0;

    /* QID path base: use high bits to avoid collision */
    conn->qid_path_base = 0x10000000ULL + ((uint64_t)id << 16);

    /* Initialize image table */
    /* Image 0 is the screen */
    conn->images[0].id = 0;
    conn->images[0].img = g_screen;
    conn->images[0].in_use = 1;
    conn->nimages = 1;

    g_connections[id] = conn;

    fprintf(stderr, "drawconn_new: created connection %d\n", id);
    return conn;
}

DrawConnection *drawconn_get(int id)
{
    if (id < 0 || id >= MAX_DRAW_CONNECTIONS) {
        return NULL;
    }

    return g_connections[id];
}

void drawconn_delete(int id)
{
    DrawConnection *conn;
    int i;

    if (id < 0 || id >= MAX_DRAW_CONNECTIONS) {
        return;
    }

    conn = g_connections[id];
    if (conn == NULL) {
        return;
    }

    /* Free images (except screen which is owned by main) */
    for (i = 1; i < conn->nimages; i++) {
        if (conn->images[i].in_use && conn->images[i].img != NULL) {
            memimage_free(conn->images[i].img);
        }
    }

    free(conn);
    g_connections[id] = NULL;

    fprintf(stderr, "drawconn_delete: deleted connection %d\n", id);
}

/* ========== /dev/draw/new Implementation ========== */

static int build_connection_info(DrawConnection *conn, char *buf, int size)
{
    int pos = 0;

    if (conn == NULL || conn->screen == NULL) {
        return -1;
    }

    if (size < 144) {
        return -1;
    }

    memset(buf, 0, 144);

    /* String 0: image name */
    sprintf(buf + pos, "screen"); pos += 12;
    /* String 1: channel format */
    sprintf(buf + pos, "RGBA32"); pos += 12;
    /* String 2: repl flag */
    sprintf(buf + pos, "0"); pos += 12;
    /* String 3: min.x */
    sprintf(buf + pos, "%d", conn->screen->r.min.x); pos += 12;
    /* String 4: min.y */
    sprintf(buf + pos, "%d", conn->screen->r.min.y); pos += 12;
    /* String 5: max.x */
    sprintf(buf + pos, "%d", conn->screen->r.max.x); pos += 12;
    /* String 6: max.y */
    sprintf(buf + pos, "%d", conn->screen->r.max.y); pos += 12;
    /* String 7: clip.min.x */
    sprintf(buf + pos, "%d", conn->screen->clipr.min.x); pos += 12;
    /* String 8: clip.min.y */
    sprintf(buf + pos, "%d", conn->screen->clipr.min.y); pos += 12;
    /* String 9: clip.max.x */
    sprintf(buf + pos, "%d", conn->screen->clipr.max.x); pos += 12;
    /* String 10: clip.max.y */
    sprintf(buf + pos, "%d", conn->screen->clipr.max.y); pos += 12;
    /* String 11: reserved */
    pos += 12;

    return 144;
}

ssize_t devdraw_new_read(char *buf, size_t count, uint64_t offset, void *data)
{
    DrawConnection *conn;
    char info[144];
    int info_len;
    size_t to_copy;

    (void)data;
    (void)offset;

    fprintf(stderr, "devdraw_new_read: drawterm requesting new connection\n");

    /* Create new connection */
    conn = drawconn_new();
    if (conn == NULL) {
        fprintf(stderr, "devdraw_new_read: failed to create connection\n");
        return -1;
    }

    /* Build connection info */
    info_len = build_connection_info(conn, info, sizeof(info));
    if (info_len < 0) {
        fprintf(stderr, "devdraw_new_read: failed to build info\n");
        return -1;
    }

    /* Copy to user buffer */
    to_copy = (size_t)info_len;
    if (to_copy > count) {
        to_copy = count;
    }

    memcpy(buf, info, to_copy);

    fprintf(stderr, "devdraw_new_read: returning %d bytes for connection %d\n",
            (int)to_copy, conn->id);

    return (ssize_t)to_copy;
}

int devdraw_new_init(P9Node *draw_dir)
{
    P9Node *new_node;

    if (draw_dir == NULL) {
        return -1;
    }

    /* Create /dev/draw/new file */
    new_node = tree_create_file(draw_dir, "new", NULL,
                                (P9ReadFunc)devdraw_new_read, NULL);
    if (new_node == NULL) {
        fprintf(stderr, "devdraw_new_init: cannot create new node\n");
        return -1;
    }

    fprintf(stderr, "devdraw_new_init: initialized /dev/draw/new\n");

    return 0;
}

/* ========== /dev/draw/[n] Directory Creation ========== */

P9Node *drawconn_create_dir(int conn_id)
{
    DrawConnection *conn;
    P9Node *dir_node;
    P9Node *ctl_node;
    P9Node *data_node;
    P9Node *refresh_node;
    char dirname[16];

    fprintf(stderr, "drawconn_create_dir: conn_id=%d creating directory\n", conn_id);

    /* Check if connection exists */
    conn = drawconn_get(conn_id);
    if (conn == NULL) {
        fprintf(stderr, "drawconn_create_dir: connection %d not found\n", conn_id);
        return NULL;
    }

    /* Create directory name */
    sprintf(dirname, "%d", conn_id);

    /* Create directory node */
    dir_node = (P9Node *)malloc(sizeof(P9Node));
    if (dir_node == NULL) {
        return NULL;
    }

    memset(dir_node, 0, sizeof(P9Node));

    dir_node->name = (char *)malloc(strlen(dirname) + 1);
    if (dir_node->name == NULL) {
        free(dir_node);
        return NULL;
    }
    strcpy(dir_node->name, dirname);

    dir_node->qid.type = QTDIR;
    dir_node->qid.version = 0;
    dir_node->qid.path = conn->qid_path_base;
    dir_node->mode = P9_DMDIR | 0555;
    dir_node->atime = (uint32_t)time(NULL);
    dir_node->mtime = dir_node->atime;
    dir_node->length = 0;
    dir_node->data = conn;
    dir_node->parent = NULL;
    dir_node->children = NULL;
    dir_node->nchildren = 0;
    dir_node->capacity = 0;

    /* Create ctl file */
    ctl_node = tree_create_file(dir_node, "ctl", conn,
                                (P9ReadFunc)devdraw_ctl_read,
                                (P9WriteFunc)devdraw_ctl_write);
    if (ctl_node == NULL) {
        fprintf(stderr, "drawconn_create_dir: failed to create ctl\n");
        free(dir_node->name);
        free(dir_node);
        return NULL;
    }
    ctl_node->qid.path = conn->qid_path_base + 1;

    /* Create data file */
    data_node = tree_create_file(dir_node, "data", conn,
                                 (P9ReadFunc)devdraw_data_read,
                                 (P9WriteFunc)devdraw_data_write);
    if (data_node == NULL) {
        fprintf(stderr, "drawconn_create_dir: failed to create data\n");
        free(ctl_node);
        free(dir_node->name);
        free(dir_node);
        return NULL;
    }
    data_node->qid.path = conn->qid_path_base + 2;

    /* Create refresh file */
    refresh_node = tree_create_file(dir_node, "refresh", conn,
                                    (P9ReadFunc)devdraw_refresh_read,
                                    NULL);
    if (refresh_node == NULL) {
        fprintf(stderr, "drawconn_create_dir: failed to create refresh\n");
        free(data_node);
        free(ctl_node);
        free(dir_node->name);
        free(dir_node);
        return NULL;
    }
    refresh_node->qid.path = conn->qid_path_base + 3;

    fprintf(stderr, "drawconn_create_dir: created /dev/draw/%d/\n", conn_id);

    return dir_node;
}

/* ========== /dev/draw/[n]/ctl Implementation ========== */

ssize_t devdraw_ctl_read(char *buf, size_t count, uint64_t offset, void *data)
{
    DrawConnection *conn = (DrawConnection *)data;
    char status[256];
    int len;

    if (conn == NULL) {
        return -1;
    }

    /* Build status string */
    len = sprintf(status,
                  "id=%d\n"
                  "screen_id=%d\n"
                  "refresh=%d\n"
                  "nimages=%d\n"
                  "screen_rect=%d,%d-%d,%d\n",
                  conn->id,
                  conn->screen_id,
                  conn->refresh_enabled,
                  conn->nimages,
                  conn->screen->r.min.x, conn->screen->r.min.y,
                  conn->screen->r.max.x, conn->screen->r.max.y);

    if (len < 0 || len > sizeof(status)) {
        return -1;
    }

    /* Handle offset */
    if (offset >= (uint64_t)len) {
        return 0;
    }

    len -= (int)offset;
    if (len > count) {
        len = count;
    }

    memcpy(buf, status + offset, len);

    return len;
}

ssize_t devdraw_ctl_write(const char *buf, size_t count, uint64_t offset, void *data)
{
    DrawConnection *conn = (DrawConnection *)data;
    char cmd[32];
    size_t cmd_len;

    (void)offset;

    if (conn == NULL) {
        return -1;
    }

    if (count == 0 || buf == NULL) {
        return 0;
    }

    /* Extract command (first word) */
    cmd_len = count < sizeof(cmd) - 1 ? count : sizeof(cmd) - 1;
    memcpy(cmd, buf, cmd_len);
    cmd[cmd_len] = '\0';

    /* Parse commands */
    if (strcmp(cmd, "refresh") == 0) {
        conn->refresh_enabled = 1;
        fprintf(stderr, "devdraw_ctl_write: conn %d refresh enabled\n", conn->id);
    } else if (strcmp(cmd, "norefresh") == 0) {
        conn->refresh_enabled = 0;
        fprintf(stderr, "devdraw_ctl_write: conn %d refresh disabled\n", conn->id);
    } else {
        fprintf(stderr, "devdraw_ctl_write: conn %d unknown command '%s'\n", conn->id, cmd);
    }

    return count;
}

/* ========== /dev/draw/[n]/data Implementation ========== */

ssize_t devdraw_data_read(char *buf, size_t count, uint64_t offset, void *data)
{
    DrawConnection *conn = (DrawConnection *)data;
    Memimage *screen;
    unsigned char *pixel_data;
    size_t total_size;
    size_t bytes_to_copy;

    if (conn == NULL || conn->screen == NULL) {
        return -1;
    }

    screen = conn->screen;

    /* Get screen pixel data */
    pixel_data = screen->data->bdata;
    total_size = Dx(screen->r) * Dy(screen->r) * 4;

    fprintf(stderr, "devdraw_data_read: conn=%d offset=%lu count=%lu total_size=%lu\n",
            conn->id, (unsigned long)offset, (unsigned long)count, (unsigned long)total_size);

    /* Check offset bounds */
    if (offset >= total_size) {
        return 0;  /* EOF */
    }

    /* Calculate bytes to copy */
    if (offset + count > total_size) {
        bytes_to_copy = total_size - offset;
    } else {
        bytes_to_copy = count;
    }

    /* Copy pixel data to buffer */
    memcpy(buf, pixel_data + offset, bytes_to_copy);

    fprintf(stderr, "devdraw_data_read: returning %lu bytes\n", (unsigned long)bytes_to_copy);

    return bytes_to_copy;
}

/* ========== Graphics Protocol Implementation ========== */

static int drawconn_add_image(DrawConnection *conn, Memimage *img, uint32_t id)
{
    int slot;

    if (conn == NULL || img == NULL) {
        return -1;
    }

    /* Use specified ID or allocate next */
    if (id != (uint32_t)-1) {
        slot = (int)id;
    } else {
        slot = conn->next_image_id;
    }

    if (slot < 0 || slot >= MAX_IMAGES_PER_CONNECTION) {
        fprintf(stderr, "drawconn_add_image: invalid image id %d\n", slot);
        return -1;
    }

    if (conn->images[slot].in_use) {
        fprintf(stderr, "drawconn_add_image: image slot %d already in use\n", slot);
        return -1;
    }

    conn->images[slot].id = slot;
    conn->images[slot].img = img;
    conn->images[slot].in_use = 1;

    if (slot >= conn->nimages) {
        conn->nimages = slot + 1;
    }

    if (slot == conn->next_image_id) {
        conn->next_image_id = slot + 1;
    }

    fprintf(stderr, "drawconn_add_image: conn %d allocated image %d\n", conn->id, slot);
    return slot;
}

static Memimage *drawconn_get_image(DrawConnection *conn, int id)
{
    if (conn == NULL || id < 0 || id >= conn->nimages) {
        return NULL;
    }

    if (!conn->images[id].in_use) {
        return NULL;
    }

    return conn->images[id].img;
}

static void parse_rectangle(const uint8_t *buf, Rectangle *r)
{
    int minx, miny, maxx, maxy;
    minx = (int)le_get32(buf + 0);
    miny = (int)le_get32(buf + 4);
    maxx = (int)le_get32(buf + 8);
    maxy = (int)le_get32(buf + 12);
    r->min.x = minx;
    r->min.y = miny;
    r->max.x = maxx;
    r->max.y = maxy;
}

static void parse_point(const uint8_t *buf, Point *p)
{
    p->x = (int)le_get32(buf + 0);
    p->y = (int)le_get32(buf + 4);
}

/* Process 'b' message - Allocate image */
static int process_b_msg(DrawConnection *conn, const uint8_t *buf, size_t len)
{
    uint32_t id, screenid, chan;
    uint8_t refresh, repl;
    Rectangle r, clipr;
    uint32_t rrggbbaa;
    Memimage *img;
    int minx, miny, maxx, maxy;

    if (len < 52) {
        fprintf(stderr, "process_b_msg: insufficient data\n");
        return -1;
    }

    id = le_get32(buf + 1);
    screenid = le_get32(buf + 5);
    refresh = buf[9];
    chan = le_get32(buf + 10);
    repl = buf[14];

    /* Parse rectangle R (16 bytes) */
    minx = (int)le_get32(buf + 15);
    miny = (int)le_get32(buf + 19);
    maxx = (int)le_get32(buf + 23);
    maxy = (int)le_get32(buf + 27);
    r.min.x = minx;
    r.min.y = miny;
    r.max.x = maxx;
    r.max.y = maxy;

    /* Parse clip rectangle (16 bytes) */
    minx = (int)le_get32(buf + 31);
    miny = (int)le_get32(buf + 35);
    maxx = (int)le_get32(buf + 39);
    maxy = (int)le_get32(buf + 43);
    clipr.min.x = minx;
    clipr.min.y = miny;
    clipr.max.x = maxx;
    clipr.max.y = maxy;

    rrggbbaa = le_get32(buf + 47);

    /* Create image */
    img = memimage_alloc(r, chan);
    if (img == NULL) {
        fprintf(stderr, "process_b_msg: failed to allocate image\n");
        return -1;
    }

    /* Set clipping rectangle */
    memimage_setclipr(img, clipr);

    /* Fill with color if specified */
    if (rrggbbaa != 0) {
        memfillcolor(img, rrggbbaa);
    }

    /* Add to connection's image table */
    if (drawconn_add_image(conn, img, id) < 0) {
        memimage_free(img);
        return -1;
    }

    return 0;
}

/* Process 'n' message - Named image (screen) */
static int process_n_msg(DrawConnection *conn, const uint8_t *buf, size_t len, char *response, int *resp_len)
{
    uint32_t id;
    uint8_t n;
    char name[32];

    if (len < 6) {
        fprintf(stderr, "process_n_msg: insufficient data\n");
        return -1;
    }

    id = le_get32(buf + 1);
    n = buf[5];

    if (len < 6 + n) {
        fprintf(stderr, "process_n_msg: insufficient data for name\n");
        return -1;
    }

    memcpy(name, buf + 6, n);
    name[n] = '\0';

    fprintf(stderr, "process_n_msg: conn %d id=%lu name='%s'\n",
            conn->id, (unsigned long)id, name);

    /* Check if requesting screen */
    if (strcmp(name, "screen") == 0) {
        /* Build 144-byte response with screen info */
        int info_len = build_connection_info(conn, response, 144);
        if (info_len < 0) {
            return -1;
        }
        *resp_len = 144;
        return 0;
    }

    fprintf(stderr, "process_n_msg: unknown name '%s'\n", name);
    return -1;
}

/* Process 'f' message - Free image */
static int process_f_msg(DrawConnection *conn, const uint8_t *buf, size_t len)
{
    uint32_t id;
    Memimage *img;

    if (len < 5) {
        fprintf(stderr, "process_f_msg: insufficient data\n");
        return -1;
    }

    id = le_get32(buf + 1);

    /* Cannot free screen (id 0) */
    if (id == 0) {
        fprintf(stderr, "process_f_msg: cannot free screen\n");
        return -1;
    }

    if (id >= MAX_IMAGES_PER_CONNECTION) {
        fprintf(stderr, "process_f_msg: invalid image id\n");
        return -1;
    }

    if (!conn->images[id].in_use) {
        fprintf(stderr, "process_f_msg: image not in use\n");
        return -1;
    }

    img = conn->images[id].img;
    if (img != NULL) {
        memimage_free(img);
    }

    conn->images[id].in_use = 0;
    conn->images[id].img = NULL;

    return 0;
}

/* Process 'A' message - Allocate screen */
static int process_A_msg(DrawConnection *conn, const uint8_t *buf, size_t len)
{
    uint32_t id, imageid, fillid;
    uint8_t public;
    Memimage *image;

    if (len < 13) {
        fprintf(stderr, "process_A_msg: insufficient data\n");
        return -1;
    }

    id = le_get32(buf + 1);
    imageid = le_get32(buf + 5);
    fillid = le_get32(buf + 9);
    public = buf[12];

    fprintf(stderr, "process_A_msg: conn %d id=%lu imageid=%lu fillid=%lu public=%d\n",
            conn->id, (unsigned long)id, (unsigned long)imageid, (unsigned long)fillid, public);

    /* Get images */
    image = drawconn_get_image(conn, (int)imageid);

    if (image == NULL) {
        fprintf(stderr, "process_A_msg: image %lu not found\n", (unsigned long)imageid);
        return -1;
    }

    /* For now, just validate - screen allocation is handled by connection setup */
    conn->screen_id = (int)imageid;
    conn->fillimage_id = (int)fillid;

    return 0;
}

/* Process 'c' message - Set repl and clip */
static int process_c_msg(DrawConnection *conn, const uint8_t *buf, size_t len)
{
    uint32_t dstid;
    uint8_t repl;
    Rectangle clipr;
    Memimage *dst;

    if (len < 21) {
        fprintf(stderr, "process_c_msg: insufficient data\n");
        return -1;
    }

    dstid = le_get32(buf + 1);
    repl = buf[5];
    parse_rectangle(buf + 6, &clipr);

    dst = drawconn_get_image(conn, (int)dstid);
    if (dst == NULL) {
        fprintf(stderr, "process_c_msg: dst image %lu not found\n", (unsigned long)dstid);
        return -1;
    }

    memimage_setclipr(dst, clipr);
    fprintf(stderr, "process_c_msg: set clipr for image %lu\n", (unsigned long)dstid);

    return 0;
}

/* Process 'd' message - Draw (bit blit) */
static int process_d_msg(DrawConnection *conn, const uint8_t *buf, size_t len)
{
    uint32_t dstid, srcid, maskid;
    Rectangle r;
    Point sp, mp;
    Memimage *dst, *src, *mask;
    int op;

    if (len < 45) {
        fprintf(stderr, "process_d_msg: insufficient data\n");
        return -1;
    }

    dstid = le_get32(buf + 1);
    srcid = le_get32(buf + 5);
    maskid = le_get32(buf + 9);
    parse_rectangle(buf + 13, &r);
    parse_point(buf + 29, &sp);
    parse_point(buf + 37, &mp);

    dst = drawconn_get_image(conn, (int)dstid);
    src = drawconn_get_image(conn, (int)srcid);
    mask = drawconn_get_image(conn, (int)maskid);

    if (dst == NULL || src == NULL) {
        fprintf(stderr, "process_d_msg: dst or src image not found\n");
        return -1;
    }

    op = SoverD;  /* Default op */

    memdraw(dst, r, src, sp, mask, mp, op);
    fprintf(stderr, "process_d_msg: drew %lu->%lu\n", (unsigned long)srcid, (unsigned long)dstid);

    return 0;
}

/* Process 'e' message - Ellipse */
static int process_e_msg(DrawConnection *conn, const uint8_t *buf, size_t len)
{
    uint32_t dstid, srcid;
    Point center, sp;
    uint32_t a, b, thick;
    Memimage *dst, *src;

    if (len < 37) {
        fprintf(stderr, "process_e_msg: insufficient data\n");
        return -1;
    }

    dstid = le_get32(buf + 1);
    srcid = le_get32(buf + 5);
    parse_point(buf + 9, &center);
    a = le_get32(buf + 17);
    b = le_get32(buf + 21);
    thick = le_get32(buf + 25);
    parse_point(buf + 29, &sp);

    dst = drawconn_get_image(conn, (int)dstid);
    src = drawconn_get_image(conn, (int)srcid);

    if (dst == NULL || src == NULL) {
        fprintf(stderr, "process_e_msg: dst or src image not found\n");
        return -1;
    }

    memdraw_ellipse(dst, center, (int)a, (int)b, 0xFF0000FF, (int)thick > 0);

    return 0;
}

/* Process 'L' message - Line */
static int process_L_msg(DrawConnection *conn, const uint8_t *buf, size_t len)
{
    uint32_t dstid, srcid;
    Point p0, p1, sp;
    uint32_t end0, end1, radius;
    Memimage *dst, *src;
    unsigned long color;

    if (len < 41) {
        fprintf(stderr, "process_L_msg: insufficient data\n");
        return -1;
    }

    dstid = le_get32(buf + 1);
    parse_point(buf + 5, &p0);
    parse_point(buf + 13, &p1);
    end0 = le_get32(buf + 21);
    end1 = le_get32(buf + 25);
    radius = le_get32(buf + 29);
    srcid = le_get32(buf + 33);
    parse_point(buf + 37, &sp);

    dst = drawconn_get_image(conn, (int)dstid);
    src = drawconn_get_image(conn, (int)srcid);

    if (dst == NULL) {
        fprintf(stderr, "process_L_msg: dst image not found\n");
        return -1;
    }

    color = 0xFFFFFFFF;

    memdraw_line(dst, p0, p1, color, (int)radius);

    return 0;
}

/* Process 'p' message - Polygon */
static int process_p_msg(DrawConnection *conn, const uint8_t *buf, size_t len)
{
    uint32_t dstid, srcid;
    uint16_t n;
    uint32_t end0, end1, radius;
    Point *points, sp;
    Memimage *dst, *src;
    unsigned long color;
    int i, expected_len;

    if (len < 27) {
        fprintf(stderr, "process_p_msg: insufficient data\n");
        return -1;
    }

    dstid = le_get32(buf + 1);
    n = le_get32(buf + 5);
    end0 = le_get32(buf + 7);
    end1 = le_get32(buf + 11);
    radius = le_get32(buf + 15);
    srcid = le_get32(buf + 19);
    parse_point(buf + 23, &sp);

    expected_len = 27 + n * 8;
    if (len < expected_len) {
        fprintf(stderr, "process_p_msg: insufficient data for %d points\n", n);
        return -1;
    }

    dst = drawconn_get_image(conn, (int)dstid);
    src = drawconn_get_image(conn, (int)srcid);

    if (dst == NULL) {
        fprintf(stderr, "process_p_msg: dst image not found\n");
        return -1;
    }

    points = (Point *)malloc(sizeof(Point) * (n + 1));
    if (points == NULL) {
        fprintf(stderr, "process_p_msg: malloc failed\n");
        return -1;
    }

    /* Parse first point */
    parse_point(buf + 31, &points[0]);

    /* Parse delta points */
    for (i = 1; i <= n; i++) {
        int dx = (int)le_get32(buf + 31 + i * 8);
        int dy = (int)le_get32(buf + 31 + i * 8 + 4);
        points[i].x = points[i-1].x + dx;
        points[i].y = points[i-1].y + dy;
    }

    color = 0xFFFFFFFF;
    memdraw_poly(dst, points, n + 1, color, 0);

    free(points);

    return 0;
}

/* Process 'r' message - Read image (stub) */
static int process_r_msg(DrawConnection *conn, const uint8_t *buf, size_t len)
{
    uint32_t id;
    Rectangle r;
    Memimage *img;

    if (len < 20) {
        fprintf(stderr, "process_r_msg: insufficient data\n");
        return -1;
    }

    id = le_get32(buf + 1);
    parse_rectangle(buf + 5, &r);

    img = drawconn_get_image(conn, (int)id);
    if (img == NULL) {
        fprintf(stderr, "process_r_msg: image %lu not found\n", (unsigned long)id);
        return -1;
    }

    /* TODO: Queue read data for client to read */
    fprintf(stderr, "process_r_msg: read request for image %lu (not fully implemented)\n",
            (unsigned long)id);

    return 0;
}

/* Process 'y' message - Write image data (stub) */
static int process_y_msg(DrawConnection *conn, const uint8_t *buf, size_t len)
{
    uint32_t id;
    Rectangle r;
    Memimage *img;
    int data_len;
    const uint8_t *data;

    if (len < 20) {
        fprintf(stderr, "process_y_msg: insufficient data\n");
        return -1;
    }

    id = le_get32(buf + 1);
    parse_rectangle(buf + 5, &r);

    img = drawconn_get_image(conn, (int)id);
    if (img == NULL) {
        fprintf(stderr, "process_y_msg: image %lu not found\n", (unsigned long)id);
        return -1;
    }

    /* Copy pixel data */
    data = buf + 20;
    data_len = len - 20;

    /* TODO: Implement actual pixel loading */
    fprintf(stderr, "process_y_msg: write %d bytes to image %lu (not fully implemented)\n",
            data_len, (unsigned long)id);

    return 0;
}

/* Process 'O' message - Set drawing operation */
static int process_O_msg(DrawConnection *conn, const uint8_t *buf, size_t len)
{
    uint8_t op;

    if (len < 2) {
        fprintf(stderr, "process_O_msg: insufficient data\n");
        return -1;
    }

    op = buf[1];

    fprintf(stderr, "process_O_msg: set op to %d\n", op);

    /* Store op in connection for next draw operation */
    /* TODO: Implement op storage */

    return 0;
}

/* Main Plan 9 graphics message processor */
int process_draw_messages(DrawConnection *conn, const char *buf, size_t count,
                          char *response, int *resp_len)
{
    const uint8_t *ubuf = (const uint8_t *)buf;
    uint8_t opcode;
    size_t pos;

    if (conn == NULL || buf == NULL || count < 1) {
        return -1;
    }

    *resp_len = 0;
    pos = 0;

    while (pos < count) {
        opcode = ubuf[pos];

        switch (opcode) {
        case 'b':
            if (pos + 52 > count) {
                fprintf(stderr, "process_draw_messages: incomplete 'b' message\n");
                return -1;
            }
            if (process_b_msg(conn, ubuf + pos, count - pos) < 0) {
                return -1;
            }
            pos += 52;
            break;

        case 'n': {
            int n;
            if (pos + 6 > count) {
                fprintf(stderr, "process_draw_messages: incomplete 'n' message\n");
                return -1;
            }
            n = ubuf[pos + 5];
            if (pos + 6 + n > count) {
                fprintf(stderr, "process_draw_messages: incomplete 'n' message name\n");
                return -1;
            }
            if (process_n_msg(conn, ubuf + pos, count - pos, response + *resp_len, &n) < 0) {
                return -1;
            }
            *resp_len += n;
            pos += 6 + ubuf[pos + 5];
            break;
        }

        case 'f':
            if (pos + 5 > count) {
                fprintf(stderr, "process_draw_messages: incomplete 'f' message\n");
                return -1;
            }
            if (process_f_msg(conn, ubuf + pos, count - pos) < 0) {
                return -1;
            }
            pos += 5;
            break;

        case 'A':
            if (pos + 13 > count) {
                fprintf(stderr, "process_draw_messages: incomplete 'A' message\n");
                return -1;
            }
            if (process_A_msg(conn, ubuf + pos, count - pos) < 0) {
                return -1;
            }
            pos += 13;
            break;

        case 'c':
            if (pos + 21 > count) {
                fprintf(stderr, "process_draw_messages: incomplete 'c' message\n");
                return -1;
            }
            if (process_c_msg(conn, ubuf + pos, count - pos) < 0) {
                return -1;
            }
            pos += 21;
            break;

        case 'd':
            if (pos + 45 > count) {
                fprintf(stderr, "process_draw_messages: incomplete 'd' message\n");
                return -1;
            }
            if (process_d_msg(conn, ubuf + pos, count - pos) < 0) {
                return -1;
            }
            pos += 45;
            break;

        case 'e':
        case 'E':
            if (pos + 45 > count) {
                fprintf(stderr, "process_draw_messages: incomplete 'e' message\n");
                return -1;
            }
            if (process_e_msg(conn, ubuf + pos, count - pos) < 0) {
                return -1;
            }
            pos += 45;
            break;

        case 'L':
            if (pos + 41 > count) {
                fprintf(stderr, "process_draw_messages: incomplete 'L' message\n");
                return -1;
            }
            if (process_L_msg(conn, ubuf + pos, count - pos) < 0) {
                return -1;
            }
            pos += 41;
            break;

        case 'p':
        case 'P': {
            if (pos + 27 > count) {
                fprintf(stderr, "process_draw_messages: incomplete 'p' message header\n");
                return -1;
            }
            if (process_p_msg(conn, ubuf + pos, count - pos) < 0) {
                return -1;
            }
            uint16_t n = le_get32(ubuf + pos + 5);
            pos += 27 + n * 8;
            break;
        }

        case 'r':
            if (pos + 20 > count) {
                fprintf(stderr, "process_draw_messages: incomplete 'r' message\n");
                return -1;
            }
            if (process_r_msg(conn, ubuf + pos, count - pos) < 0) {
                return -1;
            }
            pos += 20;
            break;

        case 'y':
        case 'Y': {
            if (pos + 20 > count) {
                fprintf(stderr, "process_draw_messages: incomplete 'y' message header\n");
                return -1;
            }
            int msg_len = 20 + (count - pos - 20);
            if (process_y_msg(conn, ubuf + pos, msg_len) < 0) {
                return -1;
            }
            pos += count - pos;
            break;
        }

        case 'O':
            if (pos + 2 > count) {
                fprintf(stderr, "process_draw_messages: incomplete 'O' message\n");
                return -1;
            }
            if (process_O_msg(conn, ubuf + pos, count - pos) < 0) {
                return -1;
            }
            pos += 2;
            break;

        default:
            fprintf(stderr, "process_draw_messages: unknown opcode '%c' (0x%02X) at pos %lu\n",
                    opcode > 32 ? opcode : '?', opcode, (unsigned long)pos);
            return -1;
        }
    }

    return 0;
}

ssize_t devdraw_data_write(const char *buf, size_t count, uint64_t offset, void *data)
{
    DrawConnection *conn = (DrawConnection *)data;
    char response[256];
    int resp_len;

    (void)offset;

    if (conn == NULL || buf == NULL || count < 1) {
        return -1;
    }

    fprintf(stderr, "devdraw_data_write: conn %d len %lu\n",
            conn->id, (unsigned long)count);

    /* Process messages */
    if (process_draw_messages(conn, buf, count, response, &resp_len) < 0) {
        fprintf(stderr, "devdraw_data_write: processing failed\n");
        return -1;
    }

    return count;
}

/* ========== /dev/draw/[n]/refresh Implementation ========== */

ssize_t devdraw_refresh_read(char *buf, size_t count, uint64_t offset, void *data)
{
    DrawConnection *conn = (DrawConnection *)data;
    uint32_t *msg;
    int width, height;

    if (conn == NULL) {
        return -1;
    }

    /* If screen is clean, return 0 (blocks until dirty) */
    if (!conn->screen_dirty) {
        return 0;
    }

    /* Plan 9 refresh rectangle format: 5 * 4 = 20 bytes */
    /* Format: [image_id(4), min_x(4), min_y(4), max_x(4), max_y(4)] */
    if (count < 20) {
        return 0;
    }

    msg = (uint32_t *)buf;

    /* Get screen dimensions */
    width = conn->screen->r.max.x - conn->screen->r.min.x;
    height = conn->screen->r.max.y - conn->screen->r.min.y;

    /* Build Plan 9 refresh rectangle */
    msg[0] = 0;       /* image_id (0 = screen) */
    msg[1] = 0;       /* min.x */
    msg[2] = 0;       /* min.y */
    msg[3] = width;   /* max.x */
    msg[4] = height;  /* max.y */

    fprintf(stderr, "devdraw_refresh_read: conn=%d dirty=%d sending refresh %dx%d\n",
            conn->id, conn->screen_dirty, width, height);

    /* Clear dirty flag after sending notification */
    conn->screen_dirty = 0;

    return 20;  /* Size of one refresh rectangle */
}

/*
 * Mark all connections as needing refresh
 */
void drawconn_mark_dirty_all(void)
{
    int i;
    int count = 0;

    for (i = 0; i < MAX_DRAW_CONNECTIONS; i++) {
        if (g_connections[i] != NULL && g_connections[i]->refresh_enabled) {
            g_connections[i]->screen_dirty = 1;
            count++;
        }
    }

    if (count > 0) {
        fprintf(stderr, "drawconn_mark_dirty_all: marking %d connections dirty\n", count);
    }
}

/*
 * Compatibility functions for existing code
 */

/* Mark screen as needing refresh (for render.c) */
void devdraw_mark_dirty(void)
{
    drawconn_mark_dirty_all();
}

/* Clear refresh flag (for render.c) */
void devdraw_clear_dirty(void)
{
    /* No-op in Plan 9 mode */
}

/* Check if refresh is needed (for render.c) */
int devdraw_is_dirty(void)
{
    /* Always return 1 to trigger rendering */
    return 1;
}
