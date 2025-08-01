/**
 * @file memory.h
 * @brief Kryon Memory Management System
 * 
 * High-performance memory management with pools, tracking, and debugging.
 * Provides custom allocators optimized for Kryon's usage patterns.
 * 
 * @version 1.0.0
 * @author Kryon Labs
 */

#ifndef KRYON_INTERNAL_MEMORY_H
#define KRYON_INTERNAL_MEMORY_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

// =============================================================================
// FORWARD DECLARATIONS
// =============================================================================

typedef struct KryonMemoryManager KryonMemoryManager;
typedef struct KryonMemoryPool KryonMemoryPool;
typedef struct KryonMemoryStats KryonMemoryStats;
typedef struct KryonAllocation KryonAllocation;

// =============================================================================
// MEMORY CONFIGURATION
// =============================================================================

/**
 * @brief Memory pool configuration
 */
typedef struct {
    size_t chunk_size;        ///< Size of each chunk in bytes
    size_t max_chunks;        ///< Maximum number of chunks
    size_t alignment;         ///< Memory alignment (must be power of 2)
    bool thread_safe;         ///< Whether pool should be thread-safe
    bool track_allocations;   ///< Whether to track individual allocations
} KryonPoolConfig;

/**
 * @brief Memory manager configuration
 */
typedef struct {
    size_t initial_heap_size;     ///< Initial heap size in bytes
    size_t max_heap_size;         ///< Maximum heap size in bytes
    bool enable_leak_detection;   ///< Enable memory leak detection
    bool enable_bounds_checking;  ///< Enable buffer overflow detection
    bool enable_statistics;       ///< Enable memory usage statistics
    bool use_system_malloc;       ///< Fall back to system malloc for large allocations
    size_t large_alloc_threshold; ///< Threshold for "large" allocations
} KryonMemoryConfig;

// =============================================================================
// MEMORY STATISTICS
// =============================================================================

/**
 * @brief Memory usage statistics
 */
struct KryonMemoryStats {
    // Current usage
    size_t total_allocated;      ///< Total bytes currently allocated
    size_t total_used;           ///< Total bytes actually used
    size_t peak_allocated;       ///< Peak allocation in bytes
    
    // Allocation counts
    uint64_t alloc_count;        ///< Total number of allocations
    uint64_t free_count;         ///< Total number of frees
    uint64_t realloc_count;      ///< Total number of reallocs
    
    // Pool statistics
    size_t pool_overhead;        ///< Memory overhead from pooling
    size_t fragmentation;        ///< Estimated fragmentation
    
    // Performance metrics
    uint64_t total_alloc_time_ns; ///< Total time spent in allocations
    uint64_t total_free_time_ns;  ///< Total time spent in frees
    
    // Error counts
    uint32_t out_of_memory_count; ///< Number of OOM errors
    uint32_t double_free_count;   ///< Number of double-free errors
    uint32_t leak_count;          ///< Number of detected leaks
};

// =============================================================================
// ALLOCATION TRACKING
// =============================================================================

/**
 * @brief Information about a single allocation
 */
struct KryonAllocation {
    void *ptr;                   ///< Pointer to allocated memory
    size_t size;                 ///< Size of allocation
    size_t alignment;            ///< Alignment used
    uint64_t alloc_time;         ///< Timestamp of allocation
    const char *file;            ///< Source file where allocated
    int line;                    ///< Source line where allocated
    const char *function;        ///< Function where allocated
    uint32_t thread_id;          ///< Thread that made the allocation
    struct KryonAllocation *next; ///< Next allocation in linked list
};

// =============================================================================
// MEMORY POOL SYSTEM
// =============================================================================

/**
 * @brief Memory pool for fixed-size allocations
 */
struct KryonMemoryPool {
    size_t chunk_size;           ///< Size of each chunk
    size_t chunk_count;          ///< Number of chunks in pool
    size_t free_count;           ///< Number of free chunks
    void *memory_block;          ///< Actual memory block
    void **free_list;            ///< Stack of free chunks
    bool thread_safe;            ///< Thread safety flag
    void *mutex;                 ///< Mutex for thread safety
    KryonMemoryStats stats;      ///< Pool-specific statistics
};

// =============================================================================
// MAIN MEMORY MANAGER
// =============================================================================

/**
 * @brief Main memory manager instance
 */
struct KryonMemoryManager {
    KryonMemoryConfig config;         ///< Configuration
    KryonMemoryStats global_stats;    ///< Global statistics
    
    // Memory pools for common sizes
    KryonMemoryPool *small_pools[16]; ///< Pools for sizes 8, 16, 32, ..., 64KB
    KryonMemoryPool *element_pool;    ///< Pool for UI elements
    KryonMemoryPool *string_pool;     ///< Pool for strings
    
    // Large allocation tracking
    KryonAllocation *allocations;     ///< Linked list of all allocations
    void *allocations_mutex;          ///< Mutex for allocation list
    
    // System integration
    void *heap_start;                 ///< Start of managed heap
    void *heap_end;                   ///< End of managed heap
    void *heap_current;               ///< Current heap pointer
    
    // Debug and profiling
    bool debug_mode;                  ///< Debug mode flag
    FILE *debug_log;                  ///< Debug log file
};

// =============================================================================
// PUBLIC API
// =============================================================================

/**
 * @brief Initialize the memory management system
 * @param config Configuration for memory manager
 * @return Pointer to memory manager instance, or NULL on failure
 */
KryonMemoryManager *kryon_memory_init(const KryonMemoryConfig *config);

/**
 * @brief Shutdown the memory management system
 * @param manager Memory manager instance
 */
void kryon_memory_shutdown(KryonMemoryManager *manager);

/**
 * @brief Allocate memory
 * @param manager Memory manager instance
 * @param size Size in bytes to allocate
 * @param alignment Memory alignment (0 for default)
 * @param file Source file name (for debugging)
 * @param line Source line number (for debugging)
 * @param function Source function name (for debugging)
 * @return Pointer to allocated memory, or NULL on failure
 */
void *kryon_memory_alloc_debug(KryonMemoryManager *manager, size_t size, size_t alignment,
                              const char *file, int line, const char *function);

/**
 * @brief Free memory
 * @param manager Memory manager instance
 * @param ptr Pointer to memory to free
 * @param file Source file name (for debugging)
 * @param line Source line number (for debugging)
 * @param function Source function name (for debugging)
 */
void kryon_memory_free_debug(KryonMemoryManager *manager, void *ptr,
                            const char *file, int line, const char *function);

/**
 * @brief Reallocate memory
 * @param manager Memory manager instance
 * @param ptr Existing pointer (can be NULL)
 * @param size New size in bytes
 * @param file Source file name (for debugging)
 * @param line Source line number (for debugging)
 * @param function Source function name (for debugging)
 * @return Pointer to reallocated memory, or NULL on failure
 */
void *kryon_memory_realloc_debug(KryonMemoryManager *manager, void *ptr, size_t size,
                                const char *file, int line, const char *function);

/**
 * @brief Get memory usage statistics
 * @param manager Memory manager instance
 * @param stats Output statistics structure
 */
void kryon_memory_get_stats(KryonMemoryManager *manager, KryonMemoryStats *stats);

/**
 * @brief Check for memory leaks
 * @param manager Memory manager instance
 * @return Number of leaks detected
 */
uint32_t kryon_memory_check_leaks(KryonMemoryManager *manager);

/**
 * @brief Validate heap integrity
 * @param manager Memory manager instance
 * @return true if heap is valid, false if corruption detected
 */
bool kryon_memory_validate_heap(KryonMemoryManager *manager);

/**
 * @brief Compact memory pools to reduce fragmentation
 * @param manager Memory manager instance
 * @return Number of bytes reclaimed
 */
size_t kryon_memory_compact(KryonMemoryManager *manager);

// =============================================================================
// MEMORY POOL API
// =============================================================================

/**
 * @brief Create a memory pool
 * @param config Pool configuration
 * @return Pointer to memory pool, or NULL on failure
 */
KryonMemoryPool *kryon_pool_create(const KryonPoolConfig *config);

/**
 * @brief Destroy a memory pool
 * @param pool Memory pool to destroy
 */
void kryon_pool_destroy(KryonMemoryPool *pool);

/**
 * @brief Allocate from memory pool
 * @param pool Memory pool
 * @return Pointer to allocated chunk, or NULL if pool is full
 */
void *kryon_pool_alloc(KryonMemoryPool *pool);

/**
 * @brief Free back to memory pool
 * @param pool Memory pool
 * @param ptr Pointer to free
 */
void kryon_pool_free(KryonMemoryPool *pool, void *ptr);

/**
 * @brief Check if pointer belongs to pool
 * @param pool Memory pool
 * @param ptr Pointer to check
 * @return true if pointer belongs to pool
 */
bool kryon_pool_owns(KryonMemoryPool *pool, void *ptr);

// =============================================================================
// CONVENIENCE MACROS
// =============================================================================

// Get the global memory manager instance
extern KryonMemoryManager *g_kryon_memory_manager;

// Debug allocation macros
#ifdef KRYON_DEBUG
    #define kryon_alloc(size) \
        kryon_memory_alloc_debug(g_kryon_memory_manager, (size), 0, __FILE__, __LINE__, __FUNCTION__)
    #define kryon_alloc_aligned(size, alignment) \
        kryon_memory_alloc_debug(g_kryon_memory_manager, (size), (alignment), __FILE__, __LINE__, __FUNCTION__)
    #define kryon_free(ptr) \
        kryon_memory_free_debug(g_kryon_memory_manager, (ptr), __FILE__, __LINE__, __FUNCTION__)
    #define kryon_realloc(ptr, size) \
        kryon_memory_realloc_debug(g_kryon_memory_manager, (ptr), (size), __FILE__, __LINE__, __FUNCTION__)
#else
    #define kryon_alloc(size) \
        kryon_memory_alloc_debug(g_kryon_memory_manager, (size), 0, NULL, 0, NULL)
    #define kryon_alloc_aligned(size, alignment) \
        kryon_memory_alloc_debug(g_kryon_memory_manager, (size), (alignment), NULL, 0, NULL)
    #define kryon_free(ptr) \
        kryon_memory_free_debug(g_kryon_memory_manager, (ptr), NULL, 0, NULL)
    #define kryon_realloc(ptr, size) \
        kryon_memory_realloc_debug(g_kryon_memory_manager, (ptr), (size), NULL, 0, NULL)
#endif

// Type-safe allocation macros
#define kryon_alloc_type(type) \
    ((type*)kryon_alloc(sizeof(type)))
#define kryon_alloc_array(type, count) \
    ((type*)kryon_alloc((count) * sizeof(type)))
#define kryon_realloc_array(ptr, type, count) \
    ((type*)kryon_realloc((ptr), (count) * sizeof(type)))

// Memory operation macros
#define kryon_zero(ptr, size) memset((ptr), 0, (size))
#define kryon_copy(dst, src, size) memcpy((dst), (src), (size))
#define kryon_move(dst, src, size) memmove((dst), (src), (size))

// Alignment utilities
#define KRYON_ALIGN_UP(value, alignment) (((value) + (alignment) - 1) & ~((alignment) - 1))
#define KRYON_ALIGN_DOWN(value, alignment) ((value) & ~((alignment) - 1))
#define KRYON_IS_ALIGNED(value, alignment) (((value) & ((alignment) - 1)) == 0)

// Common alignment values
#define KRYON_ALIGN_DEFAULT 8

// =============================================================================
// STRING UTILITIES
// =============================================================================

/**
 * @brief Duplicate a string using Kryon memory allocator
 * @param str String to duplicate
 * @return Newly allocated string copy, or NULL on failure
 */
char *kryon_strdup(const char *str);
#define KRYON_ALIGN_CACHE_LINE 64
#define KRYON_ALIGN_PAGE 4096

// =============================================================================
// COMPATIBILITY MACROS FOR WIDGET SYSTEM
// =============================================================================

// Simple compatibility macros for basic allocation functions
// These can be replaced with the full memory manager later
#include <stdlib.h>
#include <string.h>

#define kryon_malloc(size) malloc(size)
#define kryon_calloc(count, size) calloc(count, size)

#ifdef __cplusplus
}
#endif

#endif // KRYON_INTERNAL_MEMORY_H