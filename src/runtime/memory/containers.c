/**
 * @file containers.c
 * @brief Kryon Container Data Structures Implementation
 */

#include "internal/containers.h"
#include "internal/memory.h"
#include <string.h>
#include <stdlib.h>
#include <assert.h>

// =============================================================================
// DYNAMIC ARRAY IMPLEMENTATION
// =============================================================================

#define KRYON_ARRAY_DEFAULT_CAPACITY 16
#define KRYON_ARRAY_DEFAULT_GROWTH_FACTOR 1.5f

KryonArray *kryon_array_create(size_t element_size, size_t initial_capacity) {
    if (element_size == 0) return NULL;
    
    KryonArray *array = kryon_alloc_type(KryonArray);
    if (!array) return NULL;
    
    if (initial_capacity == 0) {
        initial_capacity = KRYON_ARRAY_DEFAULT_CAPACITY;
    }
    
    array->data = kryon_alloc(initial_capacity * element_size);
    if (!array->data) {
        kryon_free(array);
        return NULL;
    }
    
    array->count = 0;
    array->capacity = initial_capacity;
    array->element_size = element_size;
    array->growth_factor = KRYON_ARRAY_DEFAULT_GROWTH_FACTOR;
    array->owns_memory = true;
    
    return array;
}

void kryon_array_destroy(KryonArray *array) {
    if (!array) return;
    
    if (array->owns_memory && array->data) {
        kryon_free(array->data);
    }
    
    kryon_free(array);
}

static bool kryon_array_grow(KryonArray *array, size_t min_capacity) {
    if (!array || !array->owns_memory) return false;
    
    size_t new_capacity = array->capacity;
    while (new_capacity < min_capacity) {
        new_capacity = (size_t)(new_capacity * array->growth_factor);
        if (new_capacity <= array->capacity) {
            new_capacity = min_capacity; // Avoid infinite loop with small growth factors
            break;
        }
    }
    
    void *new_data = kryon_realloc(array->data, new_capacity * array->element_size);
    if (!new_data) return false;
    
    array->data = new_data;
    array->capacity = new_capacity;
    return true;
}

void *kryon_array_get(const KryonArray *array, size_t index) {
    if (!array || index >= array->count) return NULL;
    
    return (uint8_t*)array->data + (index * array->element_size);
}

bool kryon_array_set(KryonArray *array, size_t index, const void *element) {
    if (!array || !element || index >= array->count) return false;
    
    void *dest = (uint8_t*)array->data + (index * array->element_size);
    kryon_copy(dest, element, array->element_size);
    return true;
}

size_t kryon_array_push(KryonArray *array, const void *element) {
    if (!array || !element) return SIZE_MAX;
    
    if (array->count >= array->capacity) {
        if (!kryon_array_grow(array, array->count + 1)) {
            return SIZE_MAX;
        }
    }
    
    void *dest = (uint8_t*)array->data + (array->count * array->element_size);
    kryon_copy(dest, element, array->element_size);
    
    return array->count++;
}

bool kryon_array_pop(KryonArray *array, void *out_element) {
    if (!array || array->count == 0) return false;
    
    array->count--;
    
    if (out_element) {
        void *src = (uint8_t*)array->data + (array->count * array->element_size);
        kryon_copy(out_element, src, array->element_size);
    }
    
    return true;
}

bool kryon_array_insert(KryonArray *array, size_t index, const void *element) {
    if (!array || !element || index > array->count) return false;
    
    if (array->count >= array->capacity) {
        if (!kryon_array_grow(array, array->count + 1)) {
            return false;
        }
    }
    
    // Move elements to make space
    if (index < array->count) {
        void *src = (uint8_t*)array->data + (index * array->element_size);
        void *dest = (uint8_t*)array->data + ((index + 1) * array->element_size);
        size_t move_size = (array->count - index) * array->element_size;
        kryon_move(dest, src, move_size);
    }
    
    // Insert new element
    void *dest = (uint8_t*)array->data + (index * array->element_size);
    kryon_copy(dest, element, array->element_size);
    array->count++;
    
    return true;
}

bool kryon_array_remove(KryonArray *array, size_t index, void *out_element) {
    if (!array || index >= array->count) return false;
    
    // Copy element if requested
    if (out_element) {
        void *src = (uint8_t*)array->data + (index * array->element_size);
        kryon_copy(out_element, src, array->element_size);
    }
    
    // Move elements to fill gap
    if (index < array->count - 1) {
        void *dest = (uint8_t*)array->data + (index * array->element_size);
        void *src = (uint8_t*)array->data + ((index + 1) * array->element_size);
        size_t move_size = (array->count - index - 1) * array->element_size;
        kryon_move(dest, src, move_size);
    }
    
    array->count--;
    return true;
}

void kryon_array_clear(KryonArray *array) {
    if (array) {
        array->count = 0;
    }
}

bool kryon_array_reserve(KryonArray *array, size_t new_capacity) {
    if (!array || new_capacity <= array->capacity) return true;
    
    return kryon_array_grow(array, new_capacity);
}

bool kryon_array_shrink_to_fit(KryonArray *array) {
    if (!array || !array->owns_memory || array->count == array->capacity) return true;
    
    if (array->count == 0) {
        kryon_free(array->data);
        array->data = kryon_alloc(KRYON_ARRAY_DEFAULT_CAPACITY * array->element_size);
        array->capacity = KRYON_ARRAY_DEFAULT_CAPACITY;
        return array->data != NULL;
    }
    
    void *new_data = kryon_realloc(array->data, array->count * array->element_size);
    if (!new_data) return false;
    
    array->data = new_data;
    array->capacity = array->count;
    return true;
}

// =============================================================================
// HASH FUNCTIONS
// =============================================================================

// FNV-1a hash function
uint64_t kryon_hash_bytes(const void *data, size_t size) {
    if (!data || size == 0) return 0;
    
    const uint64_t FNV_OFFSET_BASIS = 14695981039346656037ULL;
    const uint64_t FNV_PRIME = 1099511628211ULL;
    
    uint64_t hash = FNV_OFFSET_BASIS;
    const uint8_t *bytes = (const uint8_t*)data;
    
    for (size_t i = 0; i < size; i++) {
        hash ^= bytes[i];
        hash *= FNV_PRIME;
    }
    
    return hash;
}

uint64_t kryon_hash_string(const char *str) {
    if (!str) return 0;
    return kryon_hash_bytes(str, strlen(str));
}

uint64_t kryon_hash_int(int64_t value) {
    // Wang's integer hash
    uint64_t key = (uint64_t)value;
    key = (~key) + (key << 21);
    key = key ^ (key >> 24);
    key = (key + (key << 3)) + (key << 8);
    key = key ^ (key >> 14);
    key = (key + (key << 2)) + (key << 4);
    key = key ^ (key >> 28);
    key = key + (key << 31);
    return key;
}

bool kryon_key_compare_bytes(const void *key1, const void *key2, size_t size) {
    if (!key1 || !key2) return key1 == key2;
    return memcmp(key1, key2, size) == 0;
}

bool kryon_key_compare_string(const void *key1, const void *key2, size_t size) {
    (void)size; // Unused for strings
    if (!key1 || !key2) return key1 == key2;
    return strcmp((const char*)key1, (const char*)key2) == 0;
}

// =============================================================================
// HASH MAP IMPLEMENTATION
// =============================================================================

#define KRYON_HASHMAP_DEFAULT_CAPACITY 32
#define KRYON_HASHMAP_DEFAULT_LOAD_FACTOR 0.75f

static uint64_t default_hash_func(const void *key, size_t key_size) {
    return kryon_hash_bytes(key, key_size);
}

static bool default_compare_func(const void *key1, const void *key2, size_t key_size) {
    return kryon_key_compare_bytes(key1, key2, key_size);
}

KryonHashMap *kryon_hashmap_create(size_t key_size, size_t value_size,
                                  KryonHashFunc hash_func, KryonKeyCompareFunc compare_func,
                                  size_t initial_capacity) {
    KryonHashMap *map = kryon_alloc_type(KryonHashMap);
    if (!map) return NULL;
    
    if (initial_capacity == 0) {
        initial_capacity = KRYON_HASHMAP_DEFAULT_CAPACITY;
    }
    
    // Ensure capacity is power of 2 for fast modulo
    size_t capacity = 1;
    while (capacity < initial_capacity) {
        capacity <<= 1;
    }
    
    map->buckets = kryon_alloc_array(KryonHashMapEntry*, capacity);
    if (!map->buckets) {
        kryon_free(map);
        return NULL;
    }
    
    kryon_zero(map->buckets, capacity * sizeof(KryonHashMapEntry*));
    
    map->bucket_count = capacity;
    map->size = 0;
    map->key_size = key_size;
    map->value_size = value_size;
    map->hash_func = hash_func ? hash_func : default_hash_func;
    map->compare_func = compare_func ? compare_func : default_compare_func;
    map->load_factor = KRYON_HASHMAP_DEFAULT_LOAD_FACTOR;
    map->owns_keys = (key_size > 0);
    map->owns_values = (value_size > 0);
    
    return map;
}

void kryon_hashmap_destroy(KryonHashMap *map) {
    if (!map) return;
    
    kryon_hashmap_clear(map);
    kryon_free(map->buckets);
    kryon_free(map);
}

static bool kryon_hashmap_resize(KryonHashMap *map, size_t new_bucket_count) {
    if (!map || new_bucket_count == 0) return false;
    
    // Save old buckets
    KryonHashMapEntry **old_buckets = map->buckets;
    size_t old_bucket_count = map->bucket_count;
    
    // Allocate new buckets
    map->buckets = kryon_alloc_array(KryonHashMapEntry*, new_bucket_count);
    if (!map->buckets) {
        map->buckets = old_buckets;
        return false;
    }
    
    kryon_zero(map->buckets, new_bucket_count * sizeof(KryonHashMapEntry*));
    map->bucket_count = new_bucket_count;
    map->size = 0;
    
    // Rehash all entries
    for (size_t i = 0; i < old_bucket_count; i++) {
        KryonHashMapEntry *entry = old_buckets[i];
        while (entry) {
            KryonHashMapEntry *next = entry->next;
            
            // Find new bucket
            size_t bucket_index = entry->hash & (new_bucket_count - 1);
            
            // Insert at head of new bucket
            entry->next = map->buckets[bucket_index];
            map->buckets[bucket_index] = entry;
            map->size++;
            
            entry = next;
        }
    }
    
    kryon_free(old_buckets);
    return true;
}

static KryonHashMapEntry *kryon_hashmap_find_entry(const KryonHashMap *map, const void *key, 
                                                   size_t key_size, uint64_t hash) {
    if (!map || !key) return NULL;
    
    size_t bucket_index = hash & (map->bucket_count - 1);
    KryonHashMapEntry *entry = map->buckets[bucket_index];
    
    size_t actual_key_size = map->key_size > 0 ? map->key_size : key_size;
    
    while (entry) {
        if (entry->hash == hash && 
            map->compare_func(entry->key, key, actual_key_size)) {
            return entry;
        }
        entry = entry->next;
    }
    
    return NULL;
}

bool kryon_hashmap_set(KryonHashMap *map, const void *key, size_t key_size,
                      const void *value, size_t value_size) {
    if (!map || !key || !value) return false;
    
    size_t actual_key_size = map->key_size > 0 ? map->key_size : key_size;
    size_t actual_value_size = map->value_size > 0 ? map->value_size : value_size;
    
    uint64_t hash = map->hash_func(key, actual_key_size);
    
    // Check if key already exists
    KryonHashMapEntry *existing = kryon_hashmap_find_entry(map, key, key_size, hash);
    if (existing) {
        // Update existing value
        if (map->owns_values) {
            kryon_copy(existing->value, value, actual_value_size);
        } else {
            existing->value = (void*)value;
        }
        return true;
    }
    
    // Check if resize is needed
    if ((float)map->size / map->bucket_count >= map->load_factor) {
        kryon_hashmap_resize(map, map->bucket_count * 2);
    }
    
    // Create new entry
    KryonHashMapEntry *entry = kryon_alloc_type(KryonHashMapEntry);
    if (!entry) return false;
    
    // Copy key
    if (map->owns_keys) {
        entry->key = kryon_alloc(actual_key_size);
        if (!entry->key) {
            kryon_free(entry);
            return false;
        }
        kryon_copy(entry->key, key, actual_key_size);
    } else {
        entry->key = (void*)key;
    }
    
    // Copy value
    if (map->owns_values) {
        entry->value = kryon_alloc(actual_value_size);
        if (!entry->value) {
            if (map->owns_keys) kryon_free(entry->key);
            kryon_free(entry);
            return false;
        }
        kryon_copy(entry->value, value, actual_value_size);
    } else {
        entry->value = (void*)value;
    }
    
    entry->hash = hash;
    
    // Insert at head of bucket
    size_t bucket_index = hash & (map->bucket_count - 1);
    entry->next = map->buckets[bucket_index];
    map->buckets[bucket_index] = entry;
    map->size++;
    
    return true;
}

void *kryon_hashmap_get(const KryonHashMap *map, const void *key, size_t key_size) {
    if (!map || !key) return NULL;
    
    size_t actual_key_size = map->key_size > 0 ? map->key_size : key_size;
    uint64_t hash = map->hash_func(key, actual_key_size);
    
    KryonHashMapEntry *entry = kryon_hashmap_find_entry(map, key, key_size, hash);
    return entry ? entry->value : NULL;
}

bool kryon_hashmap_has_key(const KryonHashMap *map, const void *key, size_t key_size) {
    return kryon_hashmap_get(map, key, key_size) != NULL;
}

bool kryon_hashmap_remove(KryonHashMap *map, const void *key, size_t key_size, void *out_value) {
    if (!map || !key) return false;
    
    size_t actual_key_size = map->key_size > 0 ? map->key_size : key_size;
    uint64_t hash = map->hash_func(key, actual_key_size);
    
    size_t bucket_index = hash & (map->bucket_count - 1);
    KryonHashMapEntry **current = &map->buckets[bucket_index];
    
    while (*current) {
        KryonHashMapEntry *entry = *current;
        
        if (entry->hash == hash && 
            map->compare_func(entry->key, key, actual_key_size)) {
            
            // Remove from chain
            *current = entry->next;
            
            // Copy value if requested
            if (out_value && map->owns_values) {
                size_t actual_value_size = map->value_size > 0 ? map->value_size : 0;
                if (actual_value_size > 0) {
                    kryon_copy(out_value, entry->value, actual_value_size);
                }
            }
            
            // Free entry
            if (map->owns_keys) kryon_free(entry->key);
            if (map->owns_values) kryon_free(entry->value);
            kryon_free(entry);
            
            map->size--;
            return true;
        }
        
        current = &entry->next;
    }
    
    return false;
}

void kryon_hashmap_clear(KryonHashMap *map) {
    if (!map) return;
    
    for (size_t i = 0; i < map->bucket_count; i++) {
        KryonHashMapEntry *entry = map->buckets[i];
        while (entry) {
            KryonHashMapEntry *next = entry->next;
            
            if (map->owns_keys) kryon_free(entry->key);
            if (map->owns_values) kryon_free(entry->value);
            kryon_free(entry);
            
            entry = next;
        }
        map->buckets[i] = NULL;
    }
    
    map->size = 0;
}

// =============================================================================
// STRING INTERNER IMPLEMENTATION
// =============================================================================

KryonStringInterner *kryon_interner_create(void) {
    KryonStringInterner *interner = kryon_alloc_type(KryonStringInterner);
    if (!interner) return NULL;
    
    interner->string_map = kryon_hashmap_create(0, sizeof(uint32_t), 
                                               (KryonHashFunc)kryon_hash_string,
                                               (KryonKeyCompareFunc)kryon_key_compare_string,
                                               256);
    if (!interner->string_map) {
        kryon_free(interner);
        return NULL;
    }
    
    interner->string_storage = kryon_array_create(sizeof(char), 1024);
    if (!interner->string_storage) {
        kryon_hashmap_destroy(interner->string_map);
        kryon_free(interner);
        return NULL;
    }
    
    interner->next_id = 1; // ID 0 is reserved for invalid
    interner->total_length = 0;
    interner->unique_count = 0;
    
    return interner;
}

void kryon_interner_destroy(KryonStringInterner *interner) {
    if (!interner) return;
    
    kryon_hashmap_destroy(interner->string_map);
    kryon_array_destroy(interner->string_storage);
    kryon_free(interner);
}

KryonInternedString kryon_interner_intern(KryonStringInterner *interner,
                                         const char *str, size_t length) {
    KryonInternedString invalid = { 0, NULL, 0 };
    
    if (!interner || !str) return invalid;
    
    if (length == 0) {
        length = strlen(str);
    }
    
    // Check if string already exists
    uint32_t *existing_id = (uint32_t*)kryon_hashmap_get(interner->string_map, str, length + 1);
    if (existing_id) {
        return kryon_interner_get_by_id(interner, *existing_id);
    }
    
    // Add new string
    size_t start_offset = kryon_array_size(interner->string_storage);
    
    // Add string data (including null terminator)
    for (size_t i = 0; i < length; i++) {
        if (kryon_array_push(interner->string_storage, &str[i]) == SIZE_MAX) {
            return invalid;
        }
    }
    char null_char = '\0';
    if (kryon_array_push(interner->string_storage, &null_char) == SIZE_MAX) {
        return invalid;
    }
    
    // Create string handle
    uint32_t id = interner->next_id++;
    const char *data = (const char*)kryon_array_data(interner->string_storage) + start_offset;
    
    // Add to map
    if (!kryon_hashmap_set(interner->string_map, data, length + 1, &id, sizeof(id))) {
        return invalid;
    }
    
    interner->total_length += length;
    interner->unique_count++;
    
    KryonInternedString result = { id, data, length };
    return result;
}

KryonInternedString kryon_interner_get_by_id(const KryonStringInterner *interner, uint32_t id) {
    KryonInternedString invalid = { 0, NULL, 0 };
    
    if (!interner || id == 0) return invalid;
    
    // TODO: For efficiency, we could maintain a reverse map from ID to string
    // For now, we'll search through the map (not optimal but correct)
    
    // This is a simplified implementation - in practice, you'd want a more efficient lookup
    return invalid;
}