/**
 * @file krb_file.c
 * @brief KRB File Management and Utilities Implementation
 */

#include "internal/krb_format.h"
#include "internal/memory.h"
#include "internal/error.h"
#include "internal/containers.h"
#include <string.h>
#include <stdlib.h>

// =============================================================================
// CRC32 CHECKSUM IMPLEMENTATION
// =============================================================================

static const uint32_t crc32_table[256] = {
    0x00000000, 0x77073096, 0xEE0E612C, 0x990951BA, 0x076DC419, 0x706AF48F,
    0xE963A535, 0x9E6495A3, 0x0EDB8832, 0x79DCB8A4, 0xE0D5E91E, 0x97D2D988,
    0x09B64C2B, 0x7EB17CBD, 0xE7B82D07, 0x90BF1D91, 0x1DB71064, 0x6AB020F2,
    0xF3B97148, 0x84BE41DE, 0x1ADAD47D, 0x6DDDE4EB, 0xF4D4B551, 0x83D385C7,
    0x136C9856, 0x646BA8C0, 0xFD62F97A, 0x8A65C9EC, 0x14015C4F, 0x63066CD9,
    0xFA0F3D63, 0x8D080DF5, 0x3B6E20C8, 0x4C69105E, 0xD56041E4, 0xA2677172,
    0x3C03E4D1, 0x4B04D447, 0xD20D85FD, 0xA50AB56B, 0x35B5A8FA, 0x42B2986C,
    0xDBBBC9D6, 0xACBCF940, 0x32D86CE3, 0x45DF5C75, 0xDCD60DCF, 0xABD13D59,
    0x26D930AC, 0x51DE003A, 0xC8D75180, 0xBFD06116, 0x21B4F4B5, 0x56B3C423,
    0xCFBA9599, 0xB8BDA50F, 0x2802B89E, 0x5F058808, 0xC60CD9B2, 0xB10BE924,
    0x2F6F7C87, 0x58684C11, 0xC1611DAB, 0xB6662D3D, 0x76DC4190, 0x01DB7106,
    0x98D220BC, 0xEFD5102A, 0x71B18589, 0x06B6B51F, 0x9FBFE4A5, 0xE8B8D433,
    0x7807C9A2, 0x0F00F934, 0x9609A88E, 0xE10E9818, 0x7F6A0DBB, 0x086D3D2D,
    0x91646C97, 0xE6635C01, 0x6B6B51F4, 0x1C6C6162, 0x856530D8, 0xF262004E,
    0x6C0695ED, 0x1B01A57B, 0x8208F4C1, 0xF50FC457, 0x65B0D9C6, 0x12B7E950,
    0x8BBEB8EA, 0xFCB9887C, 0x62DD1DDF, 0x15DA2D49, 0x8CD37CF3, 0xFBD44C65,
    0x4DB26158, 0x3AB551CE, 0xA3BC0074, 0xD4BB30E2, 0x4ADFA541, 0x3DD895D7,
    0xA4D1C46D, 0xD3D6F4FB, 0x4369E96A, 0x346ED9FC, 0xAD678846, 0xDA60B8D0,
    0x44042D73, 0x33031DE5, 0xAA0A4C5F, 0xDD0D7CC9, 0x5005713C, 0x270241AA,
    0xBE0B1010, 0xC90C2086, 0x5768B525, 0x206F85B3, 0xB966D409, 0xCE61E49F,
    0x5EDEF90E, 0x29D9C998, 0xB0D09822, 0xC7D7A8B4, 0x59B33D17, 0x2EB40D81,
    0xB7BD5C3B, 0xC0BA6CAD, 0xEDB88320, 0x9ABFB3B6, 0x03B6E20C, 0x74B1D29A,
    0xEAD54739, 0x9DD277AF, 0x04DB2615, 0x73DC1683, 0xE3630B12, 0x94643B84,
    0x0D6D6A3E, 0x7A6A5AA8, 0xE40ECF0B, 0x9309FF9D, 0x0A00AE27, 0x7D079EB1,
    0xF00F9344, 0x8708A3D2, 0x1E01F268, 0x6906C2FE, 0xF762575D, 0x806567CB,
    0x196C3671, 0x6E6B06E7, 0xFED41B76, 0x89D32BE0, 0x10DA7A5A, 0x67DD4ACC,
    0xF9B9DF6F, 0x8EBEEFF9, 0x17B7BE43, 0x60B08ED5, 0xD6D6A3E8, 0xA1D1937E,
    0x38D8C2C4, 0x4FDFF252, 0xD1BB67F1, 0xA6BC5767, 0x3FB506DD, 0x48B2364B,
    0xD80D2BDA, 0xAF0A1B4C, 0x36034AF6, 0x41047A60, 0xDF60EFC3, 0xA867DF55,
    0x316E8EEF, 0x4669BE79, 0xCB61B38C, 0xBC66831A, 0x256FD2A0, 0x5268E236,
    0xCC0C7795, 0xBB0B4703, 0x220216B9, 0x5505262F, 0xC5BA3BBE, 0xB2BD0B28,
    0x2BB45A92, 0x5CB36A04, 0xC2D7FFA7, 0xB5D0CF31, 0x2CD99E8B, 0x5BDEAE1D,
    0x9B64C2B0, 0xEC63F226, 0x756AA39C, 0x026D930A, 0x9C0906A9, 0xEB0E363F,
    0x72076785, 0x05005713, 0x95BF4A82, 0xE2B87A14, 0x7BB12BAE, 0x0CB61B38,
    0x92D28E9B, 0xE5D5BE0D, 0x7CDCEFB7, 0x0BDBDF21, 0x86D3D2D4, 0xF1D4E242,
    0x68DDB3F8, 0x1FDA836E, 0x81BE16CD, 0xF6B9265B, 0x6FB077E1, 0x18B74777,
    0x88085AE6, 0xFF0F6A70, 0x66063BCA, 0x11010B5C, 0x8F659EFF, 0xF862AE69,
    0x616BFFD3, 0x166CCF45, 0xA00AE278, 0xD70DD2EE, 0x4E048354, 0x3903B3C2,
    0xA7672661, 0xD06016F7, 0x4969474D, 0x3E6E77DB, 0xAED16A4A, 0xD9D65ADC,
    0x40DF0B66, 0x37D83BF0, 0xA9BCAE53, 0xDEBB9EC5, 0x47B2CF7F, 0x30B5FFE9,
    0xBDBDF21C, 0xCABAC28A, 0x53B39330, 0x24B4A3A6, 0xBAD03605, 0xCDD70693,
    0x54DE5729, 0x23D967BF, 0xB3667A2E, 0xC4614AB8, 0x5D681B02, 0x2A6F2B94,
    0xB40BBE37, 0xC30C8EA1, 0x5A05DF1B, 0x2D02EF8D
};

uint32_t kryon_krb_calculate_checksum(const uint8_t *data, size_t size) {
    if (!data || size == 0) return 0;
    
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < size; i++) {
        crc = crc32_table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFF;
}

// =============================================================================
// KRB FILE MANAGEMENT
// =============================================================================

KryonKrbFile *kryon_krb_file_create(void) {
    KryonKrbFile *krb_file = kryon_alloc_type(KryonKrbFile);
    if (!krb_file) {
        KRYON_LOG_ERROR("Failed to allocate KRB file");
        return NULL;
    }
    
    memset(krb_file, 0, sizeof(KryonKrbFile));
    
    // Initialize header
    krb_file->header.magic = KRYON_KRB_MAGIC;
    krb_file->header.version_major = KRYON_KRB_VERSION_MAJOR;
    krb_file->header.version_minor = KRYON_KRB_VERSION_MINOR;
    krb_file->header.version_patch = KRYON_KRB_VERSION_PATCH;
    krb_file->header.flags = 0;
    krb_file->header.compression = KRYON_KRB_COMPRESSION_NONE;
    
    krb_file->owns_data = true;
    
    return krb_file;
}

void kryon_krb_file_destroy(KryonKrbFile *krb_file) {
    if (!krb_file) return;
    
    // Free string table
    if (krb_file->string_table) {
        for (uint32_t i = 0; i < krb_file->string_count; i++) {
            if (krb_file->string_table[i]) {
                kryon_free(krb_file->string_table[i]);
            }
        }
        kryon_free(krb_file->string_table);
    }
    
    // Free elements
    if (krb_file->elements) {
        for (uint32_t i = 0; i < krb_file->header.element_count; i++) {
            kryon_krb_element_destroy(&krb_file->elements[i]);
        }
        kryon_free(krb_file->elements);
    }
    
    // Free raw data if owned
    if (krb_file->owns_data && krb_file->raw_data) {
        kryon_free(krb_file->raw_data);
    }
    
    kryon_free(krb_file);
}

uint16_t kryon_krb_file_add_string(KryonKrbFile *krb_file, const char *string) {
    if (!krb_file || !string) {
        KRYON_LOG_ERROR("Invalid arguments to add_string");
        return UINT16_MAX;
    }
    
    // Check if string already exists
    for (uint32_t i = 0; i < krb_file->string_count; i++) {
        if (krb_file->string_table[i] && strcmp(krb_file->string_table[i], string) == 0) {
            return (uint16_t)i;
        }
    }
    
    // Check capacity
    if (krb_file->string_count >= UINT16_MAX) {
        KRYON_LOG_ERROR("String table is full");
        return UINT16_MAX;
    }
    
    // Resize string table if needed
    uint32_t new_count = krb_file->string_count + 1;
    char **new_table = kryon_realloc(krb_file->string_table, new_count * sizeof(char*));
    if (!new_table) {
        KRYON_LOG_ERROR("Failed to resize string table");
        return UINT16_MAX;
    }
    
    // Duplicate string
    size_t len = strlen(string);
    char *str_copy = kryon_alloc(len + 1);
    if (!str_copy) {
        KRYON_LOG_ERROR("Failed to allocate string copy");
        return UINT16_MAX;
    }
    
    strcpy(str_copy, string);
    
    // Add to table
    krb_file->string_table = new_table;
    krb_file->string_table[krb_file->string_count] = str_copy;
    uint16_t index = (uint16_t)krb_file->string_count;
    krb_file->string_count++;
    
    // Update header
    krb_file->header.string_table_size += (uint32_t)(len + 3); // length + string + null
    
    return index;
}

const char *kryon_krb_file_get_string(const KryonKrbFile *krb_file, uint16_t index) {
    if (!krb_file || index >= krb_file->string_count) {
        return NULL;
    }
    
    return krb_file->string_table[index];
}

bool kryon_krb_file_add_element(KryonKrbFile *krb_file, const KryonKrbElement *element) {
    if (!krb_file || !element) {
        KRYON_LOG_ERROR("Invalid arguments to add_element");
        return false;
    }
    
    // Check capacity
    if (krb_file->header.element_count >= KRYON_KRB_MAX_ELEMENTS) {
        KRYON_LOG_ERROR("Too many elements");
        return false;
    }
    
    // Resize elements array
    uint32_t new_count = krb_file->header.element_count + 1;
    KryonKrbElement *new_elements = kryon_realloc(krb_file->elements, 
                                                 new_count * sizeof(KryonKrbElement));
    if (!new_elements) {
        KRYON_LOG_ERROR("Failed to resize elements array");
        return false;
    }
    
    // Copy element (deep copy)
    KryonKrbElement *dest = &new_elements[krb_file->header.element_count];
    memcpy(dest, element, sizeof(KryonKrbElement));
    
    // Copy properties if any
    if (element->property_count > 0 && element->properties) {
        dest->properties = kryon_alloc_array(KryonKrbProperty, element->property_count);
        if (!dest->properties) {
            KRYON_LOG_ERROR("Failed to allocate properties");
            return false;
        }
        
        for (uint16_t i = 0; i < element->property_count; i++) {
            dest->properties[i] = element->properties[i];
            
            // Deep copy string values
            if (element->properties[i].type == KRYON_PROPERTY_STRING &&
                element->properties[i].value.string_value) {
                size_t len = strlen(element->properties[i].value.string_value);
                dest->properties[i].value.string_value = kryon_alloc(len + 1);
                if (dest->properties[i].value.string_value) {
                    strcpy(dest->properties[i].value.string_value, 
                          element->properties[i].value.string_value);
                }
            }
        }
    } else {
        dest->properties = NULL;
    }
    
    // Copy child IDs if any
    if (element->child_count > 0 && element->child_ids) {
        dest->child_ids = kryon_alloc_array(uint32_t, element->child_count);
        if (!dest->child_ids) {
            KRYON_LOG_ERROR("Failed to allocate child IDs");
            return false;
        }
        
        memcpy(dest->child_ids, element->child_ids, element->child_count * sizeof(uint32_t));
    } else {
        dest->child_ids = NULL;
    }
    
    // Update file
    krb_file->elements = new_elements;
    krb_file->header.element_count++;
    krb_file->header.property_count += element->property_count;
    
    return true;
}

KryonKrbElement *kryon_krb_file_get_element(const KryonKrbFile *krb_file, uint32_t id) {
    if (!krb_file || !krb_file->elements) return NULL;
    
    for (uint32_t i = 0; i < krb_file->header.element_count; i++) {
        if (krb_file->elements[i].id == id) {
            return &krb_file->elements[i];
        }
    }
    
    return NULL;
}

KryonKrbElement **kryon_krb_file_get_root_elements(const KryonKrbFile *krb_file, size_t *out_count) {
    if (!krb_file || !out_count) {
        if (out_count) *out_count = 0;
        return NULL;
    }
    
    // Count root elements
    size_t root_count = 0;
    for (uint32_t i = 0; i < krb_file->header.element_count; i++) {
        if (krb_file->elements[i].parent_id == 0) {
            root_count++;
        }
    }
    
    if (root_count == 0) {
        *out_count = 0;
        return NULL;
    }
    
    // Allocate array
    KryonKrbElement **roots = kryon_alloc_array(KryonKrbElement*, root_count);
    if (!roots) {
        *out_count = 0;
        return NULL;
    }
    
    // Fill array
    size_t index = 0;
    for (uint32_t i = 0; i < krb_file->header.element_count; i++) {
        if (krb_file->elements[i].parent_id == 0) {
            roots[index++] = &krb_file->elements[i];
        }
    }
    
    *out_count = root_count;
    return roots;
}

// =============================================================================
// ELEMENT UTILITIES
// =============================================================================

KryonKrbElement *kryon_krb_element_create(uint32_t id, KryonElementType type, uint32_t parent_id) {
    KryonKrbElement *element = kryon_alloc_type(KryonKrbElement);
    if (!element) {
        KRYON_LOG_ERROR("Failed to allocate element");
        return NULL;
    }
    
    memset(element, 0, sizeof(KryonKrbElement));
    element->id = id;
    element->type = type;
    element->parent_id = parent_id;
    
    return element;
}

void kryon_krb_element_destroy(KryonKrbElement *element) {
    if (!element) return;
    
    // Free properties
    if (element->properties) {
        for (uint16_t i = 0; i < element->property_count; i++) {
            if (element->properties[i].type == KRYON_PROPERTY_STRING &&
                element->properties[i].value.string_value) {
                kryon_free(element->properties[i].value.string_value);
            }
        }
        kryon_free(element->properties);
    }
    
    // Free child IDs
    if (element->child_ids) {
        kryon_free(element->child_ids);
    }
    
    // Note: Don't free the element itself if it's part of an array
}

bool kryon_krb_element_add_property(KryonKrbElement *element, const KryonKrbProperty *property) {
    if (!element || !property) {
        KRYON_LOG_ERROR("Invalid arguments to add_property");
        return false;
    }
    
    // Resize properties array
    uint16_t new_count = element->property_count + 1;
    KryonKrbProperty *new_properties = kryon_realloc(element->properties,
                                                    new_count * sizeof(KryonKrbProperty));
    if (!new_properties) {
        KRYON_LOG_ERROR("Failed to resize properties array");
        return false;
    }
    
    // Copy property
    new_properties[element->property_count] = *property;
    
    // Deep copy string value if needed
    if (property->type == KRYON_PROPERTY_STRING && property->value.string_value) {
        size_t len = strlen(property->value.string_value);
        char *str_copy = kryon_alloc(len + 1);
        if (!str_copy) {
            KRYON_LOG_ERROR("Failed to allocate string copy");
            return false;
        }
        strcpy(str_copy, property->value.string_value);
        new_properties[element->property_count].value.string_value = str_copy;
    }
    
    element->properties = new_properties;
    element->property_count++;
    
    return true;
}

KryonKrbProperty *kryon_krb_element_get_property(const KryonKrbElement *element,
                                                const KryonKrbFile *krb_file,
                                                const char *name) {
    if (!element || !krb_file || !name) return NULL;
    
    for (uint16_t i = 0; i < element->property_count; i++) {
        const char *prop_name = kryon_krb_file_get_string(krb_file, element->properties[i].name_id);
        if (prop_name && strcmp(prop_name, name) == 0) {
            return &element->properties[i];
        }
    }
    
    return NULL;
}

bool kryon_krb_element_add_child(KryonKrbElement *element, uint32_t child_id) {
    if (!element) {
        KRYON_LOG_ERROR("Element cannot be NULL");
        return false;
    }
    
    // Resize child IDs array
    uint16_t new_count = element->child_count + 1;
    uint32_t *new_child_ids = kryon_realloc(element->child_ids, new_count * sizeof(uint32_t));
    if (!new_child_ids) {
        KRYON_LOG_ERROR("Failed to resize child IDs array");
        return false;
    }
    
    new_child_ids[element->child_count] = child_id;
    element->child_ids = new_child_ids;
    element->child_count++;
    
    return true;
}

// =============================================================================
// PROPERTY UTILITIES
// =============================================================================

KryonKrbProperty kryon_krb_property_string(uint16_t name_id, const char *value) {
    KryonKrbProperty prop = {0};
    prop.name_id = name_id;
    prop.type = KRYON_PROPERTY_STRING;
    prop.value.string_value = (char*)value; // Caller responsible for memory
    return prop;
}

KryonKrbProperty kryon_krb_property_int(uint16_t name_id, int64_t value) {
    KryonKrbProperty prop = {0};
    prop.name_id = name_id;
    prop.type = KRYON_PROPERTY_INTEGER;
    prop.value.int_value = value;
    return prop;
}

KryonKrbProperty kryon_krb_property_float(uint16_t name_id, double value) {
    KryonKrbProperty prop = {0};
    prop.name_id = name_id;
    prop.type = KRYON_PROPERTY_FLOAT;
    prop.value.float_value = value;
    return prop;
}

KryonKrbProperty kryon_krb_property_bool(uint16_t name_id, bool value) {
    KryonKrbProperty prop = {0};
    prop.name_id = name_id;
    prop.type = KRYON_PROPERTY_BOOLEAN;
    prop.value.bool_value = value;
    return prop;
}

KryonKrbProperty kryon_krb_property_color(uint16_t name_id, uint32_t rgba) {
    KryonKrbProperty prop = {0};
    prop.name_id = name_id;
    prop.type = KRYON_PROPERTY_COLOR;
    prop.value.color_value = rgba;
    return prop;
}

// =============================================================================
// VALIDATION UTILITIES
// =============================================================================

bool kryon_krb_file_validate(const KryonKrbFile *krb_file, char *out_error, size_t error_size) {
    if (!krb_file) {
        if (out_error) snprintf(out_error, error_size, "KRB file is NULL");
        return false;
    }
    
    // Validate header
    if (krb_file->header.magic != KRYON_KRB_MAGIC) {
        if (out_error) snprintf(out_error, error_size, "Invalid magic number");
        return false;
    }
    
    // Validate element count
    if (krb_file->header.element_count > KRYON_KRB_MAX_ELEMENTS) {
        if (out_error) snprintf(out_error, error_size, "Too many elements: %u", 
                               krb_file->header.element_count);
        return false;
    }
    
    // Validate elements exist if count > 0
    if (krb_file->header.element_count > 0 && !krb_file->elements) {
        if (out_error) snprintf(out_error, error_size, "Elements array is NULL");
        return false;
    }
    
    // Validate element IDs are unique
    for (uint32_t i = 0; i < krb_file->header.element_count; i++) {
        for (uint32_t j = i + 1; j < krb_file->header.element_count; j++) {
            if (krb_file->elements[i].id == krb_file->elements[j].id) {
                if (out_error) snprintf(out_error, error_size, 
                                       "Duplicate element ID: %u", krb_file->elements[i].id);
                return false;
            }
        }
    }
    
    // Validate parent-child relationships
    for (uint32_t i = 0; i < krb_file->header.element_count; i++) {
        const KryonKrbElement *element = &krb_file->elements[i];
        
        // Check parent exists (unless it's a root element)
        if (element->parent_id != 0) {
            KryonKrbElement *parent = kryon_krb_file_get_element(krb_file, element->parent_id);
            if (!parent) {
                if (out_error) snprintf(out_error, error_size,
                                       "Element %u has non-existent parent %u",
                                       element->id, element->parent_id);
                return false;
            }
        }
        
        // Check children exist
        for (uint16_t j = 0; j < element->child_count; j++) {
            KryonKrbElement *child = kryon_krb_file_get_element(krb_file, element->child_ids[j]);
            if (!child) {
                if (out_error) snprintf(out_error, error_size,
                                       "Element %u has non-existent child %u",
                                       element->id, element->child_ids[j]);
                return false;
            }
            
            // Check child points back to this parent
            if (child->parent_id != element->id) {
                if (out_error) snprintf(out_error, error_size,
                                       "Child %u doesn't point back to parent %u",
                                       element->child_ids[j], element->id);
                return false;
            }
        }
    }
    
    return true;
}

// =============================================================================
// TYPE NAME UTILITIES
// =============================================================================

const char *kryon_krb_element_type_name(KryonElementType type) {
    switch (type) {
        case KRYON_ELEMENT_CONTAINER: return "container";
        case KRYON_ELEMENT_ROW: return "row";
        case KRYON_ELEMENT_COLUMN: return "column";
        case KRYON_ELEMENT_GRID: return "grid";
        case KRYON_ELEMENT_STACK: return "stack";
        case KRYON_ELEMENT_TEXT: return "text";
        case KRYON_ELEMENT_IMAGE: return "image";
        case KRYON_ELEMENT_BUTTON: return "button";
        case KRYON_ELEMENT_INPUT: return "input";
        case KRYON_ELEMENT_TEXTAREA: return "textarea";
        case KRYON_ELEMENT_CHECKBOX: return "checkbox";
        case KRYON_ELEMENT_RADIO: return "radio";
        case KRYON_ELEMENT_SELECT: return "select";
        case KRYON_ELEMENT_SLIDER: return "slider";
        case KRYON_ELEMENT_PROGRESS: return "progress";
        case KRYON_ELEMENT_LIST: return "list";
        case KRYON_ELEMENT_TREE: return "tree";
        case KRYON_ELEMENT_TABLE: return "table";
        case KRYON_ELEMENT_CARD: return "card";
        case KRYON_ELEMENT_TAB: return "tab";
        case KRYON_ELEMENT_MODAL: return "modal";
        case KRYON_ELEMENT_DRAWER: return "drawer";
        case KRYON_ELEMENT_DROPDOWN: return "dropdown";
        case KRYON_ELEMENT_MENU: return "menu";
        case KRYON_ELEMENT_NAVBAR: return "navbar";
        case KRYON_ELEMENT_SIDEBAR: return "sidebar";
        case KRYON_ELEMENT_HEADER: return "header";
        case KRYON_ELEMENT_FOOTER: return "footer";
        case KRYON_ELEMENT_SECTION: return "section";
        case KRYON_ELEMENT_ARTICLE: return "article";
        case KRYON_ELEMENT_ASIDE: return "aside";
        case KRYON_ELEMENT_EVENT_DIRECTIVE: return "event_directive";
        case KRYON_ELEMENT_CUSTOM: return "custom";
        default: return "unknown";
    }
}

const char *kryon_krb_property_type_name(KryonPropertyType type) {
    switch (type) {
        case KRYON_PROPERTY_STRING: return "string";
        case KRYON_PROPERTY_INTEGER: return "integer";
        case KRYON_PROPERTY_FLOAT: return "float";
        case KRYON_PROPERTY_BOOLEAN: return "boolean";
        case KRYON_PROPERTY_COLOR: return "color";
        case KRYON_PROPERTY_SIZE: return "size";
        case KRYON_PROPERTY_POSITION: return "position";
        case KRYON_PROPERTY_MARGIN: return "margin";
        case KRYON_PROPERTY_PADDING: return "padding";
        case KRYON_PROPERTY_BORDER: return "border";
        case KRYON_PROPERTY_FONT: return "font";
        case KRYON_PROPERTY_ARRAY: return "array";
        case KRYON_PROPERTY_OBJECT: return "object";
        case KRYON_PROPERTY_REFERENCE: return "reference";
        case KRYON_PROPERTY_EVENT_HANDLER: return "event_handler";
        default: return "unknown";
    }
}