/**
 * @file virtual_filesystem.c
 * @brief Kryon Virtual Filesystem Implementation
 */

#include "internal/filesystem.h"
#include "internal/memory.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// =============================================================================
// VFS NODE TYPES
// =============================================================================

typedef enum {
    KRYON_VFS_NODE_FILE,
    KRYON_VFS_NODE_DIRECTORY,
    KRYON_VFS_NODE_MOUNT_POINT,
    KRYON_VFS_NODE_SYMLINK
} KryonVfsNodeType;

typedef struct KryonVfsNode {
    char* name;
    char* full_path;
    KryonVfsNodeType type;
    
    // File data (for memory files)
    void* data;
    size_t size;
    size_t capacity;
    
    // Directory children
    struct KryonVfsNode** children;
    size_t child_count;
    size_t child_capacity;
    
    // Parent and siblings
    struct KryonVfsNode* parent;
    
    // Mount point data
    char* mount_target;  // For mount points and symlinks
    
    // Metadata
    time_t created_time;
    time_t modified_time;
    time_t accessed_time;
    uint32_t permissions;
    
    // Reference counting
    int ref_count;
    
} KryonVfsNode;

typedef struct {
    KryonVfsNode* root;
    
    // Mount points
    struct {
        char* virtual_path;
        char* real_path;
        bool read_only;
    } mount_points[KRYON_VFS_MAX_MOUNTS];
    size_t mount_count;
    
    // Open file handles
    struct {
        KryonVfsNode* node;
        size_t position;
        KryonFileMode mode;
        bool is_open;
    } handles[KRYON_VFS_MAX_HANDLES];
    
    // Configuration
    bool case_sensitive;
    size_t max_file_size;
    
} KryonVirtualFilesystem;

static KryonVirtualFilesystem g_vfs = {0};

// =============================================================================
// HELPER FUNCTIONS
// =============================================================================

static KryonVfsNode* create_node(const char* name, KryonVfsNodeType type) {
    KryonVfsNode* node = kryon_alloc(sizeof(KryonVfsNode));
    if (!node) return NULL;
    
    memset(node, 0, sizeof(KryonVfsNode));
    
    if (name) {
        node->name = kryon_alloc(strlen(name) + 1);
        if (!node->name) {
            kryon_free(node);
            return NULL;
        }
        strcpy(node->name, name);
    }
    
    node->type = type;
    node->created_time = time(NULL);
    node->modified_time = node->created_time;
    node->accessed_time = node->created_time;
    node->permissions = 0755; // Default permissions
    node->ref_count = 1;
    
    return node;
}

static void destroy_node(KryonVfsNode* node) {
    if (!node) return;
    
    node->ref_count--;
    if (node->ref_count > 0) return;
    
    kryon_free(node->name);
    kryon_free(node->full_path);
    kryon_free(node->data);
    kryon_free(node->mount_target);
    
    // Free children
    for (size_t i = 0; i < node->child_count; i++) {
        destroy_node(node->children[i]);
    }
    kryon_free(node->children);
    
    kryon_free(node);
}

static void add_child(KryonVfsNode* parent, KryonVfsNode* child) {
    if (!parent || !child || parent->type != KRYON_VFS_NODE_DIRECTORY) return;
    
    // Expand children array if needed
    if (parent->child_count >= parent->child_capacity) {
        size_t new_capacity = parent->child_capacity == 0 ? 8 : parent->child_capacity * 2;
        KryonVfsNode** new_children = kryon_alloc(sizeof(KryonVfsNode*) * new_capacity);
        if (!new_children) return;
        
        if (parent->children) {
            memcpy(new_children, parent->children, sizeof(KryonVfsNode*) * parent->child_count);
            kryon_free(parent->children);
        }
        
        parent->children = new_children;
        parent->child_capacity = new_capacity;
    }
    
    parent->children[parent->child_count++] = child;
    child->parent = parent;
    child->ref_count++;
}

static KryonVfsNode* find_child(KryonVfsNode* parent, const char* name) {
    if (!parent || !name || parent->type != KRYON_VFS_NODE_DIRECTORY) return NULL;
    
    for (size_t i = 0; i < parent->child_count; i++) {
        const char* child_name = parent->children[i]->name;
        if (g_vfs.case_sensitive ? strcmp(child_name, name) == 0 : strcasecmp(child_name, name) == 0) {
            return parent->children[i];
        }
    }
    
    return NULL;
}

static KryonVfsNode* resolve_path(const char* path) {
    if (!path || path[0] != '/') return NULL;
    
    KryonVfsNode* current = g_vfs.root;
    if (strcmp(path, "/") == 0) return current;
    
    // Skip leading slash
    path++;
    
    char* path_copy = kryon_alloc(strlen(path) + 1);
    if (!path_copy) return NULL;
    strcpy(path_copy, path);
    
    char* token = strtok(path_copy, "/");
    while (token && current) {
        current = find_child(current, token);
        token = strtok(NULL, "/");
    }
    
    kryon_free(path_copy);
    return current;
}

static char* normalize_path(const char* path) {
    if (!path) return NULL;
    
    size_t len = strlen(path);
    char* normalized = kryon_alloc(len + 2); // +1 for null, +1 for potential leading slash
    if (!normalized) return NULL;
    
    if (path[0] != '/') {
        normalized[0] = '/';
        strcpy(normalized + 1, path);
    } else {
        strcpy(normalized, path);
    }
    
    // Remove trailing slashes (except root)
    len = strlen(normalized);
    while (len > 1 && normalized[len - 1] == '/') {
        normalized[--len] = '\0';
    }
    
    return normalized;
}

static int find_free_handle(void) {
    for (int i = 0; i < KRYON_VFS_MAX_HANDLES; i++) {
        if (!g_vfs.handles[i].is_open) {
            return i;
        }
    }
    return -1;
}

// =============================================================================
// PUBLIC API
// =============================================================================

bool kryon_vfs_init(void) {
    memset(&g_vfs, 0, sizeof(g_vfs));
    
    // Create root directory
    g_vfs.root = create_node("", KRYON_VFS_NODE_DIRECTORY);
    if (!g_vfs.root) return false;
    
    g_vfs.case_sensitive = true;
    g_vfs.max_file_size = 64 * 1024 * 1024; // 64MB default
    
    return true;
}

void kryon_vfs_shutdown(void) {
    // Close all handles
    for (int i = 0; i < KRYON_VFS_MAX_HANDLES; i++) {
        if (g_vfs.handles[i].is_open) {
            kryon_vfs_close((KryonVfsHandle)(intptr_t)(i + 1));
        }
    }
    
    // Destroy filesystem tree
    destroy_node(g_vfs.root);
    
    // Free mount point data
    for (size_t i = 0; i < g_vfs.mount_count; i++) {
        kryon_free(g_vfs.mount_points[i].virtual_path);
        kryon_free(g_vfs.mount_points[i].real_path);
    }
    
    memset(&g_vfs, 0, sizeof(g_vfs));
}

bool kryon_vfs_mount(const char* virtual_path, const char* real_path, bool read_only) {
    if (!virtual_path || !real_path || g_vfs.mount_count >= KRYON_VFS_MAX_MOUNTS) {
        return false;
    }
    
    char* norm_virtual = normalize_path(virtual_path);
    if (!norm_virtual) return false;
    
    // Create mount point directory if it doesn't exist
    KryonVfsNode* mount_node = resolve_path(norm_virtual);
    if (!mount_node) {
        // Create the mount point
        if (!kryon_vfs_create_directory(norm_virtual)) {
            kryon_free(norm_virtual);
            return false;
        }
        mount_node = resolve_path(norm_virtual);
    }
    
    if (!mount_node) {
        kryon_free(norm_virtual);
        return false;
    }
    
    // Convert to mount point
    mount_node->type = KRYON_VFS_NODE_MOUNT_POINT;
    mount_node->mount_target = kryon_alloc(strlen(real_path) + 1);
    if (!mount_node->mount_target) {
        kryon_free(norm_virtual);
        return false;
    }
    strcpy(mount_node->mount_target, real_path);
    
    // Add to mount table
    g_vfs.mount_points[g_vfs.mount_count].virtual_path = norm_virtual;
    g_vfs.mount_points[g_vfs.mount_count].real_path = kryon_alloc(strlen(real_path) + 1);
    if (!g_vfs.mount_points[g_vfs.mount_count].real_path) {
        kryon_free(norm_virtual);
        return false;
    }
    strcpy(g_vfs.mount_points[g_vfs.mount_count].real_path, real_path);
    g_vfs.mount_points[g_vfs.mount_count].read_only = read_only;
    g_vfs.mount_count++;
    
    return true;
}

bool kryon_vfs_unmount(const char* virtual_path) {
    if (!virtual_path) return false;
    
    char* norm_path = normalize_path(virtual_path);
    if (!norm_path) return false;
    
    // Find and remove from mount table
    for (size_t i = 0; i < g_vfs.mount_count; i++) {
        if (strcmp(g_vfs.mount_points[i].virtual_path, norm_path) == 0) {
            kryon_free(g_vfs.mount_points[i].virtual_path);
            kryon_free(g_vfs.mount_points[i].real_path);
            
            // Shift remaining mounts
            for (size_t j = i + 1; j < g_vfs.mount_count; j++) {
                g_vfs.mount_points[j - 1] = g_vfs.mount_points[j];
            }
            g_vfs.mount_count--;
            
            kryon_free(norm_path);
            return true;
        }
    }
    
    kryon_free(norm_path);
    return false;
}

bool kryon_vfs_create_directory(const char* path) {
    if (!path) return false;
    
    char* norm_path = normalize_path(path);
    if (!norm_path) return false;
    
    // Check if already exists
    KryonVfsNode* existing = resolve_path(norm_path);
    if (existing) {
        kryon_free(norm_path);
        return existing->type == KRYON_VFS_NODE_DIRECTORY;
    }
    
    // Find parent directory
    char* parent_path = kryon_path_dirname(norm_path);
    char* dir_name = kryon_path_basename(norm_path);
    
    if (!parent_path || !dir_name) {
        kryon_free(norm_path);
        kryon_free(parent_path);
        kryon_free(dir_name);
        return false;
    }
    
    KryonVfsNode* parent = resolve_path(parent_path);
    if (!parent || parent->type != KRYON_VFS_NODE_DIRECTORY) {
        kryon_free(norm_path);
        kryon_free(parent_path);
        kryon_free(dir_name);
        return false;
    }
    
    // Create directory node
    KryonVfsNode* dir_node = create_node(dir_name, KRYON_VFS_NODE_DIRECTORY);
    if (!dir_node) {
        kryon_free(norm_path);
        kryon_free(parent_path);
        kryon_free(dir_name);
        return false;
    }
    
    add_child(parent, dir_node);
    
    kryon_free(norm_path);
    kryon_free(parent_path);
    kryon_free(dir_name);
    return true;
}

KryonVfsHandle kryon_vfs_open(const char* path, KryonVfsMode mode) {
    if (!path) return NULL;
    
    char* norm_path = normalize_path(path);
    if (!norm_path) return NULL;
    
    KryonVfsNode* node = resolve_path(norm_path);
    
    // Handle creation if file doesn't exist
    if (!node && (mode & KRYON_VFS_MODE_CREATE)) {
        char* parent_path = kryon_path_dirname(norm_path);
        char* file_name = kryon_path_basename(norm_path);
        
        if (parent_path && file_name) {
            KryonVfsNode* parent = resolve_path(parent_path);
            if (parent && parent->type == KRYON_VFS_NODE_DIRECTORY) {
                node = create_node(file_name, KRYON_VFS_NODE_FILE);
                if (node) {
                    add_child(parent, node);
                }
            }
        }
        
        kryon_free(parent_path);
        kryon_free(file_name);
    }
    
    kryon_free(norm_path);
    
    if (!node || node->type != KRYON_VFS_NODE_FILE) {
        return NULL;
    }
    
    // Find free handle
    int handle_index = find_free_handle();
    if (handle_index == -1) return NULL;
    
    // Initialize handle
    g_vfs.handles[handle_index].node = node;
    g_vfs.handles[handle_index].position = (mode & KRYON_VFS_MODE_APPEND) ? node->size : 0;
    g_vfs.handles[handle_index].mode = mode;
    g_vfs.handles[handle_index].is_open = true;
    
    node->ref_count++;
    node->accessed_time = time(NULL);
    
    return (KryonVfsHandle)(intptr_t)(handle_index + 1);
}

void kryon_vfs_close(KryonVfsHandle handle) {
    if (!handle) return;
    
    int handle_index = (int)(intptr_t)handle - 1;
    if (handle_index < 0 || handle_index >= KRYON_VFS_MAX_HANDLES || !g_vfs.handles[handle_index].is_open) {
        return;
    }
    
    KryonVfsNode* node = g_vfs.handles[handle_index].node;
    if (node) {
        destroy_node(node);
    }
    
    memset(&g_vfs.handles[handle_index], 0, sizeof(g_vfs.handles[handle_index]));
}

size_t kryon_vfs_read(KryonVfsHandle handle, void* buffer, size_t size) {
    if (!handle || !buffer) return 0;
    
    int handle_index = (int)(intptr_t)handle - 1;
    if (handle_index < 0 || handle_index >= KRYON_VFS_MAX_HANDLES || !g_vfs.handles[handle_index].is_open) {
        return 0;
    }
    
    KryonVfsNode* node = g_vfs.handles[handle_index].node;
    if (!node || node->type != KRYON_VFS_NODE_FILE || !node->data) {
        return 0;
    }
    
    size_t position = g_vfs.handles[handle_index].position;
    size_t available = node->size > position ? node->size - position : 0;
    size_t to_read = size < available ? size : available;
    
    if (to_read > 0) {
        memcpy(buffer, (char*)node->data + position, to_read);
        g_vfs.handles[handle_index].position += to_read;
        node->accessed_time = time(NULL);
    }
    
    return to_read;
}

size_t kryon_vfs_write(KryonVfsHandle handle, const void* buffer, size_t size) {
    if (!handle || !buffer) return 0;
    
    int handle_index = (int)(intptr_t)handle - 1;
    if (handle_index < 0 || handle_index >= KRYON_VFS_MAX_HANDLES || !g_vfs.handles[handle_index].is_open) {
        return 0;
    }
    
    KryonVfsNode* node = g_vfs.handles[handle_index].node;
    if (!node || node->type != KRYON_VFS_NODE_FILE) {
        return 0;
    }
    
    size_t position = g_vfs.handles[handle_index].position;
    size_t required_size = position + size;
    
    // Check size limits
    if (required_size > g_vfs.max_file_size) {
        return 0;
    }
    
    // Expand buffer if needed
    if (required_size > node->capacity) {
        size_t new_capacity = node->capacity == 0 ? 1024 : node->capacity;
        while (new_capacity < required_size) {
            new_capacity *= 2;
        }
        
        void* new_data = kryon_alloc(new_capacity);
        if (!new_data) return 0;
        
        if (node->data && node->size > 0) {
            memcpy(new_data, node->data, node->size);
        }
        
        kryon_free(node->data);
        node->data = new_data;
        node->capacity = new_capacity;
    }
    
    // Write data
    memcpy((char*)node->data + position, buffer, size);
    g_vfs.handles[handle_index].position += size;
    
    if (position + size > node->size) {
        node->size = position + size;
    }
    
    node->modified_time = time(NULL);
    
    return size;
}

bool kryon_vfs_seek(KryonVfsHandle handle, long offset, KryonVfsSeek whence) {
    if (!handle) return false;
    
    int handle_index = (int)(intptr_t)handle - 1;
    if (handle_index < 0 || handle_index >= KRYON_VFS_MAX_HANDLES || !g_vfs.handles[handle_index].is_open) {
        return false;
    }
    
    KryonVfsNode* node = g_vfs.handles[handle_index].node;
    if (!node) return false;
    
    long new_position;
    switch (whence) {
        case KRYON_VFS_SEEK_SET:
            new_position = offset;
            break;
        case KRYON_VFS_SEEK_CUR:
            new_position = g_vfs.handles[handle_index].position + offset;
            break;
        case KRYON_VFS_SEEK_END:
            new_position = node->size + offset;
            break;
        default:
            return false;
    }
    
    if (new_position < 0) return false;
    
    g_vfs.handles[handle_index].position = new_position;
    return true;
}

long kryon_vfs_tell(KryonVfsHandle handle) {
    if (!handle) return -1;
    
    int handle_index = (int)(intptr_t)handle - 1;
    if (handle_index < 0 || handle_index >= KRYON_VFS_MAX_HANDLES || !g_vfs.handles[handle_index].is_open) {
        return -1;
    }
    
    return g_vfs.handles[handle_index].position;
}

bool kryon_vfs_exists(const char* path) {
    if (!path) return false;
    
    char* norm_path = normalize_path(path);
    if (!norm_path) return false;
    
    KryonVfsNode* node = resolve_path(norm_path);
    kryon_free(norm_path);
    
    return node != NULL;
}

bool kryon_vfs_is_directory(const char* path) {
    if (!path) return false;
    
    char* norm_path = normalize_path(path);
    if (!norm_path) return false;
    
    KryonVfsNode* node = resolve_path(norm_path);
    kryon_free(norm_path);
    
    return node && node->type == KRYON_VFS_NODE_DIRECTORY;
}

KryonVfsInfo kryon_vfs_get_info(const char* path) {
    KryonVfsInfo info = {0};
    
    if (!path) return info;
    
    char* norm_path = normalize_path(path);
    if (!norm_path) return info;
    
    KryonVfsNode* node = resolve_path(norm_path);
    kryon_free(norm_path);
    
    if (!node) return info;
    
    info.size = node->size;
    info.is_directory = node->type == KRYON_VFS_NODE_DIRECTORY;
    info.is_mount_point = node->type == KRYON_VFS_NODE_MOUNT_POINT;
    info.created_time = node->created_time;
    info.modified_time = node->modified_time;
    info.accessed_time = node->accessed_time;
    info.permissions = node->permissions;
    
    return info;
}

bool kryon_vfs_delete(const char* path) {
    if (!path) return false;
    
    char* norm_path = normalize_path(path);
    if (!norm_path) return false;
    
    KryonVfsNode* node = resolve_path(norm_path);
    if (!node) {
        kryon_free(norm_path);
        return false;
    }
    
    // Remove from parent
    if (node->parent) {
        KryonVfsNode* parent = node->parent;
        for (size_t i = 0; i < parent->child_count; i++) {
            if (parent->children[i] == node) {
                // Shift remaining children
                for (size_t j = i + 1; j < parent->child_count; j++) {
                    parent->children[j - 1] = parent->children[j];
                }
                parent->child_count--;
                break;
            }
        }
    }
    
    destroy_node(node);
    kryon_free(norm_path);
    return true;
}

void kryon_vfs_set_case_sensitive(bool case_sensitive) {
    g_vfs.case_sensitive = case_sensitive;
}

bool kryon_vfs_get_case_sensitive(void) {
    return g_vfs.case_sensitive;
}

void kryon_vfs_set_max_file_size(size_t max_size) {
    g_vfs.max_file_size = max_size;
}

size_t kryon_vfs_get_max_file_size(void) {
    return g_vfs.max_file_size;
}