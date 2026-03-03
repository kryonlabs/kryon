/*
 * Kryon /proc Device - Process Information
 * C89/C90 compliant
 *
 * Implements basic /proc filesystem for process information
 * Required for rc shell operation in CPU server mode
 */

#include "kryon.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>

/*
 * Proc entry state
 */
typedef struct {
    pid_t pid;
    char cmd[64];
    int active;
} ProcState;

/*
 * Global proc state
 */
#define MAX_PROCS 32
static ProcState g_procs[MAX_PROCS];
static int g_nprocs = 0;

/*
 * Read from /proc/[pid]/status
 */
static ssize_t proc_status_read(char *buf, size_t count, uint64_t offset,
                                void *data)
{
    ProcState *state = (ProcState *)data;
    char status_buf[512];
    int len;
    size_t to_copy;

    if (state == NULL || !state->active) {
        return -1;
    }

    /* Build status string */
    snprintf(status_buf, sizeof(status_buf),
            "Name:\t%s\n"
            "State:\tR (running)\n"
            "Pid:\t%d\n"
            "PPid:\t1\n",
            state->cmd, state->pid);

    len = strlen(status_buf);

    if (offset >= (uint64_t)len) {
        return 0;  /* EOF */
    }

    to_copy = len - (size_t)offset;
    if (to_copy > count) {
        to_copy = count;
    }

    memcpy(buf, status_buf + offset, to_copy);

    return (ssize_t)to_copy;
}

/*
 * Read from /proc/[pid]/ctl
 * Control file - reads return empty
 */
static ssize_t proc_ctl_read(char *buf, size_t count, uint64_t offset,
                             void *data)
{
    (void)data;
    (void)offset;

    if (count > 0) {
        buf[0] = '\0';
    }

    return 0;
}

/*
 * Write to /proc/[pid]/ctl
 * Control commands (not implemented yet)
 */
static ssize_t proc_ctl_write(const char *buf, size_t count, uint64_t offset,
                              void *data)
{
    /* TODO: Implement control commands */
    (void)data;
    (void)offset;

    fprintf(stderr, "proc_ctl_write: %.*s\n", (int)count, buf);

    return (ssize_t)count;
}

/*
 * Initialize /proc device
 * Creates /proc directory structure
 */
int devproc_init(P9Node *root)
{
    P9Node *proc_dir;
    P9Node *self_dir;
    char pid_str[32];
    pid_t pid;

    if (root == NULL) {
        return -1;
    }

    /* Create /proc directory */
    proc_dir = tree_create_dir(root, "proc");
    if (proc_dir == NULL) {
        fprintf(stderr, "devproc_init: cannot create proc directory\n");
        return -1;
    }

    /* Add self (current process) */
    pid = getpid();
    snprintf(pid_str, sizeof(pid_str), "%d", (int)pid);

    self_dir = tree_create_dir(proc_dir, pid_str);
    if (self_dir == NULL) {
        fprintf(stderr, "devproc_init: cannot create /proc/%s\n", pid_str);
        return -1;
    }

    /* Create status file */
    {
        P9Node *status_node;
        ProcState *state = &g_procs[0];

        state->pid = pid;
        strncpy(state->cmd, "kryon-server", sizeof(state->cmd) - 1);
        state->cmd[sizeof(state->cmd) - 1] = '\0';
        state->active = 1;
        g_nprocs = 1;

        status_node = tree_create_file(self_dir, "status", state,
                                       (P9ReadFunc)proc_status_read,
                                       NULL);
        if (status_node == NULL) {
            fprintf(stderr, "devproc_init: cannot create status\n");
            return -1;
        }
    }

    /* Create ctl file */
    {
        P9Node *ctl_node;
        ctl_node = tree_create_file(self_dir, "ctl", &g_procs[0],
                                    (P9ReadFunc)proc_ctl_read,
                                    (P9WriteFunc)proc_ctl_write);
        if (ctl_node == NULL) {
            fprintf(stderr, "devproc_init: cannot create ctl\n");
            return -1;
        }
    }

    fprintf(stderr, "devproc_init: initialized /proc\n");

    return 0;
}

/*
 * Add a process to /proc
 * Returns 0 on success, -1 on error
 */
int devproc_add_pid(pid_t pid, const char *cmd)
{
    int slot;
    char pid_str[32];
    P9Node *proc_dir;
    P9Node *proc_root;

    if (g_nprocs >= MAX_PROCS) {
        return -1;
    }

    /* Find free slot */
    for (slot = 0; slot < MAX_PROCS; slot++) {
        if (!g_procs[slot].active) {
            break;
        }
    }

    if (slot >= MAX_PROCS) {
        return -1;
    }

    /* Initialize state */
    g_procs[slot].pid = pid;
    if (cmd != NULL) {
        strncpy(g_procs[slot].cmd, cmd, sizeof(g_procs[slot].cmd) - 1);
        g_procs[slot].cmd[sizeof(g_procs[slot].cmd) - 1] = '\0';
    } else {
        strcpy(g_procs[slot].cmd, "unknown");
    }
    g_procs[slot].active = 1;
    g_nprocs++;

    /* Get /proc directory */
    proc_root = tree_root();
    proc_dir = tree_walk(proc_root, "proc");
    if (proc_dir == NULL) {
        return -1;
    }

    /* Create directory */
    snprintf(pid_str, sizeof(pid_str), "%d", (int)pid);

    /* TODO: Create proc files for this PID */

    return 0;
}

/*
 * Remove a process from /proc
 */
void devproc_remove_pid(pid_t pid)
{
    int i;

    for (i = 0; i < MAX_PROCS; i++) {
        if (g_procs[i].active && g_procs[i].pid == pid) {
            g_procs[i].active = 0;
            g_nprocs--;
            break;
        }
    }
}
