/**
 * KRY Allocator - Chunk-based memory allocation for parser
 *
 * Provides efficient memory allocation from contiguous chunks,
 * avoiding individual malloc calls for small allocations.
 */

#ifndef KRY_ALLOCATOR_H
#define KRY_ALLOCATOR_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Forward declarations
typedef struct KryParser KryParser;

/**
 * Allocate memory from parser's chunk allocator
 *
 * @param parser Parser context with memory chunks
 * @param size Number of bytes to allocate
 * @return Pointer to allocated memory, or NULL on failure
 */
void* kry_alloc(KryParser* parser, size_t size);

/**
 * Duplicate a string using chunk allocator
 *
 * @param parser Parser context
 * @param str String to duplicate
 * @return Duplicated string, or NULL on failure
 */
char* kry_strdup(KryParser* parser, const char* str);

/**
 * Duplicate a string with length using chunk allocator
 *
 * @param parser Parser context
 * @param str String to duplicate
 * @param len Number of characters to copy
 * @return Duplicated string, or NULL on failure
 */
char* kry_strndup(KryParser* parser, const char* str, size_t len);

#ifdef __cplusplus
}
#endif

#endif // KRY_ALLOCATOR_H
