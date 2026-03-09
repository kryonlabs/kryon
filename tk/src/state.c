/*
 * Kryon State Filesystem
 * C89/C90 compliant
 *
 * Implements state variable storage as 9P filesystem entries
 * State variables declared in .kry files become files under /mnt/wm/windows/{id}/state/
 */

#include "parser.h"
#include "window.h"
#include "widget.h"
#include "wm.h"
#include "kryon.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/*
 * External snprintf for C89
 */
extern int snprintf(char *str, size_t size, const char *format, ...);

/*
 * State variable data structure
 */
typedef struct {
    char *key;                /* Variable name */
    char *value;              /* Current value */
    struct KryonWindow *win;  /* Parent window */
} StateVar;

/*
 * Forward declarations
 */
static int state_var_read(char *buf, size_t count, uint64_t offset, void *data);
static int state_var_write(const char *buf, size_t count, uint64_t offset, void *data);

/*
 * State file read handler
 * Returns the current state value
 */
static int state_var_read(char *buf, size_t count, uint64_t offset, void *data)
{
    StateVar *var = (StateVar *)data;
    size_t len;
    size_t to_copy;

    if (var == NULL || var->value == NULL) {
        return 0;
    }

    len = strlen(var->value);

    if (offset >= len) {
        return 0;
    }

    to_copy = len - offset;
    if (to_copy > count) {
        to_copy = count;
    }

    memcpy(buf, var->value + offset, to_copy);
    return to_copy;
}

/*
 * State file write handler
 * Updates the state value
 */
static int state_var_write(const char *buf, size_t count, uint64_t offset, void *data)
{
    StateVar *var = (StateVar *)data;
    char *new_value;

    (void)offset;  /* TODO: handle offset properly */

    if (var == NULL) {
        return -1;
    }

    /* Allocate new value */
    new_value = (char *)malloc(count + 1);
    if (new_value == NULL) {
        return -1;
    }

    /* Copy and null-terminate */
    memcpy(new_value, buf, count);
    new_value[count] = '\0';

    /* Strip trailing newline if present */
    {
        size_t len = strlen(new_value);
        if (len > 0 && new_value[len - 1] == '\n') {
            new_value[len - 1] = '\0';
        }
    }

    /* Free old value */
    if (var->value != NULL) {
        free(var->value);
    }

    /* Store new value */
    var->value = new_value;

    fprintf(stderr, "state_var_write: updated %s to '%s'\n", var->key, var->value);

    /* TODO: Trigger change notifications for future reactive bindings */

    return count;
}

/*
 * Create state directory for a window
 * Creates /mnt/wm/windows/{id}/state/ with files for each state variable
 */
int window_create_state_directory(struct KryonWindow *win, KryonNode *state_node)
{
    P9Node *window_node;
    P9Node *state_dir;
    int i;

    if (win == NULL || win->node == NULL) {
        fprintf(stderr, "window_create_state_directory: invalid window or node\n");
        return -1;
    }

    if (state_node == NULL || state_node->nstate_vars == 0) {
        fprintf(stderr, "window_create_state_directory: no state variables\n");
        return 0;  /* Not an error, just no state */
    }

    window_node = win->node;

    /* Create /mnt/wm/windows/{id}/state/ directory */
    state_dir = tree_create_dir(window_node, "state");
    if (state_dir == NULL) {
        fprintf(stderr, "window_create_state_directory: failed to create state directory\n");
        return -1;
    }

    fprintf(stderr, "window_create_state_directory: creating %d state variables\n",
            state_node->nstate_vars);

    /* Create file for each state variable */
    for (i = 0; i < state_node->nstate_vars; i++) {
        const char *key = state_node->state_keys[i];
        const char *value = state_node->state_values[i];
        StateVar *var;
        P9Node *file;

        if (key == NULL) continue;

        /* Create state variable structure */
        var = (StateVar *)malloc(sizeof(StateVar));
        if (var == NULL) {
            fprintf(stderr, "window_create_state_directory: failed to allocate state var\n");
            continue;
        }

        var->key = strdup(key);
        var->value = strdup(value ? value : "");
        var->win = win;

        /* Create file with read/write handlers */
        file = tree_create_file(state_dir, key, var,
                               (P9ReadFunc)state_var_read,
                               (P9WriteFunc)state_var_write);

        if (file == NULL) {
            fprintf(stderr, "window_create_state_directory: failed to create state file: %s\n", key);
            if (var->key != NULL) free(var->key);
            if (var->value != NULL) free(var->value);
            free(var);
            continue;
        }

        fprintf(stderr, "window_create_state_directory: created state file: %s = %s\n",
                key, value);
    }

    return 0;
}
