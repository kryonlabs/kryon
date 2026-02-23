/*
 * Synthetic File Tree Implementation
 */

#include "kryon.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

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
 * Cleanup the tree
 */
void tree_cleanup(void)
{
    /* For Phase 1, we'll keep it simple and not recursively free */
    /* In production, we'd walk the tree and free all nodes */
}

/*
 * Get root node
 */
P9Node *tree_root(void)
{
    return root_node;
}

/*
 * Walk to a child node by name
 */
P9Node *tree_walk(P9Node *node, const char *name)
{
    int i;

    if (node == NULL || name == NULL) {
        return NULL;
    }

    /* "." means current directory */
    if (strcmp(name, ".") == 0) {
        return node;
    }

    /* ".." means parent directory */
    if (strcmp(name, "..") == 0) {
        return node->parent;
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
    ops->read = (void *)read;  /* Cast to match new signature */
    ops->write = (void *)write;
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
