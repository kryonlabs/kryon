/*
 * Kryon Marrow Client Functions
 * C89/C90 compliant
 *
 * Provides functions for Kryon to register with Marrow and mount filesystems
 */

#include "marrow.h"
#include "p9client.h"
#include "kryon.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

/*
 * Register Kryon as a service with Marrow
 *
 * Protocol: Write "register <name> <type>" to /svc/ctl
 * Example: marrow_service_register(mc, "kryon", "window-manager")
 *
 * Returns 0 on success, -1 on error
 */
int marrow_service_register(P9Client *mc, const char *name,
                           const char *type, const char *description)
{
    int svc_fd;
    char cmd[512];

    (void)description;  /* Reserved for future use */

    if (mc == NULL || name == NULL || type == NULL) {
        fprintf(stderr, "marrow_service_register: invalid arguments\n");
        return -1;
    }

    /* Build registration command: "register name type" */
    snprintf(cmd, sizeof(cmd), "register %s %s", name, type);

    /* Open /svc/ctl */
    svc_fd = p9_open(mc, "/svc/ctl", P9_OWRITE);
    if (svc_fd < 0) {
        fprintf(stderr, "marrow_service_register: failed to open /svc/ctl\n");
        return -1;
    }

    /* Send registration command */
    fprintf(stderr, "marrow_service_register: writing '%s' (%d bytes) to /svc/ctl\n",
            cmd, (int)strlen(cmd));
    if (p9_write(mc, svc_fd, cmd, strlen(cmd)) < 0) {
        fprintf(stderr, "marrow_service_register: failed to write registration\n");
        p9_close(mc, svc_fd);
        return -1;
    }

    p9_close(mc, svc_fd);

    fprintf(stderr, "Registered service '%s' (type=%s)\n", name, type);
    return 0;
}

/*
 * Mount Kryon's filesystem into Marrow's namespace
 *
 * Protocol: Write "mount <path>" to /svc/ctl
 * Example: marrow_namespace_mount(mc, "/mnt/wm")
 *
 * Returns 0 on success, -1 on error
 */
int marrow_namespace_mount(P9Client *mc, const char *path)
{
    int svc_fd;
    char cmd[512];

    if (mc == NULL || path == NULL) {
        fprintf(stderr, "marrow_namespace_mount: invalid arguments\n");
        return -1;
    }

    /* Build mount command: "mount path" */
    snprintf(cmd, sizeof(cmd), "mount %s", path);

    /* Open /svc/ctl */
    svc_fd = p9_open(mc, "/svc/ctl", P9_OWRITE);
    if (svc_fd < 0) {
        fprintf(stderr, "marrow_namespace_mount: failed to open /svc/ctl\n");
        return -1;
    }

    /* Send mount command */
    if (p9_write(mc, svc_fd, cmd, strlen(cmd)) < 0) {
        fprintf(stderr, "marrow_namespace_mount: failed to send mount\n");
        p9_close(mc, svc_fd);
        return -1;
    }

    p9_close(mc, svc_fd);

    fprintf(stderr, "Mounted '%s' into Marrow namespace\n", path);
    return 0;
}

/*
 * Unregister Kryon service from Marrow
 *
 * Protocol: Write "unregister <name>" to /svc/ctl
 *
 * Returns 0 on success, -1 on error
 */
int marrow_service_unregister(P9Client *mc, const char *name)
{
    int svc_fd;
    char cmd[512];

    if (mc == NULL || name == NULL) {
        fprintf(stderr, "marrow_service_unregister: invalid arguments\n");
        return -1;
    }

    /* Build unregister command: "unregister name" */
    snprintf(cmd, sizeof(cmd), "unregister %s", name);

    /* Open /svc/ctl */
    svc_fd = p9_open(mc, "/svc/ctl", P9_OWRITE);
    if (svc_fd < 0) {
        fprintf(stderr, "marrow_service_unregister: failed to open /svc/ctl\n");
        return -1;
    }

    /* Send unregister command */
    if (p9_write(mc, svc_fd, cmd, strlen(cmd)) < 0) {
        fprintf(stderr, "marrow_service_unregister: failed to send unregister\n");
        p9_close(mc, svc_fd);
        return -1;
    }

    p9_close(mc, svc_fd);

    fprintf(stderr, "Unregistered service '%s'\n", name);
    return 0;
}

/*
 * Unmount Kryon's filesystem from Marrow
 *
 * Note: This is done automatically when client disconnects
 * This function is reserved for future use
 *
 * Returns 0 on success, -1 on error
 */
int marrow_namespace_unmount(P9Client *mc, const char *path)
{
    (void)mc;
    (void)path;

    /* Currently unmounting is automatic on disconnect */
    fprintf(stderr, "marrow_namespace_unmount: automatic unmount on disconnect\n");
    return 0;
}
