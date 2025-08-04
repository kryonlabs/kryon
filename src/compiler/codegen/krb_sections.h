/**
 * @file krb_sections.h
 * @brief KRB format section definitions and extended types
 * 
 * Defines the extended KRB format sections for Phase 4 directive serialization,
 * including reactive variable types and event handler sections.
 */

#ifndef KRYON_KRB_SECTIONS_H
#define KRYON_KRB_SECTIONS_H

#include <stdint.h>
#include "../../shared/krb_schema.h"

// =============================================================================
// KRB SECTION MAGIC NUMBERS
// =============================================================================

// Use schema-defined magic numbers for consistency
#define KRB_MAGIC_HEADER    KRB_MAGIC_KRYN      // "KRYN"
// Metadata is merged into App properties - no separate section needed
#define KRB_MAGIC_VARIABLES KRB_MAGIC_VARS      // "VARS"
#define KRB_MAGIC_FUNCTIONS KRB_MAGIC_FUNC      // "FUNC"
#define KRB_MAGIC_COMPONENT KRB_MAGIC_COMP      // "COMP"
#define KRB_MAGIC_ELEMENTS  0x454C454D          // "ELEM"
#define KRB_MAGIC_STYLES    0x5354594C          // "STYL" - Use hex value directly
#define KRB_MAGIC_EVENTS    0x4556544E          // "EVNT" - Use hex value directly
#define KRB_MAGIC_END       0x454E4446          // "ENDF"

// =============================================================================
// EXTENDED VARIABLE TYPES (Phase 4)
// =============================================================================

typedef enum {
    // Static variable types (0x00 - 0x0F)
    KRYON_VAR_STATIC_STRING   = 0x00,
    KRYON_VAR_STATIC_INTEGER  = 0x01,
    KRYON_VAR_STATIC_FLOAT    = 0x02,
    KRYON_VAR_STATIC_BOOLEAN  = 0x03,
    KRYON_VAR_STATIC_ARRAY    = 0x04,
    KRYON_VAR_STATIC_OBJECT   = 0x05,
    KRYON_VAR_STATIC_NULL     = 0x06,
    
    // Reactive variable types (0x10 - 0x1F) - trigger re-render on change
    KRYON_VAR_REACTIVE_STRING   = 0x10,
    KRYON_VAR_REACTIVE_INTEGER  = 0x11,
    KRYON_VAR_REACTIVE_FLOAT    = 0x12,
    KRYON_VAR_REACTIVE_BOOLEAN  = 0x13,
    KRYON_VAR_REACTIVE_ARRAY    = 0x14,
    KRYON_VAR_REACTIVE_OBJECT   = 0x15,
    
    // Computed variable types (0x20 - 0x2F) - derived from other variables
    KRYON_VAR_COMPUTED_STRING   = 0x20,
    KRYON_VAR_COMPUTED_INTEGER  = 0x21,
    KRYON_VAR_COMPUTED_FLOAT    = 0x22,
    KRYON_VAR_COMPUTED_BOOLEAN  = 0x23,
    
    // Reference types (0x30 - 0x3F)
    KRYON_VAR_REF_COMPONENT     = 0x30,
    KRYON_VAR_REF_ELEMENT       = 0x31,
    KRYON_VAR_REF_FUNCTION      = 0x32
} KryonVariableType;

// =============================================================================
// VARIABLE FLAGS
// =============================================================================

typedef enum {
    KRYON_VAR_FLAG_NONE       = 0x00,
    KRYON_VAR_FLAG_REACTIVE   = 0x01,  // Variable triggers re-render
    KRYON_VAR_FLAG_COMPUTED   = 0x02,  // Variable is computed from others
    KRYON_VAR_FLAG_READONLY   = 0x04,  // Variable cannot be modified
    KRYON_VAR_FLAG_PRIVATE    = 0x08,  // Variable is component-private
    KRYON_VAR_FLAG_PERSISTENT = 0x10,  // Variable persists across sessions
    KRYON_VAR_FLAG_GLOBAL     = 0x20,  // Variable is globally accessible
    KRYON_VAR_FLAG_LAZY       = 0x40,  // Variable is lazy-evaluated
    KRYON_VAR_FLAG_DIRTY      = 0x80   // Variable has changed (runtime)
} KryonVariableFlags;

// =============================================================================
// EVENT HANDLER TYPES
// =============================================================================
// Note: We use the existing KryonEventType from events.h
// This section just documents the serialization values

// =============================================================================
// HELPER FUNCTIONS
// =============================================================================

#include "internal/events.h"  // For KryonEventType

/**
 * @brief Check if a variable type is reactive
 * @param type Variable type
 * @return true if reactive, false otherwise
 */
static inline bool kryon_var_is_reactive(KryonVariableType type) {
    return (type >= KRYON_VAR_REACTIVE_STRING && type <= KRYON_VAR_REACTIVE_OBJECT);
}

/**
 * @brief Check if a variable type is computed
 * @param type Variable type
 * @return true if computed, false otherwise
 */
static inline bool kryon_var_is_computed(KryonVariableType type) {
    return (type >= KRYON_VAR_COMPUTED_STRING && type <= KRYON_VAR_COMPUTED_BOOLEAN);
}

/**
 * @brief Convert static variable type to reactive
 * @param type Static variable type
 * @return Reactive variable type
 */
static inline KryonVariableType kryon_var_to_reactive(KryonVariableType type) {
    if (type <= KRYON_VAR_STATIC_OBJECT) {
        return (KryonVariableType)(type + 0x10);
    }
    return type;
}

/**
 * @brief Get base type from reactive/computed type
 * @param type Variable type
 * @return Base variable type
 */
static inline KryonVariableType kryon_var_base_type(KryonVariableType type) {
    if (type >= KRYON_VAR_REACTIVE_STRING && type <= KRYON_VAR_REACTIVE_OBJECT) {
        return (KryonVariableType)(type - 0x10);
    }
    if (type >= KRYON_VAR_COMPUTED_STRING && type <= KRYON_VAR_COMPUTED_BOOLEAN) {
        return (KryonVariableType)(type - 0x20);
    }
    return type;
}

#endif // KRYON_KRB_SECTIONS_H