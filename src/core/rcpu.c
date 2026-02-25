/*
 * Kryon CPU Server - rcpu Protocol Handler
 * C89/C90 compliant
 */

#include "rcpu.h"
#include "cpu_server.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <signal.h>
#include <fcntl.h>

/*
 * Read the rcpu script from the connection
 * Format: "NNNNNN\n<script_content>" (7-digit length + newline + script)
 * Returns 0 on success, -1 on error
 */
static int read_rcpu_script(int fd, char **script_out)
{
    char len_str[8];
    long script_len;
    char *script;
    ssize_t n;
    size_t total;

    /* Read 7-digit length + newline */
    n = recv(fd, len_str, 8, MSG_WAITALL);
    if (n != 8) {
        fprintf(stderr, "rcpu: failed to read script length (got %zd bytes)\n", n);
        return -1;
    }

    /* Null-terminate the length string */
    len_str[7] = '\0';

    /* Parse length */
    script_len = atol(len_str);
    if (script_len <= 0 || script_len > 1024 * 1024) {  /* Max 1MB */
        fprintf(stderr, "rcpu: invalid script length: %ld\n", script_len);
        return -1;
    }

    fprintf(stderr, "rcpu: reading %ld-byte script\n", script_len);

    /* Allocate buffer for script */
    script = (char *)malloc(script_len + 1);
    if (script == NULL) {
        fprintf(stderr, "rcpu: failed to allocate %ld-byte script buffer\n",
                script_len);
        return -1;
    }

    /* Read script content */
    total = 0;
    while (total < (size_t)script_len) {
        n = recv(fd, script + total, script_len - total, MSG_WAITALL);
        if (n <= 0) {
            fprintf(stderr, "rcpu: failed to read script content (got %zd bytes)\n", n);
            free(script);
            return -1;
        }
        total += n;
    }
    script[script_len] = '\0';

    fprintf(stderr, "rcpu: script content:\n%s\n[end of script]\n", script);

    *script_out = script;
    return 0;
}

/*
 * Forward data between file descriptors
 * Returns 0 on success, -1 on error
 */
static int forward_io(int src_fd, int dst_fd, const char *label)
{
    char buf[4096];
    ssize_t n;

    n = read(src_fd, buf, sizeof(buf));
    if (n < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            fprintf(stderr, "rcpu: %s read error: %s\n", label, strerror(errno));
            return -1;
        }
        return 0;
    }

    if (n == 0) {
        /* EOF - don't log as error, this is normal */
        return -1;
    }

    if (write(dst_fd, buf, n) < 0) {
        fprintf(stderr, "rcpu: %s write error: %s\n", label, strerror(errno));
        return -1;
    }

    return 0;
}

/*
 * Spawn shell with the rcpu script
 * Returns shell PID on success, -1 on error
 */
static pid_t spawn_shell_with_script(int fd, const char *script)
{
    pid_t pid;
    int stdin_pipe[2];
    int stdout_pipe[2];
    int stderr_pipe[3];

    /* Create pipes for shell communication */
    if (pipe(stdin_pipe) < 0) {
        fprintf(stderr, "rcpu: pipe(stdin) failed: %s\n", strerror(errno));
        return -1;
    }

    if (pipe(stdout_pipe) < 0) {
        fprintf(stderr, "rcpu: pipe(stdout) failed: %s\n", strerror(errno));
        close(stdin_pipe[0]);
        close(stdin_pipe[1]);
        return -1;
    }

    if (pipe(stderr_pipe) < 0) {
        fprintf(stderr, "rcpu: pipe(stderr) failed: %s\n", strerror(errno));
        close(stdin_pipe[0]);
        close(stdin_pipe[1]);
        close(stdout_pipe[0]);
        close(stdout_pipe[1]);
        return -1;
    }

    pid = fork();
    if (pid < 0) {
        fprintf(stderr, "rcpu: fork() failed: %s\n", strerror(errno));
        close(stdin_pipe[0]);
        close(stdin_pipe[1]);
        close(stdout_pipe[0]);
        close(stdout_pipe[1]);
        close(stderr_pipe[0]);
        close(stderr_pipe[1]);
        return -1;
    }

    if (pid == 0) {
        /* Child process: shell */

        /* Close pipe ends we don't need */
        close(stdin_pipe[1]);   /* Close write end of stdin */
        close(stdout_pipe[0]);  /* Close read end of stdout */
        close(stderr_pipe[0]);  /* Close read end of stderr */

        /* Dup pipes to stdin/stdout/stderr */
        if (dup2(stdin_pipe[0], STDIN_FILENO) < 0) {
            perror("dup2 stdin");
            exit(1);
        }
        if (dup2(stdout_pipe[1], STDOUT_FILENO) < 0) {
            perror("dup2 stdout");
            exit(1);
        }
        if (dup2(stderr_pipe[1], STDERR_FILENO) < 0) {
            perror("dup2 stderr");
            exit(1);
        }

        /* Close original pipe fds */
        close(stdin_pipe[0]);
        close(stdout_pipe[1]);
        close(stderr_pipe[1]);

        /* Execute the script via shell */
        /* The script should contain commands to start rc shell */
        execl("/bin/sh", "sh", "-c", script, NULL);
        perror("execl sh");

        /* If exec fails, try with bash */
        execl("/bin/bash", "bash", "-c", script, NULL);
        perror("execl bash");

        exit(1);
    }

    /* Parent process: handle I/O forwarding */

    /* Close pipe ends we don't need */
    close(stdin_pipe[0]);   /* Close read end of stdin */
    close(stdout_pipe[1]);  /* Close write end of stdout */
    close(stderr_pipe[1]);  /* Close write end of stderr */

    fprintf(stderr, "rcpu: spawned shell with PID %d\n", pid);

    /* Set non-blocking mode for pipes */
    {
        int flags;

        flags = fcntl(stdout_pipe[0], F_GETFL, 0);
        if (flags >= 0) {
            fcntl(stdout_pipe[0], F_SETFL, flags | O_NONBLOCK);
        }

        flags = fcntl(stderr_pipe[0], F_GETFL, 0);
        if (flags >= 0) {
            fcntl(stderr_pipe[0], F_SETFL, flags | O_NONBLOCK);
        }

        flags = fcntl(fd, F_GETFL, 0);
        if (flags >= 0) {
            fcntl(fd, F_SETFL, flags | O_NONBLOCK);
        }
    }

    /* Main I/O forwarding loop */
    {
        fd_set readfds;
        int max_fd;
        int shell_running = 1;
        int status;

        max_fd = stdout_pipe[0];
        if (stderr_pipe[0] > max_fd) max_fd = stderr_pipe[0];
        if (fd > max_fd) max_fd = fd;

        while (shell_running) {
            struct timeval tv;

            FD_ZERO(&readfds);
            FD_SET(stdout_pipe[0], &readfds);
            FD_SET(stderr_pipe[0], &readfds);
            FD_SET(fd, &readfds);

            tv.tv_sec = 1;
            tv.tv_usec = 0;

            if (select(max_fd + 1, &readfds, NULL, NULL, &tv) < 0) {
                if (errno == EINTR) {
                    /* Check if child exited */
                    if (waitpid(pid, &status, WNOHANG) == pid) {
                        shell_running = 0;
                    }
                    continue;
                }
                break;
            }

            /* Forward shell stdout to socket */
            if (FD_ISSET(stdout_pipe[0], &readfds)) {
                if (forward_io(stdout_pipe[0], fd, "stdout") < 0) {
                    shell_running = 0;
                    break;
                }
            }

            /* Forward shell stderr to socket */
            if (FD_ISSET(stderr_pipe[0], &readfds)) {
                if (forward_io(stderr_pipe[0], fd, "stderr") < 0) {
                    /* Don't break on stderr EOF */
                }
            }

            /* Forward socket to shell stdin */
            if (FD_ISSET(fd, &readfds)) {
                if (forward_io(fd, stdin_pipe[1], "socket") < 0) {
                    shell_running = 0;
                    break;
                }
            }

            /* Check if child process exited */
            if (waitpid(pid, &status, WNOHANG) == pid) {
                fprintf(stderr, "rcpu: shell exited (status=%d)\n", status);
                shell_running = 0;
            }
        }
    }

    /* Cleanup */
    close(stdin_pipe[1]);
    close(stdout_pipe[0]);
    close(stderr_pipe[0]);

    /* Kill shell if still running */
    kill(pid, SIGTERM);
    waitpid(pid, NULL, 0);

    return pid;
}

/*
 * Handle rcpu connection (basic version - spawns shell and forwards I/O)
 * Returns 0 on success, -1 on error (fd is always closed)
 *
 * NOTE: This is a simplified implementation that just spawns a shell.
 * A full rcpu implementation would continue to serve 9P after the shell.
 */
int handle_rcpu_connection(int fd)
{
    char *script = NULL;
    pid_t shell_pid;

    fprintf(stderr, "rcpu: handling new connection\n");

    /* Read the script */
    if (read_rcpu_script(fd, &script) < 0) {
        goto error;
    }

    /* The script from drawterm typically:
     * 1. Mounts /fd/0 to /mnt/term
     * 2. Binds console
     * 3. Starts rc shell
     *
     * For now, we'll just run a simple shell since we don't
     * have the full Plan 9 namespace infrastructure.
     */

    /* Spawn shell with the script */
    shell_pid = spawn_shell_with_script(fd, script);
    if (shell_pid < 0) {
        fprintf(stderr, "rcpu: failed to spawn shell\n");
        free(script);
        goto error;
    }

    fprintf(stderr, "rcpu: connection handler complete\n");

    free(script);
    close(fd);
    return 0;

error:
    close(fd);
    return -1;
}
