/*
 * Kryon RC Shell Wrapper - Plan 9 RC Shell Integration
 * C89/C90 compliant
 *
 * Manages the plan9port rc shell for CPU server functionality
 */

#ifndef RC_WRAPPER_H
#define RC_WRAPPER_H

#include <stddef.h>
#include <sys/types.h>
#include "kryon.h"

/*
 * Maximum environment variables
 */
#define RC_MAX_ENV 64

/*
 * Maximum environment variable length
 */
#define RC_MAX_ENV_LEN 256

/*
 * RC shell state
 */
typedef struct {
    pid_t pid;                   /* Process ID */
    int stdin_fd;                /* stdin pipe fd */
    int stdout_fd;               /* stdout pipe fd */
    int stderr_fd;               /* stderr pipe fd */
    int active;                  /* Shell is running */
    char user[64];               /* Username */
    char plan9_path[512];        /* Path to plan9port */
} RCState;

/*
 * Find plan9port installation
 * Returns path string, or NULL if not found
 */
const char *rc_find_plan9(void);

/*
 * Initialize RC wrapper
 * Returns 0 on success, -1 on error
 */
int rc_wrapper_init(void);

/*
 * Cleanup RC wrapper
 */
void rc_wrapper_cleanup(void);

/*
 * Start RC shell with CPU server environment
 * Returns process ID on success, -1 on error
 */
pid_t rc_start_cpu(const char *user, P9Node *mnt_term);

/*
 * Stop RC shell
 * Returns 0 on success, -1 on error
 */
int rc_stop(pid_t pid);

/*
 * Check if RC shell is running
 * Returns 1 if running, 0 if not
 */
int rc_is_running(pid_t pid);

/*
 * Write to RC shell's stdin
 * Returns bytes written, or -1 on error
 */
ssize_t rc_write_stdin(pid_t pid, const char *buf, size_t count);

/*
 * Read from RC shell's stdout
 * Returns bytes read, or -1 on error
 */
ssize_t rc_read_stdout(pid_t pid, char *buf, size_t count);

/*
 * Read from RC shell's stderr
 * Returns bytes read, or -1 on error
 */
ssize_t rc_read_stderr(pid_t pid, char *buf, size_t count);

/*
 * Set environment variable for RC shell
 * Returns 0 on success, -1 on error
 */
int rc_set_env(const char *name, const char *value);

/*
 * Get environment variable value
 * Returns value string, or NULL if not found
 */
const char *rc_get_env(const char *name);

#endif /* RC_WRAPPER_H */
