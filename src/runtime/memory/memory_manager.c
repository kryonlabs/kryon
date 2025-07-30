/**
 * @file memory_manager.c
 * @brief Kryon Memory Management System Implementation
 * 
 * High-performance memory management with pools, tracking, and debugging.
 */

#include "internal/memory.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <time.h>

#ifdef _WIN32
    #include <windows.h>
    #include <process.h>
#else
    #include <pthread.h>
    #include <unistd.h>
    #include <sys/mman.h>
#endif

// =============================================================================
// GLOBAL STATE
// =============================================================================

/// Global memory manager instance
KryonMemoryManager *g_kryon_memory_manager = NULL;

// =============================================================================
// INTERNAL UTILITIES
// =============================================================================

/**
 * @brief Get current timestamp in nanoseconds
 */
static uint64_t get_timestamp_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

/**
 * @brief Get current thread ID
 */
static uint32_t get_current_thread_id(void) {
#ifdef _WIN32
    return (uint32_t)GetCurrentThreadId();
#else
    return (uint32_t)pthread_self();
#endif
}

/**
 * @brief Create a mutex
 */
static void *create_mutex(void) {
#ifdef _WIN32
    CRITICAL_SECTION *cs = malloc(sizeof(CRITICAL_SECTION));
    if (cs) {
        InitializeCriticalSection(cs);
    }
    return cs;
#else
    pthread_mutex_t *mutex = malloc(sizeof(pthread_mutex_t));
    if (mutex) {
        pthread_mutex_init(mutex, NULL);
    }
    return mutex;
#endif
}

/**
 * @brief Destroy a mutex
 */
static void destroy_mutex(void *mutex) {
    if (!mutex) return;
    
#ifdef _WIN32
    DeleteCriticalSection((CRITICAL_SECTION*)mutex);
#else
    pthread_mutex_destroy((pthread_mutex_t*)mutex);
#endif
    free(mutex);
}

/**
 * @brief Lock a mutex
 */
static void lock_mutex(void *mutex) {
    if (!mutex) return;
    
#ifdef _WIN32
    EnterCriticalSection((CRITICAL_SECTION*)mutex);
#else
    pthread_mutex_lock((pthread_mutex_t*)mutex);
#endif
}

/**
 * @brief Unlock a mutex
 */
static void unlock_mutex(void *mutex) {
    if (!mutex) return;
    
#ifdef _WIN32
    LeaveCriticalSection((CRITICAL_SECTION*)mutex);
#else
    pthread_mutex_unlock((pthread_mutex_t*)mutex);
#endif
}

/**
 * @brief Allocate aligned memory from system
 */
static void *system_alloc_aligned(size_t size, size_t alignment) {
    if (alignment == 0) alignment = KRYON_ALIGN_DEFAULT;
    
#ifdef _WIN32
    return _aligned_malloc(size, alignment);
#else
    void *ptr = NULL;
    if (posix_memalign(&ptr, alignment, size) != 0) {
        return NULL;
    }
    return ptr;
#endif
}

/**
 * @brief Free aligned memory to system
 */
static void system_free_aligned(void *ptr) {
    if (!ptr) return;
    
#ifdef _WIN32
    _aligned_free(ptr);
#else
    free(ptr);
#endif
}

// =============================================================================
// MEMORY POOL IMPLEMENTATION
// =============================================================================

KryonMemoryPool *kryon_pool_create(const KryonPoolConfig *config) {
    if (!config || config->chunk_size == 0 || config->max_chunks == 0) {
        return NULL;
    }
    
    KryonMemoryPool *pool = malloc(sizeof(KryonMemoryPool));
    if (!pool) {
        return NULL;
    }
    
    // Initialize pool structure
    memset(pool, 0, sizeof(KryonMemoryPool));
    pool->chunk_size = KRYON_ALIGN_UP(config->chunk_size, config->alignment);
    pool->chunk_count = config->max_chunks;
    pool->free_count = config->max_chunks;
    pool->thread_safe = config->thread_safe;
    
    // Allocate memory block
    size_t total_size = pool->chunk_size * pool->chunk_count;
    pool->memory_block = system_alloc_aligned(total_size, config->alignment);
    if (!pool->memory_block) {
        free(pool);
        return NULL;
    }
    
    // Allocate free list
    pool->free_list = malloc(pool->chunk_count * sizeof(void*));
    if (!pool->free_list) {
        system_free_aligned(pool->memory_block);
        free(pool);
        return NULL;
    }
    
    // Initialize free list - treat as stack
    uint8_t *chunk_ptr = (uint8_t*)pool->memory_block;
    for (size_t i = 0; i < pool->chunk_count; i++) {
        pool->free_list[i] = chunk_ptr + (i * pool->chunk_size);
    }
    
    // Create mutex if thread-safe
    if (pool->thread_safe) {
        pool->mutex = create_mutex();
        if (!pool->mutex) {
            free(pool->free_list);
            system_free_aligned(pool->memory_block);
            free(pool);
            return NULL;
        }
    }
    
    return pool;
}

void kryon_pool_destroy(KryonMemoryPool *pool) {
    if (!pool) return;
    
    if (pool->mutex) {
        destroy_mutex(pool->mutex);
    }
    
    if (pool->free_list) {
        free(pool->free_list);
    }
    
    if (pool->memory_block) {
        system_free_aligned(pool->memory_block);
    }
    
    free(pool);
}

void *kryon_pool_alloc(KryonMemoryPool *pool) {
    if (!pool) return NULL;
    
    if (pool->thread_safe) {
        lock_mutex(pool->mutex);
    }
    
    void *ptr = NULL;
    if (pool->free_count > 0) {
        pool->free_count--;
        ptr = pool->free_list[pool->free_count];
        
        // Update statistics
        pool->stats.alloc_count++;
        pool->stats.total_allocated += pool->chunk_size;
    }
    
    if (pool->thread_safe) {
        unlock_mutex(pool->mutex);
    }
    
    return ptr;
}

void kryon_pool_free(KryonMemoryPool *pool, void *ptr) {
    if (!pool || !ptr) return;
    
    // Verify pointer belongs to this pool
    if (!kryon_pool_owns(pool, ptr)) {
        return; // Silent fail - not our pointer
    }
    
    if (pool->thread_safe) {
        lock_mutex(pool->mutex);
    }
    
    if (pool->free_count < pool->chunk_count) {
        pool->free_list[pool->free_count] = ptr;
        pool->free_count++;
        
        // Update statistics
        pool->stats.free_count++;
        pool->stats.total_allocated -= pool->chunk_size;
    }
    
    if (pool->thread_safe) {
        unlock_mutex(pool->mutex);
    }
}

bool kryon_pool_owns(KryonMemoryPool *pool, void *ptr) {
    if (!pool || !ptr) return false;
    
    uint8_t *start = (uint8_t*)pool->memory_block;
    uint8_t *end = start + (pool->chunk_size * pool->chunk_count);
    uint8_t *check = (uint8_t*)ptr;
    
    if (check < start || check >= end) {
        return false;
    }
    
    // Check if pointer is aligned to chunk boundary
    size_t offset = check - start;
    return (offset % pool->chunk_size) == 0;
}

// =============================================================================
// MEMORY MANAGER IMPLEMENTATION
// =============================================================================

KryonMemoryManager *kryon_memory_init(const KryonMemoryConfig *config) {
    if (!config) return NULL;
    
    KryonMemoryManager *manager = malloc(sizeof(KryonMemoryManager));
    if (!manager) return NULL;
    
    // Initialize manager structure
    memset(manager, 0, sizeof(KryonMemoryManager));
    manager->config = *config;
    
    // Create allocation tracking mutex
    manager->allocations_mutex = create_mutex();
    if (!manager->allocations_mutex) {
        free(manager);
        return NULL;
    }
    
    // Initialize memory pools for common sizes
    KryonPoolConfig pool_config = {
        .alignment = KRYON_ALIGN_DEFAULT,
        .thread_safe = true,
        .track_allocations = config->enable_statistics,
        .max_chunks = 1024
    };
    
    // Create pools for powers of 2: 8, 16, 32, 64, ..., 64KB
    for (int i = 0; i < 16; i++) {
        pool_config.chunk_size = 8 << i; // 8, 16, 32, 64, ...
        manager->small_pools[i] = kryon_pool_create(&pool_config);
        if (!manager->small_pools[i]) {
            // Cleanup on failure
            for (int j = 0; j < i; j++) {
                kryon_pool_destroy(manager->small_pools[j]);
            }
            destroy_mutex(manager->allocations_mutex);
            free(manager);
            return NULL;
        }
    }
    
    // Create specialized pools
    pool_config.chunk_size = 256; // Typical element size
    pool_config.max_chunks = 2048;
    manager->element_pool = kryon_pool_create(&pool_config);
    
    pool_config.chunk_size = 64;  // Small strings
    pool_config.max_chunks = 4096;
    manager->string_pool = kryon_pool_create(&pool_config);
    
    // Initialize heap if not using system malloc
    if (!config->use_system_malloc) {
        manager->heap_start = system_alloc_aligned(config->initial_heap_size, KRYON_ALIGN_PAGE);
        if (!manager->heap_start) {
            kryon_memory_shutdown(manager);
            return NULL;
        }
        manager->heap_end = (uint8_t*)manager->heap_start + config->initial_heap_size;
        manager->heap_current = manager->heap_start;
    }
    
    // Open debug log if in debug mode
    if (config->enable_leak_detection || config->enable_statistics) {
        manager->debug_mode = true;
        manager->debug_log = fopen("kryon_memory_debug.log", "w");
        if (manager->debug_log) {
            fprintf(manager->debug_log, "Kryon Memory Manager initialized\n");
            fprintf(manager->debug_log, "Configuration:\n");
            fprintf(manager->debug_log, "  Initial heap: %zu bytes\n", config->initial_heap_size);
            fprintf(manager->debug_log, "  Max heap: %zu bytes\n", config->max_heap_size);
            fprintf(manager->debug_log, "  Leak detection: %s\n", config->enable_leak_detection ? "enabled" : "disabled");
            fprintf(manager->debug_log, "  Bounds checking: %s\n", config->enable_bounds_checking ? "enabled" : "disabled");
            fflush(manager->debug_log);
        }
    }
    
    return manager;
}

void kryon_memory_shutdown(KryonMemoryManager *manager) {
    if (!manager) return;
    
    // Check for leaks before shutdown
    if (manager->config.enable_leak_detection) {
        uint32_t leaks = kryon_memory_check_leaks(manager);
        if (leaks > 0 && manager->debug_log) {
            fprintf(manager->debug_log, "WARNING: %u memory leaks detected at shutdown\n", leaks);
        }
    }
    
    // Print final statistics
    if (manager->debug_log) {
        KryonMemoryStats stats;
        kryon_memory_get_stats(manager, &stats);
        fprintf(manager->debug_log, "Final memory statistics:\n");
        fprintf(manager->debug_log, "  Total allocated: %zu bytes\n", stats.total_allocated);
        fprintf(manager->debug_log, "  Peak allocated: %zu bytes\n", stats.peak_allocated);
        fprintf(manager->debug_log, "  Allocation count: %llu\n", (unsigned long long)stats.alloc_count);
        fprintf(manager->debug_log, "  Free count: %llu\n", (unsigned long long)stats.free_count);
        fprintf(manager->debug_log, "  Leak count: %u\n", stats.leak_count);
        fclose(manager->debug_log);
    }
    
    // Destroy memory pools
    for (int i = 0; i < 16; i++) {
        kryon_pool_destroy(manager->small_pools[i]);
    }
    kryon_pool_destroy(manager->element_pool);
    kryon_pool_destroy(manager->string_pool);
    
    // Free heap
    if (manager->heap_start) {
        system_free_aligned(manager->heap_start);
    }
    
    // Free allocation tracking
    KryonAllocation *alloc = manager->allocations;
    while (alloc) {
        KryonAllocation *next = alloc->next;
        free(alloc);
        alloc = next;
    }
    
    // Destroy mutex
    destroy_mutex(manager->allocations_mutex);
    
    // Free manager
    free(manager);
}

void *kryon_memory_alloc_debug(KryonMemoryManager *manager, size_t size, size_t alignment,
                              const char *file, int line, const char *function) {
    if (!manager || size == 0) return NULL;
    
    uint64_t start_time = get_timestamp_ns();
    
    if (alignment == 0) alignment = KRYON_ALIGN_DEFAULT;
    size_t aligned_size = KRYON_ALIGN_UP(size, alignment);
    
    void *ptr = NULL;
    
    // Try to allocate from appropriate pool first
    if (aligned_size <= 65536) { // 64KB
        int pool_index = 0;
        size_t pool_size = 8;
        while (pool_size < aligned_size && pool_index < 16) {
            pool_size <<= 1;
            pool_index++;
        }
        
        if (pool_index < 16) {
            ptr = kryon_pool_alloc(manager->small_pools[pool_index]);
        }
    }
    
    // Fall back to system allocation for large allocations or pool failure
    if (!ptr) {
        if (manager->config.use_system_malloc || aligned_size >= manager->config.large_alloc_threshold) {
            ptr = system_alloc_aligned(aligned_size, alignment);
        } else {
            // Allocate from managed heap
            lock_mutex(manager->allocations_mutex);
            
            if (manager->heap_current && 
                (uint8_t*)manager->heap_current + aligned_size <= (uint8_t*)manager->heap_end) {
                ptr = manager->heap_current;
                manager->heap_current = (uint8_t*)manager->heap_current + aligned_size;
            }
            
            unlock_mutex(manager->allocations_mutex);
        }
    }
    
    if (!ptr) {
        manager->global_stats.out_of_memory_count++;
        return NULL;
    }
    
    // Track allocation if enabled
    if (manager->config.enable_leak_detection || manager->config.enable_statistics) {
        KryonAllocation *alloc_info = malloc(sizeof(KryonAllocation));
        if (alloc_info) {
            alloc_info->ptr = ptr;
            alloc_info->size = aligned_size;
            alloc_info->alignment = alignment;
            alloc_info->alloc_time = start_time;
            alloc_info->file = file;
            alloc_info->line = line;
            alloc_info->function = function;
            alloc_info->thread_id = get_current_thread_id();
            
            lock_mutex(manager->allocations_mutex);
            alloc_info->next = manager->allocations;
            manager->allocations = alloc_info;
            unlock_mutex(manager->allocations_mutex);
        }
    }
    
    // Update statistics
    manager->global_stats.total_allocated += aligned_size;
    manager->global_stats.alloc_count++;
    if (manager->global_stats.total_allocated > manager->global_stats.peak_allocated) {
        manager->global_stats.peak_allocated = manager->global_stats.total_allocated;
    }
    
    uint64_t end_time = get_timestamp_ns();
    manager->global_stats.total_alloc_time_ns += (end_time - start_time);
    
    // Clear memory if in debug mode
    if (manager->debug_mode) {
        memset(ptr, 0xCD, aligned_size); // Debug pattern
    }
    
    // Log allocation if debug logging enabled
    if (manager->debug_log && file) {
        fprintf(manager->debug_log, "ALLOC: %p size=%zu align=%zu %s:%d %s()\n",
                ptr, size, alignment, file, line, function ? function : "unknown");
        fflush(manager->debug_log);
    }
    
    return ptr;
}

void kryon_memory_free_debug(KryonMemoryManager *manager, void *ptr,
                            const char *file, int line, const char *function) {
    if (!manager || !ptr) return;
    
    uint64_t start_time = get_timestamp_ns();
    
    // Find and remove allocation tracking
    size_t freed_size = 0;
    bool found = false;
    
    if (manager->config.enable_leak_detection || manager->config.enable_statistics) {
        lock_mutex(manager->allocations_mutex);
        
        KryonAllocation **current = &manager->allocations;
        while (*current) {
            if ((*current)->ptr == ptr) {
                KryonAllocation *to_free = *current;
                *current = (*current)->next;
                freed_size = to_free->size;
                free(to_free);
                found = true;
                break;
            }
            current = &(*current)->next;
        }
        
        unlock_mutex(manager->allocations_mutex);
        
        if (!found) {
            manager->global_stats.double_free_count++;
            if (manager->debug_log) {
                fprintf(manager->debug_log, "ERROR: Double free detected: %p %s:%d %s()\n",
                        ptr, file ? file : "unknown", line, function ? function : "unknown");
                fflush(manager->debug_log);
            }
            return;
        }
    }
    
    // Try to free to appropriate pool first
    bool freed_to_pool = false;
    for (int i = 0; i < 16; i++) {
        if (kryon_pool_owns(manager->small_pools[i], ptr)) {
            kryon_pool_free(manager->small_pools[i], ptr);
            freed_to_pool = true;
            break;
        }
    }
    
    if (!freed_to_pool) {
        if (kryon_pool_owns(manager->element_pool, ptr)) {
            kryon_pool_free(manager->element_pool, ptr);
            freed_to_pool = true;
        } else if (kryon_pool_owns(manager->string_pool, ptr)) {
            kryon_pool_free(manager->string_pool, ptr);
            freed_to_pool = true;
        }
    }
    
    // If not from pool, free to system
    if (!freed_to_pool) {
        system_free_aligned(ptr);
    }
    
    // Update statistics
    manager->global_stats.total_allocated -= freed_size;
    manager->global_stats.free_count++;
    
    uint64_t end_time = get_timestamp_ns();
    manager->global_stats.total_free_time_ns += (end_time - start_time);
    
    // Log free if debug logging enabled
    if (manager->debug_log && file) {
        fprintf(manager->debug_log, "FREE:  %p size=%zu %s:%d %s()\n",
                ptr, freed_size, file, line, function ? function : "unknown");
        fflush(manager->debug_log);
    }
}

void *kryon_memory_realloc_debug(KryonMemoryManager *manager, void *ptr, size_t size,
                                const char *file, int line, const char *function) {
    if (!manager) return NULL;
    
    if (!ptr) {
        return kryon_memory_alloc_debug(manager, size, 0, file, line, function);
    }
    
    if (size == 0) {
        kryon_memory_free_debug(manager, ptr, file, line, function);
        return NULL;
    }
    
    // Allocate new memory
    void *new_ptr = kryon_memory_alloc_debug(manager, size, 0, file, line, function);
    if (!new_ptr) {
        return NULL;
    }
    
    // Find old allocation size for copy
    size_t old_size = 0;
    if (manager->config.enable_leak_detection || manager->config.enable_statistics) {
        lock_mutex(manager->allocations_mutex);
        
        KryonAllocation *alloc = manager->allocations;
        while (alloc) {
            if (alloc->ptr == ptr) {
                old_size = alloc->size;
                break;
            }
            alloc = alloc->next;
        }
        
        unlock_mutex(manager->allocations_mutex);
    }
    
    // Copy data
    if (old_size > 0) {
        size_t copy_size = (old_size < size) ? old_size : size;
        memcpy(new_ptr, ptr, copy_size);
    }
    
    // Free old memory
    kryon_memory_free_debug(manager, ptr, file, line, function);
    
    manager->global_stats.realloc_count++;
    
    return new_ptr;
}

void kryon_memory_get_stats(KryonMemoryManager *manager, KryonMemoryStats *stats) {
    if (!manager || !stats) return;
    
    *stats = manager->global_stats;
    
    // Add pool overhead
    stats->pool_overhead = 0;
    for (int i = 0; i < 16; i++) {
        if (manager->small_pools[i]) {
            stats->pool_overhead += manager->small_pools[i]->chunk_count * manager->small_pools[i]->chunk_size;
        }
    }
    
    // Estimate fragmentation
    stats->fragmentation = stats->total_allocated > 0 ? 
        (stats->pool_overhead * 100) / stats->total_allocated : 0;
}

uint32_t kryon_memory_check_leaks(KryonMemoryManager *manager) {
    if (!manager) return 0;
    
    uint32_t leak_count = 0;
    
    lock_mutex(manager->allocations_mutex);
    
    KryonAllocation *alloc = manager->allocations;
    while (alloc) {
        leak_count++;
        
        if (manager->debug_log) {
            fprintf(manager->debug_log, "LEAK: %p size=%zu allocated at %s:%d %s()\n",
                    alloc->ptr, alloc->size, 
                    alloc->file ? alloc->file : "unknown",
                    alloc->line,
                    alloc->function ? alloc->function : "unknown");
        }
        
        alloc = alloc->next;
    }
    
    unlock_mutex(manager->allocations_mutex);
    
    manager->global_stats.leak_count = leak_count;
    
    if (manager->debug_log) {
        fflush(manager->debug_log);
    }
    
    return leak_count;
}

bool kryon_memory_validate_heap(KryonMemoryManager *manager) {
    if (!manager) return false;
    
    // Basic heap validation
    if (manager->heap_start && manager->heap_end && manager->heap_current) {
        if (manager->heap_current < manager->heap_start || 
            manager->heap_current > manager->heap_end) {
            return false;
        }
    }
    
    // TODO: More sophisticated heap validation
    // - Check for corruption patterns
    // - Validate allocation headers
    // - Check free list integrity
    
    return true;
}

size_t kryon_memory_compact(KryonMemoryManager *manager) {
    if (!manager) return 0;
    
    // TODO: Implement memory compaction
    // - Defragment memory pools
    // - Compact heap
    // - Update pointers if needed
    
    return 0;
}