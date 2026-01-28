/**
 * @file process_control.h
 * @brief Process Control Service - Advanced process management
 *
 * Provides access to OS-specific process control features:
 * - Process enumeration beyond standard
 * - Control files (Inferno: /prog/PID/ctl)
 * - Process status and monitoring
 * - Process signals and commands
 *
 * @version 1.0.0
 * @author Kryon Labs
 */

#ifndef KRYON_SERVICE_PROCESS_CONTROL_H
#define KRYON_SERVICE_PROCESS_CONTROL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>

// =============================================================================
// PROCESS INFORMATION
// =============================================================================

/**
 * @brief Process state
 */
typedef enum {
    KRYON_PROC_UNKNOWN = 0,
    KRYON_PROC_RUNNING,         /**< Running or runnable */
    KRYON_PROC_SLEEPING,        /**< Sleeping (waiting for event) */
    KRYON_PROC_STOPPED,         /**< Stopped (by signal or debugger) */
    KRYON_PROC_ZOMBIE,          /**< Zombie (terminated but not reaped) */
    KRYON_PROC_DEAD,            /**< Dead/terminated */
} KryonProcessState;

/**
 * @brief Process information
 */
typedef struct {
    int pid;                    /**< Process ID */
    int ppid;                   /**< Parent process ID */
    char *name;                 /**< Process name */
    char *command;              /**< Full command line */
    KryonProcessState state;    /**< Process state */
    int64_t memory;             /**< Memory usage in bytes */
    uint64_t cpu_time;          /**< CPU time used (microseconds) */
    int priority;               /**< Process priority */
    uid_t uid;                  /**< User ID */
    gid_t gid;                  /**< Group ID */
} KryonProcessInfo;

// =============================================================================
// PROCESS CONTROL HANDLE
// =============================================================================

/**
 * @brief Opaque process control handle
 *
 * Represents an open control file for a process (Inferno: /prog/PID/ctl).
 */
typedef struct KryonProcessControl KryonProcessControl;

// =============================================================================
// PROCESS CONTROL SERVICE INTERFACE
// =============================================================================

/**
 * @brief Process control service interface
 *
 * Apps receive this interface via kryon_services_get_interface().
 */
typedef struct {
    /**
     * List all processes
     *
     * @param procs Output: array of process info (plugin allocates)
     * @param count Output: number of processes
     * @return true on success, false on failure
     *
     * @example
     * KryonProcessInfo *procs;
     * int count;
     * if (pcs->list_processes(&procs, &count)) {
     *     for (int i = 0; i < count; i++) {
     *         printf("PID %d: %s\n", procs[i].pid, procs[i].name);
     *     }
     *     pcs->free_process_list(procs, count);
     * }
     */
    bool (*list_processes)(KryonProcessInfo **procs, int *count);

    /**
     * Free process list returned by list_processes
     *
     * @param procs Process array to free
     * @param count Number of processes
     */
    void (*free_process_list)(KryonProcessInfo *procs, int count);

    /**
     * Get information about a specific process
     *
     * @param pid Process ID
     * @param info Output: process information
     * @return true on success, false if process doesn't exist
     */
    bool (*get_process_info)(int pid, KryonProcessInfo *info);

    /**
     * Free process info returned by get_process_info
     *
     * @param info Process info to free
     */
    void (*free_process_info)(KryonProcessInfo *info);

    /**
     * Send signal to process
     *
     * @param pid Process ID
     * @param signal Signal name (e.g., "kill", "stop", "cont")
     * @return true on success, false on failure
     */
    bool (*send_signal)(int pid, const char *signal);

    /**
     * Open process control file
     *
     * Opens the control file for a process (Inferno: /prog/PID/ctl).
     * This allows sending control commands beyond simple signals.
     *
     * @param pid Process ID
     * @return Control handle, or NULL on failure
     *
     * @example
     * KryonProcessControl *ctl = pcs->open_proc_control(1234);
     * if (ctl) {
     *     pcs->write_control(ctl, "hang");  // Inferno: hang process
     *     pcs->close_proc_control(ctl);
     * }
     */
    KryonProcessControl* (*open_proc_control)(int pid);

    /**
     * Close process control file
     *
     * @param ctl Control handle to close
     */
    void (*close_proc_control)(KryonProcessControl *ctl);

    /**
     * Write command to process control file
     *
     * @param ctl Control handle
     * @param command Command string (e.g., "hang", "kill", "stop")
     * @return true on success, false on failure
     */
    bool (*write_control)(KryonProcessControl *ctl, const char *command);

    /**
     * Read process status
     *
     * Reads detailed status from /prog/PID/status (Inferno) or
     * /proc/PID/status (Linux).
     *
     * @param pid Process ID
     * @return Status string (caller must free), or NULL on failure
     */
    char* (*read_status)(int pid);

    /**
     * Free status string
     *
     * @param status Status string returned by read_status
     */
    void (*free_status)(char *status);

    /**
     * Wait for process to exit
     *
     * @param pid Process ID (0 for any child)
     * @param exit_status Output: exit status
     * @param timeout_ms Timeout in milliseconds (0 = no timeout)
     * @return true if process exited, false on timeout or error
     */
    bool (*wait_for_exit)(int pid, int *exit_status, int timeout_ms);

    /**
     * Get current process ID
     *
     * @return Current process ID
     */
    int (*get_current_pid)(void);

    /**
     * Get parent process ID
     *
     * @return Parent process ID
     */
    int (*get_parent_pid)(void);

} KryonProcessControlService;

#ifdef __cplusplus
}
#endif

#endif // KRYON_SERVICE_PROCESS_CONTROL_H
