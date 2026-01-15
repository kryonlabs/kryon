/**
 * Kryon IR - Shared Constants
 *
 * Centralized magic numbers and constants used across the IR codebase.
 */

#ifndef IR_CONSTANTS_H
#define IR_CONSTANTS_H

#include <stdint.h>

// =============================================================================
// Array Growth Constants
// =============================================================================

#define IR_DEFAULT_ARRAY_CAPACITY 8
#define IR_MAX_CAPACITY UINT32_MAX

/**
 * Safe capacity doubling with overflow check
 *
 * @param cap Pointer to capacity value to double
 * @return true on success, false if overflow would occur
 */
static inline bool ir_grow_capacity(uint32_t* cap) {
    if (*cap > UINT32_MAX / 2) return false;
    *cap *= 2;
    return true;
}

// =============================================================================
// String Buffer Constants
// =============================================================================

#define IR_TEMP_BUFFER_SIZE 4096
#define IR_PATH_BUFFER_SIZE 1024
#define IR_SMALL_BUFFER_SIZE 256

// =============================================================================
// Directory Traversal Constants
// =============================================================================

#define IR_MAX_DIRECTORY_DEPTH 5
#define IR_MAX_NESTING_DEPTH 256

// =============================================================================
// Parser Limits
// =============================================================================

#define IR_MAX_IDENTIFIER_LENGTH 128
#define IR_MAX_STRING_LENGTH 4096

#endif // IR_CONSTANTS_H
