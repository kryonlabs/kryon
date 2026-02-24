/*
 * Kryon Control Interface Implementation
 * C89/C90 compliant
 */

#include "ctl.h"
#include "window.h"
#include "widget.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/*
 * External references to file system nodes
 * These will be set by main.c during initialization
 */
extern P9Node *g_windows_dir;

/*
 * Parse a control command
 * Commands:
 *   create_window
 *   create_widget <window_id> <type> <parent_id>
 *   destroy_window <window_id>
 *   destroy_widget <window_id> <widget_id>
 */
int ctl_handle_command(const char *command, char *response, size_t response_size)
{
    char cmd[64];
    const char *args;
    int parsed;

    if (command == NULL) {
        return -1;
    }

    /* Skip leading whitespace */
    while (isspace((unsigned char)*command)) {
        command++;
    }

    /* Extract command name */
    args = command;
    parsed = sscanf(args, "%63s", cmd);
    if (parsed != 1) {
        if (response_size > 0) {
            strcpy(response, "Error: no command\n");
        }
        return -1;
    }

    /* Move past command name */
    args += strlen(cmd);
    while (isspace((unsigned char)*args)) {
        args++;
    }

    /* Handle create_window */
    if (strcmp(cmd, "create_window") == 0) {
        KryonWindow *win;
        int result;

        /* Create window with default size */
        win = window_create("Untitled", 800, 600);
        if (win == NULL) {
            if (response_size > 0) {
                strcpy(response, "Error: failed to create window\n");
            }
            return -1;
        }

        /* Create filesystem entries */
        result = window_create_fs_entries(win, g_windows_dir);
        if (result < 0) {
            if (response_size > 0) {
                strcpy(response, "Error: failed to create FS entries\n");
            }
            return -1;
        }

        if (response_size > 0) {
            sprintf(response, "Created window %u\n", (unsigned int)win->id);
        }
        return 0;
    }

    /* Handle create_widget - simplified version */
    if (strcmp(cmd, "create_widget") == 0) {
        if (response_size > 0) {
            strcpy(response, "Error: create_widget not yet implemented\n");
        }
        return -1;
    }

    /* Handle destroy_window */
    if (strcmp(cmd, "destroy_window") == 0) {
        if (response_size > 0) {
            strcpy(response, "Error: destroy_window not yet implemented\n");
        }
        return -1;
    }

    /* Handle destroy_widget */
    if (strcmp(cmd, "destroy_widget") == 0) {
        if (response_size > 0) {
            strcpy(response, "Error: destroy_widget not yet implemented\n");
        }
        return -1;
    }

    /* Unknown command */
    if (response_size > 0) {
        sprintf(response, "Error: unknown command '%s'\n", cmd);
    }
    return -1;
}

/*
 * Write callback for /mnt/wm/ctl file
 */
ssize_t ctl_write(const char *buf, size_t count, uint64_t offset)
{
    char *command;
    char response[256];

    (void)offset;

    if (buf == NULL || count == 0) {
        return 0;
    }

    /* Copy command to null-terminated buffer */
    command = (char *)malloc(count + 1);
    if (command == NULL) {
        return -1;
    }

    memcpy(command, buf, count);
    command[count] = '\0';

    /* Strip trailing newlines */
    {
        size_t n = count;
        while (n > 0 && (command[n - 1] == '\n' || command[n - 1] == '\r')) {
            command[--n] = '\0';
        }
    }

    fprintf(stderr, "ctl command: %s\n", command);

    /* Handle command */
    ctl_handle_command(command, response, sizeof(response));

    free(command);

    return count;
}
