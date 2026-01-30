/**
 * @file limbo_runtime.h
 * @brief Limbo Runtime Integration for Kryon
 *
 * This header provides the interface between Kryon and the TaijiOS Limbo runtime (libinterp.a).
 * It allows Kryon to load Limbo modules (.dis files) and call their functions.
 *
 * Key Features:
 * - Load Limbo modules from .dis files
 * - Call Limbo functions with arguments
 * - Marshal values between Kryon and Limbo
 * - Manage Limbo runtime lifecycle
 *
 * @version 1.0.0
 * @author Kryon Labs
 */

#ifndef KRYON_LIMBO_RUNTIME_H
#define KRYON_LIMBO_RUNTIME_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

// Include TaijiOS Limbo runtime headers
// We need to include basic type definitions first
#include <sys/types.h>

// Forward declarations to avoid full interp.h dependency in header
typedef struct Module Module;
typedef struct Prog Prog;
typedef struct Modlink Modlink;
typedef struct Type Type;
typedef struct Heap Heap;

// In the .c file, we'll include the full interp.h

// =============================================================================
// LIMBO RUNTIME HANDLE
// =============================================================================

/**
 * @brief Opaque handle to the Limbo runtime
 *
 * This structure encapsulates the TaijiOS Limbo runtime state,
 * including loaded modules and runtime configuration.
 */
typedef struct {
    bool initialized;           /**< Whether runtime has been initialized */
    Module *modules[256];       /**< Cache of loaded modules */
    char *module_names[256];    /**< Names of loaded modules */
    int module_count;           /**< Number of loaded modules */
    Prog *program;              /**< Limbo program context */
    Modlink *modlink;           /**< Module link context */
} KryonLimboRuntime;

// =============================================================================
// VALUE MARSHALING
// =============================================================================

/**
 * @brief Represents a Limbo value for marshaling
 *
 * This union can hold different types of Limbo values
 * for transfer between Limbo and Kryon.
 */
typedef enum {
    KRYON_LIMBO_TYPE_NULL,      /**< Null/nil value */
    KRYON_LIMBO_TYPE_STRING,    /**< String value */
    KRYON_LIMBO_TYPE_INT,       /**< Integer value */
    KRYON_LIMBO_TYPE_REAL,      /**< Real/float value */
    KRYON_LIMBO_TYPE_MAPPING,   /**< Mapping/associative array */
    KRYON_LIMBO_TYPE_SEQUENCE,  /**< Sequence/array */
    KRYON_LIMBO_TYPE_ADT        /**< Algebraic data type */
} KryonLimboValueType;

typedef struct {
    KryonLimboValueType type;
    union {
        char *string_val;
        int64_t int_val;
        double real_val;
        struct {
            char **keys;
            char **values;
            size_t count;
        } mapping;
        struct {
            char **items;
            size_t count;
        } sequence;
        struct {
            char *type_tag;
            void *data;
        } adt;
    };
} KryonLimboValue;

// =============================================================================
// RUNTIME LIFECYCLE
// =============================================================================

/**
 * @brief Initialize the Limbo runtime
 *
 * Initializes the TaijiOS Limbo runtime, preparing it for module loading
 * and function execution. Must be called before any other Limbo functions.
 *
 * @param runtime Pointer to KryonLimboRuntime structure to initialize
 * @return true on success, false on failure
 *
 * @example
 * KryonLimboRuntime runtime;
 * if (kryon_limbo_runtime_init(&runtime)) {
 *     // Use Limbo runtime
 * }
 */
bool kryon_limbo_runtime_init(KryonLimboRuntime *runtime);

/**
 * @brief Cleanup and shutdown the Limbo runtime
 *
 * Releases all resources associated with the Limbo runtime,
 * including loaded modules and cached data.
 *
 * @param runtime Pointer to initialized KryonLimboRuntime
 */
void kryon_limbo_runtime_cleanup(KryonLimboRuntime *runtime);

// =============================================================================
// MODULE LOADING
// =============================================================================

/**
 * @brief Load a Limbo module from a .dis file
 *
 * Loads a Limbo module (like yaml.dis, json.dis) from the filesystem.
 * Modules are cached after first load.
 *
 * @param runtime Pointer to initialized KryonLimboRuntime
 * @param module_name Name to identify this module (e.g., "YAML")
 * @param path Full path to .dis file (e.g., "/dis/lib/yaml.dis")
 * @return Module handle on success, NULL on failure
 *
 * @example
 * Module *yaml = kryon_limbo_load_module(&runtime, "YAML", "/dis/lib/yaml.dis");
 */
Module *kryon_limbo_load_module(
    KryonLimboRuntime *runtime,
    const char *module_name,
    const char *path
);

/**
 * @brief Find a previously loaded module by name
 *
 * @param runtime Pointer to initialized KryonLimboRuntime
 * @param module_name Name of module to find
 * @return Module handle if found, NULL otherwise
 */
Module *kryon_limbo_find_module(
    KryonLimboRuntime *runtime,
    const char *module_name
);

// =============================================================================
// FUNCTION CALLING
// =============================================================================

/**
 * @brief Call a Limbo function in a module
 *
 * Invokes a function from a Limbo module with the provided arguments.
 * This is the main interface for executing Limbo code from Kryon.
 *
 * @param runtime Pointer to initialized KryonLimboRuntime
 * @param module The module containing the function
 * @param function_name Name of the function to call
 * @param args Array of string arguments
 * @param arg_count Number of arguments
 * @param result Pointer to store result (can be NULL if result not needed)
 * @return true on success, false on failure
 *
 * @example
 * char *args[] = {"data/default.yaml"};
 * KryonLimboValue result;
 * bool success = kryon_limbo_call_function(&runtime, yaml_mod, "loadfile", args, 1, &result);
 */
bool kryon_limbo_call_function(
    KryonLimboRuntime *runtime,
    Module *module,
    const char *function_name,
    char **args,
    int arg_count,
    KryonLimboValue *result
);

// =============================================================================
// VALUE MARSHALING
// =============================================================================

/**
 * @brief Convert a KryonLimboValue to a JSON string representation
 *
 * Converts a Limbo value to a string that can be used in Kryon.
 * Mappings become objects, sequences become arrays, etc.
 *
 * @param value The Limbo value to convert
 * @return Newly allocated string (caller must free), or NULL on error
 */
char *kryon_limbo_value_to_json(const KryonLimboValue *value);

/**
 * @brief Free a KryonLimboValue and its associated resources
 *
 * @param value The value to free
 */
void kryon_limbo_value_free(KryonLimboValue *value);

/**
 * @brief Create a string Limbo value
 *
 * @param string_val String value (will be copied)
 * @return New Limbo value
 */
KryonLimboValue kryon_limbo_value_string(const char *string_val);

/**
 * @brief Create an integer Limbo value
 *
 * @param int_val Integer value
 * @return New Limbo value
 */
KryonLimboValue kryon_limbo_value_int(int64_t int_val);

/**
 * @brief Create a null Limbo value
 *
 * @return New Limbo value representing null
 */
KryonLimboValue kryon_limbo_value_null(void);

// =============================================================================
// CONVENIENCE FUNCTIONS
// =============================================================================

/**
 * @brief Load and parse a YAML file using the YAML module
 *
 * Convenience function that loads the YAML module, parses a file,
 * and returns the data as a KryonLimboValue.
 *
 * @param runtime Pointer to initialized KryonLimboRuntime
 * @param file_path Path to YAML file to load
 * @param result Pointer to store result
 * @return true on success, false on failure
 */
bool kryon_limbo_load_yaml_file(
    KryonLimboRuntime *runtime,
    const char *file_path,
    KryonLimboValue *result
);

/**
 * @brief Get a value from a YAML/JSON mapping by key
 *
 * @param mapping The mapping value
 * @param key Key to look up
 * @return Value (caller should not free, it's owned by mapping)
 */
const char *kryon_limbo_mapping_get(const KryonLimboValue *mapping, const char *key);

/**
 * @brief Check if a mapping has a specific key
 *
 * @param mapping The mapping value
 * @param key Key to check
 * @return true if key exists, false otherwise
 */
bool kryon_limbo_mapping_has(const KryonLimboValue *mapping, const char *key);

/**
 * @brief Get the length of a sequence
 *
 * @param sequence The sequence value
 * @return Number of items in sequence
 */
size_t kryon_limbo_sequence_length(const KryonLimboValue *sequence);

/**
 * @brief Get an item from a sequence by index
 *
 * @param sequence The sequence value
 * @param index Index of item (0-based)
 * @return Value at index (caller should not free, owned by sequence)
 */
const char *kryon_limbo_sequence_get(const KryonLimboValue *sequence, size_t index);

#ifdef __cplusplus
}
#endif

#endif // KRYON_LIMBO_RUNTIME_H
