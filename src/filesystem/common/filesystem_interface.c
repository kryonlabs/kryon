/**
 * @file filesystem_interface.c
 * @brief Kryon Filesystem Interface Implementation
 */

#include "internal/filesystem.h"
#include "internal/memory.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>

// =============================================================================
// FILE OPERATIONS
// =============================================================================

KryonFileHandle kryon_file_open(const char* path, KryonFileMode mode) {
    if (!path) return NULL;
    
    const char* mode_str;
    switch (mode) {
        case KRYON_FILE_MODE_READ:
            mode_str = "rb";
            break;
        case KRYON_FILE_MODE_WRITE:
            mode_str = "wb";
            break;
        case KRYON_FILE_MODE_APPEND:
            mode_str = "ab";
            break;
        case KRYON_FILE_MODE_READ_WRITE:
            mode_str = "r+b";
            break;
        case KRYON_FILE_MODE_WRITE_CREATE:
            mode_str = "w+b";
            break;
        default:
            return NULL;
    }
    
    FILE* file = fopen(path, mode_str);
    return (KryonFileHandle)file;
}

void kryon_file_close(KryonFileHandle handle) {
    if (handle) {
        fclose((FILE*)handle);
    }
}

size_t kryon_file_read(KryonFileHandle handle, void* buffer, size_t size) {
    if (!handle || !buffer) return 0;
    
    return fread(buffer, 1, size, (FILE*)handle);
}

size_t kryon_file_write(KryonFileHandle handle, const void* buffer, size_t size) {
    if (!handle || !buffer) return 0;
    
    return fwrite(buffer, 1, size, (FILE*)handle);
}

bool kryon_file_seek(KryonFileHandle handle, long offset, KryonFileSeek whence) {
    if (!handle) return false;
    
    int whence_val;
    switch (whence) {
        case KRYON_FILE_SEEK_SET:
            whence_val = SEEK_SET;
            break;
        case KRYON_FILE_SEEK_CUR:
            whence_val = SEEK_CUR;
            break;
        case KRYON_FILE_SEEK_END:
            whence_val = SEEK_END;
            break;
        default:
            return false;
    }
    
    return fseek((FILE*)handle, offset, whence_val) == 0;
}

long kryon_file_tell(KryonFileHandle handle) {
    if (!handle) return -1;
    
    return ftell((FILE*)handle);
}

bool kryon_file_flush(KryonFileHandle handle) {
    if (!handle) return false;
    
    return fflush((FILE*)handle) == 0;
}

// =============================================================================
// FILE INFORMATION
// =============================================================================

bool kryon_file_exists(const char* path) {
    if (!path) return false;
    
    struct stat st;
    return stat(path, &st) == 0;
}

size_t kryon_file_size(const char* path) {
    if (!path) return 0;
    
    struct stat st;
    if (stat(path, &st) != 0) {
        return 0;
    }
    
    return st.st_size;
}

bool kryon_file_is_directory(const char* path) {
    if (!path) return false;
    
    struct stat st;
    if (stat(path, &st) != 0) {
        return false;
    }
    
    return S_ISDIR(st.st_mode);
}

bool kryon_file_is_regular(const char* path) {
    if (!path) return false;
    
    struct stat st;
    if (stat(path, &st) != 0) {
        return false;
    }
    
    return S_ISREG(st.st_mode);
}

KryonFileInfo kryon_file_get_info(const char* path) {
    KryonFileInfo info = {0};
    
    if (!path) return info;
    
    struct stat st;
    if (stat(path, &st) != 0) {
        return info;
    }
    
    info.size = st.st_size;
    info.created_time = st.st_ctime;
    info.modified_time = st.st_mtime;
    info.accessed_time = st.st_atime;
    info.is_directory = S_ISDIR(st.st_mode);
    info.is_regular = S_ISREG(st.st_mode);
    info.is_readable = access(path, R_OK) == 0;
    info.is_writable = access(path, W_OK) == 0;
    info.is_executable = access(path, X_OK) == 0;
    
    return info;
}

// =============================================================================
// DIRECTORY OPERATIONS
// =============================================================================

bool kryon_directory_create(const char* path) {
    if (!path) return false;
    
    return mkdir(path, 0755) == 0 || errno == EEXIST;
}

bool kryon_directory_create_recursive(const char* path) {
    if (!path) return false;
    
    char* path_copy = kryon_alloc(strlen(path) + 1);
    if (!path_copy) return false;
    
    strcpy(path_copy, path);
    
    char* p = path_copy;
    if (*p == '/') p++; // Skip leading slash
    
    while (*p) {
        if (*p == '/') {
            *p = '\0';
            mkdir(path_copy, 0755);
            *p = '/';
        }
        p++;
    }
    
    bool result = mkdir(path_copy, 0755) == 0 || errno == EEXIST;
    kryon_free(path_copy);
    
    return result;
}

bool kryon_directory_remove(const char* path) {
    if (!path) return false;
    
    return rmdir(path) == 0;
}

KryonDirectoryListing kryon_directory_list(const char* path) {
    KryonDirectoryListing listing = {0};
    
    if (!path) return listing;
    
    DIR* dir = opendir(path);
    if (!dir) return listing;
    
    // Count entries first
    size_t count = 0;
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
            count++;
        }
    }
    
    if (count == 0) {
        closedir(dir);
        return listing;
    }
    
    // Allocate entries
    listing.entries = kryon_alloc(sizeof(KryonDirectoryEntry) * count);
    if (!listing.entries) {
        closedir(dir);
        return listing;
    }
    
    // Fill entries
    rewinddir(dir);
    size_t index = 0;
    while ((entry = readdir(dir)) != NULL && index < count) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        KryonDirectoryEntry* dir_entry = &listing.entries[index];
        
        // Copy name
        size_t name_len = strlen(entry->d_name);
        dir_entry->name = kryon_alloc(name_len + 1);
        if (dir_entry->name) {
            strcpy(dir_entry->name, entry->d_name);
        }
        
        // Get full path for stat
        size_t full_path_len = strlen(path) + name_len + 2;
        char* full_path = kryon_alloc(full_path_len);
        if (full_path) {
            snprintf(full_path, full_path_len, "%s/%s", path, entry->d_name);
            
            struct stat st;
            if (stat(full_path, &st) == 0) {
                dir_entry->size = st.st_size;
                dir_entry->is_directory = S_ISDIR(st.st_mode);
                dir_entry->modified_time = st.st_mtime;
            }
            
            kryon_free(full_path);
        }
        
        index++;
    }
    
    listing.count = index;
    closedir(dir);
    
    return listing;
}

void kryon_directory_listing_free(KryonDirectoryListing* listing) {
    if (!listing) return;
    
    for (size_t i = 0; i < listing->count; i++) {
        kryon_free(listing->entries[i].name);
    }
    
    kryon_free(listing->entries);
    memset(listing, 0, sizeof(KryonDirectoryListing));
}

// =============================================================================
// FILE MANIPULATION
// =============================================================================

bool kryon_file_copy(const char* src, const char* dst) {
    if (!src || !dst) return false;
    
    FILE* src_file = fopen(src, "rb");
    if (!src_file) return false;
    
    FILE* dst_file = fopen(dst, "wb");
    if (!dst_file) {
        fclose(src_file);
        return false;
    }
    
    char buffer[8192];
    size_t bytes_read;
    bool success = true;
    
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), src_file)) > 0) {
        if (fwrite(buffer, 1, bytes_read, dst_file) != bytes_read) {
            success = false;
            break;
        }
    }
    
    fclose(src_file);
    fclose(dst_file);
    
    if (!success) {
        unlink(dst); // Remove incomplete file
    }
    
    return success;
}

bool kryon_file_move(const char* src, const char* dst) {
    if (!src || !dst) return false;
    
    // Try rename first (fastest if on same filesystem)
    if (rename(src, dst) == 0) {
        return true;
    }
    
    // Fall back to copy + delete
    if (kryon_file_copy(src, dst)) {
        return unlink(src) == 0;
    }
    
    return false;
}

bool kryon_file_delete(const char* path) {
    if (!path) return false;
    
    return unlink(path) == 0;
}

// =============================================================================
// PATH UTILITIES
// =============================================================================

char* kryon_path_join(const char* path1, const char* path2) {
    if (!path1 || !path2) return NULL;
    
    size_t len1 = strlen(path1);
    size_t len2 = strlen(path2);
    
    // Check if we need a separator
    bool need_separator = len1 > 0 && path1[len1 - 1] != '/' && path2[0] != '/';
    
    size_t total_len = len1 + len2 + (need_separator ? 1 : 0) + 1;
    char* result = kryon_alloc(total_len);
    if (!result) return NULL;
    
    strcpy(result, path1);
    if (need_separator) {
        strcat(result, "/");
    }
    strcat(result, path2);
    
    return result;
}

char* kryon_path_dirname(const char* path) {
    if (!path) return NULL;
    
    size_t len = strlen(path);
    if (len == 0) return NULL;
    
    // Find last slash
    const char* last_slash = strrchr(path, '/');
    if (!last_slash) {
        // No slash found, return "."
        char* result = kryon_alloc(2);
        if (result) {
            strcpy(result, ".");
        }
        return result;
    }
    
    // Calculate dirname length
    size_t dirname_len = last_slash - path;
    if (dirname_len == 0) {
        // Root directory
        char* result = kryon_alloc(2);
        if (result) {
            strcpy(result, "/");
        }
        return result;
    }
    
    char* result = kryon_alloc(dirname_len + 1);
    if (result) {
        strncpy(result, path, dirname_len);
        result[dirname_len] = '\0';
    }
    
    return result;
}

char* kryon_path_basename(const char* path) {
    if (!path) return NULL;
    
    size_t len = strlen(path);
    if (len == 0) return NULL;
    
    // Find last slash
    const char* last_slash = strrchr(path, '/');
    const char* basename_start = last_slash ? last_slash + 1 : path;
    
    size_t basename_len = strlen(basename_start);
    char* result = kryon_alloc(basename_len + 1);
    if (result) {
        strcpy(result, basename_start);
    }
    
    return result;
}

char* kryon_path_extension(const char* path) {
    if (!path) return NULL;
    
    const char* last_dot = strrchr(path, '.');
    const char* last_slash = strrchr(path, '/');
    
    // Make sure the dot is in the filename, not directory name
    if (!last_dot || (last_slash && last_dot < last_slash)) {
        return NULL;
    }
    
    size_t ext_len = strlen(last_dot);
    char* result = kryon_alloc(ext_len + 1);
    if (result) {
        strcpy(result, last_dot);
    }
    
    return result;
}

char* kryon_path_absolute(const char* path) {
    if (!path) return NULL;
    
    char* result = realpath(path, NULL);
    if (!result) return NULL;
    
    // realpath uses malloc, we need to use our allocator
    size_t len = strlen(result);
    char* kryon_result = kryon_alloc(len + 1);
    if (kryon_result) {
        strcpy(kryon_result, result);
    }
    
    free(result); // Free the malloc'd result
    return kryon_result;
}