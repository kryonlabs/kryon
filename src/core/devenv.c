/*
 * Kryon /env Device - Environment Variables
 * C89/C90 compliant
 *
 * Implements /env filesystem for environment variables
 * Required for `service=cpu` detection in CPU server mode
 */

#include "kryon.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/*
 * Maximum environment variables
 */
#define MAX_ENV_VARS 128

/*
 * Environment variable state
 */
typedef struct {
    char name[64];
    char value[256];
    int active;
} EnvVar;

/*
 * Global environment state
 */
static EnvVar g_env_vars[MAX_ENV_VARS];
static int g_nenv_vars = 0;

/*
 * Read from /env/varname
 */
static ssize_t env_read(char *buf, size_t count, uint64_t offset,
                        void *data)
{
    EnvVar *var = (EnvVar *)data;
    size_t len;
    size_t to_copy;

    if (var == NULL || !var->active) {
        return -1;
    }

    len = strlen(var->value);

    if (offset >= len) {
        return 0;  /* EOF */
    }

    to_copy = len - (size_t)offset;
    if (to_copy > count) {
        to_copy = count;
    }

    memcpy(buf, var->value + offset, to_copy);

    return (ssize_t)to_copy;
}

/*
 * Write to /env/varname
 */
static ssize_t env_write(const char *buf, size_t count, uint64_t offset,
                         void *data)
{
    EnvVar *var = (EnvVar *)data;
    size_t space_left;

    if (var == NULL || !var->active) {
        return -1;
    }

    if (offset == 0) {
        /* Replace entire value */
        if (count >= sizeof(var->value)) {
            return -1;  /* Value too large */
        }
        memcpy(var->value, buf, count);
        var->value[count] = '\0';
    } else {
        /* Append */
        space_left = sizeof(var->value) - strlen(var->value) - 1;
        if (count > space_left) {
            count = space_left;
        }
        strncat(var->value, buf, count);
    }

    return (ssize_t)count;
}

/*
 * Initialize /env device
 * Creates /env directory with initial environment variables
 */
int devenv_init(P9Node *root)
{
    P9Node *env_dir;
    P9Node *node;
    EnvVar *var;

    if (root == NULL) {
        return -1;
    }

    /* Initialize global state */
    g_nenv_vars = 0;
    memset(g_env_vars, 0, sizeof(g_env_vars));

    /* Create /env directory */
    env_dir = tree_create_dir(root, "env");
    if (env_dir == NULL) {
        fprintf(stderr, "devenv_init: cannot create env directory\n");
        return -1;
    }

    /* Create service=cpu variable (for CPU server detection) */
    var = &g_env_vars[g_nenv_vars++];
    strcpy(var->name, "service");
    strcpy(var->value, "cpu");
    var->active = 1;

    node = tree_create_file(env_dir, "service", var,
                            (P9ReadFunc)env_read,
                            (P9WriteFunc)env_write);
    if (node == NULL) {
        fprintf(stderr, "devenv_init: cannot create service\n");
        return -1;
    }

    /* Create PATH variable */
    var = &g_env_vars[g_nenv_vars++];
    strcpy(var->name, "PATH");
    strcpy(var->value, "/bin:/usr/bin");
    var->active = 1;

    node = tree_create_file(env_dir, "PATH", var,
                            (P9ReadFunc)env_read,
                            (P9WriteFunc)env_write);
    if (node == NULL) {
        fprintf(stderr, "devenv_init: cannot create PATH\n");
        return -1;
    }

    /* Create user variable */
    var = &g_env_vars[g_nenv_vars++];
    strcpy(var->name, "user");
    strcpy(var->value, "none");
    var->active = 1;

    node = tree_create_file(env_dir, "user", var,
                            (P9ReadFunc)env_read,
                            (P9WriteFunc)env_write);
    if (node == NULL) {
        fprintf(stderr, "devenv_init: cannot create user\n");
        return -1;
    }

    /* Create PLAN9 variable if set */
    {
        const char *plan9 = getenv("PLAN9");
        if (plan9 != NULL) {
            var = &g_env_vars[g_nenv_vars++];
            strcpy(var->name, "PLAN9");
            strncpy(var->value, plan9, sizeof(var->value) - 1);
            var->value[sizeof(var->value) - 1] = '\0';
            var->active = 1;

            node = tree_create_file(env_dir, "PLAN9", var,
                                    (P9ReadFunc)env_read,
                                    (P9WriteFunc)env_write);
            if (node == NULL) {
                fprintf(stderr, "devenv_init: cannot create PLAN9\n");
                return -1;
            }
        }
    }

    fprintf(stderr, "devenv_init: initialized /env\n");

    return 0;
}

/*
 * Set an environment variable
 * Returns 0 on success, -1 on error
 */
int devenv_set(const char *name, const char *value)
{
    int i;
    P9Node *env_dir;
    P9Node *env_root;
    P9Node *node;
    EnvVar *var;

    if (name == NULL || value == NULL) {
        return -1;
    }

    /* Check if variable already exists */
    for (i = 0; i < MAX_ENV_VARS; i++) {
        if (g_env_vars[i].active && strcmp(g_env_vars[i].name, name) == 0) {
            /* Update existing */
            strncpy(g_env_vars[i].value, value, sizeof(g_env_vars[i].value) - 1);
            g_env_vars[i].value[sizeof(g_env_vars[i].value) - 1] = '\0';

            /* Update environment too */
            setenv(name, value, 1);

            return 0;
        }
    }

    /* Create new variable */
    if (g_nenv_vars >= MAX_ENV_VARS) {
        return -1;
    }

    var = &g_env_vars[g_nenv_vars++];
    strncpy(var->name, name, sizeof(var->name) - 1);
    var->name[sizeof(var->name) - 1] = '\0';
    strncpy(var->value, value, sizeof(var->value) - 1);
    var->value[sizeof(var->value) - 1] = '\0';
    var->active = 1;

    /* Add to filesystem */
    env_root = tree_root();
    env_dir = tree_walk(env_root, "env");
    if (env_dir == NULL) {
        return -1;
    }

    node = tree_create_file(env_dir, name, var,
                            (P9ReadFunc)env_read,
                            (P9WriteFunc)env_write);
    if (node == NULL) {
        var->active = 0;
        g_nenv_vars--;
        return -1;
    }

    /* Update environment */
    setenv(name, value, 1);

    return 0;
}

/*
 * Get an environment variable
 * Returns value string, or NULL if not found
 */
const char *devenv_get(const char *name)
{
    int i;

    if (name == NULL) {
        return NULL;
    }

    for (i = 0; i < MAX_ENV_VARS; i++) {
        if (g_env_vars[i].active && strcmp(g_env_vars[i].name, name) == 0) {
            return g_env_vars[i].value;
        }
    }

    return NULL;
}

/*
 * Delete an environment variable
 */
int devenv_delete(const char *name)
{
    int i;

    if (name == NULL) {
        return -1;
    }

    for (i = 0; i < MAX_ENV_VARS; i++) {
        if (g_env_vars[i].active && strcmp(g_env_vars[i].name, name) == 0) {
            g_env_vars[i].active = 0;
            unsetenv(name);
            return 0;
        }
    }

    return -1;
}
