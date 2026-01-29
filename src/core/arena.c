/**

 * @file arena.c
 * @brief Kryon Memory Arena System
 * 
 * High-performance arena allocator for temporary allocations with fast reset.
 * Based on our memory management patterns but optimized for layout engine usage.
 */
#include "lib9.h"


#include "memory.h"
#include <assert.h>

// =============================================================================
// ARENA ALLOCATOR
// =============================================================================

/// Memory arena for fast temporary allocations
typedef struct {
    char* memory;           ///< Raw memory block
    size_t size;           ///< Total arena size
    size_t used;           ///< Currently used bytes
    size_t alignment;      ///< Default alignment requirement
    uint32_t generation;   ///< Reset counter for debugging
} KryonArena;

/**
 * @brief Create memory arena with specified size
 * @param size Total arena size in bytes
 * @param alignment Default alignment (typically sizeof(void*))
 * @return New arena or NULL on failure
 */
KryonArena* kryon_arena_create(size_t size, size_t alignment) {
    if (size == 0) {
        fprint(2, "Kryon Arena: Cannot create zero-size arena\n");
        return NULL;
    }
    
    if (alignment == 0 || (alignment & (alignment - 1)) != 0) {
        fprint(2, "Kryon Arena: Alignment must be power of 2\n");
        return NULL;
    }
    
    KryonArena* arena = malloc(sizeof(KryonArena));
    if (!arena) {
        fprint(2, "Kryon Arena: Failed to allocate arena structure\n");
        return NULL;
    }
    
    arena->memory = malloc(size);
    if (!arena->memory) {
        fprint(2, "Kryon Arena: Failed to allocate %zu bytes\n", size);
        free(arena);
        return NULL;
    }
    
    arena->size = size;
    arena->used = 0;
    arena->alignment = alignment;
    arena->generation = 0;
    
    return arena;
}

/**
 * @brief Destroy arena and free all memory
 * @param arena Arena to destroy
 */
void kryon_arena_destroy(KryonArena* arena) {
    if (!arena) return;
    
    free(arena->memory);
    free(arena);
}

/**
 * @brief Allocate aligned memory from arena
 * @param arena Target arena
 * @param size Number of bytes to allocate
 * @param alignment Memory alignment (0 = use default)
 * @return Allocated memory or NULL if insufficient space
 */
void* kryon_arena_alloc(KryonArena* arena, size_t size, size_t alignment) {
    if (!arena || size == 0) return NULL;
    
    if (alignment == 0) alignment = arena->alignment;
    
    // Calculate aligned position
    size_t aligned_used = (arena->used + alignment - 1) & ~(alignment - 1);
    
    if (aligned_used + size > arena->size) {
        fprint(2, "Kryon Arena: Out of memory (requested %zu bytes, %zu available)\n", 
                size, arena->size - aligned_used);
        return NULL;
    }
    
    void* result = arena->memory + aligned_used;
    arena->used = aligned_used + size;
    
    return result;
}

/**
 * @brief Reset arena to empty state (fast)
 * @param arena Arena to reset
 */
void kryon_arena_reset(KryonArena* arena) {
    if (!arena) return;
    
    arena->used = 0;
    arena->generation++;
}

/**
 * @brief Get arena statistics
 * @param arena Target arena
 * @param used_bytes Output for used bytes
 * @param free_bytes Output for free bytes
 * @param generation Output for reset generation
 */
void kryon_arena_get_stats(const KryonArena* arena, size_t* used_bytes, 
                          size_t* free_bytes, uint32_t* generation) {
    if (!arena) return;
    
    if (used_bytes) *used_bytes = arena->used;
    if (free_bytes) *free_bytes = arena->size - arena->used;
    if (generation) *generation = arena->generation;
}

// =============================================================================
// TYPED ARENA ALLOCATORS
// =============================================================================

/**
 * @brief Allocate array of typed elements from arena
 * @param arena Target arena
 * @param count Number of elements
 * @param element_size Size of each element
 * @param alignment Memory alignment
 * @return Allocated array or NULL on failure
 */
void* kryon_arena_alloc_array(KryonArena* arena, size_t count, 
                             size_t element_size, size_t alignment) {
    if (count == 0 || element_size == 0) return NULL;
    
    // Check for overflow
    if (count > SIZE_MAX / element_size) {
        fprint(2, "Kryon Arena: Array allocation overflow\n");
        return NULL;
    }
    
    size_t total_size = count * element_size;
    void* result = kryon_arena_alloc(arena, total_size, alignment);
    
    if (result) {
        memset(result, 0, total_size);
    }
    
    return result;
}

/**
 * @brief Allocate and copy string to arena
 * @param arena Target arena
 * @param str String to copy
 * @return Allocated string copy or NULL on failure
 */
char* kryon_arena_strdup(KryonArena* arena, const char* str) {
    if (!str) return NULL;
    
    size_t len = strlen(str) + 1;
    char* result = kryon_arena_alloc(arena, len, 1);
    
    if (result) {
        memcpy(result, str, len);
    }
    
    return result;
}
