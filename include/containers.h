/**
 * @file containers.h
 * @brief Kryon Container Data Structures
 * 
 * High-performance container types with type safety and memory efficiency.
 * Includes dynamic arrays, hash maps, and other common data structures.
 * 
 * @version 1.0.0
 * @author Kryon Labs
 */

#ifndef KRYON_INTERNAL_CONTAINERS_H
#define KRYON_INTERNAL_CONTAINERS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

// =============================================================================
// FORWARD DECLARATIONS
// =============================================================================

typedef struct KryonArray KryonArray;
typedef struct KryonHashMap KryonHashMap;
typedef struct KryonHashMapEntry KryonHashMapEntry;
typedef struct KryonString KryonString;
typedef struct KryonStringInterner KryonStringInterner;

// =============================================================================
// DYNAMIC ARRAY
// =============================================================================

/**
 * @brief Dynamic array with automatic resizing
 */
struct KryonArray {
    void *data;           ///< Pointer to array data
    size_t count;         ///< Number of elements
    size_t capacity;      ///< Current capacity
    size_t element_size;  ///< Size of each element in bytes
    float growth_factor;  ///< Growth factor for resizing (default 1.5)
    bool owns_memory;     ///< Whether array owns the memory
};

/**
 * @brief Create a new dynamic array
 * @param element_size Size of each element in bytes
 * @param initial_capacity Initial capacity (0 for default)
 * @return Pointer to new array, or NULL on failure
 */
KryonArray *kryon_array_create(size_t element_size, size_t initial_capacity);

/**
 * @brief Destroy a dynamic array
 * @param array Array to destroy
 */
void kryon_array_destroy(KryonArray *array);

/**
 * @brief Get element at index
 * @param array The array
 * @param index Element index
 * @return Pointer to element, or NULL if out of bounds
 */
void *kryon_array_get(const KryonArray *array, size_t index);

/**
 * @brief Set element at index
 * @param array The array
 * @param index Element index
 * @param element Pointer to element data
 * @return true on success, false on failure
 */
bool kryon_array_set(KryonArray *array, size_t index, const void *element);

/**
 * @brief Add element to end of array
 * @param array The array
 * @param element Pointer to element data
 * @return Index of new element, or SIZE_MAX on failure
 */
size_t kryon_array_push(KryonArray *array, const void *element);

/**
 * @brief Remove and return last element
 * @param array The array
 * @param out_element Output buffer for element (can be NULL)
 * @return true if element was removed, false if array is empty
 */
bool kryon_array_pop(KryonArray *array, void *out_element);

/**
 * @brief Insert element at index
 * @param array The array
 * @param index Index to insert at
 * @param element Pointer to element data
 * @return true on success, false on failure
 */
bool kryon_array_insert(KryonArray *array, size_t index, const void *element);

/**
 * @brief Remove element at index
 * @param array The array
 * @param index Index to remove
 * @param out_element Output buffer for removed element (can be NULL)
 * @return true on success, false if index out of bounds
 */
bool kryon_array_remove(KryonArray *array, size_t index, void *out_element);

/**
 * @brief Clear all elements from array
 * @param array The array
 */
void kryon_array_clear(KryonArray *array);

/**
 * @brief Reserve capacity for array
 * @param array The array
 * @param new_capacity New capacity
 * @return true on success, false on failure
 */
bool kryon_array_reserve(KryonArray *array, size_t new_capacity);

/**
 * @brief Shrink array capacity to fit current size
 * @param array The array
 * @return true on success, false on failure
 */
bool kryon_array_shrink_to_fit(KryonArray *array);

/**
 * @brief Get array size
 * @param array The array
 * @return Number of elements in array
 */
static inline size_t kryon_array_size(const KryonArray *array) {
    return array ? array->count : 0;
}

/**
 * @brief Check if array is empty
 * @param array The array
 * @return true if array is empty
 */
static inline bool kryon_array_empty(const KryonArray *array) {
    return array ? array->count == 0 : true;
}

/**
 * @brief Get direct pointer to array data
 * @param array The array
 * @return Pointer to array data
 */
static inline void *kryon_array_data(const KryonArray *array) {
    return array ? array->data : NULL;
}

// =============================================================================
// HASH MAP
// =============================================================================

/**
 * @brief Hash function type
 */
typedef uint64_t (*KryonHashFunc)(const void *key, size_t key_size);

/**
 * @brief Key comparison function type
 */
typedef bool (*KryonKeyCompareFunc)(const void *key1, const void *key2, size_t key_size);

/**
 * @brief Hash map entry
 */
struct KryonHashMapEntry {
    void *key;                      ///< Key data
    void *value;                    ///< Value data
    uint64_t hash;                  ///< Cached hash value
    struct KryonHashMapEntry *next; ///< Next entry in chain
};

/**
 * @brief Hash map with separate chaining
 */
struct KryonHashMap {
    KryonHashMapEntry **buckets;    ///< Array of bucket heads
    size_t bucket_count;            ///< Number of buckets
    size_t size;                    ///< Number of key-value pairs
    size_t key_size;                ///< Size of keys in bytes (0 for variable)
    size_t value_size;              ///< Size of values in bytes (0 for variable)
    KryonHashFunc hash_func;        ///< Hash function
    KryonKeyCompareFunc compare_func; ///< Key comparison function
    float load_factor;              ///< Load factor threshold for resizing
    bool owns_keys;                 ///< Whether map owns key memory
    bool owns_values;               ///< Whether map owns value memory
};

/**
 * @brief Create a new hash map
 * @param key_size Size of keys in bytes (0 for variable-size keys)
 * @param value_size Size of values in bytes (0 for variable-size values)
 * @param hash_func Hash function (NULL for default)
 * @param compare_func Key comparison function (NULL for default)
 * @param initial_capacity Initial number of buckets (0 for default)
 * @return Pointer to new hash map, or NULL on failure
 */
KryonHashMap *kryon_hashmap_create(size_t key_size, size_t value_size,
                                  KryonHashFunc hash_func, KryonKeyCompareFunc compare_func,
                                  size_t initial_capacity);

/**
 * @brief Destroy a hash map
 * @param map Hash map to destroy
 */
void kryon_hashmap_destroy(KryonHashMap *map);

/**
 * @brief Insert or update key-value pair
 * @param map The hash map
 * @param key Pointer to key data
 * @param key_size Size of key (only used for variable-size keys)
 * @param value Pointer to value data
 * @param value_size Size of value (only used for variable-size values)
 * @return true on success, false on failure
 */
bool kryon_hashmap_set(KryonHashMap *map, const void *key, size_t key_size,
                      const void *value, size_t value_size);

/**
 * @brief Get value for key
 * @param map The hash map
 * @param key Pointer to key data
 * @param key_size Size of key (only used for variable-size keys)
 * @return Pointer to value, or NULL if key not found
 */
void *kryon_hashmap_get(const KryonHashMap *map, const void *key, size_t key_size);

/**
 * @brief Check if key exists in map
 * @param map The hash map
 * @param key Pointer to key data
 * @param key_size Size of key (only used for variable-size keys)
 * @return true if key exists, false otherwise
 */
bool kryon_hashmap_has_key(const KryonHashMap *map, const void *key, size_t key_size);

/**
 * @brief Remove key-value pair
 * @param map The hash map
 * @param key Pointer to key data
 * @param key_size Size of key (only used for variable-size keys)
 * @param out_value Output buffer for removed value (can be NULL)
 * @return true if key was removed, false if key not found
 */
bool kryon_hashmap_remove(KryonHashMap *map, const void *key, size_t key_size, void *out_value);

/**
 * @brief Clear all key-value pairs
 * @param map The hash map
 */
void kryon_hashmap_clear(KryonHashMap *map);

/**
 * @brief Get hash map size
 * @param map The hash map
 * @return Number of key-value pairs
 */
static inline size_t kryon_hashmap_size(const KryonHashMap *map) {
    return map ? map->size : 0;
}

/**
 * @brief Check if hash map is empty
 * @param map The hash map
 * @return true if map is empty
 */
static inline bool kryon_hashmap_empty(const KryonHashMap *map) {
    return map ? map->size == 0 : true;
}

// =============================================================================
// STRING INTERNING
// =============================================================================

/**
 * @brief Interned string handle
 */
typedef struct {
    uint32_t id;        ///< Unique string ID
    const char *data;   ///< Pointer to string data (read-only)
    size_t length;      ///< String length
} KryonInternedString;

/**
 * @brief String interner for memory-efficient string storage
 */
struct KryonStringInterner {
    KryonHashMap *string_map;    ///< Map from string to ID
    KryonArray *string_storage;  ///< Array of string data
    uint32_t next_id;            ///< Next available string ID
    size_t total_length;         ///< Total length of all strings
    size_t unique_count;         ///< Number of unique strings
};

/**
 * @brief Create a new string interner
 * @return Pointer to new string interner, or NULL on failure
 */
KryonStringInterner *kryon_interner_create(void);

/**
 * @brief Destroy a string interner
 * @param interner String interner to destroy
 */
void kryon_interner_destroy(KryonStringInterner *interner);

/**
 * @brief Intern a string
 * @param interner The string interner
 * @param str String to intern
 * @param length String length (or 0 to calculate)
 * @return Interned string handle
 */
KryonInternedString kryon_interner_intern(KryonStringInterner *interner,
                                         const char *str, size_t length);

/**
 * @brief Get string by ID
 * @param interner The string interner
 * @param id String ID
 * @return Interned string handle, or invalid handle if ID not found
 */
KryonInternedString kryon_interner_get_by_id(const KryonStringInterner *interner, uint32_t id);

/**
 * @brief Check if interned string is valid
 * @param str Interned string handle
 * @return true if string is valid
 */
static inline bool kryon_interned_string_valid(KryonInternedString str) {
    return str.id != 0 && str.data != NULL;
}

/**
 * @brief Compare two interned strings
 * @param str1 First string
 * @param str2 Second string
 * @return true if strings are equal (O(1) comparison)
 */
static inline bool kryon_interned_string_equal(KryonInternedString str1, KryonInternedString str2) {
    return str1.id == str2.id;
}

// =============================================================================
// UTILITY FUNCTIONS
// =============================================================================

/**
 * @brief Default hash function for byte data
 * @param data Pointer to data
 * @param size Size of data in bytes
 * @return Hash value
 */
uint64_t kryon_hash_bytes(const void *data, size_t size);

/**
 * @brief Hash function for C strings
 * @param str C string to hash
 * @return Hash value
 */
uint64_t kryon_hash_string(const char *str);

/**
 * @brief Hash function for integers
 * @param value Integer value
 * @return Hash value
 */
uint64_t kryon_hash_int(int64_t value);

/**
 * @brief Default key comparison for byte data
 * @param key1 First key
 * @param key2 Second key
 * @param size Size of keys in bytes
 * @return true if keys are equal
 */
bool kryon_key_compare_bytes(const void *key1, const void *key2, size_t size);

/**
 * @brief Key comparison for C strings
 * @param key1 First string
 * @param key2 Second string
 * @param size Unused (strings are null-terminated)
 * @return true if strings are equal
 */
bool kryon_key_compare_string(const void *key1, const void *key2, size_t size);

// =============================================================================
// TYPE-SAFE MACROS
// =============================================================================

/**
 * @brief Create typed dynamic array
 */
#define KRYON_ARRAY_DEFINE(name, type) \
    typedef struct { \
        KryonArray *impl; \
    } name; \
    \
    static inline name name##_create(size_t initial_capacity) { \
        name arr = { kryon_array_create(sizeof(type), initial_capacity) }; \
        return arr; \
    } \
    \
    static inline void name##_destroy(name *arr) { \
        if (arr && arr->impl) { \
            kryon_array_destroy(arr->impl); \
            arr->impl = NULL; \
        } \
    } \
    \
    static inline type *name##_get(const name *arr, size_t index) { \
        return (type*)kryon_array_get(arr->impl, index); \
    } \
    \
    static inline bool name##_set(name *arr, size_t index, const type *element) { \
        return kryon_array_set(arr->impl, index, element); \
    } \
    \
    static inline size_t name##_push(name *arr, const type *element) { \
        return kryon_array_push(arr->impl, element); \
    } \
    \
    static inline bool name##_pop(name *arr, type *out_element) { \
        return kryon_array_pop(arr->impl, out_element); \
    } \
    \
    static inline size_t name##_size(const name *arr) { \
        return kryon_array_size(arr->impl); \
    } \
    \
    static inline bool name##_empty(const name *arr) { \
        return kryon_array_empty(arr->impl); \
    } \
    \
    static inline type *name##_data(const name *arr) { \
        return (type*)kryon_array_data(arr->impl); \
    }

/**
 * @brief Create typed hash map
 */
#define KRYON_HASHMAP_DEFINE(name, key_type, value_type) \
    typedef struct { \
        KryonHashMap *impl; \
    } name; \
    \
    static inline name name##_create(size_t initial_capacity) { \
        name map = { kryon_hashmap_create(sizeof(key_type), sizeof(value_type), \
                                         NULL, NULL, initial_capacity) }; \
        return map; \
    } \
    \
    static inline void name##_destroy(name *map) { \
        if (map && map->impl) { \
            kryon_hashmap_destroy(map->impl); \
            map->impl = NULL; \
        } \
    } \
    \
    static inline bool name##_set(name *map, const key_type *key, const value_type *value) { \
        return kryon_hashmap_set(map->impl, key, sizeof(key_type), value, sizeof(value_type)); \
    } \
    \
    static inline value_type *name##_get(const name *map, const key_type *key) { \
        return (value_type*)kryon_hashmap_get(map->impl, key, sizeof(key_type)); \
    } \
    \
    static inline bool name##_has_key(const name *map, const key_type *key) { \
        return kryon_hashmap_has_key(map->impl, key, sizeof(key_type)); \
    } \
    \
    static inline bool name##_remove(name *map, const key_type *key, value_type *out_value) { \
        return kryon_hashmap_remove(map->impl, key, sizeof(key_type), out_value); \
    } \
    \
    static inline size_t name##_size(const name *map) { \
        return kryon_hashmap_size(map->impl); \
    } \
    \
    static inline bool name##_empty(const name *map) { \
        return kryon_hashmap_empty(map->impl); \
    }

#ifdef __cplusplus
}
#endif

#endif // KRYON_INTERNAL_CONTAINERS_H