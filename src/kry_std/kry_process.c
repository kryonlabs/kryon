/*
 * kry_process.c - Kry standard library: subprocess execution.
 *
 * Small subprocess API for Kry apps: fork/pipe/dup2/execl with non-blocking
 * waitpid so apps can run builds, consoles, and run targets without touching
 * POSIX directly.
 */
/* Request POSIX 2008 + default extensions so fork/pipe/dup2/waitpid/kill are
 * declared regardless of how the including TU sets feature macros. */
#if !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif
#if !defined(_DEFAULT_SOURCE)
#define _DEFAULT_SOURCE
#endif

#include "kry_process.h"

#include <stdlib.h>
#include <string.h>

#if !defined(_WIN32)
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

#define KRY_READ_END 0
#define KRY_WRITE_END 1

static void
set_nonblocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);

    if(flags >= 0)
        fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

int
kry_process_spawn(KryProcess *p, const char *command, const char *cwd)
{
    int pipefd[2];
    int pid;

    if(p == NULL || command == NULL)
        return 0;
    memset(p, 0, sizeof(*p));
    if(pipe(pipefd) != 0)
        return 0;
    pid = fork();
    if(pid < 0) {
        close(pipefd[KRY_READ_END]);
        close(pipefd[KRY_WRITE_END]);
        return 0;
    }
    if(pid == 0) {
        /* Child: route stdout+stderr to the pipe write end, then exec sh. */
        setpgid(0, 0);
        close(pipefd[KRY_READ_END]);
        dup2(pipefd[KRY_WRITE_END], STDOUT_FILENO);
        dup2(pipefd[KRY_WRITE_END], STDERR_FILENO);
        close(pipefd[KRY_WRITE_END]);
        if(cwd != NULL && cwd[0] != '\0') {
            if(chdir(cwd) != 0)
                _exit(127);
        }
        execl("/bin/sh", "sh", "-lc", command, (char *)NULL);
        _exit(127);
    }
    /* Parent: keep the non-blocking read end, close the write end. */
    close(pipefd[KRY_WRITE_END]);
    set_nonblocking(pipefd[KRY_READ_END]);
    p->pid = pid;
    p->read_fd = pipefd[KRY_READ_END];
    p->running = 1;
    p->exit_status = 0;
    return 1;
}

int
kry_process_read_poll(KryProcess *p, char *buf, int cap)
{
    int n;

    if(p == NULL || buf == NULL || cap <= 1 || p->read_fd < 0)
        return -1;
    n = (int)read(p->read_fd, buf, (size_t)(cap - 1));
    if(n < 0) {
        if(errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)
            return 0;   /* nothing pending right now */
        return -1;      /* pipe broken */
    }
    if(n == 0)
        return -1;      /* EOF: write end closed */
    buf[n] = '\0';
    return n;
}

int
kry_process_wait_poll(KryProcess *p)
{
    int status;
    int wpid;

    if(p == NULL || !p->running || p->pid <= 0)
        return 0;
    wpid = waitpid(p->pid, &status, WNOHANG);
    if(wpid <= 0)
        return 0;   /* still running, or waitpid interrupted */
    /* Drained any final output before recording exit. */
    p->running = 0;
    if(WIFEXITED(status))
        p->exit_status = WEXITSTATUS(status);
    else if(WIFSIGNALED(status))
        p->exit_status = -WTERMSIG(status);
    else
        p->exit_status = 1;
    return 1;
}

void
kry_process_kill(KryProcess *p)
{
    if(p == NULL || !p->running || p->pid <= 0)
        return;
    kill(-p->pid, SIGTERM);
    kill(p->pid, SIGTERM);
}

void
kry_process_close(KryProcess *p)
{
    int status;

    if(p == NULL)
        return;
    if(p->running && p->pid > 0) {
        kill(-p->pid, SIGTERM);
        kill(p->pid, SIGTERM);
        /* Best-effort blocking reap so we don't leak a zombie. */
        while(waitpid(p->pid, &status, 0) < 0 && errno == EINTR)
            ;
    }
    if(p->read_fd >= 0)
        close(p->read_fd);
    memset(p, 0, sizeof(*p));
    p->read_fd = -1;
}

#else  /* _WIN32 */

int
kry_process_spawn(KryProcess *p, const char *command, const char *cwd)
{
    (void)p;
    (void)command;
    (void)cwd;
    return 0;
}

int
kry_process_read_poll(KryProcess *p, char *buf, int cap)
{
    (void)p;
    (void)buf;
    (void)cap;
    return -1;
}

int
kry_process_wait_poll(KryProcess *p)
{
    (void)p;
    return 0;
}

void
kry_process_kill(KryProcess *p)
{
    (void)p;
}

void
kry_process_close(KryProcess *p)
{
    (void)p;
}

#endif /* _WIN32 */
