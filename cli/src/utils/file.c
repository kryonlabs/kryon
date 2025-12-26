/**
 * File Utilities
 * Provides file I/O and path manipulation functions
 */

#include "kryon_cli.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <libgen.h>

/**
 * Check if file exists
 */
bool file_exists(const char* path) {
    if (!path) return false;

    struct stat st;
    return stat(path, &st) == 0;
}

/**
 * Check if path is a directory
 */
bool file_is_directory(const char* path) {
    if (!path) return false;

    struct stat st;
    if (stat(path, &st) != 0) return false;

    return S_ISDIR(st.st_mode);
}

/**
 * Read entire file into newly allocated string
 */
char* file_read(const char* path) {
    if (!path) return NULL;

    FILE* fp = fopen(path, "rb");
    if (!fp) return NULL;

    // Get file size
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (size < 0) {
        fclose(fp);
        return NULL;
    }

    // Allocate buffer
    char* buffer = (char*)malloc(size + 1);
    if (!buffer) {
        fclose(fp);
        return NULL;
    }

    // Read file
    size_t bytes_read = fread(buffer, 1, size, fp);
    buffer[bytes_read] = '\0';

    fclose(fp);
    return buffer;
}

/**
 * Write content to file
 */
bool file_write(const char* path, const char* content) {
    if (!path || !content) return false;

    FILE* fp = fopen(path, "wb");
    if (!fp) return false;

    size_t len = strlen(content);
    size_t written = fwrite(content, 1, len, fp);

    fclose(fp);
    return written == len;
}

/**
 * Create directory (single level)
 */
bool dir_create(const char* path) {
    if (!path) return false;

    return mkdir(path, 0755) == 0 || errno == EEXIST;
}

/**
 * Create directory recursively (like mkdir -p)
 */
bool dir_create_recursive(const char* path) {
    if (!path) return false;

    // Make a copy since dirname may modify the string
    char* path_copy = str_copy(path);
    if (!path_copy) return false;

    // Try to create the directory
    if (dir_create(path)) {
        free(path_copy);
        return true;
    }

    // If failed and parent doesn't exist, create parent first
    char* parent = dirname(path_copy);
    if (strcmp(parent, ".") != 0 && strcmp(parent, "/") != 0) {
        if (!dir_create_recursive(parent)) {
            free(path_copy);
            return false;
        }
    }

    free(path_copy);
    return dir_create(path);
}

/**
 * Get current working directory
 */
char* dir_get_current(void) {
    char* buffer = (char*)malloc(4096);
    if (!buffer) return NULL;

    if (getcwd(buffer, 4096) == NULL) {
        free(buffer);
        return NULL;
    }

    return buffer;
}

/**
 * Join two paths with separator
 */
char* path_join(const char* p1, const char* p2) {
    if (!p1 || !p2) return NULL;

    size_t len1 = strlen(p1);
    size_t len2 = strlen(p2);
    bool needs_sep = len1 > 0 && p1[len1 - 1] != '/';

    size_t total_len = len1 + len2 + (needs_sep ? 1 : 0) + 1;
    char* result = (char*)malloc(total_len);
    if (!result) return NULL;

    strcpy(result, p1);
    if (needs_sep) {
        strcat(result, "/");
    }
    strcat(result, p2);

    return result;
}

/**
 * Get file extension (including dot)
 */
const char* path_extension(const char* path) {
    if (!path) return "";

    const char* dot = strrchr(path, '.');
    const char* sep = strrchr(path, '/');

    // Make sure dot is after last separator
    if (!dot || (sep && dot < sep)) {
        return "";
    }

    return dot;
}
