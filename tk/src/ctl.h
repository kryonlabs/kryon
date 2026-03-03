/*
 * Kryon Control Interface
 * C89/C90 compliant
 */

#ifndef CTL_H
#define CTL_H

#include "kryon.h"

/*
 * Control command handler
 * Handles commands written to /mnt/wm/ctl
 */

/*
 * Handle a control command
 * Returns: 0 on success, -1 on error
 */
int ctl_handle_command(const char *command, char *response, size_t response_size);

/*
 * Write callback for /mnt/wm/ctl file
 * Returns: number of bytes written, or -1 on error
 */
ssize_t ctl_write(const char *buf, size_t count, uint64_t offset);

#endif /* CTL_H */
