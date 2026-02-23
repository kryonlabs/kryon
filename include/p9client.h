/*
 * Kryon 9P Client Library
 * C89/C90 compliant
 *
 * Enables client programs to connect to Kryon server and perform 9P operations
 */

#ifndef P9CLIENT_H
#define P9CLIENT_H

#include <stddef.h>
#include <stdint.h>

/*
 * C89 compatibility
 */
#ifdef _WIN32
typedef long ssize_t;
#else
#include <sys/types.h>
#endif

/*
 * Forward declarations
 */
typedef struct P9Client P9Client;

/*
 * 9P Open modes (matching server)
 */
#define P9_OREAD    0
#define P9_OWRITE   1
#define P9_ORDWR    2
#define P9_OEXEC    3

/*
 * Client state
 */
struct P9Client {
    int fd;                     /* TCP socket fd */
    uint16_t next_tag;          /* Next tag to use */
    uint32_t next_fid;          /* Next FID to use */
    uint32_t root_fid;          /* Root FID after attach */
    char *aname;                /* Attach name */
    int connected;              /* Connection state */
    int error;                  /* Last error code */
    char error_msg[256];        /* Last error message */
};

/*
 * Connect to Kryon server
 * Returns NULL on failure
 */
P9Client *p9_client_connect(const char *host, int port);

/*
 * Disconnect and cleanup
 */
void p9_client_disconnect(P9Client *client);

/*
 * 9P operations
 */

/*
 * Attach to server root
 * path: attach path (usually "" or "/tmp/kryon")
 */
int p9_client_attach(P9Client *client, const char *path);

/*
 * Walk to a path
 * Returns new fid on success, -1 on failure
 */
int p9_client_walk(P9Client *client, const char *path);

/*
 * Open a file
 * fid: FID from walk
 * mode: P9_OREAD, P9_OWRITE, P9_ORDWR
 * Returns 0 on success, -1 on failure
 */
int p9_client_open(P9Client *client, int fid, int mode);

/*
 * Read from an open file
 * fid: FID from open
 * buf: buffer to read into
 * count: maximum bytes to read
 * Returns bytes read on success, -1 on failure
 */
ssize_t p9_client_read(P9Client *client, int fid, char *buf, size_t count);

/*
 * Write to an open file
 * fid: FID from open
 * buf: data to write
 * count: bytes to write
 * Returns bytes written on success, -1 on failure
 */
ssize_t p9_client_write(P9Client *client, int fid, const char *buf, size_t count);

/*
 * Close/clunk a FID
 */
int p9_client_clunk(P9Client *client, int fid);

/*
 * Reset FID offset to 0 (for re-reading files)
 */
void p9_client_reset_fid(P9Client *client, int fid);

/*
 * Utility: walk and open combined
 * path: full path to open
 * mode: P9_OREAD, P9_OWRITE, P9_ORDWR
 * Returns FID on success, -1 on failure
 */
int p9_client_open_path(P9Client *client, const char *path, int mode);

/*
 * Get last error message
 */
const char *p9_client_error(P9Client *client);

/*
 * Utility: read entire file
 * Returns allocated buffer (caller frees), or NULL on failure
 */
char *p9_client_read_file(P9Client *client, const char *path, size_t *size);

#endif /* P9CLIENT_H */
