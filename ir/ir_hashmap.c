// IR Hash Map - Fast component ID lookups
// Replaces O(n) tree traversal with O(1) hash lookups

#include "ir_hashmap.h"
#include "ir_core.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

// ============================================================================
// Component ID Hash Map
// ============================================================================

#define DEFAULT_BUCKET_COUNT 256
#define LOAD_FACTOR_THRESHOLD 0.75

struct IRComponentMap {
    IRComponent** buckets;
    uint32_t bucket_count;
    uint32_t item_count;
};

// Simple hash function for component IDs
static uint32_t hash_component_id(uint32_t id, uint32_t bucket_count) {
    // FNV-1a hash
    uint32_t hash = 2166136261u;
    hash ^= (id & 0xFF);
    hash *= 16777619;
    hash ^= ((id >> 8) & 0xFF);
    hash *= 16777619;
    hash ^= ((id >> 16) & 0xFF);
    hash *= 16777619;
    hash ^= ((id >> 24) & 0xFF);
    hash *= 16777619;

    return hash % bucket_count;
}

IRComponentMap* ir_map_create(uint32_t initial_capacity) {
    IRComponentMap* map = (IRComponentMap*)malloc(sizeof(IRComponentMap));
    if (!map) return NULL;

    uint32_t capacity = initial_capacity > 0 ? initial_capacity : DEFAULT_BUCKET_COUNT;

    map->buckets = (IRComponent**)calloc(capacity, sizeof(IRComponent*));
    if (!map->buckets) {
        free(map);
        return NULL;
    }

    map->bucket_count = capacity;
    map->item_count = 0;

    return map;
}

void ir_map_destroy(IRComponentMap* map) {
    if (!map) return;

    // Note: We don't free the components themselves, just the map structure
    if (map->buckets) {
        free(map->buckets);
    }

    free(map);
}

static bool ir_map_resize(IRComponentMap* map, uint32_t new_capacity) {
    IRComponent** new_buckets = (IRComponent**)calloc(new_capacity, sizeof(IRComponent*));
    if (!new_buckets) return false;

    // Rehash all existing components
    for (uint32_t i = 0; i < map->bucket_count; i++) {
        IRComponent* component = map->buckets[i];
        while (component) {
            // Linear probing to find next slot
            uint32_t hash = hash_component_id(component->id, new_capacity);
            while (new_buckets[hash] != NULL) {
                hash = (hash + 1) % new_capacity;
            }

            IRComponent* next = (IRComponent*)component->tag; // Temporarily stored next pointer
            new_buckets[hash] = component;
            component->tag = NULL; // Clear temporary storage
            component = next;
        }
    }

    free(map->buckets);
    map->buckets = new_buckets;
    map->bucket_count = new_capacity;

    return true;
}

bool ir_map_insert(IRComponentMap* map, IRComponent* component) {
    if (!map || !component) return false;

    // Validate component ID (0 is reserved/invalid)
    if (component->id == 0) {
        fprintf(stderr, "Error: Cannot insert component with ID 0 (reserved/invalid)\n");
        return false;
    }

    // Check if we need to resize
    float load_factor = (float)map->item_count / (float)map->bucket_count;
    if (load_factor > LOAD_FACTOR_THRESHOLD) {
        if (!ir_map_resize(map, map->bucket_count * 2)) {
            return false; // Resize failed, but we can still try to insert
        }
    }

    // Linear probing to handle collisions
    uint32_t hash = hash_component_id(component->id, map->bucket_count);
    uint32_t start_hash = hash;

    while (map->buckets[hash] != NULL) {
        // Check if component already exists (update case)
        if (map->buckets[hash]->id == component->id) {
            map->buckets[hash] = component;
            return true;
        }

        hash = (hash + 1) % map->bucket_count;

        // Wrapped around - map is full
        if (hash == start_hash) {
            return false;
        }
    }

    map->buckets[hash] = component;
    map->item_count++;

    return true;
}

IRComponent* ir_map_lookup(IRComponentMap* map, uint32_t id) {
    if (!map) return NULL;

    // Validate component ID (0 is reserved/invalid)
    if (id == 0) {
        return NULL;
    }

    uint32_t hash = hash_component_id(id, map->bucket_count);
    uint32_t start_hash = hash;

    while (map->buckets[hash] != NULL) {
        if (map->buckets[hash]->id == id) {
            return map->buckets[hash];
        }

        hash = (hash + 1) % map->bucket_count;

        // Wrapped around - not found
        if (hash == start_hash) {
            break;
        }
    }

    return NULL;
}

bool ir_map_remove(IRComponentMap* map, uint32_t id) {
    if (!map) return false;

    // Validate component ID (0 is reserved/invalid)
    if (id == 0) {
        return false;
    }

    uint32_t hash = hash_component_id(id, map->bucket_count);
    uint32_t start_hash = hash;

    while (map->buckets[hash] != NULL) {
        if (map->buckets[hash]->id == id) {
            // Found the component - remove it
            map->buckets[hash] = NULL;
            map->item_count--;

            // Rehash subsequent entries in the cluster to maintain linear probing invariant
            uint32_t next_hash = (hash + 1) % map->bucket_count;
            while (map->buckets[next_hash] != NULL) {
                IRComponent* component = map->buckets[next_hash];
                uint32_t ideal_hash = hash_component_id(component->id, map->bucket_count);

                // If this component's ideal position is before or at the removed slot,
                // we need to move it back
                bool should_move = false;
                if (ideal_hash <= hash) {
                    should_move = (next_hash > hash) || (next_hash < ideal_hash);
                } else {
                    should_move = (next_hash > hash) && (next_hash < ideal_hash);
                }

                if (should_move) {
                    map->buckets[hash] = component;
                    map->buckets[next_hash] = NULL;
                    hash = next_hash;
                }

                next_hash = (next_hash + 1) % map->bucket_count;
                if (next_hash == start_hash) break; // Prevent infinite loop
            }

            return true;
        }

        hash = (hash + 1) % map->bucket_count;

        // Wrapped around - not found
        if (hash == start_hash) {
            break;
        }
    }

    return false;
}

uint32_t ir_map_get_count(IRComponentMap* map) {
    return map ? map->item_count : 0;
}

void ir_map_clear(IRComponentMap* map) {
    if (!map) return;

    memset(map->buckets, 0, map->bucket_count * sizeof(IRComponent*));
    map->item_count = 0;
}

// ============================================================================
// Event Hash Map (by event type)
// ============================================================================

#define EVENT_BUCKET_COUNT 16 // Small, since we only have ~8 event types

struct IREventMap {
    IREvent* buckets[EVENT_BUCKET_COUNT];
};

IREventMap* ir_event_map_create(void) {
    IREventMap* map = (IREventMap*)calloc(1, sizeof(IREventMap));
    return map;
}

void ir_event_map_destroy(IREventMap* map) {
    if (!map) return;

    // Free all event chains
    for (int i = 0; i < EVENT_BUCKET_COUNT; i++) {
        IREvent* event = map->buckets[i];
        while (event) {
            IREvent* next = event->next;
            if (event->logic_id) free(event->logic_id);
            if (event->handler_data) free(event->handler_data);
            free(event);
            event = next;
        }
    }

    free(map);
}

void ir_event_map_add(IREventMap* map, IREvent* event) {
    if (!map || !event) return;

    uint32_t bucket = event->type % EVENT_BUCKET_COUNT;

    // Add to front of chain
    event->next = map->buckets[bucket];
    map->buckets[bucket] = event;
}

IREvent* ir_event_map_find(IREventMap* map, IREventType type) {
    if (!map) return NULL;

    uint32_t bucket = type % EVENT_BUCKET_COUNT;
    return map->buckets[bucket];
}

bool ir_event_map_remove(IREventMap* map, IREventType type) {
    if (!map) return false;

    uint32_t bucket = type % EVENT_BUCKET_COUNT;
    IREvent** current = &map->buckets[bucket];

    while (*current) {
        if ((*current)->type == type) {
            IREvent* to_remove = *current;
            *current = to_remove->next;

            if (to_remove->logic_id) free(to_remove->logic_id);
            if (to_remove->handler_data) free(to_remove->handler_data);
            free(to_remove);

            return true;
        }
        current = &(*current)->next;
    }

    return false;
}
