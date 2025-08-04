/**
 * @file krb_schema.h
 * @brief KRB Format Schema Definitions
 * 
 * Centralized format definitions to ensure writer/loader consistency.
 * This prevents format mismatches by defining the exact binary layout.
 */

#ifndef KRYON_KRB_SCHEMA_H
#define KRYON_KRB_SCHEMA_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// =============================================================================
// KRB FORMAT VERSION
// =============================================================================

#define KRB_FORMAT_VERSION_MAJOR 0
#define KRB_FORMAT_VERSION_MINOR 1
#define KRB_FORMAT_VERSION_PATCH 0

// =============================================================================
// SECTION MAGIC NUMBERS
// =============================================================================

#define KRB_MAGIC_KRYN      0x4B52594E  // "KRYN" - File header
#define KRB_MAGIC_FUNC      0x46554E43  // "FUNC" - Function definition
#define KRB_MAGIC_COMP      0x434F4D50  // "COMP" - Component definition
#define KRB_MAGIC_VARS      0x56415253  // "VARS" - Variables section
#define KRB_MAGIC_EVENTS    0x4556544E  // "EVNT" - Events section
#define KRB_MAGIC_STYLES    0x5354594C  // "STYL" - Styles section

// =============================================================================
// KRB FUNCTION FORMAT (Binary Layout)
// =============================================================================

/**
 * @brief KRB Function Definition Binary Format
 * 
 * This structure defines the exact binary layout for function definitions
 * in KRB files. Both writer and loader MUST follow this format exactly.
 */
typedef struct {
    uint32_t magic;           // KRB_MAGIC_FUNC (0x46554E43)
    uint32_t lang_ref;        // String table index for language ("lua", "js", etc.)
    uint32_t name_ref;        // String table index for function name
    uint16_t param_count;     // Number of parameters
    // uint32_t param_refs[param_count];  // String table indices for parameter names
    uint16_t flags;           // Function flags (0 for now)
    uint32_t code_ref;        // String table index for function code/bytecode
} KRBFunctionHeader;

/**
 * @brief KRB Function Flags
 */
typedef enum {
    KRB_FUNC_FLAG_NONE       = 0x0000,
    KRB_FUNC_FLAG_ASYNC      = 0x0001,
    KRB_FUNC_FLAG_EXPORTED   = 0x0002,
    KRB_FUNC_FLAG_BYTECODE   = 0x0004,  // Code is bytecode, not source
} KRBFunctionFlags;

// =============================================================================
// KRB ELEMENT FORMAT (Binary Layout)
// =============================================================================

/**
 * @brief KRB Element Instance Binary Format
 */
typedef struct {
    uint32_t instance_id;     // Unique instance ID
    uint32_t element_type;    // Element type hex code
    uint32_t parent_id;       // Parent instance ID (0 for root)
    uint32_t style_ref;       // Style reference ID (0 for none)
    uint16_t property_count;  // Number of properties
    uint16_t child_count;     // Number of child elements
    uint16_t event_count;     // Number of event handlers
    uint32_t flags;           // Element flags
    // Properties follow: KRBProperty[property_count]
    // Child IDs follow: uint32_t[child_count]
    // Event handlers follow: KRBEventHandler[event_count]
} KRBElementHeader;

/**
 * @brief KRB Property Binary Format
 */
typedef struct {
    uint16_t property_id;     // Property hex code
    // Value data follows (format depends on property type hint)
} KRBPropertyHeader;

// =============================================================================
// SCHEMA VALIDATION FUNCTIONS
// =============================================================================

/**
 * @brief Validate KRB function header format
 * @param data Binary data
 * @param size Data size
 * @param offset Current offset (updated)
 * @param header Output header structure
 * @return true if valid format
 */
bool krb_validate_function_header(const uint8_t *data, size_t size, size_t *offset, KRBFunctionHeader *header);

/**
 * @brief Write KRB function header with validation
 * @param writer Writer function (e.g., write_uint32)
 * @param context Writer context
 * @param header Header to write
 * @return true on success
 */
bool krb_write_function_header(bool (*writer)(void*, const void*, size_t), void *context, const KRBFunctionHeader *header);

/**
 * @brief Get expected size for KRB function with parameters
 * @param param_count Number of parameters
 * @return Size in bytes
 */
size_t krb_function_size(uint16_t param_count);

#ifdef __cplusplus
}
#endif

#endif // KRYON_KRB_SCHEMA_H