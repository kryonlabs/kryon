/*
 * Kryon Core - 9P2000 Server Library
 * C89/C90 compliant public API
 */

#ifndef KRYON_H
#define KRYON_H

/* Include lib9 first for 9P protocol types */
#include <lib9.h>
#include <fcall.h>

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
 * 9P Constants (Kryon-specific limits)
 */
#define P9_MAX_VERSION  32
#define P9_MAX_MSG      8192
#define P9_MAX_FID      256
#define P9_MAX_TAG      256
#define P9_MAX_WELEM    16
#define P9_MAX_STR      256

/*
 * Tree node structure
 */
typedef struct P9Node {
    char            *name;
    Qid             qid;            /* Use lib9's Qid */
    uint32_t        mode;
    uint32_t        atime;
    uint32_t        mtime;
    uint64_t        length;
    void            *data;
    struct P9Node   *parent;
    struct P9Node   **children;
    int             nchildren;
    int             capacity;

    /* Bind mount fields */
    int             is_bind;        /* Non-zero if this is a bind mount */
    char            *bind_target;   /* Target path (e.g., "/dev/win1/screen") */
    struct P9Node   *bind_node;     /* Resolved target node */
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
 * File operation handlers
 * Note: The actual implementation includes a void *data parameter
 */
typedef ssize_t (*P9ReadFunc)(char *buf, size_t count, uint64_t offset, void *data);
typedef ssize_t (*P9WriteFunc)(const char *buf, size_t count, uint64_t offset, void *data);

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
 * Bind mount support
 */
P9Node *tree_create_bind(P9Node *parent, const char *name, const char *target);
P9Node *tree_resolve_path(P9Node *root, const char *path);

/*
 * Free a tree and all its children
 */
void tree_free(P9Node *root);

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
 * 9P Operation handlers (updated to use lib9's Fcall)
 */
int handle_tversion(int client_fd, const Fcall *f);
int handle_tauth(int client_fd, const Fcall *f);
int handle_tattach(int client_fd, const Fcall *f);
int handle_twalk(int client_fd, const Fcall *f);
int handle_topen(int client_fd, const Fcall *f);
int handle_tread(int client_fd, const Fcall *f);
int handle_twrite(int client_fd, const Fcall *f);
int handle_tclunk(int client_fd, const Fcall *f);
int handle_tremove(int client_fd, const Fcall *f);
int handle_tstat(int client_fd, const Fcall *f);

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
P9Node *namespace_create_mnt_term(P9Node *root, int client_id);

/*
 * Bind device to path (like Plan 9 bind command)
 * Returns 0 on success, -1 on error
 */
int namespace_bind(P9Node *root, const char *device, const char *path, int type);

/*
 * Get /mnt/term for a client
 * Returns the mnt_term node, or NULL if not found
 */
P9Node *namespace_get_mnt_term(int client_id);
#endif

#endif /* KRYON_H */
