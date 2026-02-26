/*
 * Kryon Core - 9P2000 Server Library
 * C89/C90 compliant public API
 */

#ifndef KRYON_H
#define KRYON_H

#include <stddef.h>
#include <stdint.h>

/*
 * C89 compatibility: ssize_t is not defined in C89
 */
#ifdef _WIN32
typedef long ssize_t;
#else
#include <sys/types.h>
#endif

/*
 * 9P2000 Message Types
 * These must match plan9port's enum values from fcall.h
 */
typedef enum {
    Tversion = 100,
    Rversion = 101,
    Tauth = 102,
    Rauth = 103,
    Tattach = 104,
    Rattach = 105,
    Terror = 106,    /* illegal */
    Rerror = 107,
    Tflush = 108,
    Rflush = 109,
    Twalk = 110,
    Rwalk = 111,
    Topen = 112,
    Ropen = 113,
    Tcreate = 114,
    Rcreate = 115,
    Tread = 116,
    Rread = 117,
    Twrite = 118,
    Rwrite = 119,
    Tclunk = 120,
    Rclunk = 121,
    Tremove = 122,
    Rremove = 123,
    Tstat = 124,
    Rstat = 125,
    Twstat = 126,
    Rwstat = 127
} P9MsgType;

/*
 * 9P Message Header (4 + 1 + 2 = 7 bytes)
 */
typedef struct {
    uint32_t size;     /* Total message size including this header */
    uint8_t  type;     /* Message type */
    uint16_t tag;      /* Transaction ID */
} P9Hdr;

/*
 * QID - 13 byte unique identifier
 */
typedef struct {
    uint8_t  type;     /* Qtdir, Qtappend, etc. */
    uint32_t version;  /* Version number for cache coherence */
    uint64_t path;     /* Unique path identifier */
} P9Qid;

/*
 * 9P Constants
 */
#define P9_MAX_VERSION  32
#define P9_MAX_MSG      8192
#define P9_MAX_FID      256
#define P9_MAX_TAG      256
#define P9_MAX_WELEM    16
#define P9_MAX_STR      256

/*
 * QID Types
 */
#define QTDIR       0x80    /* Directory */
#define QTAPPEND    0x40    /* Append only */
#define QTEXCL      0x20    /* Exclusive use */
#define QTMOUNT     0x10    /* Mount point */
#define QTAUTH      0x08    /* Authentication file */
#define QTTMP       0x04    /* Temporary file */
#define QTSYMLINK   0x02    /* Symbolic link */
#define QTFILE      0x00    /* Plain file */

/*
 * File permissions
 */
#define P9_DMDIR    0x80000000  /* Directory */
#define P9_DMAPPEND 0x40000000  /* Append only */
#define P9_DMEXCL   0x20000000  /* Exclusive */
#define P9_DMMOUNT  0x10000000  /* Mount point */
#define P9_DMAUTH   0x08000000  /* Authentication */
#define P9_DMTMP    0x04000000  /* Temporary */
#define P9_DMREAD   0x4         /* Read permission */
#define P9_DMWRITE  0x2         /* Write permission */
#define P9_DMEXEC   0x1         /* Execute permission */

/*
 * Open modes
 */
#define P9_OREAD    0   /* Read only */
#define P9_OWRITE   1   /* Write only */
#define P9_ORDWR    2   /* Read and write */
#define P9_OEXEC    3   /* Execute */

/*
 * Tree node structure
 */
typedef struct P9Node {
    char            *name;
    P9Qid           qid;
    uint32_t        mode;
    uint32_t        atime;
    uint32_t        mtime;
    uint64_t        length;
    void            *data;
    struct P9Node   *parent;
    struct P9Node   **children;
    int             nchildren;
    int             capacity;
} P9Node;

/*
 * Forward declarations for authentication
 */
struct AuthInfo;

/*
 * FID (File ID) tracking
 */
typedef struct {
    uint32_t    fid;
    P9Node      *node;
    int client_fd;
    int         is_open;
    uint8_t     mode;   /* Open mode if open */
    struct AuthInfo *auth_info;  /* Authentication info */
} P9Fid;

/*
 * Statistics structure
 */
typedef struct {
    uint16_t    type;
    uint16_t    dev;
    P9Qid       qid;
    uint32_t    mode;
    uint32_t    atime;
    uint32_t    mtime;
    uint64_t    length;
    char        name[P9_MAX_STR];
    char        uid[P9_MAX_STR];
    char        gid[P9_MAX_STR];
    char        muid[P9_MAX_STR];
} P9Stat;

/*
 * File operation handlers
 * Note: The actual implementation includes a void *data parameter
 */
typedef ssize_t (*P9ReadFunc)(char *buf, size_t count, uint64_t offset, void *data);
typedef ssize_t (*P9WriteFunc)(const char *buf, size_t count, uint64_t offset, void *data);

/*
 * Endianness conversion (9P uses little-endian)
 */
uint32_t le_get32(const uint8_t *buf);
uint64_t le_get64(const uint8_t *buf);
void le_put32(uint8_t *buf, uint32_t val);
void le_put64(uint8_t *buf, uint64_t val);
uint16_t le_get16(const uint8_t *buf);
void le_put16(uint8_t *buf, uint16_t val);

/*
 * 9P Message parsing
 */
int p9_parse_header(const uint8_t *buf, size_t len, P9Hdr *hdr);
int p9_parse_tversion(const uint8_t *buf, size_t len, uint32_t *msize, char *version);
int p9_parse_tattach(const uint8_t *buf, size_t len, uint32_t *fid, uint32_t *afid, char *uname, char *aname);
int p9_parse_tauth(const uint8_t *buf, size_t len, char *uname, char *aname);
int p9_parse_twalk(const uint8_t *buf, size_t len, uint32_t *fid, uint32_t *newfid,
                   char *wnames[], int *nwname);
int p9_parse_topen(const uint8_t *buf, size_t len, uint32_t *fid, uint8_t *mode);
int p9_parse_tread(const uint8_t *buf, size_t len, uint32_t *fid, uint64_t *offset, uint32_t *count);
int p9_parse_twrite(const uint8_t *buf, size_t len, uint32_t *fid, uint64_t *offset, const char **data, uint32_t *count);
int p9_parse_tclunk(const uint8_t *buf, size_t len, uint32_t *fid);
int p9_parse_tstat(const uint8_t *buf, size_t len, uint32_t *fid);

/*
 * 9P Message building
 */
size_t p9_build_header(uint8_t *buf, P9MsgType type, uint16_t tag, size_t payload_len);
size_t p9_build_rversion(uint8_t *buf, uint16_t tag, uint32_t msize, const char *version);
size_t p9_build_rerror(uint8_t *buf, uint16_t tag, const char *ename);
size_t p9_build_rauth(uint8_t *buf, uint16_t tag, P9Qid *qid);
size_t p9_build_rattach(uint8_t *buf, uint16_t tag, P9Qid *qid);
size_t p9_build_rwalk(uint8_t *buf, uint16_t tag, P9Qid *wqid, int nwqid);
size_t p9_build_ropen(uint8_t *buf, uint16_t tag, P9Qid *qid, uint32_t iounit);
size_t p9_build_rread(uint8_t *buf, uint16_t tag, const char *data, uint32_t count);
size_t p9_build_rwrite(uint8_t *buf, uint16_t tag, uint32_t count);
size_t p9_build_rclunk(uint8_t *buf, uint16_t tag);
size_t p9_build_rremove(uint8_t *buf, uint16_t tag);
size_t p9_build_rstat(uint8_t *buf, uint16_t tag, const P9Stat *stat);

/*
 * Stat packing helper
 */
size_t p9_pack_stat(uint8_t *buf, const P9Stat *stat);

/*
 * Tree management
 */
int tree_init(void);
void tree_cleanup(void);
P9Node *tree_root(void);
P9Node *tree_lookup(P9Node *root, const char *path);
P9Node *tree_walk(P9Node *node, const char *name);
P9Node *tree_create_dir(P9Node *parent, const char *name);
P9Node *tree_create_file(P9Node *parent, const char *name, void *data,
                         P9ReadFunc read,
                         P9WriteFunc write);
int tree_add_child(P9Node *parent, P9Node *child);
int tree_remove_node(P9Node *node);

/*
 * Node operations
 */
ssize_t node_read(P9Node *node, char *buf, size_t count, uint64_t offset);
ssize_t node_write(P9Node *node, const char *buf, size_t count, uint64_t offset);

/*
 * FID management
 */
int fid_init(void);
void fid_cleanup_conn(int client_fd);
P9Fid *fid_new(uint32_t fid_num, P9Node *node);
P9Fid *fid_get(uint32_t fid_num);
int fid_put(uint32_t fid_num);
int fid_clunk(uint32_t fid_num);

/*
 * 9P Operation handlers
 */
size_t handle_tversion(const uint8_t *in_buf, size_t in_len, uint8_t *out_buf);
size_t handle_tauth(const uint8_t *in_buf, size_t in_len, uint8_t *out_buf);
size_t handle_tattach(const uint8_t *in_buf, size_t in_len, uint8_t *out_buf);
size_t handle_twalk(const uint8_t *in_buf, size_t in_len, uint8_t *out_buf);
size_t handle_topen(const uint8_t *in_buf, size_t in_len, uint8_t *out_buf);
size_t handle_tread(const uint8_t *in_buf, size_t in_len, uint8_t *out_buf);
size_t handle_twrite(const uint8_t *in_buf, size_t in_len, uint8_t *out_buf);
size_t handle_tclunk(const uint8_t *in_buf, size_t in_len, uint8_t *out_buf);
size_t handle_tremove(const uint8_t *in_buf, size_t in_len, uint8_t *out_buf);
size_t handle_tstat(const uint8_t *in_buf, size_t in_len, uint8_t *out_buf);

/*
 * Forward declaration for Memimage (from graphics.h in Marrow)
 */
struct Memimage;

/*
 * Graphics functions (now provided by Marrow)
 * These are linked from Marrow's graphics library
 */
#ifdef INCLUDE_GRAPHICS

/* Graphics types from Marrow */
typedef struct {
    int x;
    int y;
} Point;

typedef struct {
    Point min;
    Point max;
} Rectangle;

typedef struct Memimage Memimage;

typedef struct {
    char *name;
    short n;
    unsigned char height;
    char ascent;
    void *info;  /* Fontchar* */
    Memimage *bits;
} Subfont;

/* Graphics functions from Marrow's graphics library */
extern void memfillcolor(Memimage *dst, unsigned long color);
extern void memfillcolor_rect(Memimage *dst, Rectangle r, unsigned long color);
extern int parse_rect(const char *str, Rectangle *r);
extern int ptinrect(Point p, Rectangle r);
extern void memdraw_text(Memimage *dst, Point p, const char *str, unsigned long color);
extern void memdraw_text_font(Memimage *dst, Point p, const char *str, Subfont *sf, unsigned long color);
extern Subfont* memdraw_get_default_font(void);
extern void devdraw_mark_dirty(void);

/* Color constants */
#define DBlack        0x000000FF
#define DWhite        0xFFFFFFFF
#define DRed          0xFF0000FF
#define DGreen        0x00FF00FF
#define DBlue         0x0000FFFF
#define DCyan         0x00FFFFFF
#define DMagenta      0xFF00FFFF
#define DYellow       0xFFFF00FF

/* Rectangle helpers */
#define Pt(x, y)      ((Point){(x), (y)})
#define Rect(x0, y0, x1, y1)  ((Rectangle){(Point){(x0), (y0)}, (Point){(x1), (y1)}})
#define Dx(r)   ((r).max.x - (r).min.x)
#define Dy(r)   ((r).max.y - (r).min.y)

#endif /* INCLUDE_GRAPHICS */

/*
 * Draw connection state (for /dev/draw/[n])
 * NOTE: These are now provided by Marrow
 * Kryon uses /dev/draw via the 9P client interface
 */

/*
 * /dev/kbd initialization
 * NOTE: This is now provided by Marrow
 */

/*
 * Plan 9 graphics protocol processing
 * NOTE: This is now handled by Marrow
 * Kryon uses /dev/draw via the 9P client interface
 */

/*
 * Compatibility functions for legacy code
 * NOTE: These are now provided by Marrow
 */

/*
 * Main dispatcher
 * NOTE: This is now provided by Marrow
 * Kryon is now a client, not a 9P server
 */

/*
 * Namespace Manager Functions
 */
#ifdef INCLUDE_NAMESPACE
/*
 * Initialize namespace manager
 * Returns 0 on success, -1 on error
 */
int namespace_init(void);

/*
 * Create /mnt/term structure for a client
 * Returns the mnt_term root node, or NULL on error
 */
struct P9Node *namespace_create_mnt_term(struct P9Node *root, int client_id);

/*
 * Bind device to path (like Plan 9 bind command)
 * Returns 0 on success, -1 on error
 */
int namespace_bind(struct P9Node *root, const char *device, const char *path, int type);

/*
 * Get /mnt/term for a client
 * Returns the mnt_term node, or NULL if not found
 */
struct P9Node *namespace_get_mnt_term(int client_id);
#endif

#endif /* KRYON_H */
