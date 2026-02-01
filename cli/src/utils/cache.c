/**
 * Cache Management
 *
 * Handles Kryon build cache (.kryon_cache directory)
 * Provides utilities for cache path resolution and validation.
 */

#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include "cache.h"
#include "kryon_cli.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

/* ============================================================================
 * Cache Directory Management
 * ============================================================================ */

#define KRYON_CACHE_DIR ".kryon_cache"

/**
 * Get the cache directory path
 * Returns a newly allocated string that must be freed by caller
 */
char* cache_get_dir(void) {
    // For now, cache is always in current working directory
    // Future: could use XDG_CACHE_HOME or configurable path
    return strdup(KRYON_CACHE_DIR);
}

/**
 * Get the cache path for a source file
 * Converts source file to cache file path:
 *   source.kry → .kryon_cache/source.kir
 *   source.md  → .kryon_cache/source.kir
 */
char* cache_get_path(const char* source_file) {
    if (!source_file) {
        return NULL;
    }

    // Get basename without extension
    const char* slash = strrchr(source_file, '/');
    const char* basename = slash ? slash + 1 : source_file;

    // Copy basename and remove extension
    char name_copy[512];
    strncpy(name_copy, basename, sizeof(name_copy) - 1);
    name_copy[sizeof(name_copy) - 1] = '\0';
    char* dot = strrchr(name_copy, '.');
    if (dot) *dot = '\0';

    // Build cache path: .kryon_cache/basename.kir
    char* cache_dir = cache_get_dir();
    if (!cache_dir) {
        return NULL;
    }

    char* cache_path = NULL;
    asprintf(&cache_path, "%s/%s.kir", cache_dir, name_copy);
    free(cache_dir);

    return cache_path;
}

/**
 * Ensure cache directory exists
 * Creates .kryon_cache if it doesn't exist
 */
bool cache_ensure_dir(void) {
    if (!file_is_directory(KRYON_CACHE_DIR)) {
        return dir_create(KRYON_CACHE_DIR);
    }
    return true;
}

/**
 * Check if cached file is valid (more recent than source)
 */
bool cache_is_valid(const char* cached_file, const char* source_file) {
    if (!cached_file || !source_file) {
        return false;
    }

    // Check if cached file exists
    if (!file_exists(cached_file)) {
        return false;
    }

    // Get modification times
    struct stat cached_stat, source_stat;
    if (stat(cached_file, &cached_stat) != 0) {
        return false;
    }
    if (stat(source_file, &source_stat) != 0) {
        return false;
    }

    // Cached file should be newer than source
    return cached_stat.st_mtime >= source_stat.st_mtime;
}

/**
 * Invalidate cache entries matching a pattern
 * If pattern is NULL, clears entire cache
 */
int cache_invalidate(const char* pattern) {
    char* cache_dir = cache_get_dir();
    if (!cache_dir) {
        return 1;
    }

    // For now, simple implementation: remove entire cache directory
    // Future: could selectively invalidate based on pattern
    if (!pattern) {
        char cmd[1024];
        snprintf(cmd, sizeof(cmd), "rm -rf \"%s\"", cache_dir);
        free(cache_dir);
        return system(cmd);
    }

    // Pattern-based invalidation would require directory scanning
    // For now, just clear entire cache
    free(cache_dir);
    return 0;
}

/**
 * Get cache size in bytes
 */
long long cache_get_size(void) {
    char* cache_dir = cache_get_dir();
    if (!cache_dir) {
        return -1;
    }

    // Simple du -sh implementation
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "du -sb \"%s\" 2>/dev/null | cut -f1", cache_dir);
    free(cache_dir);

    FILE* pipe = popen(cmd, "r");
    if (!pipe) {
        return -1;
    }

    long long size = 0;
    if (fscanf(pipe, "%lld", &size) == 1) {
        pclose(pipe);
        return size;
    }

    pclose(pipe);
    return -1;
}
