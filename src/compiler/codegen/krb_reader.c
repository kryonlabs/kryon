/**

 * @file krb_reader.c
 * @brief KRB File Reader Implementation
 */
#include "lib9.h"


#include "krb_format.h"
#include "memory.h"
#include "error.h"
#include "binary_io.h"

// =============================================================================
// READER CREATION AND DESTRUCTION
// =============================================================================

KryonKrbReader *kryon_krb_reader_create_file(const char *filename) {
    if (!filename) {
        KRYON_LOG_ERROR("Filename cannot be NULL");
        return NULL;
    }
    
    FILE *file = fopen(filename, "rb");
    if (!file) {
        KRYON_LOG_ERROR("Failed to open file: %s", filename);
        return NULL;
    }
    
    KryonKrbReader *reader = kryon_alloc_type(KryonKrbReader);
    if (!reader) {
        fclose(file);
        KRYON_LOG_ERROR("Failed to allocate reader");
        return NULL;
    }
    
    memset(reader, 0, sizeof(KryonKrbReader));
    reader->file = file;
    
    return reader;
}

KryonKrbReader *kryon_krb_reader_create_memory(const uint8_t *data, size_t size) {
    if (!data || size == 0) {
        KRYON_LOG_ERROR("Invalid memory buffer");
        return NULL;
    }
    
    KryonKrbReader *reader = kryon_alloc_type(KryonKrbReader);
    if (!reader) {
        KRYON_LOG_ERROR("Failed to allocate reader");
        return NULL;
    }
    
    memset(reader, 0, sizeof(KryonKrbReader));
    reader->data = data;
    reader->data_size = size;
    reader->position = 0;
    
    return reader;
}

void kryon_krb_reader_destroy(KryonKrbReader *reader) {
    if (!reader) return;
    
    if (reader->file) {
        fclose(reader->file);
    }
    
    if (reader->krb_file) {
        kryon_krb_file_destroy(reader->krb_file);
    }
    
    kryon_free(reader);
}

const char *kryon_krb_reader_get_error(const KryonKrbReader *reader) {
    return reader ? reader->error_message : "Reader is NULL";
}

// =============================================================================
// BINARY READING UTILITIES
// =============================================================================

static bool read_bytes(KryonKrbReader *reader, void *buffer, size_t size) {
    if (!reader || !buffer || size == 0) return false;
    
    if (reader->file) {
        size_t read = fread(buffer, 1, size, reader->file);
        if (read != size) {
            snprint(reader->error_message, sizeof(reader->error_message),
                    "Failed to read %zu bytes from file", size);
            return false;
        }
    } else if (reader->data) {
        if (reader->position + size > reader->data_size) {
            snprint(reader->error_message, sizeof(reader->error_message),
                    "Not enough data remaining (%zu bytes needed, %zu available)",
                    size, reader->data_size - reader->position);
            return false;
        }
        
        memcpy(buffer, reader->data + reader->position, size);
        reader->position += size;
    } else {
        strcpy(reader->error_message, "No data source available");
        return false;
    }
    
    return true;
}

static bool read_uint8(KryonKrbReader *reader, uint8_t *value) {
    if (reader->data) {
        return kryon_read_uint8(reader->data, &reader->position, reader->data_size, value);
    } else {
        uint8_t temp_buffer[sizeof(uint8_t)];
        size_t bytes_read = fread(temp_buffer, 1, sizeof(uint8_t), reader->file);
        if (bytes_read != sizeof(uint8_t)) {
            strcpy(reader->error_message, "Failed to read uint8 from file");
            return false;
        }
        size_t offset = 0;
        return kryon_read_uint8(temp_buffer, &offset, sizeof(temp_buffer), value);
    }
}

static bool read_uint16(KryonKrbReader *reader, uint16_t *value) {
    if (reader->data) {
        return kryon_read_uint16(reader->data, &reader->position, reader->data_size, value);
    } else {
        uint8_t temp_buffer[sizeof(uint16_t)];
        size_t bytes_read = fread(temp_buffer, 1, sizeof(uint16_t), reader->file);
        if (bytes_read != sizeof(uint16_t)) {
            strcpy(reader->error_message, "Failed to read uint16 from file");
            return false;
        }
        size_t offset = 0;
        return kryon_read_uint16(temp_buffer, &offset, sizeof(temp_buffer), value);
    }
}

static bool read_uint32(KryonKrbReader *reader, uint32_t *value) {
    if (reader->data) {
        return kryon_read_uint32(reader->data, &reader->position, reader->data_size, value);
    } else {
        uint8_t temp_buffer[sizeof(uint32_t)];
        size_t bytes_read = fread(temp_buffer, 1, sizeof(uint32_t), reader->file);
        if (bytes_read != sizeof(uint32_t)) {
            strcpy(reader->error_message, "Failed to read uint32 from file");
            return false;
        }
        size_t offset = 0;
        return kryon_read_uint32(temp_buffer, &offset, sizeof(temp_buffer), value);
    }
}

static bool read_uint64(KryonKrbReader *reader, uint64_t *value) {
    if (reader->data) {
        return kryon_read_uint64(reader->data, &reader->position, reader->data_size, value);
    } else {
        uint8_t temp_buffer[sizeof(uint64_t)];
        size_t bytes_read = fread(temp_buffer, 1, sizeof(uint64_t), reader->file);
        if (bytes_read != sizeof(uint64_t)) {
            strcpy(reader->error_message, "Failed to read uint64 from file");
            return false;
        }
        size_t offset = 0;
        return kryon_read_uint64(temp_buffer, &offset, sizeof(temp_buffer), value);
    }
}

static bool read_double(KryonKrbReader *reader, double *value) {
    if (reader->data) {
        return kryon_read_bytes(reader->data, &reader->position, reader->data_size, value, sizeof(double));
    } else {
        uint8_t temp_buffer[sizeof(double)];
        size_t bytes_read = fread(temp_buffer, 1, sizeof(double), reader->file);
        if (bytes_read != sizeof(double)) {
            strcpy(reader->error_message, "Failed to read double from file");
            return false;
        }
        size_t offset = 0;
        return kryon_read_bytes(temp_buffer, &offset, sizeof(temp_buffer), value, sizeof(double));
    }
}

static bool read_string(KryonKrbReader *reader, char **out_string, uint16_t length) {
    if (!reader || !out_string || length == 0) return false;
    
    char *string = kryon_alloc(length + 1);
    if (!string) {
        strcpy(reader->error_message, "Failed to allocate string memory");
        return false;
    }
    
    if (!read_bytes(reader, string, length)) {
        kryon_free(string);
        return false;
    }
    
    string[length] = '\0';
    *out_string = string;
    return true;
}

// =============================================================================
// HEADER PARSING
// =============================================================================

static bool parse_header(KryonKrbReader *reader, KryonKrbHeader *header) {
    if (!reader || !header) return false;
    
    // Read magic number
    if (!read_uint32(reader, &header->magic)) return false;
    if (header->magic != KRYON_KRB_MAGIC) {
        snprint(reader->error_message, sizeof(reader->error_message),
                "Invalid magic number: 0x%08X (expected 0x%08X)", 
                header->magic, KRYON_KRB_MAGIC);
        return false;
    }
    
    // Read version (validation removed for alpha)
    if (!read_uint16(reader, &header->version_major) ||
        !read_uint16(reader, &header->version_minor) ||
        !read_uint16(reader, &header->version_patch)) {
        return false;
    }

    // Version validation removed - accepting any version during alpha phase
    // All KRB files are compatible during alpha development
    
    // Read remaining header fields
    if (!read_uint16(reader, &header->flags) ||
        !read_uint32(reader, &header->element_count) ||
        !read_uint32(reader, &header->property_count) ||
        !read_uint32(reader, &header->string_table_size) ||
        !read_uint32(reader, &header->data_size) ||
        !read_uint32(reader, &header->checksum)) {
        return false;
    }
    
    // Read compression type
    uint8_t compression;
    if (!read_uint8(reader, &compression)) return false;
    header->compression = (KryonKrbCompressionType)compression;
    
    if (!read_uint32(reader, &header->uncompressed_size)) return false;
    
    // Read reserved bytes
    if (!read_bytes(reader, header->reserved, sizeof(header->reserved))) return false;
    
    // Validate header values
    if (header->element_count > KRYON_KRB_MAX_ELEMENTS) {
        snprint(reader->error_message, sizeof(reader->error_message),
                "Too many elements: %u (maximum: %u)", 
                header->element_count, KRYON_KRB_MAX_ELEMENTS);
        return false;
    }
    
    if (header->property_count > KRYON_KRB_MAX_PROPERTIES) {
        snprint(reader->error_message, sizeof(reader->error_message),
                "Too many properties: %u (maximum: %u)", 
                header->property_count, KRYON_KRB_MAX_PROPERTIES);
        return false;
    }
    
    KRYON_LOG_DEBUG("Parsed KRB header: version %d.%d.%d, %u elements, %u properties",
                   header->version_major, header->version_minor, header->version_patch,
                   header->element_count, header->property_count);
    
    return true;
}

// =============================================================================
// STRING TABLE PARSING
// =============================================================================

static bool parse_string_table(KryonKrbReader *reader, KryonKrbFile *krb_file) {
    if (!reader || !krb_file) return false;
    
    if (krb_file->header.string_table_size == 0) {
        krb_file->string_table = NULL;
        krb_file->string_count = 0;
        return true;
    }
    
    // Read string count
    uint32_t string_count;
    if (!read_uint32(reader, &string_count)) return false;
    
    if (string_count == 0) {
        krb_file->string_table = NULL;
        krb_file->string_count = 0;
        return true;
    }
    
    // Allocate string table
    krb_file->string_table = kryon_alloc_array(char*, string_count);
    if (!krb_file->string_table) {
        strcpy(reader->error_message, "Failed to allocate string table");
        return false;
    }
    
    krb_file->string_count = string_count;
    
    // Read string lengths and data
    for (uint32_t i = 0; i < string_count; i++) {
        uint16_t length;
        if (!read_uint16(reader, &length)) {
            return false;
        }
        
        // length is uint16_t, so it's always <= KRYON_KRB_MAX_STRING_LENGTH (65535)
        
        if (!read_string(reader, &krb_file->string_table[i], length)) {
            return false;
        }
    }
    
    KRYON_LOG_DEBUG("Parsed string table: %u strings", string_count);
    return true;
}

// =============================================================================
// PROPERTY PARSING
// =============================================================================

static bool parse_property_value(KryonKrbReader *reader, KryonPropertyType type, 
                                KryonPropertyValue *value) {
    if (!reader || !value) return false;
    
    switch (type) {
        case KRYON_PROPERTY_STRING: {
            uint16_t length;
            if (!read_uint16(reader, &length)) return false;
            return read_string(reader, &value->string_value, length);
        }
        
        case KRYON_PROPERTY_INTEGER: {
            int64_t int_val;
            if (!read_uint64(reader, (uint64_t*)&int_val)) return false;
            value->int_value = int_val;
            return true;
        }
        
        case KRYON_PROPERTY_FLOAT: {
            return read_double(reader, &value->float_value);
        }
        
        case KRYON_PROPERTY_BOOLEAN: {
            uint8_t bool_val;
            if (!read_uint8(reader, &bool_val)) return false;
            value->bool_value = bool_val != 0;
            return true;
        }
        
        case KRYON_PROPERTY_COLOR: {
            return read_uint32(reader, &value->color_value);
        }
        
        case KRYON_PROPERTY_SIZE: {
            double width, height;
            bool result = read_double(reader, &width) && read_double(reader, &height);
            if (result) {
                value->size_value.width = (float)width;
                value->size_value.height = (float)height;
            }
            return result;
        }
        
        case KRYON_PROPERTY_POSITION: {
            double x, y;
            bool result = read_double(reader, &x) && read_double(reader, &y);
            if (result) {
                value->position_value.x = (float)x;
                value->position_value.y = (float)y;
            }
            return result;
        }
        
        case KRYON_PROPERTY_MARGIN:
        case KRYON_PROPERTY_PADDING: {
            double top, right, bottom, left;
            bool result = read_double(reader, &top) &&
                         read_double(reader, &right) &&
                         read_double(reader, &bottom) &&
                         read_double(reader, &left);
            if (result) {
                value->spacing_value.top = (float)top;
                value->spacing_value.right = (float)right;
                value->spacing_value.bottom = (float)bottom;
                value->spacing_value.left = (float)left;
            }
            return result;
        }
        
        case KRYON_PROPERTY_REFERENCE: {
            return read_uint32(reader, &value->reference_id);
        }
        
        default:
            snprint(reader->error_message, sizeof(reader->error_message),
                    "Unsupported property type: %d", type);
            return false;
    }
}

static bool parse_property(KryonKrbReader *reader, KryonKrbProperty *property) {
    if (!reader || !property) return false;
    
    // Read property header
    if (!read_uint16(reader, &property->name_id)) return false;
    
    uint8_t type;
    if (!read_uint8(reader, &type)) return false;
    property->type = (KryonPropertyType)type;
    
    if (!read_uint32(reader, &property->flags)) return false;
    
    // Read property value
    return parse_property_value(reader, property->type, &property->value);
}

// =============================================================================
// ELEMENT PARSING
// =============================================================================

static bool parse_element(KryonKrbReader *reader, KryonKrbElement *element) {
    if (!reader || !element) return false;
    
    // Read element header
    if (!read_uint32(reader, &element->id)) return false;
    
    uint8_t type;
    if (!read_uint8(reader, &type)) return false;
    element->type = type;
    
    if (!read_uint16(reader, &element->name_id) ||
        !read_uint32(reader, &element->parent_id) ||
        !read_uint16(reader, &element->property_count) ||
        !read_uint16(reader, &element->child_count) ||
        !read_uint32(reader, &element->flags)) {
        return false;
    }
    
    // Allocate and read properties
    if (element->property_count > 0) {
        element->properties = kryon_alloc_array(KryonKrbProperty, element->property_count);
        if (!element->properties) {
            strcpy(reader->error_message, "Failed to allocate properties array");
            return false;
        }
        
        for (uint16_t i = 0; i < element->property_count; i++) {
            if (!parse_property(reader, &element->properties[i])) {
                return false;
            }
        }
    } else {
        element->properties = NULL;
    }
    
    // Allocate and read child IDs
    if (element->child_count > 0) {
        element->child_ids = kryon_alloc_array(uint32_t, element->child_count);
        if (!element->child_ids) {
            strcpy(reader->error_message, "Failed to allocate child IDs array");
            return false;
        }
        
        for (uint16_t i = 0; i < element->child_count; i++) {
            if (!read_uint32(reader, &element->child_ids[i])) {
                return false;
            }
        }
    } else {
        element->child_ids = NULL;
    }
    
    return true;
}

// =============================================================================
// FUNCTION PARSING
// =============================================================================

static bool parse_function(KryonKrbReader *reader, KryonKrbFunction *function) {
    if (!reader || !function) return false;

    // Read and verify function magic
    uint32_t magic;
    if (!read_uint32(reader, &magic)) return false;

    if (magic != 0x46554E43) {  // "FUNC"
        strcpy(reader->error_message, "Invalid function magic number");
        return false;
    }

    // Read function header (matches KRBFunctionHeader from schema)
    uint32_t language_id_32, name_id_32, code_id_32;
    if (!read_uint32(reader, &language_id_32) ||
        !read_uint32(reader, &name_id_32) ||
        !read_uint16(reader, &function->param_count) ||
        !read_uint16(reader, &function->flags) ||
        !read_uint32(reader, &code_id_32)) {
        return false;
    }

    // Store as uint16_t (string table indices)
    function->language_id = (uint16_t)language_id_32;
    function->name_id = (uint16_t)name_id_32;
    function->code_id = (uint16_t)code_id_32;

    // Allocate and read parameter string table indices
    if (function->param_count > 0) {
        function->param_ids = kryon_alloc_array(uint16_t, function->param_count);
        if (!function->param_ids) {
            strcpy(reader->error_message, "Failed to allocate parameter IDs array");
            return false;
        }

        for (uint16_t i = 0; i < function->param_count; i++) {
            uint32_t param_id_32;
            if (!read_uint32(reader, &param_id_32)) {
                kryon_free(function->param_ids);
                function->param_ids = NULL;
                return false;
            }
            function->param_ids[i] = (uint16_t)param_id_32;
        }
    } else {
        function->param_ids = NULL;
    }

    return true;
}

static bool parse_script_section(KryonKrbReader *reader, KryonKrbFile *krb_file) {
    if (!reader || !krb_file) return false;

    // Try to read section magic (optional section)
    uint32_t magic;
    size_t saved_position = reader->position;

    if (!read_uint32(reader, &magic)) {
        // End of file - no script section
        return true;
    }

    if (magic != 0x53435054) {  // "SCPT"
        // Not a script section - rewind and continue
        reader->position = saved_position;
        return true;
    }

    // Read section header
    uint32_t section_size, function_count;
    if (!read_uint32(reader, &section_size) ||
        !read_uint32(reader, &function_count)) {
        return false;
    }

    if (function_count == 0) {
        return true;
    }

    // Allocate functions array
    krb_file->functions = kryon_alloc_array(KryonKrbFunction, function_count);
    if (!krb_file->functions) {
        strcpy(reader->error_message, "Failed to allocate functions array");
        return false;
    }
    krb_file->function_count = function_count;

    // Parse each function
    for (uint32_t i = 0; i < function_count; i++) {
        if (!parse_function(reader, &krb_file->functions[i])) {
            return false;
        }
        krb_file->functions[i].id = i;  // Assign sequential ID
    }

    return true;
}

// =============================================================================
// MAIN PARSING FUNCTION
// =============================================================================

KryonKrbFile *kryon_krb_reader_parse(KryonKrbReader *reader) {
    if (!reader) {
        KRYON_LOG_ERROR("Reader cannot be NULL");
        return NULL;
    }
    
    // Create KRB file structure
    KryonKrbFile *krb_file = kryon_krb_file_create();
    if (!krb_file) {
        strcpy(reader->error_message, "Failed to create KRB file structure");
        return NULL;
    }
    
    // Parse header
    if (!parse_header(reader, &krb_file->header)) {
        kryon_krb_file_destroy(krb_file);
        return NULL;
    }
    
    // Parse string table
    if (!parse_string_table(reader, krb_file)) {
        kryon_krb_file_destroy(krb_file);
        return NULL;
    }
    
    // Allocate elements array
    if (krb_file->header.element_count > 0) {
        krb_file->elements = kryon_alloc_array(KryonKrbElement, krb_file->header.element_count);
        if (!krb_file->elements) {
            strcpy(reader->error_message, "Failed to allocate elements array");
            kryon_krb_file_destroy(krb_file);
            return NULL;
        }
        
        // Parse elements
        for (uint32_t i = 0; i < krb_file->header.element_count; i++) {
            if (!parse_element(reader, &krb_file->elements[i])) {
                kryon_krb_file_destroy(krb_file);
                return NULL;
            }
        }
    }

    // Parse script section (optional - functions)
    if (!parse_script_section(reader, krb_file)) {
        kryon_krb_file_destroy(krb_file);
        return NULL;
    }

    // Store the parsed file in reader
    reader->krb_file = krb_file;

    KRYON_LOG_INFO("Successfully parsed KRB file: %u elements, %u strings, %u functions",
                  krb_file->header.element_count, krb_file->string_count,
                  krb_file->function_count);
    
    return krb_file;
}
