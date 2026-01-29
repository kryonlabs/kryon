
#include "lib9.h"

#include "validation.h"
#include "memory.h" 

/**
 * @brief Check if a pointer looks valid
 */
bool kryon_is_valid_pointer(const void* ptr) {
    if (!ptr) return false;
    
    uintptr_t ptr_val = (uintptr_t)ptr;
    
    // Check for common invalid pointer ranges
    if (ptr_val < 0x1000) return false;                    // NULL vicinity
    if (ptr_val > 0x7FFFFFFFFFFF) return false;            // Beyond user space
    if ((ptr_val & (sizeof(void*) - 1)) != 0) return false; // Unaligned
    
    return true;
}

/**
 * @brief Check if a string pointer is safe to use
 */
bool kryon_is_valid_string(const char* str) {
    if (!kryon_is_valid_pointer(str)) return false;
    
    // Try to find null terminator within reasonable bounds
    size_t len = strnlen(str, 65536);
    return len < 65536; // Found null terminator
}

/**
 * @brief Check string with specific bounds
 */
bool kryon_is_valid_string_bounded(const char* str, size_t max_len) {
    if (!kryon_is_valid_pointer(str)) return false;
    
    size_t len = strnlen(str, max_len);
    return len < max_len;
}

/**
 * @brief Validate a KryonProperty structure
 */
KryonValidationResult kryon_validate_property(const KryonProperty* property, KryonValidationContext* ctx) {
    if (!ctx) return KRYON_INVALID_NULL;
    
    ctx->result = KRYON_VALID;
    ctx->error_message = "Property is valid";
    ctx->context = "property_validation";
    ctx->pointer_value = (uintptr_t)property;
    
    if (!property) {
        ctx->result = KRYON_INVALID_NULL;
        ctx->error_message = "Property pointer is NULL";
        return ctx->result;
    }
    
    if (!kryon_is_valid_pointer(property)) {
        ctx->result = KRYON_INVALID_POINTER;
        ctx->error_message = "Property pointer is invalid";
        return ctx->result;
    }
    
    // Validate property name if present
    if (property->name && !kryon_is_valid_string(property->name)) {
        ctx->result = KRYON_INVALID_STRING;
        ctx->error_message = "Property name string is invalid";
        ctx->pointer_value = (uintptr_t)property->name;
        return ctx->result;
    }
    
    // Validate string properties
    if (property->type == KRYON_RUNTIME_PROP_STRING) {
        if (property->value.string_value && !kryon_is_valid_string(property->value.string_value)) {
            ctx->result = KRYON_INVALID_STRING;
            ctx->error_message = "Property string value is invalid";
            ctx->pointer_value = (uintptr_t)property->value.string_value;
            return ctx->result;
        }
    }
    
    // Validate binding path if present
    if (property->binding_path && !kryon_is_valid_string(property->binding_path)) {
        ctx->result = KRYON_INVALID_STRING;
        ctx->error_message = "Property binding path is invalid";
        ctx->pointer_value = (uintptr_t)property->binding_path;
        return ctx->result;
    }
    
    return KRYON_VALID;
}

/**
 * @brief Validate a KryonElement structure (shallow)
 */
KryonValidationResult kryon_validate_element(const KryonElement* element, KryonValidationContext* ctx) {
    if (!ctx) return KRYON_INVALID_NULL;
    
    ctx->result = KRYON_VALID;
    ctx->error_message = "Element is valid";
    ctx->context = "element_validation";
    ctx->pointer_value = (uintptr_t)element;
    
    if (!element) {
        ctx->result = KRYON_INVALID_NULL;
        ctx->error_message = "Element pointer is NULL";
        return ctx->result;
    }
    
    if (!kryon_is_valid_pointer(element)) {
        ctx->result = KRYON_INVALID_POINTER;
        ctx->error_message = "Element pointer is invalid";
        return ctx->result;
    }
    
    // Validate type_name
    if (element->type_name && !kryon_is_valid_string(element->type_name)) {
        ctx->result = KRYON_INVALID_STRING;
        ctx->error_message = "Element type_name is invalid";
        ctx->pointer_value = (uintptr_t)element->type_name;
        return ctx->result;
    }
    
    // Validate properties array if present
    if (element->properties && element->property_count > 0) {
        if (!kryon_is_valid_pointer(element->properties)) {
            ctx->result = KRYON_INVALID_POINTER;
            ctx->error_message = "Element properties array pointer is invalid";
            ctx->pointer_value = (uintptr_t)element->properties;
            return ctx->result;
        }
        
        // Check for reasonable bounds
        if (element->property_count > 1000) {
            ctx->result = KRYON_INVALID_BOUNDS;
            ctx->error_message = "Element property count exceeds reasonable bounds";
            return ctx->result;
        }
    }
    
    // Validate children array if present
    if (element->children && element->child_count > 0) {
        if (!kryon_is_valid_pointer(element->children)) {
            ctx->result = KRYON_INVALID_POINTER;
            ctx->error_message = "Element children array pointer is invalid";
            ctx->pointer_value = (uintptr_t)element->children;
            return ctx->result;
        }
        
        // Check for reasonable bounds
        if (element->child_count > 10000) {
            ctx->result = KRYON_INVALID_BOUNDS;
            ctx->error_message = "Element child count exceeds reasonable bounds";
            return ctx->result;
        }
    }
    
    return KRYON_VALID;
}

/**
 * @brief Deep validation of element including all properties and children
 */
KryonValidationResult kryon_validate_element_deep(const KryonElement* element, KryonValidationContext* ctx) {
    // First do shallow validation
    KryonValidationResult result = kryon_validate_element(element, ctx);
    if (result != KRYON_VALID) return result;
    
    // Validate all properties
    for (size_t i = 0; i < element->property_count; i++) {
        result = kryon_validate_property(element->properties[i], ctx);
        if (result != KRYON_VALID) {
            char* error_buf = kryon_alloc(256);
            if (error_buf) {
                snprint(error_buf, 256, "Property %zu validation failed: %s", i, ctx->error_message);
                ctx->error_message = error_buf;
            }
            return result;
        }
    }
    
    return KRYON_VALID;
}

/**
 * @brief Safe string duplication with validation
 */
char* kryon_safe_strdup(const char* str, KryonValidationContext* ctx) {
    if (!ctx) return NULL;
    
    ctx->result = KRYON_VALID;
    ctx->error_message = "String duplication successful";
    ctx->context = "safe_strdup";
    ctx->pointer_value = (uintptr_t)str;
    
    if (!str) {
        ctx->result = KRYON_INVALID_NULL;
        ctx->error_message = "Input string is NULL";
        return NULL;
    }
    
    if (!kryon_is_valid_string(str)) {
        ctx->result = KRYON_INVALID_STRING;
        ctx->error_message = "Input string is invalid or corrupted";
        return NULL;
    }
    
    // Use the existing kryon_strdup which now has validation
    char* result = kryon_strdup(str);
    if (!result) {
        ctx->result = KRYON_INVALID_MEMORY_CORRUPTION;
        ctx->error_message = "Failed to allocate memory for string copy";
    }
    
    return result;
}

/**
 * @brief Safe string length calculation
 */
size_t kryon_safe_strlen(const char* str) {
    if (!kryon_is_valid_string(str)) return 0;
    return strnlen(str, 65536);
}

/**
 * @brief Report validation error to console
 */
void kryon_report_validation_error(const KryonValidationContext* ctx) {
    if (!ctx) return;
    
    print("❌ VALIDATION ERROR [%s]: %s (pointer=0x%lx, result=%d)\n", 
           ctx->context ? ctx->context : "unknown",
           ctx->error_message ? ctx->error_message : "no message",
           ctx->pointer_value,
           ctx->result);
}

/**
 * @brief Convert validation result to human-readable string
 */
const char* kryon_validation_result_to_string(KryonValidationResult result) {
    switch (result) {
        case KRYON_VALID: return "Valid";
        case KRYON_INVALID_NULL: return "NULL pointer";
        case KRYON_INVALID_POINTER: return "Invalid pointer";
        case KRYON_INVALID_STRING: return "Invalid string";
        case KRYON_INVALID_ELEMENT: return "Invalid element";
        case KRYON_INVALID_PROPERTY: return "Invalid property";
        case KRYON_INVALID_MEMORY_CORRUPTION: return "Memory corruption";
        case KRYON_INVALID_BOUNDS: return "Out of bounds";
        default: return "Unknown error";
    }
}
