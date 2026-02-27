/*
 * Synthetic File Tree Implementation
 */

#include "kryon.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

/*
 * Stub for dynamic node creation
 * /dev/draw/[n] directories are now provided by Marrow, not created dynamically
 * This stub returns NULL to disable dynamic creation
 */
static P9Node *drawconn_create_dir(int conn_id)
{
    (void)conn_id;
    return NULL;
}

/*
 * Forward declarations
 */
typedef struct FileOps FileOps;

/*
 * File operation context - stores read/write functions and state data
 */
struct FileOps {
    void *data;  /* State data passed to read/write functions */
    ssize_t (*read)(char *buf, size_t count, uint64_t offset, void *data);
    ssize_t (*write)(const char *buf, size_t count, uint64_t offset, void *data);
};

/*
 * Global tree state
 */
static P9Node *root_node = NULL;
static uint64_t next_qid_path = 1;

/*
 * Allocate a new tree node
 */
static P9Node *node_alloc(const char *name, uint32_t mode)
{
    P9Node *node;
    size_t name_len;

    node = (P9Node *)malloc(sizeof(P9Node));
    if (node == NULL) {
        return NULL;
    }

    name_len = strlen(name);
    node->name = (char *)malloc(name_len + 1);
    if (node->name == NULL) {
        free(node);
        return NULL;
    }
    strcpy(node->name, name);

    node->qid.type = (mode & P9_DMDIR) ? QTDIR : QTFILE;
    node->qid.version = 0;
    node->qid.path = next_qid_path++;

    node->mode = mode;
    node->atime = (uint32_t)time(NULL);
    node->mtime = node->atime;
    node->length = 0;
    node->data = NULL;
    node->parent = NULL;
    node->children = NULL;
    node->nchildren = 0;
    node->capacity = 0;

    /* Initialize bind mount fields */
    node->is_bind = 0;
    node->bind_target = NULL;
    node->bind_node = NULL;

    return node;
}

/*
 * Initialize the synthetic file tree
 */
int tree_init(void)
{
    if (root_node != NULL) {
        return -1;  /* Already initialized */
    }

    root_node = node_alloc("/", P9_DMDIR | 0555);
    if (root_node == NULL) {
        return -1;
    }

    root_node->parent = root_node;  /* Root is its own parent */

    return 0;
}

/*
 * Free a node and all its children recursively
 */
static void tree_free_recursive(P9Node *node)
{
    int i;

    if (node == NULL) {
        return;
    }

    /* Free all children first */
    for (i = 0; i < node->nchildren; i++) {
        tree_free_recursive(node->children[i]);
    }

    /* Free this node's resources */
    if (node->name != NULL) {
        free(node->name);
    }
    if (node->data != NULL) {
        free(node->data);
    }
    if (node->bind_target != NULL) {
        free(node->bind_target);
    }
    if (node->children != NULL) {
        free(node->children);
    }

    free(node);
}

/*
 * Free a tree (public API)
 */
void tree_free(P9Node *root)
{
    if (root == NULL) {
        return;
    }

    /* Don't free root's parent reference */
    root->parent = NULL;

    tree_free_recursive(root);
}

/*
 * Cleanup the tree
 */
void tree_cleanup(void)
{
    if (root_node != NULL) {
        /* Only free the children, not root itself (it's statically allocated) */
        int i;
        for (i = 0; i < root_node->nchildren; i++) {
            tree_free_recursive(root_node->children[i]);
        }
        root_node->nchildren = 0;
    }
}

/*
 * Get root node
 */
P9Node *tree_root(void)
{
    return root_node;
}

/*
 * Check if string is a numeric connection ID (for /dev/draw/[n])
 */
static int is_numeric_id(const char *name)
{
    const char *p;

    if (name == NULL || *name == '\0') {
        return 0;
    }

    p = name;
    while (*p != '\0') {
        if (*p < '0' || *p > '9') {
            return 0;
        }
        p++;
    }

    return 1;
}

/*
 * Walk to a child node by name
 * NOTE: name may not be null-terminated (from Twath parsing).
 * We handle special cases "." and ".." which are single chars.
 */
P9Node *tree_walk(P9Node *node, const char *name)
{
    int i;

    if (node == NULL || name == NULL) {
        return NULL;
    }

    /* "." means current directory (single char, safe to check) */
    if (name[0] == '.' && (name[1] == '\0' || name[1] == '/')) {
        return node;
    }

    /* ".." means parent directory (check first two chars) */
    if (name[0] == '.' && name[1] == '.' && (name[2] == '\0' || name[2] == '/')) {
        return node->parent;
    }

    /* Check for dynamic /dev/draw/[n] directories */
    if (node->name != NULL && strcmp(node->name, "draw") == 0) {
        if (is_numeric_id(name)) {
            int conn_id = atoi(name);
            P9Node *dynamic_node = drawconn_create_dir(conn_id);
            if (dynamic_node != NULL) {
                /* Add to children if not already present */
                int found = 0;
                for (i = 0; i < node->nchildren; i++) {
                    if (node->children[i] != NULL &&
                        strcmp(node->children[i]->name, name) == 0) {
                        found = 1;
                        break;
                    }
                }
                if (!found) {
                    tree_add_child(node, dynamic_node);
                }
                return dynamic_node;
            }
        }
    }

    /* Search children */
    if (node->children == NULL) {
        return NULL;
    }

    for (i = 0; i < node->nchildren; i++) {
        if (node->children[i] != NULL &&
            strcmp(node->children[i]->name, name) == 0) {
            return node->children[i];
        }
    }

    return NULL;
}

/*
 * Lookup a path from a starting node
 */
P9Node *tree_lookup(P9Node *root, const char *path)
{
    P9Node *node;
    char name[P9_MAX_STR];
    const char *p;
    int i;

    if (root == NULL || path == NULL) {
        return NULL;
    }

    node = root;

    /* Skip leading slash */
    if (*path == '/') {
        path++;
    }

    /* Empty path means current node */
    if (*path == '\0') {
        return node;
    }

    p = path;
    while (*p != '\0') {
        /* Extract next component */
        i = 0;
        while (*p != '/' && *p != '\0' && i < P9_MAX_STR - 1) {
            name[i++] = *p++;
        }
        name[i] = '\0';

        /* Walk to that component */
        node = tree_walk(node, name);
        if (node == NULL) {
            return NULL;
        }

        /* Skip separator */
        if (*p == '/') {
            p++;
        }
    }

    return node;
}

/*
 * Add a child to a parent node
 */
int tree_add_child(P9Node *parent, P9Node *child)
{
    int new_capacity;
    P9Node **new_children;

    if (parent == NULL || child == NULL) {
        return -1;
    }

    /* Check if we need to expand the children array */
    if (parent->nchildren >= parent->capacity) {
        new_capacity = parent->capacity == 0 ? 8 : parent->capacity * 2;
        new_children = (P9Node **)realloc(parent->children,
                                           new_capacity * sizeof(P9Node *));
        if (new_children == NULL) {
            return -1;
        }
        parent->children = new_children;
        parent->capacity = new_capacity;
    }

    parent->children[parent->nchildren++] = child;
    child->parent = parent;

    return 0;
}

/*
 * Create a directory
 */
P9Node *tree_create_dir(P9Node *parent, const char *name)
{
    P9Node *node;

    if (parent == NULL) {
        parent = root_node;
    }

    node = node_alloc(name, P9_DMDIR | 0555);
    if (node == NULL) {
        return NULL;
    }

    if (tree_add_child(parent, node) < 0) {
        free(node->name);
        free(node);
        return NULL;
    }

    return node;
}

/*
 * Create a file with custom operations
 */
P9Node *tree_create_file(P9Node *parent, const char *name, void *data,
                         P9ReadFunc read, P9WriteFunc write)
{
    P9Node *node;
    FileOps *ops;

    if (parent == NULL) {
        parent = root_node;
    }

    node = node_alloc(name, 0644);
    if (node == NULL) {
        return NULL;
    }

    /* Allocate file operations structure */
    ops = (FileOps *)malloc(sizeof(FileOps));
    if (ops == NULL) {
        free(node->name);
        free(node);
        return NULL;
    }

    ops->data = data;
    ops->read = read;
    ops->write = write;
    node->data = ops;

    if (tree_add_child(parent, node) < 0) {
        free(ops);
        free(node->name);
        free(node);
        return NULL;
    }

    return node;
}

/*
 * Get file operations for a node
 */
struct FileOps *node_get_ops(P9Node *node)
{
    if (node == NULL) {
        return NULL;
    }
    return (struct FileOps *)node->data;
}

/*
 * Read from a file node
 */
ssize_t node_read(P9Node *node, char *buf, size_t count, uint64_t offset)
{
    struct FileOps *ops;
    ssize_t (*read_func)(char *, size_t, uint64_t, void *);

    if (node == NULL || buf == NULL) {
        return -1;
    }

    ops = node_get_ops(node);
    if (ops == NULL || ops->read == NULL) {
        return -1;
    }

    /* Call read function with state data */
    read_func = (ssize_t (*)(char *, size_t, uint64_t, void *))ops->read;
    return read_func(buf, count, offset, ops->data);
}

/*
 * Write to a file node
 */
ssize_t node_write(P9Node *node, const char *buf, size_t count, uint64_t offset)
{
    struct FileOps *ops;
    ssize_t (*write_func)(const char *, size_t, uint64_t, void *);

    if (node == NULL || buf == NULL) {
        return -1;
    }

    ops = node_get_ops(node);
    if (ops == NULL || ops->write == NULL) {
        return -1;
    }

    /* Call write function with state data */
    write_func = (ssize_t (*)(const char *, size_t, uint64_t, void *))ops->write;
    return write_func(buf, count, offset, ops->data);
}

/*
 * Remove a node from its parent's children array
 * Returns 0 on success, -1 on failure
 */
int tree_remove_node(P9Node *node)
{
    P9Node *parent;
    int i, found;

    if (node == NULL) {
        return -1;
    }

    /* Cannot remove root node */
    if (node->parent == node) {
        return -1;
    }

    parent = node->parent;
    if (parent == NULL || parent->children == NULL) {
        return -1;
    }

    /* Find the node in parent's children array */
    found = -1;
    for (i = 0; i < parent->nchildren; i++) {
        if (parent->children[i] == node) {
            found = i;
            break;
        }
    }

    if (found < 0) {
        return -1;  /* Node not found in parent's children */
    }

    /* Remove node from children array by shifting */
    for (i = found; i < parent->nchildren - 1; i++) {
        parent->children[i] = parent->children[i + 1];
    }
    parent->nchildren--;

    /* Free node resources */
    if (node->name != NULL) {
        free(node->name);
    }
    if (node->data != NULL) {
        free(node->data);
    }
    /* Free bind target if present */
    if (node->bind_target != NULL) {
        free(node->bind_target);
    }
    /* Note: We don't recursively free children - in production this would be needed */

    free(node);
    return 0;
}

/*
 * Find a child node by name
 * Returns the child node or NULL if not found
 */
static P9Node *tree_find_child(P9Node *parent, const char *name)
{
    int i;

    if (parent == NULL || name == NULL) {
        return NULL;
    }

    if (parent->children == NULL) {
        return NULL;
    }

    for (i = 0; i < parent->nchildren; i++) {
        if (parent->children[i] != NULL &&
            strcmp(parent->children[i]->name, name) == 0) {
            return parent->children[i];
        }
    }

    return NULL;
}

/*
 * Create a bind mount node
 *
 * 'name' is the local name (e.g., "screen")
 * 'target' is the target path (e.g., "/dev/win1/screen")
 *
 * Returns the bind mount node or NULL on error
 */
P9Node *tree_create_bind(P9Node *parent, const char *name, const char *target)
{
    P9Node *node;
    P9Node *bind_node;

    if (parent == NULL || name == NULL || target == NULL) {
        return NULL;
    }

    /* Resolve target path in global tree */
    bind_node = tree_resolve_path(root_node, target);
    if (bind_node == NULL) {
        fprintf(stderr, "tree_create_bind: target not found: %s\n", target);
        return NULL;
    }

    /* Create directory node */
    node = tree_create_dir(parent, name);
    if (node == NULL) {
        return NULL;
    }

    /* Mark as bind mount */
    node->is_bind = 1;
    node->bind_target = (char *)malloc(strlen(target) + 1);
    if (node->bind_target == NULL) {
        tree_remove_node(node);
        return NULL;
    }
    strcpy(node->bind_target, target);
    node->bind_node = bind_node;

    return node;
}

/*
 * Resolve a path like "/dev/win1/screen"
 * Follows bind mounts automatically
 *
 * Returns the resolved node or NULL if not found
 */
P9Node *tree_resolve_path(P9Node *root, const char *path)
{
    char path_copy[512];
    char *segment;
    P9Node *current;
    P9Node *visited[32];
    int visited_count = 0;
    int i;

    if (root == NULL || path == NULL) {
        return NULL;
    }

    /* Copy path (strtok modifies input) */
    strncpy(path_copy, path, sizeof(path_copy) - 1);
    path_copy[sizeof(path_copy) - 1] = '\0';

    current = root;

    /* Skip leading '/' */
    if (path_copy[0] == '/') {
        segment = strtok(path_copy + 1, "/");
    } else {
        segment = strtok(path_copy, "/");
    }

    while (segment != NULL) {
        /* Check for cycles */
        for (i = 0; i < visited_count; i++) {
            if (visited[i] == current) {
                fprintf(stderr, "tree_resolve_path: cycle detected at node '%s'\n",
                        current->name ? current->name : "(null)");
                return NULL;
            }
        }

        /* Mark as visited */
        if (visited_count < 32) {
            visited[visited_count++] = current;
        }

        /* If current node is a bind mount, jump to target */
        if (current->is_bind && current->bind_node != NULL) {
            current = current->bind_node;
        }

        /* Find child */
        current = tree_find_child(current, segment);
        if (current == NULL) {
            return NULL;  /* Path not found */
        }

        segment = strtok(NULL, "/");
    }

    /* Final bind mount resolution */
    if (current->is_bind && current->bind_node != NULL) {
        /* Check one more time for cycles */
        for (i = 0; i < visited_count; i++) {
            if (visited[i] == current->bind_node) {
                fprintf(stderr, "tree_resolve_path: cycle detected at final bind\n");
                return NULL;
            }
        }
        current = current->bind_node;
    }

    return current;
}
