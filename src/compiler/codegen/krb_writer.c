/**
 * @file krb_writer.c
 * @brief KRB File Writer Implementation
 */

#include "internal/krb_format.h"
#include "internal/memory.h"
#include "internal/error.h"
#include <string.h>
#include <stdlib.h>

// =============================================================================
// WRITER CREATION AND DESTRUCTION
// =============================================================================

KryonKrbWriter *kryon_krb_writer_create_file(const char *filename) {
    if (!filename) {
        KRYON_LOG_ERROR("Filename cannot be NULL");
        return NULL;
    }
    
    FILE *file = fopen(filename, "wb");
    if (!file) {
        KRYON_LOG_ERROR("Failed to create file: %s", filename);
        return NULL;
    }
    
    KryonKrbWriter *writer = kryon_alloc_type(KryonKrbWriter);
    if (!writer) {
        fclose(file);
        KRYON_LOG_ERROR("Failed to allocate writer");
        return NULL;
    }
    
    memset(writer, 0, sizeof(KryonKrbWriter));
    writer->file = file;
    writer->big_endian = false; // Use little-endian by default
    writer->compression = KRYON_KRB_COMPRESSION_NONE;
    
    return writer;
}

KryonKrbWriter *kryon_krb_writer_create_memory(size_t initial_capacity) {
    if (initial_capacity == 0) {
        initial_capacity = 4096; // Default 4KB
    }
    
    KryonKrbWriter *writer = kryon_alloc_type(KryonKrbWriter);
    if (!writer) {
        KRYON_LOG_ERROR("Failed to allocate writer");
        return NULL;
    }
    
    writer->buffer = kryon_alloc(initial_capacity);
    if (!writer->buffer) {
        kryon_free(writer);
        KRYON_LOG_ERROR("Failed to allocate buffer");
        return NULL;
    }
    
    memset(writer, 0, sizeof(KryonKrbWriter));
    writer->buffer_capacity = initial_capacity;
    writer->buffer_size = 0;
    writer->big_endian = false;
    writer->compression = KRYON_KRB_COMPRESSION_NONE;
    
    return writer;
}

void kryon_krb_writer_destroy(KryonKrbWriter *writer) {
    if (!writer) return;
    
    if (writer->file) {
        fclose(writer->file);
    }
    
    if (writer->buffer) {
        kryon_free(writer->buffer);
    }
    
    kryon_free(writer);
}

const char *kryon_krb_writer_get_error(const KryonKrbWriter *writer) {
    return writer ? writer->error_message : "Writer is NULL";
}

void kryon_krb_writer_set_compression(KryonKrbWriter *writer, KryonKrbCompressionType compression) {
    if (writer) {
        writer->compression = compression;
    }
}

const uint8_t *kryon_krb_writer_get_data(const KryonKrbWriter *writer, size_t *out_size) {
    if (!writer || !out_size) return NULL;
    
    *out_size = writer->buffer_size;
    return writer->buffer;
}

// =============================================================================
// BINARY WRITING UTILITIES
// =============================================================================

static bool ensure_buffer_capacity(KryonKrbWriter *writer, size_t additional_size) {
    if (!writer->buffer) return false;
    
    size_t needed_size = writer->buffer_size + additional_size;
    if (needed_size <= writer->buffer_capacity) {
        return true;
    }
    
    // Grow buffer by 50% or needed size, whichever is larger
    size_t new_capacity = writer->buffer_capacity * 3 / 2;
    if (new_capacity < needed_size) {
        new_capacity = needed_size;
    }
    
    uint8_t *new_buffer = kryon_realloc(writer->buffer, new_capacity);
    if (!new_buffer) {
        strcpy(writer->error_message, "Failed to grow buffer");
        return false;
    }
    
    writer->buffer = new_buffer;
    writer->buffer_capacity = new_capacity;
    return true;
}

static bool write_bytes(KryonKrbWriter *writer, const void *data, size_t size) {
    if (!writer || !data || size == 0) return false;
    
    if (writer->file) {
        size_t written = fwrite(data, 1, size, writer->file);
        if (written != size) {
            snprintf(writer->error_message, sizeof(writer->error_message),
                    "Failed to write %zu bytes to file", size);
            return false;
        }
    } else if (writer->buffer) {
        if (!ensure_buffer_capacity(writer, size)) {
            return false;
        }
        
        memcpy(writer->buffer + writer->buffer_size, data, size);
        writer->buffer_size += size;
    } else {
        strcpy(writer->error_message, "No output destination available");
        return false;
    }
    
    return true;
}

static bool write_uint8(KryonKrbWriter *writer, uint8_t value) {
    return write_bytes(writer, &value, sizeof(uint8_t));
}

static bool write_uint16(KryonKrbWriter *writer, uint16_t value) {
    if (writer->big_endian) {
        value = (value << 8) | (value >> 8);
    }
    return write_bytes(writer, &value, sizeof(uint16_t));
}

static bool write_uint32(KryonKrbWriter *writer, uint32_t value) {
    if (writer->big_endian) {
        value = ((value << 24) & 0xff000000) |
                ((value << 8)  & 0x00ff0000) |
                ((value >> 8)  & 0x0000ff00) |
                ((value >> 24) & 0x000000ff);
    }
    return write_bytes(writer, &value, sizeof(uint32_t));
}

static bool write_uint64(KryonKrbWriter *writer, uint64_t value) {
    if (writer->big_endian) {
        value = ((value << 56) & 0xff00000000000000ULL) |
                ((value << 40) & 0x00ff000000000000ULL) |
                ((value << 24) & 0x0000ff0000000000ULL) |
                ((value << 8)  & 0x000000ff00000000ULL) |
                ((value >> 8)  & 0x00000000ff000000ULL) |
                ((value >> 24) & 0x0000000000ff0000ULL) |
                ((value >> 40) & 0x000000000000ff00ULL) |
                ((value >> 56) & 0x00000000000000ffULL);
    }
    return write_bytes(writer, &value, sizeof(uint64_t));
}

static bool write_double(KryonKrbWriter *writer, double value) {
    if (writer->big_endian) {
        union { double d; uint64_t i; } u;
        u.d = value;
        u.i = ((u.i << 56) & 0xff00000000000000ULL) |
              ((u.i << 40) & 0x00ff000000000000ULL) |
              ((u.i << 24) & 0x0000ff0000000000ULL) |
              ((u.i << 8)  & 0x000000ff00000000ULL) |
              ((u.i >> 8)  & 0x00000000ff000000ULL) |
              ((u.i >> 24) & 0x0000000000ff0000ULL) |
              ((u.i >> 40) & 0x000000000000ff00ULL) |
              ((u.i >> 56) & 0x00000000000000ffULL);
        value = u.d;
    }
    return write_bytes(writer, &value, sizeof(double));
}

static bool write_string(KryonKrbWriter *writer, const char *str) {
    if (!str) {
        return write_uint16(writer, 0);
    }
    
    size_t length = strlen(str);
    if (length > KRYON_KRB_MAX_STRING_LENGTH) {
        snprintf(writer->error_message, sizeof(writer->error_message),
                "String too long: %zu characters (maximum: %u)", 
                length, KRYON_KRB_MAX_STRING_LENGTH);
        return false;
    }
    
    return write_uint16(writer, (uint16_t)length) &&
           write_bytes(writer, str, length);
}

// =============================================================================
// HEADER WRITING
// =============================================================================

static bool write_header(KryonKrbWriter *writer, const KryonKrbHeader *header) {
    if (!writer || !header) return false;
    
    return write_uint32(writer, header->magic) &&
           write_uint16(writer, header->version_major) &&
           write_uint16(writer, header->version_minor) &&
           write_uint16(writer, header->version_patch) &&
           write_uint16(writer, header->flags) &&
           write_uint32(writer, header->element_count) &&
           write_uint32(writer, header->property_count) &&
           write_uint32(writer, header->string_table_size) &&
           write_uint32(writer, header->data_size) &&
           write_uint32(writer, header->checksum) &&
           write_uint8(writer, (uint8_t)header->compression) &&
           write_uint32(writer, header->uncompressed_size) &&
           write_bytes(writer, header->reserved, sizeof(header->reserved));
}

// =============================================================================
// STRING TABLE WRITING
// =============================================================================

static bool write_string_table(KryonKrbWriter *writer, const KryonKrbFile *krb_file) {
    if (!writer || !krb_file) return false;
    
    if (!write_uint32(writer, krb_file->string_count)) return false;
    
    for (uint32_t i = 0; i < krb_file->string_count; i++) {
        if (!write_string(writer, krb_file->string_table[i])) {
            return false;
        }
    }
    
    return true;
}

// =============================================================================
// PROPERTY WRITING
// =============================================================================

static bool write_property_value(KryonKrbWriter *writer, const KryonKrbProperty *property) {
    if (!writer || !property) return false;
    
    switch (property->type) {
        case KRYON_PROPERTY_STRING:
            return write_string(writer, property->value.string_value);
        
        case KRYON_PROPERTY_INTEGER:
            return write_uint64(writer, (uint64_t)property->value.int_value);
        
        case KRYON_PROPERTY_FLOAT:
            return write_double(writer, property->value.float_value);
        
        case KRYON_PROPERTY_BOOLEAN:
            return write_uint8(writer, property->value.bool_value ? 1 : 0);
        
        case KRYON_PROPERTY_COLOR:
            return write_uint32(writer, property->value.color_value);
        
        case KRYON_PROPERTY_SIZE:
            return write_double(writer, property->value.size_value.width) &&
                   write_double(writer, property->value.size_value.height);
        
        case KRYON_PROPERTY_POSITION:
            return write_double(writer, property->value.position_value.x) &&
                   write_double(writer, property->value.position_value.y);
        
        case KRYON_PROPERTY_MARGIN:
        case KRYON_PROPERTY_PADDING:
            return write_double(writer, property->value.spacing_value.top) &&
                   write_double(writer, property->value.spacing_value.right) &&
                   write_double(writer, property->value.spacing_value.bottom) &&
                   write_double(writer, property->value.spacing_value.left);
        
        case KRYON_PROPERTY_REFERENCE:
            return write_uint32(writer, property->value.reference_id);
        
        default:
            snprintf(writer->error_message, sizeof(writer->error_message),
                    "Unsupported property type: %d", property->type);
            return false;
    }
}

static bool write_property(KryonKrbWriter *writer, const KryonKrbProperty *property) {
    if (!writer || !property) return false;
    
    return write_uint16(writer, property->name_id) &&
           write_uint8(writer, (uint8_t)property->type) &&
           write_uint32(writer, property->flags) &&
           write_property_value(writer, property);
}

// =============================================================================
// ELEMENT WRITING
// =============================================================================

static bool write_element(KryonKrbWriter *writer, const KryonKrbElement *element) {
    if (!writer || !element) return false;
    
    // Write element header
    if (!write_uint32(writer, element->id) ||
        !write_uint8(writer, (uint8_t)element->type) ||
        !write_uint16(writer, element->name_id) ||
        !write_uint32(writer, element->parent_id) ||
        !write_uint16(writer, element->property_count) ||
        !write_uint16(writer, element->child_count) ||
        !write_uint32(writer, element->flags)) {
        return false;
    }
    
    // Write properties
    for (uint16_t i = 0; i < element->property_count; i++) {
        if (!write_property(writer, &element->properties[i])) {
            return false;
        }
    }
    
    // Write child IDs
    for (uint16_t i = 0; i < element->child_count; i++) {
        if (!write_uint32(writer, element->child_ids[i])) {
            return false;
        }
    }
    
    return true;
}

// =============================================================================
// MAIN WRITING FUNCTION
// =============================================================================

bool kryon_krb_writer_write(KryonKrbWriter *writer, const KryonKrbFile *krb_file) {
    if (!writer || !krb_file) {
        if (writer) strcpy(writer->error_message, "KRB file cannot be NULL");
        return false;
    }
    
    // Validate KRB file
    if (!kryon_krb_file_validate(krb_file, writer->error_message, sizeof(writer->error_message))) {
        return false;
    }
    
    // Write header
    if (!write_header(writer, &krb_file->header)) {
        return false;
    }
    
    // Write string table
    if (!write_string_table(writer, krb_file)) {
        return false;
    }
    
    // Write elements
    for (uint32_t i = 0; i < krb_file->header.element_count; i++) {
        if (!write_element(writer, &krb_file->elements[i])) {
            return false;
        }
    }
    
    // Flush file if writing to file
    if (writer->file) {
        if (fflush(writer->file) != 0) {
            strcpy(writer->error_message, "Failed to flush file");
            return false;
        }
    }
    
    KRYON_LOG_INFO("Successfully wrote KRB file: %u elements, %u strings",
                  krb_file->header.element_count, krb_file->string_count);
    
    return true;
}