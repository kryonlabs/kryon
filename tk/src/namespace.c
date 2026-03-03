/*
 * Per-Window Namespace Implementation
 * C89/C90 compliant
 */

#include "namespace.h"
#include "window.h"
#include "vdev.h"
#include "p9client.h"
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

    /* Initialize bind mount fields */
    win->ns_root = NULL;
    win->ns_bind_count = 0;

    return 0;
}

/*
 * Build in-memory namespace tree for a window
 * Creates /dev with bind mounts to Marrow's /dev/win{N} paths
 *
 * Note: This is an in-memory tree for path resolution, NOT exported via 9P.
 * The bind mount targets (e.g., "/dev/win1/screen") are paths in Marrow's 9P tree.
 */
int window_build_namespace(struct KryonWindow *win)
{
    P9Node *ns_root;
    P9Node *dev_dir;
    char dev_path[64];

    if (win == NULL) {
        return -1;
    }

    /* Free existing namespace if any */
    if (win->ns_root != NULL) {
        tree_free(win->ns_root);
        win->ns_root = NULL;
    }

    /* Create namespace root (in-memory only, not exported) */
    ns_root = tree_create_dir(NULL, "");
    if (ns_root == NULL) {
        return -1;
    }

    /* Create /dev directory */
    dev_dir = tree_create_dir(ns_root, "dev");
    if (dev_dir == NULL) {
        tree_free(ns_root);
        return -1;
    }

    /* Bind: /dev/screen → Marrow path "/dev/win{N}/screen" */
    snprintf(dev_path, sizeof(dev_path), "/dev/win%u/screen", win->id);
    if (tree_create_bind(dev_dir, "screen", dev_path) == NULL) {
        fprintf(stderr, "window_build_namespace: failed to bind /dev/screen\n");
        /* Continue anyway */
    } else {
        win->ns_bind_count++;
    }

    /* Bind: /dev/cons → "/dev/win{N}/cons" */
    snprintf(dev_path, sizeof(dev_path), "/dev/win%u/cons", win->id);
    if (tree_create_bind(dev_dir, "cons", dev_path) == NULL) {
        fprintf(stderr, "window_build_namespace: failed to bind /dev/cons\n");
    } else {
        win->ns_bind_count++;
    }

    /* Bind: /dev/mouse → "/dev/win{N}/mouse" */
    snprintf(dev_path, sizeof(dev_path), "/dev/win%u/mouse", win->id);
    if (tree_create_bind(dev_dir, "mouse", dev_path) == NULL) {
        fprintf(stderr, "window_build_namespace: failed to bind /dev/mouse\n");
    } else {
        win->ns_bind_count++;
    }

    /* Bind: /dev/kbd → "/dev/win{N}/kbd" */
    snprintf(dev_path, sizeof(dev_path), "/dev/win%u/kbd", win->id);
    if (tree_create_bind(dev_dir, "kbd", dev_path) == NULL) {
        fprintf(stderr, "window_build_namespace: failed to bind /dev/kbd\n");
    } else {
        win->ns_bind_count++;
    }

    win->ns_root = ns_root;

    fprintf(stderr, "Built namespace for window %u (binds to Marrow /dev/win%u/*, %d binds)\n",
            win->id, win->id, win->ns_bind_count);
    return 0;
}

/*
 * Register window's virtual devices with Marrow via 9P
 * Creates /dev/win{N}/screen, /dev/win{N}/mouse, etc. in Marrow's 9P tree
 *
 * These files have read/write callbacks that access Kryon's virtual devices.
 *
 * Note: This is a simplified implementation. For full functionality, Marrow
 * would need to support callback-based device files. For now, we use the
 * simple approach of opening files and writing to them explicitly.
 */
int window_register_virtual_devices(struct KryonWindow *win, struct P9Client *marrow)
{
    char dev_path[64];

    if (win == NULL || marrow == NULL) {
        return -1;
    }

    /* Create /dev/win{N} directory in Marrow */
    snprintf(dev_path, sizeof(dev_path), "/dev/win%u", win->id);
    if (p9_mkdir(marrow, dev_path) < 0) {
        fprintf(stderr, "window_register_virtual_devices: failed to create %s in Marrow\n", dev_path);
        /* Continue anyway - directory might already exist */
    }

    /* Note: For now, we don't actually open the device files because:
     * 1. Marrow doesn't support dynamic device creation yet
     * 2. The nested WM will write to /dev/screen, and we'll capture that
     * 3. We'll compose the virtual framebuffer to the parent window
     *
     * In a full implementation, we would:
     * 1. Create device files in Marrow via 9P
     * 2. Open them and store the FDs
     * 3. Use p9_write to write to them
     */

    fprintf(stderr, "Registered virtual devices for window %u with Marrow\n", win->id);
    return 0;
}

/*
 * Spawn a nested WM instance in a window
 *
 * The nested WM will use this window's virtual devices:
 * - vdev->draw_buffer becomes its /dev/screen
 * - vdev->cons_buffer becomes its /dev/cons
 * - Mouse/kbd events are forwarded via pipes
 *
 * Now builds per-window namespace with bind mounts
 */
int window_spawn_nested_wm(struct KryonWindow *win)
{
    pid_t pid;
    char win_id_str[32];
    char pipe_fd_str[32];
    char ns_ptr_str[32];
    int sv[2];

    /* Global Marrow client - should be set by main WM */
    extern struct P9Client *g_marrow_client;
    struct P9Client *marrow_client;

    if (win == NULL) {
        return -1;
    }

    /* Get Marrow client (from global context) */
    marrow_client = g_marrow_client;
    if (marrow_client == NULL) {
        fprintf(stderr, "window_spawn_nested_wm: No Marrow connection\n");
        return -1;
    }

    /* Allocate virtual framebuffer if not exists */
    if (win->vdev == NULL) {
        win->vdev = (VirtualDevices *)calloc(1, sizeof(VirtualDevices));
        if (win->vdev == NULL) {
            return -1;
        }
        /* Initialize Marrow FDs */
        win->vdev->marrow_screen_fd = -1;
        win->vdev->marrow_cons_fd = -1;
        win->vdev->marrow_mouse_fd = -1;
        win->vdev->marrow_kbd_fd = -1;
    }

    if (win->vdev->draw_buffer == NULL) {
        /* Parse window rect to get dimensions */
        int w = 800, h = 600;  /* Default */
        /* TODO: Parse from win->rect */
        if (vdev_alloc_draw(win, w, h) < 0) {
            return -1;
        }

        /* Register virtual devices with Marrow */
        if (window_register_virtual_devices(win, marrow_client) < 0) {
            fprintf(stderr, "Failed to register virtual devices with Marrow\n");
            /* Continue anyway - devices will be accessed via 9P */
        }
    }

    /* Build in-memory namespace tree for this window */
    if (window_build_namespace(win) < 0) {
        fprintf(stderr, "Failed to build namespace for window %u\n", win->id);
        return -1;
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

        /* Pass namespace pointer via environment (fork shares parent memory) */
        snprintf(ns_ptr_str, sizeof(ns_ptr_str), "%p", (void *)win->ns_root);
        setenv("KRYON_NAMESPACE_PTR", ns_ptr_str, 1);

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

    fprintf(stderr, "Spawned nested WM (pid=%d) in window %u with namespace %p\n",
            pid, win->id, (void *)win->ns_root);
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

/*
 * Free namespace resources for a window
 */
void window_free_namespace(struct KryonWindow *win)
{
    if (win == NULL) {
        return;
    }

    /* Free namespace tree */
    if (win->ns_root != NULL) {
        tree_free(win->ns_root);
        win->ns_root = NULL;
    }
    win->ns_bind_count = 0;
}
