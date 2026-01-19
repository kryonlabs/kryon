// IR Memory Management - Pool and Arena Allocators
// Reduces malloc/free churn and improves cache locality

#include "../utils/ir_memory.h"
#include "../include/ir_core.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// ============================================================================
// Component Pool Allocator
// ============================================================================

#define COMPONENTS_PER_BLOCK 64

typedef struct IRComponentBlock {
    IRComponent components[COMPONENTS_PER_BLOCK];
    struct IRComponentBlock* next;
    uint32_t allocated_count;  // For debugging/statistics
} IRComponentBlock;

struct IRComponentPool {
    IRComponent* free_list;
    IRComponentBlock* blocks;
    uint32_t total_allocated;
    uint32_t total_freed;
    uint32_t block_count;
};

IRComponentPool* ir_pool_create(void) {
    IRComponentPool* pool = (IRComponentPool*)malloc(sizeof(IRComponentPool));
    if (!pool) return NULL;

    pool->free_list = NULL;
    pool->blocks = NULL;
    pool->total_allocated = 0;
    pool->total_freed = 0;
    pool->block_count = 0;

    return pool;
}

static IRComponentBlock* ir_pool_allocate_block(void) {
    IRComponentBlock* block = (IRComponentBlock*)malloc(sizeof(IRComponentBlock));
    if (!block) return NULL;

    memset(block, 0, sizeof(IRComponentBlock));
    block->next = NULL;
    block->allocated_count = 0;

    // Link all components in the block to the free list
    for (int i = 0; i < COMPONENTS_PER_BLOCK - 1; i++) {
        // Use the 'parent' pointer as the next pointer in the free list
        block->components[i].parent = (IRComponent*)&block->components[i + 1];
    }
    block->components[COMPONENTS_PER_BLOCK - 1].parent = NULL;

    return block;
}

IRComponent* ir_pool_alloc_component(IRComponentPool* pool) {
    if (!pool) return NULL;

    // If free list is empty, allocate a new block
    if (!pool->free_list) {
        IRComponentBlock* new_block = ir_pool_allocate_block();
        if (!new_block) return NULL;

        // Add block to the pool's block list
        new_block->next = pool->blocks;
        pool->blocks = new_block;
        pool->block_count++;

        // Set free list to point to the first component in the new block
        pool->free_list = &new_block->components[0];
    }

    // Pop from free list
    IRComponent* component = pool->free_list;
    pool->free_list = (IRComponent*)component->parent;

    // Zero out the component
    memset(component, 0, sizeof(IRComponent));

    // Initialize cache
    component->layout_cache.dirty = true;
    component->layout_cache.cache_generation = 0;

    pool->total_allocated++;

    return component;
}

void ir_pool_free_component(IRComponentPool* pool, IRComponent* component) {
    if (!pool || !component) return;

    // Clean up component's allocated resources
    // Note: This doesn't free children, events, logic, etc. - that's the caller's responsibility

    // Push onto free list (reuse parent pointer)
    component->parent = (IRComponent*)pool->free_list;
    pool->free_list = component;

    pool->total_freed++;
}

void ir_pool_destroy(IRComponentPool* pool) {
    if (!pool) return;

    // Free all blocks
    IRComponentBlock* block = pool->blocks;
    while (block) {
        IRComponentBlock* next = block->next;
        free(block);
        block = next;
    }

    free(pool);
}

void ir_pool_get_stats(IRComponentPool* pool, IRPoolStats* stats) {
    if (!pool || !stats) return;

    stats->total_allocated = pool->total_allocated;
    stats->total_freed = pool->total_freed;
    stats->current_in_use = pool->total_allocated - pool->total_freed;
    stats->blocks_allocated = pool->block_count;
    stats->total_capacity = pool->block_count * COMPONENTS_PER_BLOCK;
}

// ============================================================================
// Arena Allocator (for temporary layout calculations)
// ============================================================================

#define DEFAULT_ARENA_SIZE (64 * 1024)  // 64 KB default

struct IRArena {
    uint8_t* buffer;
    size_t size;
    size_t offset;
    bool owns_buffer;  // true if arena allocated the buffer
};

IRArena* ir_arena_create(size_t size) {
    IRArena* arena = (IRArena*)malloc(sizeof(IRArena));
    if (!arena) return NULL;

    size_t actual_size = size > 0 ? size : DEFAULT_ARENA_SIZE;

    arena->buffer = (uint8_t*)malloc(actual_size);
    if (!arena->buffer) {
        free(arena);
        return NULL;
    }

    arena->size = actual_size;
    arena->offset = 0;
    arena->owns_buffer = true;

    return arena;
}

IRArena* ir_arena_create_from_buffer(void* buffer, size_t size) {
    if (!buffer || size == 0) return NULL;

    IRArena* arena = (IRArena*)malloc(sizeof(IRArena));
    if (!arena) return NULL;

    arena->buffer = (uint8_t*)buffer;
    arena->size = size;
    arena->offset = 0;
    arena->owns_buffer = false;

    return arena;
}

void* ir_arena_alloc(IRArena* arena, size_t size) {
    if (!arena || size == 0) return NULL;

    // Align to 8 bytes
    size_t aligned_size = (size + 7) & ~7;

    // Check if we have enough space
    if (arena->offset + aligned_size > arena->size) {
        return NULL;
    }

    void* ptr = arena->buffer + arena->offset;
    arena->offset += aligned_size;

    return ptr;
}

void* ir_arena_alloc_aligned(IRArena* arena, size_t size, size_t alignment) {
    if (!arena || size == 0 || alignment == 0) return NULL;

    // Align current offset to the requested alignment
    size_t aligned_offset = (arena->offset + alignment - 1) & ~(alignment - 1);

    // Calculate aligned size
    size_t aligned_size = (size + alignment - 1) & ~(alignment - 1);

    // Check if we have enough space
    if (aligned_offset + aligned_size > arena->size) {
        return NULL;
    }

    void* ptr = arena->buffer + aligned_offset;
    arena->offset = aligned_offset + aligned_size;

    return ptr;
}

void ir_arena_reset(IRArena* arena) {
    if (!arena) return;
    arena->offset = 0;
}

size_t ir_arena_get_used(IRArena* arena) {
    return arena ? arena->offset : 0;
}

size_t ir_arena_get_remaining(IRArena* arena) {
    return arena ? (arena->size - arena->offset) : 0;
}

void ir_arena_destroy(IRArena* arena) {
    if (!arena) return;

    if (arena->owns_buffer && arena->buffer) {
        free(arena->buffer);
    }

    free(arena);
}

// ============================================================================
// String Pool (for efficient string storage)
// ============================================================================

char* ir_arena_strdup(IRArena* arena, const char* str) {
    if (!arena || !str) return NULL;

    size_t len = strlen(str) + 1;
    char* copy = (char*)ir_arena_alloc(arena, len);
    if (!copy) return NULL;

    memcpy(copy, str, len);
    return copy;
}

char* ir_arena_strndup(IRArena* arena, const char* str, size_t max_len) {
    if (!arena || !str) return NULL;

    size_t len = 0;
    while (len < max_len && str[len] != '\0') {
        len++;
    }

    char* copy = (char*)ir_arena_alloc(arena, len + 1);
    if (!copy) return NULL;

    memcpy(copy, str, len);
    copy[len] = '\0';

    return copy;
}
