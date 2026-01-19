#ifndef IR_EXPRESSION_CACHE_H
#define IR_EXPRESSION_CACHE_H

#include "../include/ir_expression_compiler.h"
#include <stdint.h>
#include <stdbool.h>

// ============================================================================
// CACHE ENTRY STRUCTURE
// ============================================================================

// Forward declaration
typedef struct IRExprCacheEntry IRExprCacheEntry;

// Cache entry for a compiled expression
struct IRExprCacheEntry {
    // Key
    uint64_t key_hash;             // Hash of expression structure
    char* source_expr;             // Original expression string (optional, for debugging)

    // Compiled bytecode
    IRCompiledExpr* compiled;

    // Cached result (for pure expressions only)
    IRValue cached_result;
    bool has_cached_result;

    // LRU tracking
    uint64_t last_access;          // Timestamp of last access
    uint64_t access_count;         // Number of accesses

    // Dependencies (for invalidation)
    uint64_t* var_dependencies;    // Hashes of variables this expression depends on
    uint32_t var_dep_count;

    // Linked list for LRU (doubly-linked)
    IRExprCacheEntry* lru_prev;
    IRExprCacheEntry* lru_next;

    // Hash table bucket chain (singly-linked)
    IRExprCacheEntry* bucket_next;
};

// ============================================================================
// LRU CACHE STRUCTURE
// ============================================================================

typedef struct {
    IRExprCacheEntry** buckets;    // Hash table buckets
    uint32_t bucket_count;         // Number of buckets (power of 2)

    IRExprCacheEntry* lru_head;    // Most recently used
    IRExprCacheEntry* lru_tail;    // Least recently used

    uint32_t entry_count;          // Total entries
    uint32_t max_entries;          // Maximum entries before eviction

    // Statistics
    uint64_t hits;
    uint64_t misses;
    uint64_t evictions;

    // Global timestamp counter for LRU
    uint64_t timestamp;
} IRExprCache;

// ============================================================================
// CACHE STATISTICS
// ============================================================================

typedef struct {
    uint32_t entry_count;
    uint32_t max_entries;
    uint32_t bucket_count;
    uint64_t hits;
    uint64_t misses;
    uint64_t evictions;
    double hit_rate;
} IRExprCacheStats;

// ============================================================================
// CACHE LIFECYCLE
// ============================================================================

// Create a new expression cache
IRExprCache* ir_expr_cache_create(uint32_t max_entries);

// Destroy cache and free all entries
void ir_expr_cache_destroy(IRExprCache* cache);

// Free a single cache entry
void ir_expr_cache_entry_free(IRExprCacheEntry* entry);

// ============================================================================
// CACHE OPERATIONS
// ============================================================================

// Lookup expression in cache, returning compiled bytecode if found
// Returns NULL if not found
IRCompiledExpr* ir_expr_cache_lookup(IRExprCache* cache, IRExpression* expr);

// Get cached result if available (for pure expressions)
// Returns NULL if not found or no cached result
IRValue* ir_expr_cache_get_result(IRExprCache* cache, IRExpression* expr);

// Store result in cache (only for pure expressions)
void ir_expr_cache_store_result(IRExprCache* cache, IRExpression* expr, IRValue result);

// Insert compiled expression into cache
// Takes ownership of the compiled expression
void ir_expr_cache_insert(IRExprCache* cache, IRExpression* expr, IRCompiledExpr* compiled);

// ============================================================================
// CACHE INVALIDATION
// ============================================================================

// Invalidate all cached results that depend on a specific variable
void ir_expr_cache_invalidate_var(IRExprCache* cache, const char* var_name);

// Invalidate all cached results (but keep compiled bytecode)
void ir_expr_cache_invalidate_all(IRExprCache* cache);

// Clear entire cache (remove all entries)
void ir_expr_cache_clear(IRExprCache* cache);

// ============================================================================
// CACHE STATISTICS
// ============================================================================

// Get cache statistics
void ir_expr_cache_get_stats(IRExprCache* cache, IRExprCacheStats* stats);

// Print cache statistics (for debugging)
void ir_expr_cache_print_stats(IRExprCache* cache);

// Reset statistics
void ir_expr_cache_reset_stats(IRExprCache* cache);

// ============================================================================
// GLOBAL CACHE INSTANCE
// ============================================================================

// Initialize global cache
void ir_expr_cache_global_init(uint32_t max_entries);

// Get global cache (returns NULL if not initialized)
IRExprCache* ir_expr_cache_global_get(void);

// Cleanup global cache
void ir_expr_cache_global_cleanup(void);

// Check if global cache is initialized
bool ir_expr_cache_global_is_initialized(void);

#endif // IR_EXPRESSION_CACHE_H
