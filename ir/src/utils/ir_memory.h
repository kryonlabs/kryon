#ifndef IR_MEMORY_H
#define IR_MEMORY_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

// Forward declarations
typedef struct IRComponent IRComponent;
typedef struct IRComponentPool IRComponentPool;
typedef struct IRArena IRArena;

// ============================================================================
// Component Pool Allocator
// ============================================================================

// Statistics for pool allocator
typedef struct {
    uint32_t total_allocated;
    uint32_t total_freed;
    uint32_t current_in_use;
    uint32_t blocks_allocated;
    uint32_t total_capacity;
} IRPoolStats;

// Create a new component pool
IRComponentPool* ir_pool_create(void);

// Allocate a component from the pool
IRComponent* ir_pool_alloc_component(IRComponentPool* pool);

// Return a component to the pool
void ir_pool_free_component(IRComponentPool* pool, IRComponent* component);

// Destroy the pool and free all memory
void ir_pool_destroy(IRComponentPool* pool);

// Get pool statistics
void ir_pool_get_stats(IRComponentPool* pool, IRPoolStats* stats);

// ============================================================================
// Arena Allocator
// ============================================================================

// Create a new arena with the specified size (or default if size == 0)
IRArena* ir_arena_create(size_t size);

// Create an arena from an existing buffer (arena doesn't own the buffer)
IRArena* ir_arena_create_from_buffer(void* buffer, size_t size);

// Allocate memory from the arena (8-byte aligned)
void* ir_arena_alloc(IRArena* arena, size_t size);

// Allocate memory with custom alignment
void* ir_arena_alloc_aligned(IRArena* arena, size_t size, size_t alignment);

// Reset the arena (doesn't free memory, just resets offset)
void ir_arena_reset(IRArena* arena);

// Get used bytes in arena
size_t ir_arena_get_used(IRArena* arena);

// Get remaining bytes in arena
size_t ir_arena_get_remaining(IRArena* arena);

// Destroy the arena
void ir_arena_destroy(IRArena* arena);

// String allocation helpers
char* ir_arena_strdup(IRArena* arena, const char* str);
char* ir_arena_strndup(IRArena* arena, const char* str, size_t max_len);

#endif // IR_MEMORY_H
