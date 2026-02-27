/*
 * Per-Window Namespace Implementation
 * C89/C90 compliant
 */

#include "namespace.h"
#include "window.h"
#include "p9client.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

    win->ns_client = NULL;
    win->ns_mount_spec = NULL;
    win->ns_type = NS_LOCAL;
    win->ns_root = NULL;
    win->remote_screen_fd = -1;
    win->remote_mouse_fd = -1;
    win->remote_kbd_fd = -1;

    return 0;
}

/*
 * Mount a remote WM or filesystem into window
 * Spec formats:
 *   "local!"              - Empty local namespace
 *   "tcp!host!port"       - Connect to remote WM
 *   "nest!widthxheight"   - Run nested WM (future)
 *   "local!/path"         - Mount local filesystem (future)
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

    /* Handle "local!" - empty namespace */
    if (strcmp(type, "local") == 0) {
        /* Check if it's just "local!" (not "local!/path") */
        size_t spec_len = strlen(spec);
        if (spec_len == 6) {
            /* "local!" - empty namespace */
            win->ns_type = NS_LOCAL;
            if (win->ns_mount_spec != NULL) {
                free(win->ns_mount_spec);
            }
            win->ns_mount_spec = strdup("local");
            if (win->ns_mount_spec == NULL) {
                return -1;
            }
            fprintf(stderr, "Mounted local namespace for window %u\n", win->id);
            return 0;
        }
        /* TODO: Handle "local!/path" for filesystem mounting */
    }

    /* Handle "tcp!host!port" - connect to remote WM */
    if (strcmp(type, "tcp") == 0) {
        char *host = strtok(NULL, "!");
        char *port_str = strtok(NULL, "!");
        char addr[256];

        if (host == NULL || port_str == NULL) {
            fprintf(stderr, "window_namespace_mount: invalid tcp spec: %s\n", spec);
            return -1;
        }

        /* Build address string */
        snprintf(addr, sizeof(addr), "tcp!%s!%s", host, port_str);

        /* Disconnect existing connection if any */
        if (win->ns_client != NULL) {
            p9_disconnect(win->ns_client);
            win->ns_client = NULL;
        }

        /* Connect to remote WM */
        win->ns_client = p9_connect(addr);
        if (win->ns_client == NULL) {
            fprintf(stderr, "window_namespace_mount: failed to connect to %s\n", addr);
            return -1;
        }

        /* Authenticate */
        if (p9_authenticate(win->ns_client, 0, "none", "") < 0) {
            fprintf(stderr, "window_namespace_mount: failed to authenticate with %s\n", addr);
            p9_disconnect(win->ns_client);
            win->ns_client = NULL;
            return -1;
        }

        /* Open remote /dev/screen for reading */
        win->remote_screen_fd = p9_open(win->ns_client, "/dev/screen", P9_OREAD);
        if (win->remote_screen_fd < 0) {
            fprintf(stderr, "window_namespace_mount: failed to open remote /dev/screen\n");
            p9_disconnect(win->ns_client);
            win->ns_client = NULL;
            return -1;
        }

        /* Open remote /dev/mouse for writing (optional) */
        win->remote_mouse_fd = p9_open(win->ns_client, "/dev/mouse", P9_OWRITE);
        if (win->remote_mouse_fd < 0) {
            fprintf(stderr, "window_namespace_mount: warning: failed to open remote /dev/mouse\n");
        }

        /* Open remote /dev/kbd for writing (optional) */
        win->remote_kbd_fd = p9_open(win->ns_client, "/dev/kbd", P9_OWRITE);
        if (win->remote_kbd_fd < 0) {
            fprintf(stderr, "window_namespace_mount: warning: failed to open remote /dev/kbd\n");
        }

        win->ns_type = NS_REMOTE_WM;
        if (win->ns_mount_spec != NULL) {
            free(win->ns_mount_spec);
        }
        win->ns_mount_spec = strdup(spec);
        if (win->ns_mount_spec == NULL) {
            /* Connection succeeded but strdup failed - non-fatal */
            fprintf(stderr, "window_namespace_mount: warning: failed to strdup mount spec\n");
        }

        fprintf(stderr, "Connected window %u to remote WM at %s\n", win->id, addr);
        return 0;
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

    /* Close remote connections */
    if (win->remote_screen_fd >= 0 && win->ns_client != NULL) {
        p9_close(win->ns_client, win->remote_screen_fd);
        win->remote_screen_fd = -1;
    }

    if (win->remote_mouse_fd >= 0 && win->ns_client != NULL) {
        p9_close(win->ns_client, win->remote_mouse_fd);
        win->remote_mouse_fd = -1;
    }

    if (win->remote_kbd_fd >= 0 && win->ns_client != NULL) {
        p9_close(win->ns_client, win->remote_kbd_fd);
        win->remote_kbd_fd = -1;
    }

    /* Disconnect client */
    if (win->ns_client != NULL) {
        p9_disconnect(win->ns_client);
        win->ns_client = NULL;
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
 * Resolve path in window's namespace
 */
P9Node *window_namespace_lookup(struct KryonWindow *win, const char *path)
{
    /* TODO: Implement namespace lookup */
    (void)win;
    (void)path;
    return NULL;
}
