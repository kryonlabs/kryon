#include "ir_expression_cache.h"
#include "../include/ir_expression.h"
#include "../include/ir_log.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// ============================================================================
// HASH FUNCTIONS
// ============================================================================

// Combine two hashes (similar to Boost's hash_combine)
static uint64_t hash_combine(uint64_t a, uint64_t b) {
    return a ^ (b + 0x9e3779b9ull + (a << 6) + (a >> 2));
}

// String hash (FNV-1a) - same as ir_hash_string but static here
static uint64_t hash_string(const char* str) {
    if (!str) return 0;

    uint64_t hash = 14695981039346656037ULL;
    while (*str) {
        hash ^= (uint64_t)(unsigned char)*str++;
        hash *= 1099511628211ULL;
    }
    return hash;
}

// Get next power of 2 (same as ir_next_power_of_2 but static here)
static uint32_t next_power_of_2(uint32_t value) {
    if (value == 0) return 1;

    value--;
    value |= value >> 1;
    value |= value >> 2;
    value |= value >> 4;
    value |= value >> 8;
    value |= value >> 16;
    value++;

    return value;
}

// Forward declaration
static uint64_t expr_get_key(IRExpression* expr);

// Generate cache key from expression (structural hash)
static uint64_t expr_get_key(IRExpression* expr) {
    if (!expr) return 0;

    uint64_t hash = (uint64_t)expr->type;

    switch (expr->type) {
        case EXPR_LITERAL_INT:
            hash = hash_combine(hash, (uint64_t)expr->int_value);
            break;

        case EXPR_LITERAL_FLOAT: {
            // Hash float as bytes
            uint64_t float_bits;
            memcpy(&float_bits, &expr->float_value, sizeof(double));
            hash = hash_combine(hash, float_bits);
            break;
        }

        case EXPR_LITERAL_STRING:
            hash = hash_combine(hash, hash_string(expr->string_value));
            break;

        case EXPR_LITERAL_BOOL:
            hash = hash_combine(hash, (uint64_t)expr->bool_value);
            break;

        case EXPR_LITERAL_NULL:
            // No additional data
            break;

        case EXPR_VAR_REF:
            hash = hash_combine(hash, hash_string(expr->var_ref.name));
            break;

        case EXPR_MEMBER_ACCESS:
            hash = hash_combine(hash, expr_get_key(expr->member_access.object));
            hash = hash_combine(hash, hash_string(expr->member_access.property));
            break;

        case EXPR_COMPUTED_MEMBER:
            hash = hash_combine(hash, expr_get_key(expr->computed_member.object));
            hash = hash_combine(hash, expr_get_key(expr->computed_member.key));
            break;

        case EXPR_METHOD_CALL:
            hash = hash_combine(hash, expr_get_key(expr->method_call.receiver));
            hash = hash_combine(hash, hash_string(expr->method_call.method_name));
            // Hash args count
            hash = hash_combine(hash, (uint64_t)expr->method_call.arg_count);
            for (int i = 0; i < expr->method_call.arg_count; i++) {
                hash = hash_combine(hash, expr_get_key(expr->method_call.args[i]));
            }
            break;

        case EXPR_BINARY:
            hash = hash_combine(hash, (uint64_t)expr->binary.op);
            hash = hash_combine(hash, expr_get_key(expr->binary.left));
            hash = hash_combine(hash, expr_get_key(expr->binary.right));
            break;

        case EXPR_UNARY:
            hash = hash_combine(hash, (uint64_t)expr->unary.op);
            hash = hash_combine(hash, expr_get_key(expr->unary.operand));
            break;

        case EXPR_TERNARY:
            hash = hash_combine(hash, expr_get_key(expr->ternary.condition));
            hash = hash_combine(hash, expr_get_key(expr->ternary.then_expr));
            hash = hash_combine(hash, expr_get_key(expr->ternary.else_expr));
            break;

        case EXPR_GROUP:
            hash = hash_combine(hash, expr_get_key(expr->group.inner));
            break;

        case EXPR_INDEX:
            hash = hash_combine(hash, expr_get_key(expr->index_access.array));
            hash = hash_combine(hash, expr_get_key(expr->index_access.index));
            break;

        case EXPR_CALL: {
            // Function call
            hash = hash_combine(hash, hash_string(expr->call.function));
            hash = hash_combine(hash, (uint64_t)expr->call.arg_count);
            for (int i = 0; i < expr->call.arg_count; i++) {
                hash = hash_combine(hash, expr_get_key(expr->call.args[i]));
            }
            break;
        }

        default:
            break;
    }

    return hash;
}

// ============================================================================
// DEPENDENCY EXTRACTION
// ============================================================================

// Forward declaration
static void collect_var_dependencies(IRExpression* expr, IRExprCacheEntry* entry, uint32_t* index);

// Count variable dependencies in expression
static uint32_t count_var_dependencies(IRExpression* expr) {
    if (!expr) return 0;

    switch (expr->type) {
        case EXPR_VAR_REF:
            return 1;

        case EXPR_MEMBER_ACCESS:
            return count_var_dependencies(expr->member_access.object);

        case EXPR_COMPUTED_MEMBER:
            return count_var_dependencies(expr->computed_member.object) +
                   count_var_dependencies(expr->computed_member.key);

        case EXPR_METHOD_CALL: {
            uint32_t count = count_var_dependencies(expr->method_call.receiver);
            for (int i = 0; i < expr->method_call.arg_count; i++) {
                count += count_var_dependencies(expr->method_call.args[i]);
            }
            return count;
        }

        case EXPR_BINARY:
            return count_var_dependencies(expr->binary.left) +
                   count_var_dependencies(expr->binary.right);

        case EXPR_UNARY:
            return count_var_dependencies(expr->unary.operand);

        case EXPR_TERNARY:
            return count_var_dependencies(expr->ternary.condition) +
                   count_var_dependencies(expr->ternary.then_expr) +
                   count_var_dependencies(expr->ternary.else_expr);

        case EXPR_GROUP:
            return count_var_dependencies(expr->group.inner);

        case EXPR_INDEX:
            return count_var_dependencies(expr->index_access.array) +
                   count_var_dependencies(expr->index_access.index);

        case EXPR_CALL: {
            uint32_t count = 0;
            for (int i = 0; i < expr->call.arg_count; i++) {
                count += count_var_dependencies(expr->call.args[i]);
            }
            return count;
        }

        default:
            return 0;
    }
}

// Collect variable dependencies from expression
static void collect_var_dependencies(IRExpression* expr, IRExprCacheEntry* entry, uint32_t* index) {
    if (!expr) return;

    switch (expr->type) {
        case EXPR_VAR_REF:
            if (*index < entry->var_dep_count) {
                entry->var_dependencies[(*index)++] = hash_string(expr->var_ref.name);
            }
            break;

        case EXPR_MEMBER_ACCESS:
            collect_var_dependencies(expr->member_access.object, entry, index);
            break;

        case EXPR_COMPUTED_MEMBER:
            collect_var_dependencies(expr->computed_member.object, entry, index);
            collect_var_dependencies(expr->computed_member.key, entry, index);
            break;

        case EXPR_METHOD_CALL:
            collect_var_dependencies(expr->method_call.receiver, entry, index);
            for (int i = 0; i < expr->method_call.arg_count; i++) {
                collect_var_dependencies(expr->method_call.args[i], entry, index);
            }
            break;

        case EXPR_BINARY:
            collect_var_dependencies(expr->binary.left, entry, index);
            collect_var_dependencies(expr->binary.right, entry, index);
            break;

        case EXPR_UNARY:
            collect_var_dependencies(expr->unary.operand, entry, index);
            break;

        case EXPR_TERNARY:
            collect_var_dependencies(expr->ternary.condition, entry, index);
            collect_var_dependencies(expr->ternary.then_expr, entry, index);
            collect_var_dependencies(expr->ternary.else_expr, entry, index);
            break;

        case EXPR_GROUP:
            collect_var_dependencies(expr->group.inner, entry, index);
            break;

        case EXPR_INDEX:
            collect_var_dependencies(expr->index_access.array, entry, index);
            collect_var_dependencies(expr->index_access.index, entry, index);
            break;

        case EXPR_CALL:
            for (int i = 0; i < expr->call.arg_count; i++) {
                collect_var_dependencies(expr->call.args[i], entry, index);
            }
            break;

        default:
            break;
    }
}

// Extract variable dependencies from expression
static void extract_var_dependencies(IRExpression* expr, IRExprCacheEntry* entry) {
    uint32_t count = count_var_dependencies(expr);

    if (count == 0) {
        entry->var_dependencies = NULL;
        entry->var_dep_count = 0;
        return;
    }

    entry->var_dependencies = (uint64_t*)calloc(count, sizeof(uint64_t));
    entry->var_dep_count = count;

    uint32_t index = 0;
    collect_var_dependencies(expr, entry, &index);
}

// ============================================================================
// LRU LIST OPERATIONS
// ============================================================================

// Move entry to front of LRU list
static void lru_move_to_front(IRExprCache* cache, IRExprCacheEntry* entry) {
    if (!cache || !entry) return;
    if (cache->lru_head == entry) {
        return;  // Already at front
    }

    // Remove from current position
    if (entry->lru_prev) {
        entry->lru_prev->lru_next = entry->lru_next;
    }
    if (entry->lru_next) {
        entry->lru_next->lru_prev = entry->lru_prev;
    }
    if (cache->lru_tail == entry) {
        cache->lru_tail = entry->lru_prev;
    }

    // Add to front
    entry->lru_prev = NULL;
    entry->lru_next = cache->lru_head;

    if (cache->lru_head) {
        cache->lru_head->lru_prev = entry;
    }
    cache->lru_head = entry;

    if (!cache->lru_tail) {
        cache->lru_tail = entry;
    }
}

// Evict least recently used entry
static void evict_lru(IRExprCache* cache) {
    if (!cache || !cache->lru_tail) return;

    IRExprCacheEntry* to_evict = cache->lru_tail;

    // Remove from LRU list
    if (to_evict->lru_prev) {
        to_evict->lru_prev->lru_next = NULL;
    }
    cache->lru_tail = to_evict->lru_prev;

    if (!cache->lru_tail) {
        cache->lru_head = NULL;
    }

    // Remove from hash table
    uint32_t bucket = to_evict->key_hash & (cache->bucket_count - 1);

    IRExprCacheEntry* entry = cache->buckets[bucket];
    IRExprCacheEntry* prev = NULL;

    while (entry) {
        if (entry == to_evict) {
            if (prev) {
                prev->bucket_next = entry->bucket_next;
            } else {
                cache->buckets[bucket] = entry->bucket_next;
            }
            break;
        }
        prev = entry;
        entry = entry->bucket_next;
    }

    cache->evictions++;
    cache->entry_count--;

    ir_expr_cache_entry_free(to_evict);
}

// ============================================================================
// CACHE LIFECYCLE
// ============================================================================

// Create a new expression cache
IRExprCache* ir_expr_cache_create(uint32_t max_entries) {
    IRExprCache* cache = (IRExprCache*)calloc(1, sizeof(IRExprCache));
    if (!cache) return NULL;

    // Use next power of 2 for bucket count
    cache->bucket_count = next_power_of_2(max_entries / 4);
    if (cache->bucket_count < 16) cache->bucket_count = 16;

    cache->buckets = (IRExprCacheEntry**)calloc(cache->bucket_count, sizeof(IRExprCacheEntry*));
    if (!cache->buckets) {
        free(cache);
        return NULL;
    }

    cache->max_entries = max_entries;
    cache->entry_count = 0;
    cache->hits = 0;
    cache->misses = 0;
    cache->evictions = 0;
    cache->timestamp = 0;
    cache->lru_head = NULL;
    cache->lru_tail = NULL;

    return cache;
}

// Free a single cache entry
void ir_expr_cache_entry_free(IRExprCacheEntry* entry) {
    if (!entry) return;

    free(entry->source_expr);

    if (entry->compiled) {
        ir_expr_compiled_free(entry->compiled);
    }

    if (entry->has_cached_result) {
        ir_value_free(&entry->cached_result);
    }

    free(entry->var_dependencies);
    free(entry);
}

// Destroy cache and free all entries
void ir_expr_cache_destroy(IRExprCache* cache) {
    if (!cache) return;

    // Free all entries by traversing the LRU list
    // (which contains all entries, unlike bucket chains which may be broken)
    IRExprCacheEntry* entry = cache->lru_head;
    while (entry) {
        IRExprCacheEntry* next = entry->lru_next;
        ir_expr_cache_entry_free(entry);
        entry = next;
    }

    // Clear buckets (they may have dangling pointers)
    for (uint32_t i = 0; i < cache->bucket_count; i++) {
        cache->buckets[i] = NULL;
    }

    free(cache->buckets);
    free(cache);
}

// ============================================================================
// CACHE OPERATIONS
// ============================================================================

// Lookup expression in cache
IRCompiledExpr* ir_expr_cache_lookup(IRExprCache* cache, IRExpression* expr) {
    if (!cache || !expr) return NULL;

    uint64_t key = expr_get_key(expr);
    uint32_t bucket = key & (cache->bucket_count - 1);

    // Search bucket
    IRExprCacheEntry* entry = cache->buckets[bucket];
    while (entry) {
        if (entry->key_hash == key) {
            // Found - move to front of LRU
            lru_move_to_front(cache, entry);

            // Update access tracking
            cache->timestamp++;
            entry->last_access = cache->timestamp;
            entry->access_count++;

            cache->hits++;
            return entry->compiled;
        }
        entry = entry->bucket_next;
    }

    cache->misses++;
    return NULL;
}

// Get cached result if available
IRValue* ir_expr_cache_get_result(IRExprCache* cache, IRExpression* expr) {
    if (!cache || !expr) return NULL;

    uint64_t key = expr_get_key(expr);
    uint32_t bucket = key & (cache->bucket_count - 1);

    IRExprCacheEntry* entry = cache->buckets[bucket];
    while (entry) {
        if (entry->key_hash == key) {
            if (entry->has_cached_result) {
                lru_move_to_front(cache, entry);

                cache->timestamp++;
                entry->last_access = cache->timestamp;
                entry->access_count++;
                cache->hits++;

                return &entry->cached_result;
            }
            // Found entry but no cached result
            cache->hits++;
            return NULL;
        }
        entry = entry->bucket_next;
    }

    cache->misses++;
    return NULL;
}

// Store result in cache
void ir_expr_cache_store_result(IRExprCache* cache, IRExpression* expr, IRValue result) {
    if (!cache || !expr) return;

    uint64_t key = expr_get_key(expr);
    uint32_t bucket = key & (cache->bucket_count - 1);

    IRExprCacheEntry* entry = cache->buckets[bucket];
    while (entry) {
        if (entry->key_hash == key) {
            // Found entry - store result
            if (entry->has_cached_result) {
                ir_value_free(&entry->cached_result);
            }

            entry->cached_result = ir_value_copy(&result);
            entry->has_cached_result = true;
            return;
        }
        entry = entry->bucket_next;
    }
}

// Insert compiled expression into cache
void ir_expr_cache_insert(IRExprCache* cache, IRExpression* expr, IRCompiledExpr* compiled) {
    if (!cache || !expr || !compiled) return;

    uint64_t key = expr_get_key(expr);
    uint32_t bucket = key & (cache->bucket_count - 1);

    // Check if already exists
    IRExprCacheEntry* entry = cache->buckets[bucket];
    while (entry) {
        if (entry->key_hash == key) {
            // Already exists - update compiled if different
            if (entry->compiled != compiled) {
                ir_expr_compiled_free(entry->compiled);
                entry->compiled = compiled;
            }
            lru_move_to_front(cache, entry);

            cache->timestamp++;
            entry->last_access = cache->timestamp;
            entry->access_count++;

            return;
        }
        entry = entry->bucket_next;
    }

    // Create new entry
    entry = (IRExprCacheEntry*)calloc(1, sizeof(IRExprCacheEntry));
    if (!entry) {
        // Can't allocate, free the compiled expression
        ir_expr_compiled_free(compiled);
        return;
    }

    entry->key_hash = key;
    entry->compiled = compiled;
    entry->has_cached_result = false;

    cache->timestamp++;
    entry->last_access = cache->timestamp;
    entry->access_count = 1;

    // Extract variable dependencies for invalidation
    extract_var_dependencies(expr, entry);

    // Evict if necessary
    if (cache->entry_count >= cache->max_entries) {
        evict_lru(cache);
    }

    // Add to bucket (at head) - singly linked list
    entry->bucket_next = cache->buckets[bucket];
    cache->buckets[bucket] = entry;

    // Add to LRU list (at front) - doubly linked list
    entry->lru_prev = NULL;
    entry->lru_next = cache->lru_head;

    if (cache->lru_head) {
        cache->lru_head->lru_prev = entry;
    }
    cache->lru_head = entry;

    if (!cache->lru_tail) {
        cache->lru_tail = entry;
    }

    cache->entry_count++;
}

// ============================================================================
// CACHE INVALIDATION
// ============================================================================

// Invalidate all cached results that depend on a specific variable
void ir_expr_cache_invalidate_var(IRExprCache* cache, const char* var_name) {
    if (!cache || !var_name) return;

    uint64_t var_hash = hash_string(var_name);

    for (uint32_t i = 0; i < cache->bucket_count; i++) {
        IRExprCacheEntry* entry = cache->buckets[i];
        while (entry) {
            IRExprCacheEntry* next = entry->bucket_next;

            // Check if this entry depends on the variable
            for (uint32_t j = 0; j < entry->var_dep_count; j++) {
                if (entry->var_dependencies[j] == var_hash) {
                    // Clear cached result
                    if (entry->has_cached_result) {
                        ir_value_free(&entry->cached_result);
                        entry->has_cached_result = false;
                    }
                    break;
                }
            }

            entry = next;
        }
    }
}

// Invalidate all cached results (but keep compiled bytecode)
void ir_expr_cache_invalidate_all(IRExprCache* cache) {
    if (!cache) return;

    for (uint32_t i = 0; i < cache->bucket_count; i++) {
        IRExprCacheEntry* entry = cache->buckets[i];
        while (entry) {
            if (entry->has_cached_result) {
                ir_value_free(&entry->cached_result);
                entry->has_cached_result = false;
            }
            entry = entry->bucket_next;
        }
    }
}

// Clear entire cache (remove all entries)
void ir_expr_cache_clear(IRExprCache* cache) {
    if (!cache) return;

    // Free all entries by traversing the LRU list
    IRExprCacheEntry* entry = cache->lru_head;
    while (entry) {
        IRExprCacheEntry* next = entry->lru_next;
        ir_expr_cache_entry_free(entry);
        entry = next;
    }

    // Clear buckets
    for (uint32_t i = 0; i < cache->bucket_count; i++) {
        cache->buckets[i] = NULL;
    }

    cache->entry_count = 0;
    cache->lru_head = NULL;
    cache->lru_tail = NULL;
}

// ============================================================================
// CACHE STATISTICS
// ============================================================================

// Get cache statistics
void ir_expr_cache_get_stats(IRExprCache* cache, IRExprCacheStats* stats) {
    if (!cache || !stats) return;

    stats->entry_count = cache->entry_count;
    stats->max_entries = cache->max_entries;
    stats->bucket_count = cache->bucket_count;
    stats->hits = cache->hits;
    stats->misses = cache->misses;
    stats->evictions = cache->evictions;

    uint64_t total = cache->hits + cache->misses;
    stats->hit_rate = total > 0 ? (double)cache->hits / total : 0.0;
}

// Print cache statistics
void ir_expr_cache_print_stats(IRExprCache* cache) {
    if (!cache) return;

    IRExprCacheStats stats;
    ir_expr_cache_get_stats(cache, &stats);

    IR_LOG_INFO("CACHE", "Expression Cache Statistics:");
    IR_LOG_INFO("CACHE", "  Entries: %u / %u", stats.entry_count, stats.max_entries);
    IR_LOG_INFO("CACHE", "  Buckets: %u", stats.bucket_count);
    IR_LOG_INFO("CACHE", "  Hits: %llu", (unsigned long long)stats.hits);
    IR_LOG_INFO("CACHE", "  Misses: %llu", (unsigned long long)stats.misses);
    IR_LOG_INFO("CACHE", "  Evictions: %llu", (unsigned long long)stats.evictions);
    IR_LOG_INFO("CACHE", "  Hit Rate: %.2f%%", stats.hit_rate * 100.0);
}

// Reset statistics
void ir_expr_cache_reset_stats(IRExprCache* cache) {
    if (!cache) return;

    cache->hits = 0;
    cache->misses = 0;
    cache->evictions = 0;
}

// ============================================================================
// GLOBAL CACHE INSTANCE
// ============================================================================

static IRExprCache* g_global_cache = NULL;

// Initialize global cache
void ir_expr_cache_global_init(uint32_t max_entries) {
    if (g_global_cache) {
        ir_expr_cache_destroy(g_global_cache);
    }

    g_global_cache = ir_expr_cache_create(max_entries);
}

// Get global cache
IRExprCache* ir_expr_cache_global_get(void) {
    return g_global_cache;
}

// Cleanup global cache
void ir_expr_cache_global_cleanup(void) {
    if (g_global_cache) {
        ir_expr_cache_print_stats(g_global_cache);
        ir_expr_cache_destroy(g_global_cache);
        g_global_cache = NULL;
    }
}

// Check if global cache is initialized
bool ir_expr_cache_global_is_initialized(void) {
    return g_global_cache != NULL;
}
