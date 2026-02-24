/*
 * Kryon Namespace Manager - Plan 9 Namespace Operations
 * C89/C90 compliant
 */

#include "kryon.h"
#include "namespace.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>

/*
 * Global namespace state
 */
static NSBind g_binds[NS_MAX_BINDS];
static int g_nbinds = 0;
static P9Node *g_mnt_terms[16];  /* Track up to 16 client /mnt/term roots */

/*
 * Initialize namespace manager
 */
int namespace_init(void)
{
    int i;

    for (i = 0; i < NS_MAX_BINDS; i++) {
        g_binds[i].active = 0;
        g_binds[i].src[0] = '\0';
        g_binds[i].dst[0] = '\0';
    }

    for (i = 0; i < 16; i++) {
        g_mnt_terms[i] = NULL;
    }

    g_nbinds = 0;

    fprintf(stderr, "namespace_init: initialized\n");

    return 0;
}

/*
 * Cleanup namespace manager
 */
void namespace_cleanup(void)
{
    int i;

    /* Unbind all active binds */
    for (i = 0; i < NS_MAX_BINDS; i++) {
        if (g_binds[i].active) {
            namespace_unbind(g_binds[i].dst);
        }
    }

    g_nbinds = 0;
}

/*
 * Find free bind slot
 */
static int find_free_bind(void)
{
    int i;

    for (i = 0; i < NS_MAX_BINDS; i++) {
        if (!g_binds[i].active) {
            return i;
        }
    }

    return -1;
}

/*
 * Normalize a path (remove . and .., resolve /)
 */
static void normalize_path(char *path)
{
    char result[NS_MAX_PATH];
    char *token;
    char *saveptr = NULL;
    int first = 1;

    if (path == NULL || path[0] == '\0') {
        return;
    }

    result[0] = '\0';

    /* Split by / */
    token = strtok_r(path, "/", &saveptr);
    while (token != NULL) {
        /* Skip . */
        if (strcmp(token, ".") == 0) {
            token = strtok_r(NULL, "/", &saveptr);
            continue;
        }

        /* Handle .. */
        if (strcmp(token, "..") == 0) {
            /* Remove last component from result */
            char *last_slash = strrchr(result, '/');
            if (last_slash != NULL) {
                *last_slash = '\0';
            } else {
                result[0] = '\0';
            }
            token = strtok_r(NULL, "/", &saveptr);
            continue;
        }

        /* Add component */
        if (!first) {
            strcat(result, "/");
        }
        strcat(result, token);
        first = 0;

        token = strtok_r(NULL, "/", &saveptr);
    }

    /* Copy back */
    if (result[0] == '\0') {
        strcpy(path, "/");
    } else {
        strcpy(path, result);
    }
}

/*
 * Create /mnt/term structure for a client
 */
P9Node *namespace_create_mnt_term(P9Node *root, int client_id)
{
    P9Node *mnt_node;
    P9Node *term_node;
    P9Node *client_node;
    P9Node *dev_node;
    P9Node *env_node;
    P9Node *fd_node;
    P9Node *proc_node;
    char dirname[64];

    if (root == NULL) {
        fprintf(stderr, "namespace_create_mnt_term: root is NULL\n");
        return NULL;
    }

    if (client_id < 0 || client_id >= 16) {
        fprintf(stderr, "namespace_create_mnt_term: invalid client_id %d\n",
                client_id);
        return NULL;
    }

    /* Create /mnt if it doesn't exist */
    mnt_node = tree_walk(root, "mnt");
    if (mnt_node == NULL) {
        mnt_node = tree_create_dir(root, "mnt");
        if (mnt_node == NULL) {
            fprintf(stderr, "namespace_create_mnt_term: failed to create /mnt\n");
            return NULL;
        }
    }

    /* Create /mnt/term if it doesn't exist */
    term_node = tree_walk(mnt_node, "term");
    if (term_node == NULL) {
        term_node = tree_create_dir(mnt_node, "term");
        if (term_node == NULL) {
            fprintf(stderr, "namespace_create_mnt_term: failed to create /mnt/term\n");
            return NULL;
        }
    }

    /* Create /mnt/term/[client_id] directory */
    snprintf(dirname, sizeof(dirname), "%d", client_id);
    client_node = tree_create_dir(term_node, dirname);
    if (client_node == NULL) {
        fprintf(stderr, "namespace_create_mnt_term: failed to create /mnt/term/%s\n",
                dirname);
        return NULL;
    }

    /* Create /mnt/term/[client_id]/dev directory */
    dev_node = tree_create_dir(client_node, "dev");
    if (dev_node == NULL) {
        fprintf(stderr, "namespace_create_mnt_term: failed to create /mnt/term/%s/dev\n",
                dirname);
        return NULL;
    }

    /* Create /mnt/term/[client_id]/env directory */
    env_node = tree_create_dir(client_node, "env");
    if (env_node == NULL) {
        fprintf(stderr, "namespace_create_mnt_term: failed to create /mnt/term/%s/env\n",
                dirname);
        return NULL;
    }

    /* Create /mnt/term/[client_id]/fd directory (for /dev/fd) */
    fd_node = tree_create_dir(client_node, "fd");
    if (fd_node == NULL) {
        fprintf(stderr, "namespace_create_mnt_term: failed to create /mnt/term/%s/fd\n",
                dirname);
        return NULL;
    }

    /* Create /mnt/term/[client_id]/proc directory */
    proc_node = tree_create_dir(client_node, "proc");
    if (proc_node == NULL) {
        fprintf(stderr, "namespace_create_mnt_term: failed to create /mnt/term/%s/proc\n",
                dirname);
        return NULL;
    }

    /* Store reference for later lookup */
    g_mnt_terms[client_id] = client_node;

    fprintf(stderr, "namespace_create_mnt_term: created /mnt/term/%s\n", dirname);

    return client_node;
}

/*
 * Mount client's exported namespace
 * This is a simplified version - in a real implementation, we'd
 * use the exportfs protocol to mount the client's filesystem
 */
int namespace_mount_export(P9Node *mnt_term, int client_fd)
{
    /* TODO: Implement exportfs protocol */
    /* For now, we just create a placeholder */

    (void)mnt_term;
    (void)client_fd;

    fprintf(stderr, "namespace_mount_export: exportfs protocol not yet implemented\n");

    return 0;
}

/*
 * Bind device to path
 */
int namespace_bind(P9Node *root, const char *device, const char *path,
                   NSBindType type)
{
    P9Node *device_node;
    P9Node *parent_node;
    P9Node *new_node;
    char path_copy[NS_MAX_PATH];
    char *slash;
    char *name;
    int bind_slot;
    char normalized_path[NS_MAX_PATH];

    if (root == NULL || device == NULL || path == NULL) {
        fprintf(stderr, "namespace_bind: invalid arguments\n");
        return -1;
    }

    /* Normalize paths */
    strncpy(normalized_path, path, sizeof(normalized_path) - 1);
    normalized_path[sizeof(normalized_path) - 1] = '\0';
    normalize_path(normalized_path);

    /* Find device node */
    device_node = tree_lookup(root, device);
    if (device_node == NULL) {
        fprintf(stderr, "namespace_bind: device '%s' not found\n", device);
        return -1;
    }

    /* Find parent of destination path */
    strncpy(path_copy, normalized_path, sizeof(path_copy) - 1);
    path_copy[sizeof(path_copy) - 1] = '\0';

    slash = strrchr(path_copy, '/');
    if (slash == NULL) {
        fprintf(stderr, "namespace_bind: invalid path '%s'\n", path);
        return -1;
    }

    *slash = '\0';
    name = slash + 1;

    parent_node = tree_lookup(root, path_copy);
    if (parent_node == NULL) {
        fprintf(stderr, "namespace_bind: parent path '%s' not found\n",
                path_copy);
        return -1;
    }

    /* Check if destination already exists */
    {
        P9Node *existing = tree_walk(parent_node, name);
        if (existing != NULL && type == NS_BIND_REPLACE) {
            /* TODO: Remove existing node */
            fprintf(stderr, "namespace_bind: replacing '%s' not implemented\n",
                    normalized_path);
        }
    }

    /* Create new node (copy of device node) */
    new_node = (P9Node *)malloc(sizeof(P9Node));
    if (new_node == NULL) {
        fprintf(stderr, "namespace_bind: malloc failed\n");
        return -1;
    }

    /* Copy node structure */
    memcpy(new_node, device_node, sizeof(P9Node));

    /* Allocate new name */
    new_node->name = (char *)malloc(strlen(name) + 1);
    if (new_node->name == NULL) {
        free(new_node);
        fprintf(stderr, "namespace_bind: malloc name failed\n");
        return -1;
    }
    strcpy(new_node->name, name);

    /* Set parent */
    new_node->parent = parent_node;

    /* Add to parent's children */
    if (tree_add_child(parent_node, new_node) < 0) {
        free(new_node->name);
        free(new_node);
        fprintf(stderr, "namespace_bind: tree_add_child failed\n");
        return -1;
    }

    /* Record bind */
    bind_slot = find_free_bind();
    if (bind_slot >= 0) {
        g_binds[bind_slot].active = 1;
        strncpy(g_binds[bind_slot].src, device, sizeof(g_binds[bind_slot].src) - 1);
        g_binds[bind_slot].src[sizeof(g_binds[bind_slot].src) - 1] = '\0';
        strncpy(g_binds[bind_slot].dst, normalized_path,
                sizeof(g_binds[bind_slot].dst) - 1);
        g_binds[bind_slot].dst[sizeof(g_binds[bind_slot].dst) - 1] = '\0';
        g_binds[bind_slot].type = type;
        g_nbinds++;
    }

    fprintf(stderr, "namespace_bind: bound '%s' to '%s'\n", device,
            normalized_path);

    return 0;
}

/*
 * Unbind a previously bound mount
 */
int namespace_unbind(const char *path)
{
    int i;
    char normalized_path[NS_MAX_PATH];

    if (path == NULL) {
        return -1;
    }

    /* Normalize path */
    strncpy(normalized_path, path, sizeof(normalized_path) - 1);
    normalized_path[sizeof(normalized_path) - 1] = '\0';
    normalize_path(normalized_path);

    /* Find bind entry */
    for (i = 0; i < NS_MAX_BINDS; i++) {
        if (g_binds[i].active &&
            strcmp(g_binds[i].dst, normalized_path) == 0) {
            g_binds[i].active = 0;
            g_nbinds--;

            fprintf(stderr, "namespace_unbind: unbound '%s'\n", normalized_path);
            return 0;
        }
    }

    fprintf(stderr, "namespace_unbind: '%s' not found\n", normalized_path);
    return -1;
}

/*
 * Lookup a node in the namespace
 */
P9Node *namespace_lookup(P9Node *root, const char *path)
{
    char normalized_path[NS_MAX_PATH];

    if (root == NULL || path == NULL) {
        return NULL;
    }

    /* Normalize path */
    strncpy(normalized_path, path, sizeof(normalized_path) - 1);
    normalized_path[sizeof(normalized_path) - 1] = '\0';
    normalize_path(normalized_path);

    /* Use tree_lookup */
    return tree_lookup(root, normalized_path);
}

/*
 * Create a symlink in the namespace
 */
int namespace_symlink(P9Node *root, const char *target, const char *linkpath)
{
    /* TODO: Implement symlinks */
    (void)root;
    (void)target;
    (void)linkpath;

    fprintf(stderr, "namespace_symlink: not implemented\n");
    return -1;
}

/*
 * Get /mnt/term for a client
 */
P9Node *namespace_get_mnt_term(int client_id)
{
    if (client_id < 0 || client_id >= 16) {
        return NULL;
    }

    return g_mnt_terms[client_id];
}

/*
 * Delete /mnt/term for a client
 */
int namespace_delete_mnt_term(int client_id)
{
    P9Node *mnt_term;
    P9Node *mnt;
    P9Node *term;
    P9Node *root;
    char dirname[64];
    int i;

    if (client_id < 0 || client_id >= 16) {
        return -1;
    }

    mnt_term = g_mnt_terms[client_id];
    if (mnt_term == NULL) {
        return 0;  /* Already deleted */
    }

    /* Navigate to parent to remove child */
    term = mnt_term->parent;
    if (term == NULL) {
        return -1;
    }

    mnt = term->parent;
    if (mnt == NULL) {
        return -1;
    }

    root = mnt->parent;
    if (root == NULL) {
        return -1;
    }

    /* Remove from parent's children */
    snprintf(dirname, sizeof(dirname), "%d", client_id);

    /* TODO: Implement proper node removal */
    /* For now, just clear reference */
    g_mnt_terms[client_id] = NULL;

    fprintf(stderr, "namespace_delete_mnt_term: deleted /mnt/term/%s\n",
            dirname);

    return 0;
}
