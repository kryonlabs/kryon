/**
 * @file cache.h
 * @brief Cache management for Kryon build system
 */

#ifndef KRYON_CACHE_H
#define KRYON_CACHE_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Get the cache directory path
 * @return Newly allocated string that must be freed, or NULL on error
 */
char* cache_get_dir(void);

/**
 * Get the cache path for a source file
 * @param source_file Path to source file
 * @return Newly allocated string that must be freed, or NULL on error
 */
char* cache_get_path(const char* source_file);

/**
 * Ensure cache directory exists
 * @return true if directory exists or was created successfully
 */
bool cache_ensure_dir(void);

/**
 * Check if cached file is valid (more recent than source)
 * @param cached_file Path to cached file
 * @param source_file Path to source file
 * @return true if cache is valid
 */
bool cache_is_valid(const char* cached_file, const char* source_file);

/**
 * Invalidate cache entries
 * @param pattern Glob pattern (NULL to clear entire cache)
 * @return 0 on success, non-zero on error
 */
int cache_invalidate(const char* pattern);

/**
 * Get cache size in bytes
 * @return Cache size in bytes, or -1 on error
 */
long long cache_get_size(void);

#ifdef __cplusplus
}
#endif

#endif // KRYON_CACHE_H
