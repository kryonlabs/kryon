/**
 * @file process_control.c
 * @brief Inferno Process Control Service Implementation
 *
 * Implements process control using Inferno's /prog device:
 * - Process enumeration via /prog
 * - Control files: /prog/PID/ctl
 * - Status files: /prog/PID/status
 * - Wait files: /prog/PID/wait
 *
 * @version 1.0.0
 * @author Kryon Labs
 */

#ifdef KRYON_PLUGIN_INFERNO

#include "services/process_control.h"
#include "inferno_compat.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

// =============================================================================
// PROCESS CONTROL HANDLE
// =============================================================================

/**
 * Process control handle (wraps /prog/PID/ctl file)
 */
struct KryonProcessControl {
    int pid;                    /**< Process ID */
    int ctl_fd;                 /**< Control file descriptor */
};

// =============================================================================
// HELPER FUNCTIONS
// =============================================================================

/**
 * Parse process state from status string
 */
static KryonProcessState parse_process_state(const char *state_str) {
    if (!state_str) {
        return KRYON_PROC_UNKNOWN;
    }

    // Inferno process states
    if (strcmp(state_str, "Running") == 0 || strcmp(state_str, "Ready") == 0) {
        return KRYON_PROC_RUNNING;
    }
    if (strcmp(state_str, "Sleeping") == 0 || strcmp(state_str, "Queueing") == 0) {
        return KRYON_PROC_SLEEPING;
    }
    if (strcmp(state_str, "Stopped") == 0 || strcmp(state_str, "Broken") == 0) {
        return KRYON_PROC_STOPPED;
    }
    if (strcmp(state_str, "Dead") == 0 || strcmp(state_str, "Moribund") == 0) {
        return KRYON_PROC_DEAD;
    }

    return KRYON_PROC_UNKNOWN;
}

/**
 * Read process status file
 */
static bool read_proc_status_file(int pid, KryonProcessInfo *info) {
    char path[64];
    snprintf(path, sizeof(path), "/prog/%d/status", pid);

    int fd = inferno_open(path, OREAD);
    if (fd < 0) {
        return false;
    }

    char buf[512];
    int n = inferno_read(fd, buf, sizeof(buf) - 1);
    inferno_close(fd);

    if (n <= 0) {
        return false;
    }

    buf[n] = '\0';

    // Parse status file
    // Format: "name user state pri time mem ..."
    char name[128] = "";
    char user[64] = "";
    char state[32] = "";
    long mem = 0;

    sscanf(buf, "%127s %63s %31s %*d %*s %ld", name, user, state, &mem);

    info->pid = pid;
    info->name = strdup(name);
    info->command = strdup(name); // Inferno doesn't have separate command
    info->state = parse_process_state(state);
    info->memory = mem;
    info->cpu_time = 0; // Not easily available from status
    info->priority = 0;
    info->uid = 0; // Inferno uses user names, not UIDs
    info->gid = 0;

    return true;
}

// =============================================================================
// PROCESS ENUMERATION
// =============================================================================

/**
 * List all processes
 */
static bool inferno_list_processes(KryonProcessInfo **procs, int *count) {
    if (!procs || !count) {
        return false;
    }

    // Open /prog directory
    int fd = inferno_open("/prog", OREAD);
    if (fd < 0) {
        return false;
    }

    // Read directory entries
    int max_procs = 64;
    KryonProcessInfo *proc_list = malloc(max_procs * sizeof(KryonProcessInfo));
    if (!proc_list) {
        inferno_close(fd);
        return false;
    }

    int num_procs = 0;
    InfernoDir *dir;
    long nr;

    while ((nr = inferno_dirread(fd, &dir)) > 0) {
        // Skip non-directory entries
        if (!(dir->mode & DMDIR)) {
            free(dir);
            continue;
        }

        // Parse PID from directory name
        int pid = atoi(dir->name);
        if (pid <= 0) {
            free(dir);
            continue;
        }

        // Expand array if needed
        if (num_procs >= max_procs) {
            max_procs *= 2;
            KryonProcessInfo *new_list = realloc(proc_list, max_procs * sizeof(KryonProcessInfo));
            if (!new_list) {
                // Cleanup on failure
                for (int i = 0; i < num_procs; i++) {
                    if (proc_list[i].name) free(proc_list[i].name);
                    if (proc_list[i].command) free(proc_list[i].command);
                }
                free(proc_list);
                free(dir);
                inferno_close(fd);
                return false;
            }
            proc_list = new_list;
        }

        // Read process status
        if (read_proc_status_file(pid, &proc_list[num_procs])) {
            num_procs++;
        }

        free(dir);
    }

    inferno_close(fd);

    *procs = proc_list;
    *count = num_procs;
    return true;
}

/**
 * Free process list
 */
static void inferno_free_process_list(KryonProcessInfo *procs, int count) {
    if (!procs) {
        return;
    }

    for (int i = 0; i < count; i++) {
        if (procs[i].name) {
            free(procs[i].name);
        }
        if (procs[i].command) {
            free(procs[i].command);
        }
    }

    free(procs);
}

/**
 * Get information about a specific process
 */
static bool inferno_get_process_info(int pid, KryonProcessInfo *info) {
    if (!info || pid <= 0) {
        return false;
    }

    return read_proc_status_file(pid, info);
}

/**
 * Free process info
 */
static void inferno_free_process_info(KryonProcessInfo *info) {
    if (!info) {
        return;
    }

    if (info->name) {
        free(info->name);
    }
    if (info->command) {
        free(info->command);
    }
}

// =============================================================================
// PROCESS CONTROL
// =============================================================================

/**
 * Send signal to process
 */
static bool inferno_send_signal(int pid, const char *signal) {
    if (pid <= 0 || !signal) {
        return false;
    }

    // Open control file
    char path[64];
    snprintf(path, sizeof(path), "/prog/%d/ctl", pid);

    int fd = inferno_open(path, OWRITE);
    if (fd < 0) {
        return false;
    }

    // Write signal command
    int result = inferno_write(fd, (void*)signal, strlen(signal));
    inferno_close(fd);

    return result > 0;
}

/**
 * Open process control file
 */
static KryonProcessControl* inferno_open_proc_control(int pid) {
    if (pid <= 0) {
        return NULL;
    }

    // Open control file
    char path[64];
    snprintf(path, sizeof(path), "/prog/%d/ctl", pid);

    int fd = inferno_open(path, OWRITE);
    if (fd < 0) {
        return NULL;
    }

    // Allocate handle
    KryonProcessControl *ctl = malloc(sizeof(KryonProcessControl));
    if (!ctl) {
        inferno_close(fd);
        return NULL;
    }

    ctl->pid = pid;
    ctl->ctl_fd = fd;

    return ctl;
}

/**
 * Close process control file
 */
static void inferno_close_proc_control(KryonProcessControl *ctl) {
    if (!ctl) {
        return;
    }

    if (ctl->ctl_fd >= 0) {
        inferno_close(ctl->ctl_fd);
    }

    free(ctl);
}

/**
 * Write command to process control file
 */
static bool inferno_write_control(KryonProcessControl *ctl, const char *command) {
    if (!ctl || ctl->ctl_fd < 0 || !command) {
        return false;
    }

    int result = inferno_write(ctl->ctl_fd, (void*)command, strlen(command));
    return result > 0;
}

/**
 * Read process status
 */
static char* inferno_read_status(int pid) {
    if (pid <= 0) {
        return NULL;
    }

    char path[64];
    snprintf(path, sizeof(path), "/prog/%d/status", pid);

    int fd = inferno_open(path, OREAD);
    if (fd < 0) {
        return NULL;
    }

    // Read entire file
    char *buf = malloc(1024);
    if (!buf) {
        inferno_close(fd);
        return NULL;
    }

    int n = inferno_read(fd, buf, 1023);
    inferno_close(fd);

    if (n <= 0) {
        free(buf);
        return NULL;
    }

    buf[n] = '\0';
    return buf;
}

/**
 * Free status string
 */
static void inferno_free_status(char *status) {
    if (status) {
        free(status);
    }
}

/**
 * Wait for process to exit
 */
static bool inferno_wait_for_exit(int pid, int *exit_status, int timeout_ms) {
    if (pid <= 0) {
        return false;
    }

    // Open wait file
    char path[64];
    snprintf(path, sizeof(path), "/prog/%d/wait", pid);

    int fd = inferno_open(path, OREAD);
    if (fd < 0) {
        return false;
    }

    // Read wait result
    // TODO: Implement timeout using alarm() or similar
    (void)timeout_ms; // Unused for now

    char buf[256];
    int n = inferno_read(fd, buf, sizeof(buf) - 1);
    inferno_close(fd);

    if (n <= 0) {
        return false;
    }

    buf[n] = '\0';

    // Parse exit status from wait string
    // Format: "pid text: status"
    if (exit_status) {
        char *colon = strchr(buf, ':');
        if (colon) {
            *exit_status = atoi(colon + 1);
        } else {
            *exit_status = 0;
        }
    }

    return true;
}

/**
 * Get current process ID
 */
static int inferno_get_current_pid(void) {
    return getpid();
}

/**
 * Get parent process ID
 */
static int inferno_get_parent_pid(void) {
    return getppid();
}

// =============================================================================
// SERVICE INTERFACE
// =============================================================================

/**
 * Process control service interface for Inferno
 */
static KryonProcessControlService inferno_process_control_service = {
    .list_processes = inferno_list_processes,
    .free_process_list = inferno_free_process_list,
    .get_process_info = inferno_get_process_info,
    .free_process_info = inferno_free_process_info,
    .send_signal = inferno_send_signal,
    .open_proc_control = inferno_open_proc_control,
    .close_proc_control = inferno_close_proc_control,
    .write_control = inferno_write_control,
    .read_status = inferno_read_status,
    .free_status = inferno_free_status,
    .wait_for_exit = inferno_wait_for_exit,
    .get_current_pid = inferno_get_current_pid,
    .get_parent_pid = inferno_get_parent_pid,
};

/**
 * Get the Process Control service
 */
KryonProcessControlService* inferno_get_process_control_service(void) {
    return &inferno_process_control_service;
}

#endif // KRYON_PLUGIN_INFERNO
