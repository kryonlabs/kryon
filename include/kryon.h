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
 * FID (File ID) tracking
 */
typedef struct {
    uint32_t    fid;
    P9Node      *node;
    int         is_open;
    uint8_t     mode;   /* Open mode if open */
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
 */
typedef ssize_t (*P9ReadFunc)(char *buf, size_t count, uint64_t offset);
typedef ssize_t (*P9WriteFunc)(const char *buf, size_t count, uint64_t offset);

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
                         ssize_t (*read)(char *, size_t, uint64_t),
                         ssize_t (*write)(const char *, size_t, uint64_t));
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
void fid_cleanup(void);
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
 * Forward declaration for Memimage (from graphics.h)
 */
struct Memimage;

/*
 * Draw connection state (for /dev/draw/[n])
 */
#define MAX_DRAW_CONNECTIONS 32
#define MAX_IMAGES_PER_CONNECTION 256

typedef struct DrawImage {
    uint32_t id;
    struct Memimage *img;
    int in_use;
} DrawImage;

typedef struct DrawConnection {
    int id;                       /* Connection number */
    int screen_id;                /* Screen image ID */
    int fillimage_id;             /* Fill image ID */
    int next_image_id;            /* Next allocated image ID */
    struct Memimage *screen;      /* Screen image */
    int refresh_enabled;          /* Refresh flag */
    int screen_dirty;             /* Flag: screen needs refresh */
    uint32_t qid_path_base;       /* Base QID path for this connection's files */
    DrawImage images[MAX_IMAGES_PER_CONNECTION];
    int nimages;
} DrawConnection;

/*
 * Draw connection management
 */
int drawconn_init(struct Memimage *screen);
void drawconn_cleanup(void);
DrawConnection *drawconn_new(void);
DrawConnection *drawconn_get(int id);
void drawconn_delete(int id);
int drawconn_next_id(void);

/*
 * /dev/draw/new implementation
 */
int devdraw_new_init(P9Node *draw_dir);
ssize_t devdraw_new_read(char *buf, size_t count, uint64_t offset, void *data);

/*
 * /dev/draw/[n] directory implementation
 */
P9Node *drawconn_create_dir(int conn_id);

/*
 * /dev/draw/[n]/data implementation
 */
ssize_t devdraw_data_read(char *buf, size_t count, uint64_t offset, void *data);
ssize_t devdraw_data_write(const char *buf, size_t count, uint64_t offset, void *data);

/*
 * /dev/draw/[n]/ctl implementation
 */
ssize_t devdraw_ctl_read(char *buf, size_t count, uint64_t offset, void *data);
ssize_t devdraw_ctl_write(const char *buf, size_t count, uint64_t offset, void *data);

/*
 * /dev/draw/[n]/refresh implementation
 */
ssize_t devdraw_refresh_read(char *buf, size_t count, uint64_t offset, void *data);

/*
 * Screen refresh notification functions
 */
void drawconn_mark_dirty_all(void);

/*
 * /dev/kbd initialization
 */
int devkbd_init(P9Node *dev_dir);

/*
 * Plan 9 graphics protocol processing
 */
int process_draw_messages(DrawConnection *conn, const char *buf, size_t count,
                          char *response, int *resp_len);

/*
 * Compatibility functions for legacy code
 */
void devdraw_mark_dirty(void);
void devdraw_clear_dirty(void);
int devdraw_is_dirty(void);
void devdraw_cleanup(void);

/*
 * Main dispatcher
 */
size_t dispatch_9p(const uint8_t *in_buf, size_t in_len, uint8_t *out_buf);

/*
 * CPU Server Functions
 */
#ifdef INCLUDE_CPU_SERVER
/*
 * Initialize CPU server subsystem
 * Returns 0 on success, -1 on error
 */
int cpu_server_init(struct P9Node *root);

/*
 * Handle new CPU client connection
 * Called from Tattach handler when aname="cpu"
 * Returns session ID on success, -1 on error
 */
int cpu_handle_new_client(int client_fd, const char *user, const char *aname);

/*
 * Get CPU session by client ID
 * Returns pointer to session, or NULL if not found
 */
void *cpu_get_session(int client_id);

/*
 * Close and cleanup a CPU session
 */
void cpu_close_session(int client_id);

/*
 * Find plan9port installation path
 * Returns path string, or NULL if not found
 */
const char *cpu_find_plan9_path(void);
#endif

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
