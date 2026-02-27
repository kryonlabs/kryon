/*
 * 9P Client Library
 * C89/C90 compliant
 *
 * Generic 9P client for connecting to any 9P server
 * (Marrow, Plan 9, drawterm, etc.)
 */

#ifndef KRYON_P9CLIENT_H
#define KRYON_P9CLIENT_H

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
 * 9P client connection handle
 */
typedef struct P9Client P9Client;

/*
 * Authentication methods (Plan 9 compatible)
 */
#define P9_AUTH_NONE    0
#define P9_AUTH_P9ANY   1

/*
 * Open flags (Plan 9 compatible)
 */
#define P9_OREAD   0   /* Open for reading */
#define P9_OWRITE  1   /* Open for writing */
#define P9_ORDWR   2   /* Open for reading and writing */
#define P9_OEXEC   3   /* Open for execution */

/*
 * Connect to 9P server
 * Supports: tcp!host!port, tcp!host, /path/to/socket
 * Returns client handle on success, NULL on failure
 */
P9Client *p9_connect(const char *address);

/*
 * Disconnect from 9P server
 */
void p9_disconnect(P9Client *client);

/*
 * Authenticate with 9P server
 * Returns 0 on success, -1 on failure
 */
int p9_authenticate(P9Client *client, int auth_method,
                    const char *user, const char *password);

/*
 * Open a file on 9P server
 * Returns file descriptor (fid) on success, -1 on failure
 */
int p9_open(P9Client *client, const char *path, int mode);

/*
 * Close a file on 9P server
 * Returns 0 on success, -1 on failure
 */
int p9_close(P9Client *client, int fd);

/*
 * Read from a file on 9P server
 * Returns number of bytes read on success, -1 on failure
 */
ssize_t p9_read(P9Client *client, int fd, void *buf, size_t count);

/*
 * Write to a file on 9P server
 * Returns number of bytes written on success, -1 on failure
 */
ssize_t p9_write(P9Client *client, int fd, const void *buf, size_t count);

/*
 * Create a directory on 9P server
 * Returns 0 on success, -1 on failure
 */
int p9_mkdir(P9Client *client, const char *path);

/*
 * Get last error message
 */
const char *p9_error(P9Client *client);

/*
 * Clunk (close) a file descriptor
 * Returns 0 on success, -1 on failure
 */
int p9_clunk(P9Client *client, int fd);

/*
 * Reset file offset to beginning
 * Returns 0 on success, -1 on failure
 */
int p9_reset_fid(P9Client *client, int fd);

/*
 * Namespace management
 *
 * Forward declaration - P9Node is defined in kryon.h
 * Include kryon.h before p9client.h to use namespace functions
 */
struct P9Node;

/*
 * Set current namespace for this thread/connection
 * This affects all subsequent 9P operations
 */
void p9_set_namespace(struct P9Node *ns_root);

/*
 * Get current namespace
 */
struct P9Node *p9_get_namespace(void);

#endif /* KRYON_P9CLIENT_H */
