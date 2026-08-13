#include "krb.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint16_t
rd_u16(const unsigned char *p)
{
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static int16_t
rd_i16(const unsigned char *p)
{
    return (int16_t)rd_u16(p);
}

static uint32_t
rd_u32(const unsigned char *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static int
in_range(const KrbImage *img, const void *ptr, size_t n)
{
    const unsigned char *p = (const unsigned char *)ptr;

    return p >= img->bytes && p + n <= img->bytes + img->len;
}

static void
reset_image(KrbImage *img)
{
    memset(img, 0, sizeof(*img));
}

int
KrbLoad(KrbImage *img, const unsigned char *bytes, size_t len)
{
    const unsigned char *p;
    uint32_t node_count;
    uint32_t string_bytes;
    uint32_t prog_bytes;
    uint32_t import_count;
    size_t need;

    if(img == NULL || bytes == NULL)
        return -1;
    reset_image(img);
    if(len < 32 || rd_u32(bytes) != KRB_MAGIC)
        return -1;
    if(rd_u16(bytes + 4) != KRB_VERSION)
        return -1;
    node_count = rd_u32(bytes + 8);
    string_bytes = rd_u32(bytes + 12);
    prog_bytes = rd_u32(bytes + 16);
    import_count = rd_u32(bytes + 20);
    need = 32 + (size_t)node_count * KRB_NODE_SIZE + string_bytes +
           prog_bytes + (size_t)import_count * 4;
    if(len < need)
        return -1;
    p = bytes + 32;
    img->bytes = bytes;
    img->len = len;
    img->nodes = p;
    p += (size_t)node_count * KRB_NODE_SIZE;
    img->strings = (const char *)p;
    p += string_bytes;
    img->prog = p;
    p += prog_bytes;
    img->imports = (const uint32_t *)p;
    img->header = (const KrbHeader *)bytes;
    if(string_bytes == 0 || img->strings[0] != '\0')
        return -1;
    (void)import_count;
    return 0;
}

int
KrbLoadFile(KrbImage *img, const char *path)
{
    FILE *f;
    unsigned char *buf;
    long size;

    if(img == NULL || path == NULL)
        return -1;
    reset_image(img);
    f = fopen(path, "rb");
    if(f == NULL)
        return -1;
    if(fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return -1;
    }
    size = ftell(f);
    if(size < 32 || fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return -1;
    }
    buf = malloc((size_t)size);
    if(buf == NULL) {
        fclose(f);
        return -1;
    }
    if(fread(buf, 1, (size_t)size, f) != (size_t)size) {
        free(buf);
        fclose(f);
        return -1;
    }
    fclose(f);
    if(KrbLoad(img, buf, (size_t)size) != 0) {
        free(buf);
        return -1;
    }
    img->owned = 1;
    return 0;
}

void
KrbFree(KrbImage *img)
{
    if(img == NULL)
        return;
    if(img->owned)
        free((void *)img->bytes);
    reset_image(img);
}

const char *
KrbString(const KrbImage *img, unsigned off)
{
    uint32_t n;

    if(img == NULL || img->bytes == NULL)
        return "";
    n = rd_u32(img->bytes + 12);
    if(off >= n)
        return "";
    return img->strings + off;
}

unsigned
KrbNodeCount(const KrbImage *img)
{
    if(img == NULL || img->bytes == NULL)
        return 0;
    return rd_u32(img->bytes + 8);
}

unsigned
KrbImportCount(const KrbImage *img)
{
    if(img == NULL || img->bytes == NULL)
        return 0;
    return rd_u32(img->bytes + 20);
}

const char *
KrbImportName(const KrbImage *img, unsigned slot)
{
    const unsigned char *p;
    uint32_t off;

    if(img == NULL || slot >= KrbImportCount(img))
        return "";
    p = (const unsigned char *)img->imports + slot * 4;
    if(!in_range(img, p, 4))
        return "";
    off = rd_u32(p);
    return KrbString(img, off);
}

int
KrbReadNode(const KrbImage *img, unsigned index, KrbNode *out)
{
    const unsigned char *p;

    if(img == NULL || out == NULL || index >= KrbNodeCount(img))
        return -1;
    p = img->nodes + (size_t)index * KRB_NODE_SIZE;
    out->id = rd_u16(p);
    out->parent = rd_i16(p + 2);
    out->name_off = rd_u16(p + 4);
    out->type = p[6];
    out->flags = p[7];
    out->bind_slot = rd_u16(p + 8);
    out->x = rd_i16(p + 10);
    out->y = rd_i16(p + 12);
    out->w = rd_i16(p + 14);
    out->h = rd_i16(p + 16);
    out->color = rd_u32(p + 18);
    out->text_off = rd_u16(p + 22);
    out->font_size = rd_u16(p + 24);
    out->style = p[26];
    out->pad = p[27];
    return 0;
}

int
KrbBindSlot(KrbImage *img, unsigned slot, KrbFn fn, void *userdata)
{
    if(img == NULL || slot >= KRB_BIND_MAX)
        return -1;
    img->binds[slot] = fn;
    img->bind_ud[slot] = userdata;
    return 0;
}

int
KrbBind(KrbImage *img, const char *name, KrbFn fn, void *userdata)
{
    unsigned i;
    unsigned n;

    if(img == NULL || name == NULL)
        return -1;
    n = KrbImportCount(img);
    for(i = 0; i < n; i++) {
        if(strcmp(KrbImportName(img, i), name) == 0)
            return KrbBindSlot(img, i, fn, userdata);
    }
    return -1;
}

static void
join_path(char *dst, size_t dst_size, const char *root, const char *rel)
{
    if(rel == NULL || rel[0] == '\0') {
        snprintf(dst, dst_size, "%s", (root != NULL && root[0] != '\0') ? root : "/");
        return;
    }
    if(rel[0] == '/') {
        snprintf(dst, dst_size, "%s", rel);
        return;
    }
    if(root == NULL || root[0] == '\0' || strcmp(root, "/") == 0)
        snprintf(dst, dst_size, "/%s", rel);
    else
        snprintf(dst, dst_size, "%s/%s", root, rel);
}

static int
path_same(const char *a, const char *b)
{
    if(a == NULL || b == NULL)
        return 0;
    if(a[0] == '/' && a[1] != '\0')
        a++;
    if(b[0] == '/' && b[1] != '\0')
        b++;
    return strcmp(a, b) == 0;
}

static int
find_field(const KrbImage *img, const char *path, void **ptr, unsigned *kind,
           unsigned *size)
{
    int i;
    int j;
    char full[128];

    if(img == NULL || path == NULL)
        return -1;
    for(i = 0; i < img->mount_count; i++) {
        const KrbMountEntry *m = &img->mounts[i];

        for(j = 0; j < m->field_count; j++) {
            join_path(full, sizeof(full), m->root, m->fields[j].path);
            if(!path_same(full, path) && !path_same(m->fields[j].path, path))
                continue;
            if(ptr != NULL)
                *ptr = (unsigned char *)m->base + m->fields[j].offset;
            if(kind != NULL)
                *kind = m->fields[j].kind;
            if(size != NULL)
                *size = m->fields[j].size;
            return 0;
        }
    }
    return -1;
}

int
KrbMount(KrbImage *img, const char *root, void *base, const KrbField *fields)
{
    KrbMountEntry *m;
    int i;

    if(img == NULL || base == NULL || fields == NULL)
        return -1;
    if(img->mount_count >= KRB_MOUNT_MAX)
        return -1;
    m = &img->mounts[img->mount_count];
    memset(m, 0, sizeof(*m));
    snprintf(m->root, sizeof(m->root), "%s", root != NULL ? root : "/");
    m->base = base;
    for(i = 0; fields[i].path != NULL && i < KRB_FIELD_MAX; i++)
        m->fields[i] = fields[i];
    m->field_count = i;
    img->mount_count++;
    return 0;
}

int
KrbBindMem(KrbImage *img, const char *path, void *ptr, unsigned kind,
           unsigned size)
{
    KrbField fields[2];

    if(path == NULL || ptr == NULL)
        return -1;
    memset(fields, 0, sizeof(fields));
    fields[0].path = "";
    fields[0].offset = 0;
    fields[0].kind = kind;
    fields[0].size = size;
    return KrbMount(img, path, ptr, fields);
}

int
KrbReadI32(const KrbImage *img, const char *path, int *out)
{
    void *ptr;
    unsigned kind;
    unsigned size;
    int value = 0;

    if(find_field(img, path, &ptr, &kind, &size) != 0 || ptr == NULL)
        return -1;
    if(kind == KRB_I32 || kind == KRB_BOOL) {
        if(size >= 4)
            memcpy(&value, ptr, 4);
        else if(size == 1)
            value = *(unsigned char *)ptr;
    } else if(kind == KRB_U32 && size >= 4) {
        unsigned u;

        memcpy(&u, ptr, 4);
        value = (int)u;
    } else {
        return -1;
    }
    if(out != NULL)
        *out = value;
    return 0;
}

int
KrbWriteI32(KrbImage *img, const char *path, int value)
{
    void *ptr;
    unsigned kind;
    unsigned size;

    if(find_field(img, path, &ptr, &kind, &size) != 0 || ptr == NULL)
        return -1;
    if(kind == KRB_I32 || kind == KRB_U32) {
        if(size < 4)
            return -1;
        memcpy(ptr, &value, 4);
        return 0;
    }
    if(kind == KRB_BOOL) {
        unsigned char b = value != 0;

        memcpy(ptr, &b, 1);
        return 0;
    }
    return -1;
}

int
KrbReadF32(const KrbImage *img, const char *path, float *out)
{
    void *ptr;
    unsigned kind;
    unsigned size;
    float value = 0.0f;

    if(find_field(img, path, &ptr, &kind, &size) != 0 || ptr == NULL)
        return -1;
    if(kind != KRB_F32 || size < 4)
        return -1;
    memcpy(&value, ptr, 4);
    if(out != NULL)
        *out = value;
    return 0;
}

int
KrbWriteF32(KrbImage *img, const char *path, float value)
{
    void *ptr;
    unsigned kind;
    unsigned size;

    if(find_field(img, path, &ptr, &kind, &size) != 0 || ptr == NULL)
        return -1;
    if(kind != KRB_F32 || size < 4)
        return -1;
    memcpy(ptr, &value, 4);
    return 0;
}

int
KrbReadCStr(const KrbImage *img, const char *path, char *out, size_t out_size)
{
    void *ptr;
    unsigned kind;
    unsigned size;

    if(find_field(img, path, &ptr, &kind, &size) != 0 || ptr == NULL)
        return -1;
    if(kind != KRB_CSTR)
        return -1;
    if(out == NULL || out_size == 0)
        return 0;
    if(size == 0)
        size = (unsigned)out_size;
    snprintf(out, out_size, "%s", (const char *)ptr);
    return 0;
}

int
KrbWriteCStr(KrbImage *img, const char *path, const char *value)
{
    void *ptr;
    unsigned kind;
    unsigned size;

    if(value == NULL)
        value = "";
    if(find_field(img, path, &ptr, &kind, &size) != 0 || ptr == NULL)
        return -1;
    if(kind != KRB_CSTR || size == 0)
        return -1;
    snprintf((char *)ptr, size, "%s", value);
    return 0;
}

static int
format_bound(const KrbImage *img, const char *path, const char *fmt,
             char *dst, size_t dst_size)
{
    int i32;
    float f32;
    char str[256];

    if(KrbReadI32(img, path, &i32) == 0) {
        if(fmt != NULL && strstr(fmt, "%") != NULL)
            snprintf(dst, dst_size, fmt, i32);
        else
            snprintf(dst, dst_size, "%d", i32);
        return 0;
    }
    if(KrbReadF32(img, path, &f32) == 0) {
        if(fmt != NULL && strstr(fmt, "%") != NULL)
            snprintf(dst, dst_size, fmt, f32);
        else
            snprintf(dst, dst_size, "%g", f32);
        return 0;
    }
    if(KrbReadCStr(img, path, str, sizeof(str)) == 0) {
        if(fmt != NULL && strstr(fmt, "%") != NULL)
            snprintf(dst, dst_size, fmt, str);
        else
            snprintf(dst, dst_size, "%s", str);
        return 0;
    }
    return -1;
}

static unsigned
resolve_color(const KryBackend *b, uint32_t color)
{
    if((color & KRB_COLOR_THEME) != 0)
        return b->theme_color((int)(color & 0xff));
    return color;
}

static int
coord(const KryBackend *b, int16_t value, int scaled)
{
    if(scaled)
        return b->scale_px((int)value);
    return (int)value;
}

static void
draw_node(KrbImage *img, const KryBackend *b, const KrbNode *n,
          int origin_x, int origin_y)
{
    int x = origin_x + coord(b, n->x, n->flags & KRB_FLAG_SCALE_X);
    int y = origin_y + coord(b, n->y, n->flags & KRB_FLAG_SCALE_Y);
    int w = coord(b, n->w, n->flags & KRB_FLAG_SCALE_W);
    int h = coord(b, n->h, n->flags & KRB_FLAG_SCALE_H);
    unsigned color = resolve_color(b, n->color);
    const char *text = KrbString(img, n->text_off);
    int mx = 0;
    int my = 0;

    switch(n->type) {
    case KRB_NODE_BACKGROUND:
        b->rect(origin_x, origin_y, w, h, color);
        break;
    case KRB_NODE_RECT:
        b->rect(x, y, w, h, color);
        break;
    case KRB_NODE_TEXT: {
        char bound[256];
        const char *name = KrbString(img, n->name_off);
        const char *draw = text;

        if(format_bound(img, name, text, bound, sizeof(bound)) == 0)
            draw = bound;
        else if(text[0] == '/' &&
                format_bound(img, text, NULL, bound, sizeof(bound)) == 0)
            draw = bound;
        b->text(draw, x, y, n->font_size > 0 ? n->font_size : 16, color);
        break;
    }
    case KRB_NODE_BUTTON: {
        unsigned fill = color;
        unsigned label_color = b->theme_color(KRY_THEME_TEXT);
        int tw;
        int tx;
        int ty;
        int font = n->font_size > 0 ? n->font_size : 16;

        if(n->style == 2) /* danger */
            fill = 0xb83b3bffu;
        else if(n->style == 0) /* primary */
            fill = b->theme_color(KRY_THEME_BUTTON);
        else
            fill = b->theme_color(KRY_THEME_SURFACE);
        b->rect(x, y, w, h, fill);
        {
            unsigned border = b->theme_color(KRY_THEME_ICON);

            b->rect(x, y, w, 1, border);
            b->rect(x, y + h - 1, w, 1, border);
            b->rect(x, y, 1, h, border);
            b->rect(x + w - 1, y, 1, h, border);
        }
        tw = b->measure_text(text, font);
        tx = x + (w - tw) / 2;
        ty = y + (h - font) / 2;
        b->text(text, tx, ty, font, label_color);
        b->mouse(&mx, &my);
        if(n->bind_slot != 0xffff && n->bind_slot < KRB_BIND_MAX &&
           img->binds[n->bind_slot] != NULL &&
           b->mouse_pressed(KRY_MOUSE_LEFT) &&
           mx >= x && my >= y && mx < x + w && my < y + h)
            img->binds[n->bind_slot](img->bind_ud[n->bind_slot]);
        break;
    }
    case KRB_NODE_PICTURE:
        /* text_off holds the asset path; style holds the UIPictureFit; color
         * the tint. */
        if(b->texture != NULL && text[0] != '\0')
            b->texture(text, x, y, w, h, color, n->style);
        break;
    default:
        break;
    }
}

static void
draw_tree(KrbImage *img, int x, int y, int w, int h)
{
    const KryBackend *b = KryBackendCurrent();
    unsigned i;
    unsigned n;

    if(b == NULL)
        return;
    n = KrbNodeCount(img);
    for(i = 0; i < n; i++) {
        KrbNode node;

        if(KrbReadNode(img, i, &node) != 0)
            continue;
        if(node.type == KRB_NODE_BACKGROUND) {
            node.w = (int16_t)w;
            node.h = (int16_t)h;
            node.flags &= (unsigned char)~(KRB_FLAG_SCALE_W | KRB_FLAG_SCALE_H);
        }
        draw_node(img, b, &node, x, y);
    }
}

int
KrbExec(KrbImage *img)
{
    const unsigned char *p;
    const unsigned char *end;
    uint32_t prog_bytes;
    int drew = 0;

    if(img == NULL || img->bytes == NULL)
        return -1;
    prog_bytes = rd_u32(img->bytes + 16);
    p = img->prog;
    end = p + prog_bytes;
    if(prog_bytes == 0) {
        draw_tree(img, 0, 0, 0, 0);
        return 0;
    }
    while(p < end) {
        unsigned char op = *p++;

        if(op == KRB_OP_DRAW_TREE) {
            const KryBackend *b = KryBackendCurrent();
            int w = b != NULL ? b->width() : 0;
            int h = b != NULL ? b->height() : 0;

            draw_tree(img, 0, 0, w, h);
            drew = 1;
        } else if(op == KRB_OP_CALL_HOST) {
            unsigned slot;

            if(p >= end)
                return -1;
            slot = *p++;
            if(slot < KRB_BIND_MAX && img->binds[slot] != NULL)
                img->binds[slot](img->bind_ud[slot]);
        } else if(op == KRB_OP_SET_I32) {
            uint16_t off;
            int value;

            if(p + 6 > end)
                return -1;
            off = rd_u16(p);
            p += 2;
            value = (int)rd_u32(p);
            p += 4;
            KrbWriteI32(img, KrbString(img, off), value);
        } else {
            return -1;
        }
    }
    (void)drew;
    return 0;
}

void
KrbDraw(KrbImage *img, int x, int y, int w, int h)
{
    const unsigned char *prog;
    uint32_t prog_bytes;

    if(img == NULL || img->bytes == NULL)
        return;
    prog_bytes = rd_u32(img->bytes + 16);
    prog = img->prog;
    if(prog_bytes == 0 || (prog_bytes == 1 && prog[0] == KRB_OP_DRAW_TREE)) {
        draw_tree(img, x, y, w, h);
        return;
    }
    KrbExec(img);
}
