/**
 * @file krb_schema.c
 * @brief KRB Format Schema Implementation
 *
 * Centralized format validation and utilities to ensure writer/loader consistency.
 */
#include "lib9.h"


#include "krb_schema.h"

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

// =============================================================================
// ELEMENT SCHEMA VALIDATION FUNCTIONS
// =============================================================================

bool krb_validate_element_header(const uint8_t *data, size_t size, size_t *offset, KRBElementHeader *header) {
    if (!data || !offset || !header) {
        return false;
    }
    
    size_t start_offset = *offset;
    
    // Check minimum size for element header
    if (start_offset + sizeof(KRBElementHeader) > size) {
        return false;
    }
    
    // Read header fields in exact schema order
    memcpy(&header->instance_id, data + *offset, sizeof(uint32_t));
    *offset += sizeof(uint32_t);
    
    memcpy(&header->element_type, data + *offset, sizeof(uint32_t));
    *offset += sizeof(uint32_t);
    
    memcpy(&header->parent_id, data + *offset, sizeof(uint32_t));
    *offset += sizeof(uint32_t);
    
    memcpy(&header->style_ref, data + *offset, sizeof(uint32_t));
    *offset += sizeof(uint32_t);
    
    memcpy(&header->property_count, data + *offset, sizeof(uint16_t));
    *offset += sizeof(uint16_t);
    
    memcpy(&header->child_count, data + *offset, sizeof(uint16_t));
    *offset += sizeof(uint16_t);
    
    memcpy(&header->event_count, data + *offset, sizeof(uint16_t));
    *offset += sizeof(uint16_t);
    
    memcpy(&header->flags, data + *offset, sizeof(uint32_t));
    *offset += sizeof(uint32_t);
    
    // Validate reasonable bounds
    if (header->property_count > 1000 || header->child_count > 1000 || header->event_count > 100) {
        fprintf(stderr, "❌ DEBUG: Element header bounds check failed - id=%u type=0x%x props=%u children=%u events=%u\n",
               header->instance_id, header->element_type, header->property_count, 
               header->child_count, header->event_count);
        return false;
    }
    
    // Calculate expected size for properties, children, and events
    size_t properties_size = header->property_count * sizeof(KRBPropertyHeader);
    size_t children_size = header->child_count * sizeof(uint32_t);
    size_t events_size = header->event_count * sizeof(KRBEventHandler);
    
    // Ensure we have enough remaining data
    if (*offset + properties_size + children_size + events_size > size) {
        fprintf(stderr, "❌ DEBUG: Element header size validation failed - offset=%zu + props=%zu + children=%zu + events=%zu = %zu > size=%zu\n",
               *offset, properties_size, children_size, events_size, 
               *offset + properties_size + children_size + events_size, size);
        return false;
    }
    
    return true;
}

bool krb_write_element_header(bool (*writer)(void*, const void*, size_t), void *context, const KRBElementHeader *header) {
    if (!writer || !context || !header) {
        return false;
    }
    
    // Write in exact schema order
    if (!writer(context, &header->instance_id, sizeof(uint32_t))) return false;
    if (!writer(context, &header->element_type, sizeof(uint32_t))) return false;
    if (!writer(context, &header->parent_id, sizeof(uint32_t))) return false;
    if (!writer(context, &header->style_ref, sizeof(uint32_t))) return false;
    if (!writer(context, &header->property_count, sizeof(uint16_t))) return false;
    if (!writer(context, &header->child_count, sizeof(uint16_t))) return false;
    if (!writer(context, &header->event_count, sizeof(uint16_t))) return false;
    if (!writer(context, &header->flags, sizeof(uint32_t))) return false;
    
    return true;
}

bool krb_validate_property_header(const uint8_t *data, size_t size, size_t *offset, KRBPropertyHeader *header) {
    if (!data || !offset || !header) {
        return false;
    }
    
    // Check minimum size for property header
    if (*offset + sizeof(KRBPropertyHeader) > size) {
        return false;
    }

    // Read property ID and value type in native byte order (little-endian)
    memcpy(&header->property_id, data + *offset, sizeof(uint16_t));
    *offset += sizeof(uint16_t);

    memcpy(&header->value_type, data + *offset, sizeof(uint8_t));
    *offset += sizeof(uint8_t);
    
    // Validate property ID and type are reasonable
    if (header->property_id == 0 || header->value_type > 20) {
        return false;
    }
    
    return true;
}

bool krb_write_property_header(bool (*writer)(void*, const void*, size_t), void *context, const KRBPropertyHeader *header) {
    if (!writer || !context || !header) {
        return false;
    }
    
    if (!writer(context, &header->property_id, sizeof(uint16_t))) return false;
    if (!writer(context, &header->value_type, sizeof(uint8_t))) return false;
    
    return true;
}

size_t krb_element_header_size(uint16_t property_count, uint16_t child_count, uint16_t event_count) {
    return sizeof(KRBElementHeader) + 
           (property_count * sizeof(KRBPropertyHeader)) +
           (child_count * sizeof(uint32_t)) +
           (event_count * sizeof(KRBEventHandler));
}
