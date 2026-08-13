#ifndef KRB_H
#define KRB_H

#include "kry_backend.h"

#include <stddef.h>
#include <stdint.h>

/*
 * Kryon cartridge image (krb). Packed VFS nodes plus a tiny program.
 * See docs/KRB_FORMAT.md.
 */

#define KRB_MAGIC 0x0042524Bu /* "KRB\0" little-endian; version in version field */
#define KRB_VERSION 1
#define KRB_NODE_SIZE 28
#define KRB_BIND_MAX 32
#define KRB_MOUNT_MAX 16
#define KRB_FIELD_MAX 64

enum {
    KRB_NODE_BACKGROUND = 1,
    KRB_NODE_TEXT = 2,
    KRB_NODE_RECT = 3,
    KRB_NODE_BUTTON = 4,
    KRB_NODE_DATA = 5,
    KRB_NODE_PICTURE = 6
};

enum {
    KRB_FLAG_SCALE_X = 1 << 2,
    KRB_FLAG_SCALE_Y = 1 << 3,
    KRB_FLAG_SCALE_W = 1 << 4,
    KRB_FLAG_SCALE_H = 1 << 5,
    KRB_COLOR_THEME = 0x80000000u
};

enum {
    KRB_OP_DRAW_TREE = 0x01,
    KRB_OP_CALL_HOST = 0x02, /* u8 slot */
    KRB_OP_SET_I32 = 0x03    /* u16 path_off, i32 value */
};

enum {
    KRB_I32 = 1,
    KRB_U32 = 2,
    KRB_F32 = 3,
    KRB_BOOL = 4,
    KRB_CSTR = 5
};

typedef int (*KrbFn)(void *userdata);

typedef struct KrbField {
    const char *path;
    unsigned offset;
    unsigned kind;
    unsigned size;
} KrbField;

typedef struct KrbMountEntry {
    char root[96];
    void *base;
    KrbField fields[KRB_FIELD_MAX];
    int field_count;
} KrbMountEntry;

typedef struct KrbHeader {
    uint32_t magic;
    uint16_t version;
    uint16_t flags;
    uint32_t node_count;
    uint32_t string_bytes;
    uint32_t prog_bytes;
    uint32_t import_count;
    uint32_t reserved;
} KrbHeader;

typedef struct KrbNode {
    uint16_t id;
    int16_t parent;
    uint16_t name_off;
    uint8_t type;
    uint8_t flags;
    uint16_t bind_slot;
    int16_t x;
    int16_t y;
    int16_t w;
    int16_t h;
    uint32_t color;
    uint16_t text_off;
    uint16_t font_size;
    uint8_t style;
    uint8_t pad;
} KrbNode;

typedef struct KrbImage {
    const unsigned char *bytes;
    size_t len;
    int owned;
    const KrbHeader *header;
    const unsigned char *nodes;
    const char *strings;
    const unsigned char *prog;
    const uint32_t *imports;
    KrbFn binds[KRB_BIND_MAX];
    void *bind_ud[KRB_BIND_MAX];
    KrbMountEntry mounts[KRB_MOUNT_MAX];
    int mount_count;
} KrbImage;

int KrbLoad(KrbImage *img, const unsigned char *bytes, size_t len);
int KrbLoadFile(KrbImage *img, const char *path);
void KrbFree(KrbImage *img);
int KrbBind(KrbImage *img, const char *name, KrbFn fn, void *userdata);
int KrbBindSlot(KrbImage *img, unsigned slot, KrbFn fn, void *userdata);
const char *KrbString(const KrbImage *img, unsigned off);
const char *KrbImportName(const KrbImage *img, unsigned slot);
unsigned KrbNodeCount(const KrbImage *img);
unsigned KrbImportCount(const KrbImage *img);
int KrbReadNode(const KrbImage *img, unsigned index, KrbNode *out);
int KrbMount(KrbImage *img, const char *root, void *base,
             const KrbField *fields);
int KrbBindMem(KrbImage *img, const char *path, void *ptr, unsigned kind,
               unsigned size);
int KrbReadI32(const KrbImage *img, const char *path, int *out);
int KrbWriteI32(KrbImage *img, const char *path, int value);
int KrbReadF32(const KrbImage *img, const char *path, float *out);
int KrbWriteF32(KrbImage *img, const char *path, float value);
int KrbReadCStr(const KrbImage *img, const char *path, char *out, size_t out_size);
int KrbWriteCStr(KrbImage *img, const char *path, const char *value);
int KrbExec(KrbImage *img);
void KrbDraw(KrbImage *img, int x, int y, int w, int h);

#endif
