/*
 * Kryon /env Device - Public Interface
 */

#ifndef DEVENV_H
#define DEVENV_H

#include "kryon.h"

/*
 * Initialize /env device
 * Creates /env directory with initial environment variables
 */
int devenv_init(P9Node *root);

/*
 * Set an environment variable
 * Returns 0 on success, -1 on error
 */
int devenv_set(const char *name, const char *value);

/*
 * Get an environment variable
 * Returns value string, or NULL if not found
 */
const char *devenv_get(const char *name);

/*
 * Delete an environment variable
 */
int devenv_delete(const char *name);

#endif /* DEVENV_H */
