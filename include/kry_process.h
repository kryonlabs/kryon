/*
 * kry_process.h - Kry standard library: subprocess execution.
 *
 * A small, IDE-facing wrapper around the POSIX fork/pipe/dup2/exec/waitpid
 * pattern. A KryProcess runs a shell command with combined stdout/stderr
 * captured to a non-blocking pipe, so a UI frame loop can drain output a few
 * times per second without blocking. Windows currently has no implementation;
 * callers should treat a -1 return as "unavailable".
 */
#ifndef KRYON_KRY_PROCESS_H
#define KRYON_KRY_PROCESS_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int pid;          /* child process id, or 0 when not running */
    int read_fd;      /* non-blocking read end of the stdout+stderr pipe */
    int running;      /* 1 until the child is reaped */
    int exit_status;  /* valid once running == 0 */
} KryProcess;

/* Spawn `command` (run via "/bin/sh -lc") with the child's working directory
 * set to `cwd` (pass NULL to inherit). On success fills `p` and returns 1.
 * Returns 0 (and leaves p zeroed) if the platform has no implementation or
 * spawn fails. Combined stdout/stderr is readable via kry_process_read_poll. */
int kry_process_spawn(KryProcess *p, const char *command, const char *cwd);

/* Non-blocking read of pending process output into `buf` (up to `cap-1` bytes,
 * NUL-terminated). Returns the byte count read (0 if nothing pending), or -1
 * if the process has no pipe / is not running. Safe to call every frame. */
int kry_process_read_poll(KryProcess *p, char *buf, int cap);

/* Non-blocking reap (waitpid WNOHANG). Returns 1 if the child exited this call
 * (exit_status is then set, running cleared); 0 if still running. A reaped
 * process drains any remaining output before recording its exit status. */
int kry_process_wait_poll(KryProcess *p);

/* Send SIGTERM to the child if still running. Does not block; pair with a final
 * kry_process_wait_poll or kry_process_close to reap. */
void kry_process_kill(KryProcess *p);

/* Reap (blocking) if still running and close the read pipe. Resets the struct
 * to a clean not-running state. Safe to call on an already-closed process. */
void kry_process_close(KryProcess *p);

#ifdef __cplusplus
}
#endif

#endif /* KRYON_KRY_PROCESS_H */
