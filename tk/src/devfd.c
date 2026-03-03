/*
 * Kryon /dev/fd Device - File Descriptor Device
 * C89/C90 compliant
 *
 * Implements /dev/fd/[0-9] for file descriptor access
 * Required for namespace export in CPU server mode
 */

#include "kryon.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

/*
 * Maximum file descriptors to expose
 */
#define MAX_FD 64

/*
 * FD device state
 */
typedef struct {
    int target_fd;    /* Actual file descriptor */
    int in_use;       /* Slot is in use */
} FDState;

/*
 * Global FD state per client
 * For simplicity, we use a global state (single client for now)
 */
static FDState g_fd_state[MAX_FD];
static int g_fd_initialized = 0;

/*
 * Read from /dev/fd/[n]
 */
static ssize_t devfd_read(char *buf, size_t count, uint64_t offset,
                          void *data)
{
    FDState *state = (FDState *)data;
    ssize_t n;

    if (state == NULL || !state->in_use) {
        return -1;
    }

    if (state->target_fd < 0) {
        return -1;
    }

    (void)offset;  /* Ignore offset for fd reads */

    n = read(state->target_fd, buf, count);
    if (n < 0) {
        return -1;
    }

    return n;
}

/*
 * Write to /dev/fd/[n]
 */
static ssize_t devfd_write(const char *buf, size_t count, uint64_t offset,
                           void *data)
{
    FDState *state = (FDState *)data;
    ssize_t n;

    if (state == NULL || !state->in_use) {
        return -1;
    }

    if (state->target_fd < 0) {
        return -1;
    }

    (void)offset;  /* Ignore offset for fd writes */

    n = write(state->target_fd, buf, count);
    if (n < 0) {
        return -1;
    }

    return n;
}

/*
 * Initialize /dev/fd device
 * Creates /dev/fd directory with fd entries
 */
int devfd_init(P9Node *dev_dir)
{
    P9Node *fd_dir;
    int i;

    if (dev_dir == NULL) {
        return -1;
    }

    /* Initialize global state */
    if (!g_fd_initialized) {
        for (i = 0; i < MAX_FD; i++) {
            g_fd_state[i].target_fd = -1;
            g_fd_state[i].in_use = 0;
        }
        g_fd_initialized = 1;
    }

    /* Create /dev/fd directory */
    fd_dir = tree_create_dir(dev_dir, "fd");
    if (fd_dir == NULL) {
        fprintf(stderr, "devfd_init: cannot create fd directory\n");
        return -1;
    }

    /* Create standard fd entries */
    /* /dev/fd/0 = stdin */
    {
        P9Node *node;
        g_fd_state[0].target_fd = STDIN_FILENO;
        g_fd_state[0].in_use = 1;

        node = tree_create_file(fd_dir, "0", &g_fd_state[0],
                                (P9ReadFunc)devfd_read,
                                (P9WriteFunc)devfd_write);
        if (node == NULL) {
            fprintf(stderr, "devfd_init: cannot create fd/0\n");
            return -1;
        }
    }

    /* /dev/fd/1 = stdout */
    {
        P9Node *node;
        g_fd_state[1].target_fd = STDOUT_FILENO;
        g_fd_state[1].in_use = 1;

        node = tree_create_file(fd_dir, "1", &g_fd_state[1],
                                (P9ReadFunc)devfd_read,
                                (P9WriteFunc)devfd_write);
        if (node == NULL) {
            fprintf(stderr, "devfd_init: cannot create fd/1\n");
            return -1;
        }
    }

    /* /dev/fd/2 = stderr */
    {
        P9Node *node;
        g_fd_state[2].target_fd = STDERR_FILENO;
        g_fd_state[2].in_use = 1;

        node = tree_create_file(fd_dir, "2", &g_fd_state[2],
                                (P9ReadFunc)devfd_read,
                                (P9WriteFunc)devfd_write);
        if (node == NULL) {
            fprintf(stderr, "devfd_init: cannot create fd/2\n");
            return -1;
        }
    }

    fprintf(stderr, "devfd_init: initialized /dev/fd\n");

    return 0;
}

/*
 * Create a new fd entry
 * Returns 0 on success, -1 on error
 */
int devfd_create_fd(P9Node *fd_dir, int fd_num, int target_fd)
{
    P9Node *node;
    char name[16];

    if (fd_dir == NULL) {
        return -1;
    }

    if (fd_num < 0 || fd_num >= MAX_FD) {
        return -1;
    }

    /* Update state */
    g_fd_state[fd_num].target_fd = target_fd;
    g_fd_state[fd_num].in_use = 1;

    /* Create file node */
    snprintf(name, sizeof(name), "%d", fd_num);

    node = tree_create_file(fd_dir, name, &g_fd_state[fd_num],
                            (P9ReadFunc)devfd_read,
                            (P9WriteFunc)devfd_write);
    if (node == NULL) {
        fprintf(stderr, "devfd_create_fd: cannot create fd/%s\n", name);
        return -1;
    }

    return 0;
}

/*
 * Close an fd entry
 */
void devfd_close_fd(int fd_num)
{
    if (fd_num < 0 || fd_num >= MAX_FD) {
        return;
    }

    g_fd_state[fd_num].in_use = 0;
    g_fd_state[fd_num].target_fd = -1;
}
