/*
 * Kryon /dev/fd Device - Public Interface
 */

#ifndef DEVFD_H
#define DEVFD_H

#include "kryon.h"

/*
 * Initialize /dev/fd device
 * Creates /dev/fd directory with fd entries
 */
int devfd_init(P9Node *dev_dir);

/*
 * Create a new fd entry
 * Returns 0 on success, -1 on error
 */
int devfd_create_fd(P9Node *fd_dir, int fd_num, int target_fd);

/*
 * Close an fd entry
 */
void devfd_close_fd(int fd_num);

#endif /* DEVFD_H */
