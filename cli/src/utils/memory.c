/**
 * Memory Safety Wrappers
 * Provides safe allocation functions that handle out-of-memory gracefully
 */

#include "kryon_cli.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/**
 * Safe malloc wrapper - exits on failure
 */
void* safe_malloc(size_t size) {
    if (size == 0) return NULL;

    void* ptr = malloc(size);
    if (!ptr) {
        fprintf(stderr, "Error: Out of memory (failed to allocate %zu bytes)\n", size);
        exit(1);
    }
    return ptr;
}

/**
 * Safe calloc wrapper - exits on failure
 */
void* safe_calloc(size_t count, size_t size) {
    if (count == 0 || size == 0) return NULL;

    void* ptr = calloc(count, size);
    if (!ptr) {
        fprintf(stderr, "Error: Out of memory (failed to allocate %zu * %zu bytes)\n", count, size);
        exit(1);
    }
    return ptr;
}

/**
 * Safe realloc wrapper - exits on failure
 */
void* safe_realloc(void* ptr, size_t size) {
    // realloc with size 0 is equivalent to free
    if (size == 0) {
        free(ptr);
        return NULL;
    }

    void* new_ptr = realloc(ptr, size);
    if (!new_ptr && size > 0) {
        fprintf(stderr, "Error: Out of memory (failed to reallocate to %zu bytes)\n", size);
        exit(1);
    }
    return new_ptr;
}
