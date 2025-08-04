/**
 * @file krb_schema.c
 * @brief KRB Format Schema Implementation
 * 
 * Centralized format validation and utilities to ensure writer/loader consistency.
 */

#include "krb_schema.h"
#include <string.h>
#include <stdbool.h>

// =============================================================================
// VALIDATION FUNCTIONS
// =============================================================================

bool krb_validate_function_header(const uint8_t *data, size_t size, size_t *offset, KRBFunctionHeader *header) {
    if (!data || !offset || !header || size < sizeof(KRBFunctionHeader)) {
        return false;
    }
    
    size_t start_offset = *offset;
    
    // Check minimum size
    if (start_offset + sizeof(KRBFunctionHeader) > size) {
        return false;
    }
    
    // Read header fields
    memcpy(&header->magic, data + *offset, sizeof(uint32_t));
    *offset += sizeof(uint32_t);
    
    // Validate magic number
    if (header->magic != KRB_MAGIC_FUNC) {
        return false;
    }
    
    memcpy(&header->lang_ref, data + *offset, sizeof(uint32_t));
    *offset += sizeof(uint32_t);
    
    memcpy(&header->name_ref, data + *offset, sizeof(uint32_t));
    *offset += sizeof(uint32_t);
    
    memcpy(&header->param_count, data + *offset, sizeof(uint16_t));
    *offset += sizeof(uint16_t);
    
    // Check if we have enough space for parameters
    size_t params_size = header->param_count * sizeof(uint32_t);
    if (*offset + params_size + sizeof(uint16_t) + sizeof(uint32_t) > size) {
        return false;
    }
    
    // Skip parameters for now (caller can read them separately)
    *offset += params_size;
    
    memcpy(&header->flags, data + *offset, sizeof(uint16_t));
    *offset += sizeof(uint16_t);
    
    memcpy(&header->code_ref, data + *offset, sizeof(uint32_t));
    *offset += sizeof(uint32_t);
    
    return true;
}

bool krb_write_function_header(bool (*writer)(void*, const void*, size_t), void *context, const KRBFunctionHeader *header) {
    if (!writer || !context || !header) {
        return false;
    }
    
    // Write in exact order defined by schema
    if (!writer(context, &header->magic, sizeof(uint32_t))) return false;
    if (!writer(context, &header->lang_ref, sizeof(uint32_t))) return false;
    if (!writer(context, &header->name_ref, sizeof(uint32_t))) return false;
    if (!writer(context, &header->param_count, sizeof(uint16_t))) return false;
    
    // Note: Parameters must be written separately by caller
    // This function only writes the fixed header portion
    
    return true;
}

size_t krb_function_size(uint16_t param_count) {
    return sizeof(KRBFunctionHeader) + (param_count * sizeof(uint32_t));
}

// =============================================================================
// FORMAT UTILITIES
// =============================================================================

const char* krb_get_section_name(uint32_t magic) {
    switch (magic) {
        case KRB_MAGIC_KRYN:   return "KRYN";
        case KRB_MAGIC_FUNC:   return "FUNC";
        case KRB_MAGIC_COMP:   return "COMP"; 
        case KRB_MAGIC_VARS:   return "VARS";
        case KRB_MAGIC_EVENTS: return "EVNT";
        case KRB_MAGIC_STYLES: return "STYL";
        default:               return "UNKN";
    }
}