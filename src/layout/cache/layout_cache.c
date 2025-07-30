/**
 * @file layout_cache.c
 * @brief Kryon Layout Cache Implementation
 */

#include "internal/layout.h"
#include "internal/memory.h"
#include <string.h>
#include <stdio.h>

// =============================================================================
// CACHE STRUCTURES
// =============================================================================

typedef struct {
    // Input hash (style + constraints)
    uint64_t input_hash;
    
    // Computed layout
    KryonLayoutResult result;
    
    // Cache metadata
    double timestamp;
    uint32_t access_count;
    bool is_valid;
    
} KryonLayoutCacheEntry;

typedef struct {
    KryonLayoutNode* node;
    KryonLayoutCacheEntry* entries;
    size_t entry_count;
    size_t entry_capacity;
    
    // Cache statistics
    uint64_t hit_count;
    uint64_t miss_count;
    
} KryonNodeCache;

typedef struct {
    KryonNodeCache* node_caches;
    size_t cache_count;
    size_t cache_capacity;
    
    // Global cache settings
    size_t max_entries_per_node;
    double cache_timeout;
    bool cache_enabled;
    
    // Global statistics
    uint64_t total_hits;
    uint64_t total_misses;
    uint64_t total_invalidations;
    
} KryonLayoutCache;

static KryonLayoutCache g_layout_cache = {0};

// =============================================================================
// HASH COMPUTATION
// =============================================================================

static uint64_t hash_style(const KryonLayoutStyle* style) {
    if (!style) return 0;
    
    // Simple FNV-1a hash
    uint64_t hash = 14695981039346656037ULL;
    const uint8_t* data = (const uint8_t*)style;
    size_t size = sizeof(KryonLayoutStyle);
    
    for (size_t i = 0; i < size; i++) {
        hash ^= data[i];
        hash *= 1099511628211ULL;
    }
    
    return hash;
}

static uint64_t hash_constraints(const KryonLayoutNode* node) {
    // Hash constraints that affect this node
    uint64_t hash = 0;
    
    // Include parent size if it affects layout
    if (node->parent) {
        hash ^= (uint64_t)(node->parent->computed.width * 1000);
        hash ^= (uint64_t)(node->parent->computed.height * 1000) << 16;
    }
    
    // Include sibling count and positions for relative layouts
    if (node->parent) {
        hash ^= (uint64_t)node->parent->child_count << 32;
        
        // Hash sibling positions for context-dependent layouts
        for (size_t i = 0; i < node->parent->child_count && i < 8; i++) {
            KryonLayoutNode* sibling = node->parent->children[i];
            if (sibling != node) {
                hash ^= (uint64_t)(sibling->computed.width * 100) << (i * 4);
                hash ^= (uint64_t)(sibling->computed.height * 100) << (i * 4 + 2);
            }
        }
    }
    
    return hash;
}

static uint64_t compute_input_hash(const KryonLayoutNode* node) {
    uint64_t style_hash = hash_style(&node->style);
    uint64_t constraint_hash = hash_constraints(node);
    
    // Combine hashes
    return style_hash ^ (constraint_hash << 1);
}

// =============================================================================
// CACHE ENTRY MANAGEMENT
// =============================================================================

static KryonNodeCache* find_or_create_node_cache(KryonLayoutNode* node) {
    // Look for existing cache
    for (size_t i = 0; i < g_layout_cache.cache_count; i++) {
        if (g_layout_cache.node_caches[i].node == node) {
            return &g_layout_cache.node_caches[i];
        }
    }
    
    // Create new cache
    if (g_layout_cache.cache_count >= g_layout_cache.cache_capacity) {
        size_t new_capacity = g_layout_cache.cache_capacity == 0 ? 16 : g_layout_cache.cache_capacity * 2;
        KryonNodeCache* new_caches = kryon_alloc(sizeof(KryonNodeCache) * new_capacity);
        if (!new_caches) return NULL;
        
        if (g_layout_cache.node_caches) {
            memcpy(new_caches, g_layout_cache.node_caches, 
                   sizeof(KryonNodeCache) * g_layout_cache.cache_count);
            kryon_free(g_layout_cache.node_caches);
        }
        
        g_layout_cache.node_caches = new_caches;
        g_layout_cache.cache_capacity = new_capacity;
    }
    
    KryonNodeCache* cache = &g_layout_cache.node_caches[g_layout_cache.cache_count];
    memset(cache, 0, sizeof(KryonNodeCache));
    cache->node = node;
    
    g_layout_cache.cache_count++;
    return cache;
}

static KryonLayoutCacheEntry* find_cache_entry(KryonNodeCache* cache, uint64_t input_hash) {
    if (!cache || !cache->entries) return NULL;
    
    double current_time = kryon_platform_get_time();
    
    for (size_t i = 0; i < cache->entry_count; i++) {
        KryonLayoutCacheEntry* entry = &cache->entries[i];
        
        if (entry->is_valid && entry->input_hash == input_hash) {
            // Check if entry is still valid (not expired)
            if (g_layout_cache.cache_timeout > 0 && 
                current_time - entry->timestamp > g_layout_cache.cache_timeout) {
                entry->is_valid = false;
                continue;
            }
            
            entry->access_count++;
            return entry;
        }
    }
    
    return NULL;
}

static KryonLayoutCacheEntry* create_cache_entry(KryonNodeCache* cache, uint64_t input_hash) {
    if (!cache) return NULL;
    
    // Expand entry array if needed
    if (cache->entry_count >= cache->entry_capacity) {
        size_t new_capacity = cache->entry_capacity == 0 ? 4 : cache->entry_capacity * 2;
        
        // Limit entries per node to prevent excessive memory usage
        if (new_capacity > g_layout_cache.max_entries_per_node) {
            new_capacity = g_layout_cache.max_entries_per_node;
            
            // If we've hit the limit, evict the least recently used entry
            if (cache->entry_count >= new_capacity) {
                // Find LRU entry
                size_t lru_index = 0;
                uint32_t min_access_count = cache->entries[0].access_count;
                
                for (size_t i = 1; i < cache->entry_count; i++) {
                    if (cache->entries[i].access_count < min_access_count) {
                        min_access_count = cache->entries[i].access_count;
                        lru_index = i;
                    }
                }
                
                // Reuse the LRU entry
                KryonLayoutCacheEntry* entry = &cache->entries[lru_index];
                memset(entry, 0, sizeof(KryonLayoutCacheEntry));
                entry->input_hash = input_hash;
                entry->timestamp = kryon_platform_get_time();
                entry->is_valid = true;
                
                return entry;
            }
        }
        
        KryonLayoutCacheEntry* new_entries = kryon_alloc(sizeof(KryonLayoutCacheEntry) * new_capacity);
        if (!new_entries) return NULL;
        
        if (cache->entries) {
            memcpy(new_entries, cache->entries, sizeof(KryonLayoutCacheEntry) * cache->entry_count);
            kryon_free(cache->entries);
        }
        
        cache->entries = new_entries;
        cache->entry_capacity = new_capacity;
    }
    
    // Create new entry
    KryonLayoutCacheEntry* entry = &cache->entries[cache->entry_count];
    memset(entry, 0, sizeof(KryonLayoutCacheEntry));
    
    entry->input_hash = input_hash;
    entry->timestamp = kryon_platform_get_time();
    entry->is_valid = true;
    
    cache->entry_count++;
    return entry;
}

// =============================================================================
// CACHE OPERATIONS
// =============================================================================

static void copy_layout_result(KryonLayoutResult* dst, const KryonLayoutResult* src) {
    if (!dst || !src) return;
    
    dst->x = src->x;
    dst->y = src->y;
    dst->width = src->width;
    dst->height = src->height;
    dst->content_width = src->content_width;
    dst->content_height = src->content_height;
    dst->baseline = src->baseline;
    dst->overflow_x = src->overflow_x;
    dst->overflow_y = src->overflow_y;
}

static bool layout_results_equal(const KryonLayoutResult* a, const KryonLayoutResult* b) {
    if (!a || !b) return false;
    
    const float epsilon = 0.001f;
    
    return fabsf(a->x - b->x) < epsilon &&
           fabsf(a->y - b->y) < epsilon &&
           fabsf(a->width - b->width) < epsilon &&
           fabsf(a->height - b->height) < epsilon &&
           fabsf(a->content_width - b->content_width) < epsilon &&
           fabsf(a->content_height - b->content_height) < epsilon;
}

// =============================================================================
// PUBLIC API
// =============================================================================

bool kryon_layout_cache_init(void) {
    memset(&g_layout_cache, 0, sizeof(g_layout_cache));
    
    g_layout_cache.max_entries_per_node = 8;
    g_layout_cache.cache_timeout = 5.0; // 5 seconds
    g_layout_cache.cache_enabled = true;
    
    return true;
}

void kryon_layout_cache_shutdown(void) {
    // Clean up all caches
    for (size_t i = 0; i < g_layout_cache.cache_count; i++) {
        kryon_free(g_layout_cache.node_caches[i].entries);
    }
    
    kryon_free(g_layout_cache.node_caches);
    memset(&g_layout_cache, 0, sizeof(g_layout_cache));
}

bool kryon_layout_cache_get(KryonLayoutNode* node, KryonLayoutResult* result) {
    if (!g_layout_cache.cache_enabled || !node || !result) {
        return false;
    }
    
    uint64_t input_hash = compute_input_hash(node);
    KryonNodeCache* cache = find_or_create_node_cache(node);
    if (!cache) return false;
    
    KryonLayoutCacheEntry* entry = find_cache_entry(cache, input_hash);
    if (entry) {
        copy_layout_result(result, &entry->result);
        cache->hit_count++;
        g_layout_cache.total_hits++;
        return true;
    }
    
    cache->miss_count++;
    g_layout_cache.total_misses++;
    return false;
}

void kryon_layout_cache_set(KryonLayoutNode* node, const KryonLayoutResult* result) {
    if (!g_layout_cache.cache_enabled || !node || !result) {
        return;
    }
    
    uint64_t input_hash = compute_input_hash(node);
    KryonNodeCache* cache = find_or_create_node_cache(node);
    if (!cache) return;
    
    // Check if we already have this entry
    KryonLayoutCacheEntry* entry = find_cache_entry(cache, input_hash);
    if (!entry) {
        entry = create_cache_entry(cache, input_hash);
    }
    
    if (entry) {
        copy_layout_result(&entry->result, result);
        entry->timestamp = kryon_platform_get_time();
        entry->is_valid = true;
    }
}

void kryon_layout_cache_invalidate_node(KryonLayoutNode* node) {
    if (!g_layout_cache.cache_enabled || !node) return;
    
    // Find and invalidate cache for this node
    for (size_t i = 0; i < g_layout_cache.cache_count; i++) {
        KryonNodeCache* cache = &g_layout_cache.node_caches[i];
        if (cache->node == node) {
            for (size_t j = 0; j < cache->entry_count; j++) {
                cache->entries[j].is_valid = false;
            }
            g_layout_cache.total_invalidations++;
            break;
        }
    }
    
    // Also invalidate children, as they might depend on this node
    for (size_t i = 0; i < node->child_count; i++) {
        kryon_layout_cache_invalidate_node(node->children[i]);
    }
}

void kryon_layout_cache_invalidate_all(void) {
    for (size_t i = 0; i < g_layout_cache.cache_count; i++) {
        KryonNodeCache* cache = &g_layout_cache.node_caches[i];
        for (size_t j = 0; j < cache->entry_count; j++) {
            cache->entries[j].is_valid = false;
        }
    }
    
    g_layout_cache.total_invalidations += g_layout_cache.cache_count;
}

void kryon_layout_cache_clear(void) {
    for (size_t i = 0; i < g_layout_cache.cache_count; i++) {
        KryonNodeCache* cache = &g_layout_cache.node_caches[i];
        cache->entry_count = 0;
        cache->hit_count = 0;
        cache->miss_count = 0;
    }
    
    g_layout_cache.total_hits = 0;
    g_layout_cache.total_misses = 0;
    g_layout_cache.total_invalidations = 0;
}

void kryon_layout_cache_set_enabled(bool enabled) {
    g_layout_cache.cache_enabled = enabled;
    
    if (!enabled) {
        kryon_layout_cache_clear();
    }
}

bool kryon_layout_cache_is_enabled(void) {
    return g_layout_cache.cache_enabled;
}

void kryon_layout_cache_set_timeout(double timeout_seconds) {
    g_layout_cache.cache_timeout = timeout_seconds;
}

double kryon_layout_cache_get_timeout(void) {
    return g_layout_cache.cache_timeout;
}

void kryon_layout_cache_set_max_entries_per_node(size_t max_entries) {
    g_layout_cache.max_entries_per_node = max_entries > 0 ? max_entries : 1;
}

size_t kryon_layout_cache_get_max_entries_per_node(void) {
    return g_layout_cache.max_entries_per_node;
}

KryonLayoutCacheStats kryon_layout_cache_get_stats(void) {
    KryonLayoutCacheStats stats = {0};
    
    stats.total_hits = g_layout_cache.total_hits;
    stats.total_misses = g_layout_cache.total_misses;
    stats.total_invalidations = g_layout_cache.total_invalidations;
    stats.cache_count = g_layout_cache.cache_count;
    
    // Calculate hit rate
    uint64_t total_requests = stats.total_hits + stats.total_misses;
    stats.hit_rate = total_requests > 0 ? (double)stats.total_hits / total_requests : 0.0;
    
    // Calculate total entries
    stats.total_entries = 0;
    for (size_t i = 0; i < g_layout_cache.cache_count; i++) {
        stats.total_entries += g_layout_cache.node_caches[i].entry_count;
    }
    
    // Calculate memory usage estimate
    stats.memory_usage = g_layout_cache.cache_count * sizeof(KryonNodeCache) +
                        stats.total_entries * sizeof(KryonLayoutCacheEntry);
    
    return stats;
}

void kryon_layout_cache_print_stats(void) {
    KryonLayoutCacheStats stats = kryon_layout_cache_get_stats();
    
    printf("Layout Cache Statistics:\n");
    printf("  Hits: %llu\n", (unsigned long long)stats.total_hits);
    printf("  Misses: %llu\n", (unsigned long long)stats.total_misses);
    printf("  Hit Rate: %.2f%%\n", stats.hit_rate * 100.0);
    printf("  Invalidations: %llu\n", (unsigned long long)stats.total_invalidations);
    printf("  Cached Nodes: %zu\n", stats.cache_count);
    printf("  Total Entries: %zu\n", stats.total_entries);
    printf("  Memory Usage: %zu bytes\n", stats.memory_usage);
}