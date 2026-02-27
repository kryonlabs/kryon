/*
 * Per-Window Namespace Implementation
 * C89/C90 compliant
 */

#include "namespace.h"
#include "window.h"
#include "vdev.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <signal.h>

/*
 * snprintf prototype for C89 compatibility
 */
extern int snprintf(char *str, size_t size, const char *format, ...);

/*
 * Initialize namespace for a window
 */
int window_namespace_init(struct KryonWindow *win)
{
    if (win == NULL) {
        return -1;
    }

    win->ns_mount_spec = NULL;
    win->ns_type = NS_LOCAL;
    win->nested_wm_pid = 0;
    win->nested_fd_in = -1;
    win->nested_fd_out = -1;

    return 0;
}

/*
 * Spawn a nested WM instance in a window
 *
 * The nested WM will use this window's virtual devices:
 * - vdev->draw_buffer becomes its /dev/screen
 * - vdev->cons_buffer becomes its /dev/cons
 * - Mouse/kbd events are forwarded via pipes
 */
int window_spawn_nested_wm(struct KryonWindow *win)
{
    pid_t pid;
    char win_id_str[32];
    char pipe_fd_str[32];
    int sv[2];

    if (win == NULL) {
        return -1;
    }

    /* Allocate virtual framebuffer if not exists */
    if (win->vdev == NULL) {
        win->vdev = (VirtualDevices *)calloc(1, sizeof(VirtualDevices));
        if (win->vdev == NULL) {
            return -1;
        }
    }

    if (win->vdev->draw_buffer == NULL) {
        /* Parse window rect to get dimensions */
        int w = 800, h = 600;  /* Default */
        /* TODO: Parse from win->rect */
        if (vdev_alloc_draw(win, w, h) < 0) {
            return -1;
        }
    }

    /* Create communication pipes for input forwarding */
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) < 0) {
        perror("socketpair");
        return -1;
    }
    win->nested_fd_in = sv[0];
    win->nested_fd_out = sv[1];

    /* Mark window as hosting nested WM */
    win->ns_type = NS_NESTED_WM;
    if (win->ns_mount_spec != NULL) {
        free(win->ns_mount_spec);
    }
    win->ns_mount_spec = (char *)malloc(6);
    if (win->ns_mount_spec != NULL) {
        strcpy(win->ns_mount_spec, "nest!");
    }

    /* Fork and exec nested WM */
    pid = fork();
    if (pid < 0) {
        perror("fork");
        close(sv[0]);
        close(sv[1]);
        return -1;
    }

    if (pid == 0) {
        /* Child: nested WM process */

        /* Close parent's end of pipe */
        close(sv[0]);

        /* Create unique identifier for this window's devices */
        snprintf(win_id_str, sizeof(win_id_str), "%u", win->id);

        /* Set environment variables so nested WM knows which window to use */
        setenv("KRYON_NESTED_WIN_ID", win_id_str, 1);

        /* Pass pipe FD as env var */
        snprintf(pipe_fd_str, sizeof(pipe_fd_str), "%d", sv[1]);
        setenv("KRYON_NESTED_PIPE_FD", pipe_fd_str, 1);

        /* Exec nested WM */
        execl("./bin/kryon-wm", "kryon-wm", "--nested", win_id_str, NULL);

        /* If exec fails */
        perror("execl kryon-wm");
        close(sv[1]);
        exit(1);
    }

    /* Parent: close child's end of pipe */
    close(sv[1]);

    /* Store child PID for later cleanup */
    win->nested_wm_pid = pid;

    fprintf(stderr, "Spawned nested WM (pid=%d) in window %u\n", pid, win->id);
    return 0;
}

/*
 * Mount filesystem into window's namespace
 *
 * Spec formats:
 *   "nest!"              - Run nested WM instance
 *   "local!/path"        - Mount local filesystem
 */
int window_namespace_mount(struct KryonWindow *win, const char *spec)
{
    char spec_copy[256];
    char *type;

    if (win == NULL || spec == NULL) {
        return -1;
    }

    /* Copy spec (strtok modifies input) */
    strncpy(spec_copy, spec, sizeof(spec_copy) - 1);
    spec_copy[sizeof(spec_copy) - 1] = '\0';

    /* Get mount type */
    type = strtok(spec_copy, "!");
    if (type == NULL) {
        fprintf(stderr, "window_namespace_mount: invalid spec '%s'\n", spec);
        return -1;
    }

    /* Handle "nest!" - spawn nested WM */
    if (strcmp(type, "nest") == 0) {
        return window_spawn_nested_wm(win);
    }

    /* Handle "local!" - empty namespace */
    if (strcmp(type, "local") == 0) {
        size_t spec_len = strlen(spec);

        /* "local!" - empty namespace */
        if (spec_len == 6) {
            win->ns_type = NS_LOCAL;
            if (win->ns_mount_spec != NULL) {
                free(win->ns_mount_spec);
            }
            win->ns_mount_spec = (char *)malloc(6);
            if (win->ns_mount_spec != NULL) {
                strcpy(win->ns_mount_spec, "local");
            }
            fprintf(stderr, "Mounted local namespace for window %u\n", win->id);
            return 0;
        }

        /* TODO: Handle "local!/path" for filesystem mounting */
        fprintf(stderr, "window_namespace_mount: filesystem mount not yet implemented\n");
        return -1;
    }

    fprintf(stderr, "window_namespace_mount: unknown mount type: %s\n", type);
    return -1;
}

/*
 * Unmount/cleanup namespace
 */
int window_namespace_unmount(struct KryonWindow *win)
{
    if (win == NULL) {
        return -1;
    }

    /* Kill nested WM if running */
    if (win->nested_wm_pid > 0) {
        fprintf(stderr, "Killing nested WM (pid=%d) in window %u\n",
                win->nested_wm_pid, win->id);
        kill(win->nested_wm_pid, SIGTERM);
        waitpid(win->nested_wm_pid, NULL, 0);
        win->nested_wm_pid = 0;
    }

    /* Close communication pipes */
    if (win->nested_fd_in >= 0) {
        close(win->nested_fd_in);
        win->nested_fd_in = -1;
    }

    if (win->nested_fd_out >= 0) {
        close(win->nested_fd_out);
        win->nested_fd_out = -1;
    }

    /* Free mount spec */
    if (win->ns_mount_spec != NULL) {
        free(win->ns_mount_spec);
        win->ns_mount_spec = NULL;
    }

    win->ns_type = NS_LOCAL;
    fprintf(stderr, "Unmounted namespace for window %u\n", win->id);

    return 0;
}
