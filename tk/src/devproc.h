/*
 * Kryon /proc Device - Public Interface
 */

#ifndef DEVPROC_H
#define DEVPROC_H

#include "kryon.h"
#include <sys/types.h>

/*
 * Initialize /proc device
 * Creates /proc directory structure
 */
int devproc_init(P9Node *root);

/*
 * Add a process to /proc
 * Returns 0 on success, -1 on error
 */
int devproc_add_pid(pid_t pid, const char *cmd);

/*
 * Remove a process from /proc
 */
void devproc_remove_pid(pid_t pid);

#endif /* DEVPROC_H */
