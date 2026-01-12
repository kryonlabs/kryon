# Phase 5: Expression Cache

**Duration**: 3 days
**Dependencies**: Phase 4
**Status**: Pending

## Overview

Phase 5 implements an LRU (Least Recently Used) cache for compiled expressions. This avoids recompiling the same expressions repeatedly and enables caching of expression results for pure expressions.

## Objectives

1. Implement LRU cache for compiled expressions
2. Add cache key generation (hash of expression source)
3. Implement result caching for pure expressions
4. Add cache invalidation on state changes
5. Provide cache statistics for debugging

---

## Cache Architecture

### 1.1 Cache Entry Structure

**File**: `ir/ir_expression_cache.c`

```c
// Cache entry for a compiled expression
typedef struct {
    // Key
    uint64_t key_hash;        // Hash of expression source
    char* source_expr;        // Original expression string (optional, for debugging)

    // Compiled bytecode
    IRCompiledExpr* compiled;

    // Cached result (for pure expressions only)
    IRValue cached_result;
    bool has_cached_result;

    // LRU tracking
    uint64_t last_access;
    uint64_t access_count;

    // Dependencies (for invalidation)
    uint32_t* var_dependencies;  // Variables this expression depends on
    uint32_t var_dep_count;

    // Linked list for LRU
    struct IRExprCacheEntry* lru_prev;
    struct IRExprCacheEntry* lru_next;
} IRExprCacheEntry;

// LRU Cache structure
typedef struct {
    IRExprCacheEntry** buckets;     // Hash table buckets
    uint32_t bucket_count;          // Number of buckets (power of 2)

    IRExprCacheEntry* lru_head;     // Most recently used
    IRExprCacheEntry* lru_tail;     // Least recently used

    uint32_t entry_count;           // Total entries
    uint32_t max_entries;           // Maximum entries before eviction

    // Statistics
    uint64_t hits;
    uint64_t misses;
    uint64_t evictions;

    // Mutex for thread safety (optional, for future)
    // pthread_mutex_t lock;
} IRExprCache;
```

### 1.2 Cache Initialization

```c
// Create a new expression cache
IRExprCache* ir_expr_cache_create(uint32_t max_entries) {
    IRExprCache* cache = (IRExprCache*)calloc(1, sizeof(IRExprCache));

    // Use next power of 2 for bucket count
    cache->bucket_count = next_power_of_2(max_entries / 4);
    if (cache->bucket_count < 16) cache->bucket_count = 16;

    cache->buckets = (IRExprCacheEntry**)calloc(cache->bucket_count, sizeof(IRExprCacheEntry*));
    cache->max_entries = max_entries;
    cache->entry_count = 0;

    cache->hits = 0;
    cache->misses = 0;
    cache->evictions = 0;

    return cache;
}

// Destroy cache and free all entries
void ir_expr_cache_destroy(IRExprCache* cache) {
    if (!cache) return;

    // Free all entries
    for (uint32_t i = 0; i < cache->bucket_count; i++) {
        IRExprCacheEntry* entry = cache->buckets[i];
        while (entry) {
            IRExprCacheEntry* next = entry->lru_next;
            ir_expr_cache_entry_free(entry);
            entry = next;
        }
    }

    free(cache->buckets);
    free(cache);
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
```

---

## Cache Operations

### 2.1 Key Generation

```c
// Generate cache key from expression
static uint64_t expr_get_key(IRExpression* expr) {
    // For simple expressions, we can use a simple hash
    // For complex expressions, we need structural hashing

    uint64_t hash = 0;

    switch (expr->type) {
        case EXPR_LITERAL_INT:
            hash = hash_combine(hash, (uint64_t)EXPR_LITERAL_INT);
            hash = hash_combine(hash, (uint64_t)expr->int_value);
            break;

        case EXPR_LITERAL_STRING:
            hash = hash_combine(hash, (uint64_t)EXPR_LITERAL_STRING);
            hash = hash_combine(hash, hash_string(expr->string_value));
            break;

        case EXPR_VAR_REF:
            hash = hash_combine(hash, (uint64_t)EXPR_VAR_REF);
            hash = hash_combine(hash, hash_string(expr->var_ref.name));
            break;

        case EXPR_MEMBER_ACCESS:
            hash = hash_combine(hash, (uint64_t)EXPR_MEMBER_ACCESS);
            hash = hash_combine(hash, expr_get_key(expr->member_access.object));
            hash = hash_combine(hash, hash_string(expr->member_access.property));
            break;

        case EXPR_BINARY:
            hash = hash_combine(hash, (uint64_t)EXPR_BINARY);
            hash = hash_combine(hash, (uint64_t)expr->binary.op);
            hash = hash_combine(hash, expr_get_key(expr->binary.left));
            hash = hash_combine(hash, expr_get_key(expr->binary.right));
            break;

        // ... other types
    }

    return hash;
}

// Combine two hashes
static uint64_t hash_combine(uint64_t a, uint64_t b) {
    // Similar to Boost's hash_combine
    return a ^ (b + 0x9e3779b9 + (a << 6) + (a >> 2));
}

// String hash (FNV-1a)
static uint64_t hash_string(const char* str) {
    if (!str) return 0;

    uint64_t hash = 14695981039346656037ULL;
    while (*str) {
        hash ^= (uint64_t)(unsigned char)*str++;
        hash *= 1099511628211ULL;
    }
    return hash;
}
```

### 2.2 Cache Lookup

```c
// Lookup expression in cache, returning compiled bytecode if found
IRCompiledExpr* ir_expr_cache_lookup(IRExprCache* cache, IRExpression* expr) {
    if (!cache || !expr) return NULL;

    uint64_t key = expr_get_key(expr);
    uint32_t bucket = key & (cache->bucket_count - 1);

    // Search bucket
    IRExprCacheEntry* entry = cache->buckets[bucket];
    while (entry) {
        if (entry->key_hash == key && expr_equals(entry->compiled->source_expr, expr)) {
            // Found - move to front of LRU
            lru_move_to_front(cache, entry);

            // Update access tracking
            entry->last_access = get_timestamp();
            entry->access_count++;

            cache->hits++;
            return entry->compiled;
        }
        entry = entry->lru_next;
    }

    cache->misses++;
    return NULL;
}

// Get cached result if available (for pure expressions)
IRValue* ir_expr_cache_get_result(IRExprCache* cache, IRExpression* expr) {
    if (!cache || !expr) return NULL;

    uint64_t key = expr_get_key(expr);
    uint32_t bucket = key & (cache->bucket_count - 1);

    IRExprCacheEntry* entry = cache->buckets[bucket];
    while (entry) {
        if (entry->key_hash == key && expr_equals(entry->compiled->source_expr, expr)) {
            if (entry->has_cached_result) {
                lru_move_to_front(cache, entry);
                return &entry->cached_result;
            }
            return NULL;
        }
        entry = entry->lru_next;
    }

    return NULL;
}

// Store result in cache (only for pure expressions)
void ir_expr_cache_store_result(IRExprCache* cache, IRExpression* expr, IRValue result) {
    if (!cache || !expr) return;

    uint64_t key = expr_get_key(expr);
    uint32_t bucket = key & (cache->bucket_count - 1);

    IRExprCacheEntry* entry = cache->buckets[bucket];
    while (entry) {
        if (entry->key_hash == key && expr_equals(entry->compiled->source_expr, expr)) {
            // Found entry - store result
            if (entry->has_cached_result) {
                ir_value_free(&entry->cached_result);
            }

            entry->cached_result = ir_value_copy(&result);
            entry->has_cached_result = true;
            return;
        }
        entry = entry->lru_next;
    }
}
```

### 2.3 Cache Insert

```c
// Insert compiled expression into cache
void ir_expr_cache_insert(IRExprCache* cache, IRExpression* expr, IRCompiledExpr* compiled) {
    if (!cache || !expr || !compiled) return;

    uint64_t key = expr_get_key(expr);
    uint32_t bucket = key & (cache->bucket_count - 1);

    // Check if already exists
    IRExprCacheEntry* entry = cache->buckets[bucket];
    while (entry) {
        if (entry->key_hash == key && expr_equals(entry->compiled->source_expr, expr)) {
            // Already exists - update
            if (entry->compiled != compiled) {
                ir_expr_compiled_free(entry->compiled);
                entry->compiled = compiled;
            }
            lru_move_to_front(cache, entry);
            return;
        }
        entry = entry->lru_next;
    }

    // Create new entry
    entry = (IRExprCacheEntry*)calloc(1, sizeof(IRExprCacheEntry));
    entry->key_hash = key;
    entry->compiled = compiled;
    entry->last_access = get_timestamp();
    entry->access_count = 1;
    entry->has_cached_result = false;

    // Extract variable dependencies for invalidation
    extract_var_dependencies(expr, entry);

    // Evict if necessary
    if (cache->entry_count >= cache->max_entries) {
        evict_lru(cache);
    }

    // Add to bucket
    entry->lru_next = cache->buckets[bucket];
    if (cache->buckets[bucket]) {
        cache->buckets[bucket]->lru_prev = entry;
    }
    cache->buckets[bucket] = entry;

    // Add to LRU list (front)
    if (cache->lru_head) {
        cache->lru_head->lru_prev = entry;
    }
    entry->lru_next = cache->lru_head;
    cache->lru_head = entry;

    if (!cache->lru_tail) {
        cache->lru_tail = entry;
    }

    cache->entry_count++;
}
```

---

## LRU Management

### 3.1 LRU Operations

```c
// Move entry to front of LRU list
static void lru_move_to_front(IRExprCache* cache, IRExprCacheEntry* entry) {
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
    if (!cache->lru_tail) return;

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
                prev->lru_next = entry->lru_next;
            } else {
                cache->buckets[bucket] = entry->lru_next;
            }
            break;
        }
        prev = entry;
        entry = entry->lru_next;
    }

    cache->evictions++;
    cache->entry_count--;

    ir_expr_cache_entry_free(to_evict);
}
```

---

## Cache Invalidation

### 4.1 Dependency Tracking

```c
// Extract variable dependencies from expression
static void extract_var_dependencies(IRExpression* expr, IRExprCacheEntry* entry) {
    // Count dependencies first
    uint32_t count = 0;
    count_var_dependencies(expr, &count);

    if (count == 0) return;

    // Allocate and collect dependencies
    entry->var_dependencies = (uint32_t*)calloc(count, sizeof(uint32_t));
    entry->var_dep_count = 0;

    collect_var_dependencies(expr, entry);
}

static void count_var_dependencies(IRExpression* expr, uint32_t* count) {
    if (!expr) return;

    switch (expr->type) {
        case EXPR_VAR_REF:
            (*count)++;
            break;

        case EXPR_MEMBER_ACCESS:
            count_var_dependencies(expr->member_access.object, count);
            break;

        case EXPR_BINARY:
            count_var_dependencies(expr->binary.left, count);
            count_var_dependencies(expr->binary.right, count);
            break;

        case EXPR_UNARY:
            count_var_dependencies(expr->unary.operand, count);
            break;

        case EXPR_TERNARY:
            count_var_dependencies(expr->ternary.condition, count);
            count_var_dependencies(expr->ternary.then_expr, count);
            count_var_dependencies(expr->ternary.else_expr, count);
            break;

        // ... other types
    }
}

// Invalidate all cache entries that depend on a variable
void ir_expr_cache_invalidate_var(IRExprCache* cache, const char* var_name) {
    if (!cache || !var_name) return;

    uint64_t var_hash = hash_string(var_name);

    for (uint32_t i = 0; i < cache->bucket_count; i++) {
        IRExprCacheEntry* entry = cache->buckets[i];
        while (entry) {
            IRExprCacheEntry* next = entry->lru_next;

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

// Invalidate entire cache (e.g., after hot reload)
void ir_expr_cache_invalidate_all(IRExprCache* cache) {
    if (!cache) return;

    for (uint32_t i = 0; i < cache->bucket_count; i++) {
        IRExprCacheEntry* entry = cache->buckets[i];
        while (entry) {
            if (entry->has_cached_result) {
                ir_value_free(&entry->cached_result);
                entry->has_cached_result = false;
            }
            entry = entry->lru_next;
        }
    }
}
```

---

## Cache Statistics

### 5.1 Statistics API

```c
// Get cache statistics
typedef struct {
    uint32_t entry_count;
    uint32_t max_entries;
    uint32_t bucket_count;
    uint64_t hits;
    uint64_t misses;
    uint64_t evictions;
    double hit_rate;
} IRExprCacheStats;

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

// Print cache statistics (for debugging)
void ir_expr_cache_print_stats(IRExprCache* cache) {
    if (!cache) return;

    IRExprCacheStats stats;
    ir_expr_cache_get_stats(cache, &stats);

    printf("Expression Cache Statistics:\n");
    printf("  Entries: %u / %u\n", stats.entry_count, stats.max_entries);
    printf("  Buckets: %u\n", stats.bucket_count);
    printf("  Hits: %llu\n", (unsigned long long)stats.hits);
    printf("  Misses: %llu\n", (unsigned long long)stats.misses);
    printf("  Evictions: %llu\n", (unsigned long long)stats.evictions);
    printf("  Hit Rate: %.2f%%\n", stats.hit_rate * 100.0);
}

// Reset statistics
void ir_expr_cache_reset_stats(IRExprCache* cache) {
    if (!cache) return;

    cache->hits = 0;
    cache->misses = 0;
    cache->evictions = 0;
}
```

---

## Global Cache Instance

### 6.1 Global Cache

```c
// Global expression cache (shared across all executors)
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
```

---

## Implementation Steps

### Day 1: Core Cache Implementation

1. **Morning** (4 hours)
   - [ ] Create `ir/ir_expression_cache.h` and `.c`
   - [ ] Implement cache structures
   - [ ] Implement cache creation/destruction
   - [ ] Implement hash functions

2. **Afternoon** (4 hours)
   - [ ] Implement cache lookup
   - [ ] Implement cache insert
   - [ ] Write unit tests for basic operations

### Day 2: LRU & Invalidation

1. **Morning** (4 hours)
   - [ ] Implement LRU list management
   - [ ] Implement LRU eviction
   - [ ] Test eviction behavior

2. **Afternoon** (4 hours)
   - [ ] Implement dependency extraction
   - [ ] Implement variable-based invalidation
   - [ ] Test invalidation

### Day 3: Integration & Statistics

1. **Morning** (4 hours)
   - [ ] Implement global cache instance
   - [ ] Integrate with evaluator
   - [ ] Add cache statistics

2. **Afternoon** (4 hours)
   - [ ] Performance testing
   - [ ] Hit rate optimization
   - [ ] Documentation

---

## Testing Strategy

**File**: `tests/expression-engine/test_phase5_cache.c`

```c
// Test 1: Cache creation and destruction
void test_cache_create_destroy(void) {
    IRExprCache* cache = ir_expr_cache_create(100);

    assert(cache != NULL);
    assert(cache->max_entries == 100);
    assert(cache->entry_count == 0);

    ir_expr_cache_destroy(cache);
}

// Test 2: Cache insertion and lookup
void test_cache_insert_lookup(void) {
    IRExprCache* cache = ir_expr_cache_create(100);

    IRExpression* expr = ir_expr_literal_int(42);
    IRCompiledExpr* compiled = ir_expr_compile(expr);

    ir_expr_cache_insert(cache, expr, compiled);

    IRCompiledExpr* found = ir_expr_cache_lookup(cache, expr);
    assert(found == compiled);

    ir_expr_cache_destroy(cache);
    ir_expr_free(expr);
}

// Test 3: LRU eviction
void test_cache_lru_eviction(void) {
    IRExprCache* cache = ir_expr_cache_create(3);  // Small cache

    // Insert 4 expressions
    for (int i = 0; i < 4; i++) {
        IRExpression* expr = ir_expr_literal_int(i);
        IRCompiledExpr* compiled = ir_expr_compile(expr);
        ir_expr_cache_insert(cache, expr, compiled);
    }

    // First expression should have been evicted
    assert(cache->entry_count == 3);
    assert(cache->evictions == 1);

    ir_expr_cache_destroy(cache);
}

// Test 4: Cache hit rate
void test_cache_hit_rate(void) {
    IRExprCache* cache = ir_expr_cache_create(100);

    IRExpression* expr = ir_expr_literal_int(42);
    IRCompiledExpr* compiled = ir_expr_compile(expr);
    ir_expr_cache_insert(cache, expr, compiled);

    // Multiple lookups
    for (int i = 0; i < 10; i++) {
        IRCompiledExpr* found = ir_expr_cache_lookup(cache, expr);
        assert(found == compiled);
    }

    IRExprCacheStats stats;
    ir_expr_cache_get_stats(cache, &stats);
    assert(stats.hits == 10);
    assert(stats.misses == 0);
    assert(stats.hit_rate == 1.0);

    ir_expr_cache_destroy(cache);
    ir_expr_free(expr);
}

// Test 5: Variable invalidation
void test_cache_var_invalidation(void) {
    IRExprCache* cache = ir_expr_cache_create(100);

    // Create expression: count + 1
    IRExpression* expr = ir_expr_binary(
        BINARY_OP_ADD,
        ir_expr_var_ref("count"),
        ir_expr_literal_int(1)
    );

    IRCompiledExpr* compiled = ir_expr_compile(expr);
    ir_expr_cache_insert(cache, expr, compiled);

    // Before invalidation, should find in cache
    IRCompiledExpr* found = ir_expr_cache_lookup(cache, expr);
    assert(found != NULL);

    // Invalidate variable dependency
    ir_expr_cache_invalidate_var(cache, "count");

    // After invalidation, result cache should be cleared
    // (compiled bytecode remains)
    assert(cache->entry_count == 1);

    ir_expr_cache_destroy(cache);
    ir_expr_free(expr);
}

// Performance test
void test_cache_performance(void) {
    IRExprCache* cache = ir_expr_cache_create(1000);

    // Create a complex expression
    IRExpression* expr = ir_expr_binary(
        BINARY_OP_ADD,
        ir_expr_var_ref("a"),
        ir_expr_var_ref("b")
    );

    // First lookup - should miss
    IRCompiledExpr* found = ir_expr_cache_lookup(cache, expr);
    assert(found == NULL);

    // Insert
    IRCompiledExpr* compiled = ir_expr_compile(expr);
    ir_expr_cache_insert(cache, expr, compiled);

    // Benchmark cached lookups
    clock_t start = clock();
    for (int i = 0; i < 1000000; i++) {
        IRCompiledExpr* result = ir_expr_cache_lookup(cache, expr);
        assert(result != NULL);
    }
    clock_t end = clock();

    double elapsed = (double)(end - start) / CLOCKS_PER_SEC;
    printf("1M cached lookups: %.4f seconds\n", elapsed);

    // Should be very fast (< 0.1 seconds)
    assert(elapsed < 0.1);

    ir_expr_cache_destroy(cache);
    ir_expr_free(expr);
}
```

---

## Acceptance Criteria

Phase 5 is complete when:

- [ ] LRU cache correctly evicts least recently used entries
- [ ] Cache hit rate > 80% for typical workloads
- [ ] Variable-based invalidation works correctly
- [ ] Cache statistics are accurate
- [ ] Memory usage stays within configured limits
- [ ] Thread-safe for future multi-threading (preparation)
- [ ] Unit test coverage > 90%
- [ ] Performance: cached lookup < 100ns

---

## Integration with Executor

The cache integrates with `ir_executor.c` to automatically cache compiled expressions:

```c
// In ir_executor.c, when evaluating expressions:
IRValue eval_expr_cached(IRExpression* expr, IRExecutorContext* ctx) {
    IRExprCache* cache = ir_expr_cache_global_get();

    // Try to get cached result (for pure expressions)
    IRValue* cached = ir_expr_cache_get_result(cache, expr);
    if (cached) {
        return ir_value_copy(cached);
    }

    // Lookup or compile bytecode
    IRCompiledExpr* compiled = ir_expr_cache_lookup(cache, expr);
    if (!compiled) {
        compiled = ir_expr_compile(expr);
        ir_expr_cache_insert(cache, expr, compiled);
    }

    // Evaluate
    IRValue result = ir_expr_eval(compiled, ctx, ctx->current_instance_id);

    // Cache result if expression is pure (no side effects)
    if (expr_is_pure(expr)) {
        ir_expr_cache_store_result(cache, expr, result);
    }

    return result;
}
```

---

## Next Phase

Once Phase 5 is complete, proceed to **[Phase 6: Builtin Function Library](./phase-6-builtin-functions.md)**.

---

**Last Updated**: 2026-01-11
