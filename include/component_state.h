/**
 * @file component_state.h
 * @brief Kryon Component State Management System
 *
 * Provides type-safe, isolated state management for component instances
 * with unique ID generation and hash-based lookup.
 *
 * 0BSD License
 */

#ifndef KRYON_COMPONENT_STATE_H
#define KRYON_COMPONENT_STATE_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "types.h"

// Forward declarations
struct KryonRuntime;
struct KryonElement;

#ifdef __cplusplus
extern "C" {
#endif

// =============================================================================
//  Component ID Generation
// =============================================================================

/**
 * @brief Generates a unique component ID.
 * @param component_name Base component name (optional, can be NULL)
 * @param user_id User-provided ID (optional, can be NULL)
 * @return Allocated unique component ID string (caller must free)
 */
char* kryon_component_generate_unique_id(const char *component_name, const char *user_id);

/**
 * @brief Validates if a component ID is unique within the runtime.
 * @param runtime Runtime context
 * @param component_id Component ID to validate
 * @return true if unique, false if already exists
 */
bool kryon_component_is_id_unique(struct KryonRuntime *runtime, const char *component_id);

// =============================================================================
//  Component State Table Management
// =============================================================================

/**
 * @brief Creates a new component state table.
 * @param component_id Unique component instance identifier
 * @return Pointer to state table or NULL on failure
 */
KryonComponentStateTable* kryon_component_state_table_create(const char *component_id);

/**
 * @brief Destroys a component state table and frees all resources.
 * @param table State table to destroy
 */
void kryon_component_state_table_destroy(KryonComponentStateTable *table);

/**
 * @brief Sets a string state variable.
 * @param table State table
 * @param name Variable name
 * @param value String value
 * @return true on success, false on failure
 */
bool kryon_component_state_set_string(KryonComponentStateTable *table, const char *name, const char *value);

/**
 * @brief Gets a string state variable.
 * @param table State table
 * @param name Variable name
 * @return String value or NULL if not found
 */
const char* kryon_component_state_get_string(KryonComponentStateTable *table, const char *name);

/**
 * @brief Sets an integer state variable.
 * @param table State table
 * @param name Variable name
 * @param value Integer value
 * @return true on success, false on failure
 */
bool kryon_component_state_set_int(KryonComponentStateTable *table, const char *name, int64_t value);

/**
 * @brief Gets an integer state variable.
 * @param table State table
 * @param name Variable name
 * @param out_value Pointer to store the result
 * @return true if found, false if not found
 */
bool kryon_component_state_get_int(KryonComponentStateTable *table, const char *name, int64_t *out_value);

/**
 * @brief Sets a float state variable.
 * @param table State table
 * @param name Variable name
 * @param value Float value
 * @return true on success, false on failure
 */
bool kryon_component_state_set_float(KryonComponentStateTable *table, const char *name, double value);

/**
 * @brief Gets a float state variable.
 * @param table State table
 * @param name Variable name
 * @param out_value Pointer to store the result
 * @return true if found, false if not found
 */
bool kryon_component_state_get_float(KryonComponentStateTable *table, const char *name, double *out_value);

/**
 * @brief Sets a boolean state variable.
 * @param table State table
 * @param name Variable name
 * @param value Boolean value
 * @return true on success, false on failure
 */
bool kryon_component_state_set_bool(KryonComponentStateTable *table, const char *name, bool value);

/**
 * @brief Gets a boolean state variable.
 * @param table State table
 * @param name Variable name
 * @param out_value Pointer to store the result
 * @return true if found, false if not found
 */
bool kryon_component_state_get_bool(KryonComponentStateTable *table, const char *name, bool *out_value);

/**
 * @brief Gets a state variable as a string (regardless of type).
 * @param table State table
 * @param name Variable name
 * @return String representation of the value or NULL if not found
 */
char* kryon_component_state_get_as_string(KryonComponentStateTable *table, const char *name);

/**
 * @brief Checks if a state variable exists.
 * @param table State table
 * @param name Variable name
 * @return true if exists, false if not found
 */
bool kryon_component_state_has_variable(KryonComponentStateTable *table, const char *name);

/**
 * @brief Removes a state variable.
 * @param table State table
 * @param name Variable name
 * @return true if removed, false if not found
 */
bool kryon_component_state_remove_variable(KryonComponentStateTable *table, const char *name);

// =============================================================================
//  Component Instance Management
// =============================================================================

/**
 * @brief Creates a new component instance with unique ID.
 * @param runtime Runtime context
 * @param definition Component definition
 * @param user_id User-provided ID (optional)
 * @param parent_element Parent element (optional)
 * @return Component instance or NULL on failure
 */
KryonComponentInstance* kryon_component_instance_create(struct KryonRuntime *runtime,
                                                      KryonComponentDefinition *definition,
                                                      const char *user_id,
                                                      struct KryonElement *parent_element);

/**
 * @brief Destroys a component instance.
 * @param instance Component instance to destroy
 */
void kryon_component_instance_destroy(KryonComponentInstance *instance);

/**
 * @brief Finds a component instance by its unique ID.
 * @param runtime Runtime context
 * @param component_id Component instance ID
 * @return Component instance or NULL if not found
 */
KryonComponentInstance* kryon_component_instance_find_by_id(struct KryonRuntime *runtime, const char *component_id);

/**
 * @brief Initializes component state from definition defaults.
 * @param instance Component instance to initialize
 * @return true on success, false on failure
 */
bool kryon_component_instance_initialize_state(KryonComponentInstance *instance);

// =============================================================================
//  State Resolution
// =============================================================================

/**
 * @brief Resolves a state variable using scoped path resolution.
 * @param runtime Runtime context
 * @param element Element to start resolution from
 * @param state_path State path (e.g., "component_id.variable_name" or "variable_name")
 * @return State value as string or NULL if not found
 */
const char* kryon_resolve_scoped_state_variable(struct KryonRuntime *runtime,
                                              struct KryonElement *element,
                                              const char *state_path);

/**
 * @brief Gets the component state path for a variable.
 * @param component_id Component instance ID
 * @param variable_name State variable name
 * @return Allocated path string (caller must free)
 */
char* kryon_component_get_state_path(const char *component_id, const char *variable_name);

#ifdef __cplusplus
}
#endif

#endif // KRYON_COMPONENT_STATE_H