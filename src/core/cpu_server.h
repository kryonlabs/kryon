/*
 * Kryon CPU Server - Plan 9 CPU Server Implementation
 * C89/C90 compliant
 *
 * Transforms Kryon from a simple 9P file server into a full Plan 9 CPU server
 * that works with drawterm and provides /mnt/term namespace mounting.
 */

#ifndef CPU_SERVER_H
#define CPU_SERVER_H

#include <stdint.h>
#include <stddef.h>
#include "kryon.h"

/*
 * Maximum number of CPU clients
 */
#define MAX_CPU_CLIENTS 16

/*
 * CPU server session state per client
 */
typedef struct {
    int client_id;              /* Client identifier */
    int active;                 /* Session is active */
    P9Node *mnt_term;           /* /mnt/term root for this client */
    P9Node *mnt_term_dev;       /* /mnt/term/dev */
    P9Node *mnt_term_env;       /* /mnt/term/env */
    int rc_pid;                 /* RC shell process ID */
    int rc_stdin;               /* Pipe for RC stdin */
    int rc_stdout;              /* Pipe for RC stdout */
    int rc_stderr;              /* Pipe for RC stderr */
    char user[64];              /* Username */
    char aname[64];             /* Attach name (e.g., "cpu") */
} CPUSession;

/*
 * Initialize CPU server subsystem
 * Returns 0 on success, -1 on error
 */
int cpu_server_init(P9Node *root);

/*
 * Cleanup CPU server subsystem
 */
void cpu_server_cleanup(void);

/*
 * Handle new CPU client connection
 * Called from Tattach handler when aname="cpu"
 * Returns 0 on success, -1 on error
 */
int cpu_handle_new_client(int client_fd, const char *user, const char *aname);

/*
 * Create /mnt/term structure for a client
 * Returns the mnt_term root node, or NULL on error
 */
P9Node *cpu_create_mnt_term(P9Node *root, int client_id);

/*
 * Get CPU session by client ID
 * Returns pointer to session, or NULL if not found
 */
CPUSession *cpu_get_session(int client_id);

/*
 * Get CPU session by file descriptor
 * Returns pointer to session, or NULL if not found
 */
CPUSession *cpu_get_session_by_fd(int fd);

/*
 * Close and cleanup a CPU session
 */
void cpu_close_session(int client_id);

/*
 * Write to client's /mnt/term/dev/cons (for RC shell output)
 * Returns bytes written, or -1 on error
 */
int cpu_write_cons(int client_id, const char *buf, size_t count);

/*
 * Read from client's /mnt/term/dev/cons (for keyboard input)
 * Returns bytes read, or -1 on error
 */
int cpu_read_cons(int client_id, char *buf, size_t count);

/*
 * Check if a connection is a CPU client
 * Returns 1 if true, 0 if false
 */
int cpu_is_cpu_client(int client_fd);

/*
 * Find plan9port installation path
 * Returns path string, or NULL if not found
 */
const char *cpu_find_plan9_path(void);

#endif /* CPU_SERVER_H */
