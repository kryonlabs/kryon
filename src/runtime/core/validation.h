#ifndef KRYON_VALIDATION_H
#define KRYON_VALIDATION_H

#include "runtime.h"
#include <stdbool.h>
#include <stdint.h>

/**
 * @file validation.h
 * @brief Comprehensive validation system for Kryon runtime
 */

// Validation result codes
typedef enum {
    KRYON_VALID = 0,
    KRYON_INVALID_NULL,
    KRYON_INVALID_POINTER,
    KRYON_INVALID_STRING,
    KRYON_INVALID_ELEMENT,
    KRYON_INVALID_PROPERTY,
    KRYON_INVALID_MEMORY_CORRUPTION,
    KRYON_INVALID_BOUNDS
} KryonValidationResult;

// Validation context for detailed error reporting
typedef struct {
    KryonValidationResult result;
    const char* error_message;
    const char* context;
    uintptr_t pointer_value;
} KryonValidationContext;

// Core validation functions
bool kryon_is_valid_pointer(const void* ptr);
bool kryon_is_valid_string(const char* str);
bool kryon_is_valid_string_bounded(const char* str, size_t max_len);

// Element validation functions
KryonValidationResult kryon_validate_element(const KryonElement* element, KryonValidationContext* ctx);
KryonValidationResult kryon_validate_element_deep(const KryonElement* element, KryonValidationContext* ctx);
KryonValidationResult kryon_validate_property(const KryonProperty* property, KryonValidationContext* ctx);

// Safe string operations with validation
char* kryon_safe_strdup(const char* str, KryonValidationContext* ctx);
size_t kryon_safe_strlen(const char* str);

// Error reporting
void kryon_report_validation_error(const KryonValidationContext* ctx);
const char* kryon_validation_result_to_string(KryonValidationResult result);

#endif // KRYON_VALIDATION_H