/**
 * @file state.h
 * @brief Kryon State Management System Interface
 * 
 * 0BSD License
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted.
 */

#ifndef KRYON_STATE_H
#define KRYON_STATE_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// =============================================================================
//  Types and Structures
// =============================================================================

/**
 * @brief State variable types.
 */
typedef enum {
    KRYON_STATE_STRING = 0,
    KRYON_STATE_INTEGER,
    KRYON_STATE_FLOAT,
    KRYON_STATE_BOOLEAN
} KryonStateType;

/**
 * @brief State variable value union.
 */
typedef union {
    char* string_value;
    int int_value;
    float float_value;
    bool bool_value;
} KryonStateValue;

/**
 * @brief State variable structure.
 */
typedef struct KryonStateVariable {
    char* name;                          // Variable name
    KryonStateType type;                 // Variable type
    KryonStateValue value;               // Variable value
    struct KryonStateVariable* next;     // Next variable in hash chain
} KryonStateVariable;

/**
 * @brief State manager structure.
 */
typedef struct {
    KryonStateVariable** variables;     // Hash table of variables
    size_t capacity;                     // Hash table capacity
    size_t count;                        // Number of variables
} KryonStateManager;

// =============================================================================
//  State Manager API
// =============================================================================

/**
 * @brief Creates a new state manager.
 * @return Pointer to state manager or NULL on failure
 */
KryonStateManager* kryon_state_manager_create(void);

/**
 * @brief Destroys a state manager and frees all resources.
 * @param manager State manager to destroy
 */
void kryon_state_manager_destroy(KryonStateManager* manager);

/**
 * @brief Sets a string variable in the state manager.
 * @param manager State manager
 * @param name Variable name
 * @param value String value
 */
void kryon_state_manager_set_string(KryonStateManager* manager, const char* name, const char* value);

/**
 * @brief Gets a string variable from the state manager.
 * @param manager State manager
 * @param name Variable name
 * @return String value or NULL if not found
 */
const char* kryon_state_manager_get_string(KryonStateManager* manager, const char* name);

/**
 * @brief Sets an integer variable in the state manager.
 * @param manager State manager
 * @param name Variable name
 * @param value Integer value
 */
void kryon_state_manager_set_int(KryonStateManager* manager, const char* name, int value);

/**
 * @brief Gets an integer variable from the state manager.
 * @param manager State manager
 * @param name Variable name
 * @return Integer value or 0 if not found
 */
int kryon_state_manager_get_int(KryonStateManager* manager, const char* name);

/**
 * @brief Sets a float variable in the state manager.
 * @param manager State manager
 * @param name Variable name
 * @param value Float value
 */
void kryon_state_manager_set_float(KryonStateManager* manager, const char* name, float value);

/**
 * @brief Gets a float variable from the state manager.
 * @param manager State manager
 * @param name Variable name
 * @return Float value or 0.0f if not found
 */
float kryon_state_manager_get_float(KryonStateManager* manager, const char* name);

/**
 * @brief Sets a boolean variable in the state manager.
 * @param manager State manager
 * @param name Variable name
 * @param value Boolean value
 */
void kryon_state_manager_set_bool(KryonStateManager* manager, const char* name, bool value);

/**
 * @brief Gets a boolean variable from the state manager.
 * @param manager State manager
 * @param name Variable name
 * @return Boolean value or false if not found
 */
bool kryon_state_manager_get_bool(KryonStateManager* manager, const char* name);

#ifdef __cplusplus
}
#endif

#endif // KRYON_STATE_H