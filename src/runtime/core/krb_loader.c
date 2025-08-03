/**
 * @file krb_loader.c
 * @brief KRB File Loader for Runtime
 * 
 * 0BSD License
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted.
 * 
 * Loads and processes KRB binary files into runtime element trees.
 */

#include "internal/runtime.h"
#include "internal/krb_format.h"
#include "internal/events.h"
#include "internal/memory.h"
#include "internal/binary_io.h"
#include "internal/script_vm.h"
#include "internal/parser.h"
#include "../../shared/kryon_mappings.h"
#include <string.h>
#include <stdlib.h>

// =============================================================================
// ELEMENT TYPE MAPPING
// =============================================================================

// Using centralized element mappings

// =============================================================================
// PROPERTY TYPE MAPPING
// =============================================================================

// Using centralized property mappings - need to map to runtime types
static KryonRuntimePropertyType get_runtime_property_type(const char *property_name) {
    if (!property_name) return KRYON_RUNTIME_PROP_STRING;
    
    // Color properties
    if (strstr(property_name, "Color") || strcmp(property_name, "color") == 0 ||
        strcmp(property_name, "background") == 0 || strcmp(property_name, "backgroundColor") == 0) {
        return KRYON_RUNTIME_PROP_COLOR;
    }
    
    // Float properties (numeric dimensions, positions)
    if (strstr(property_name, "width") || strstr(property_name, "Width") ||
        strstr(property_name, "height") || strstr(property_name, "Height") ||
        strstr(property_name, "pos") || strstr(property_name, "Size") ||
        strstr(property_name, "padding") || strstr(property_name, "margin") ||
        strstr(property_name, "borderWidth") || strstr(property_name, "borderRadius") ||
        strcmp(property_name, "fontSize") == 0 || strcmp(property_name, "gap") == 0) {
        return KRYON_RUNTIME_PROP_FLOAT;
    }
    
    // Boolean properties
    if (strcmp(property_name, "visible") == 0 || strcmp(property_name, "enabled") == 0 ||
        strcmp(property_name, "windowResizable") == 0 || strcmp(property_name, "resizable") == 0 ||
        strcmp(property_name, "keepAspectRatio") == 0 || strcmp(property_name, "aspectRatio") == 0 ||
        strcmp(property_name, "multiSelect") == 0) {
        return KRYON_RUNTIME_PROP_BOOLEAN;
    }
    
    // Function properties (event handlers)
    if (strncmp(property_name, "on", 2) == 0) {
        return KRYON_RUNTIME_PROP_FUNCTION;
    }
    
    // Integer properties
    if (strcmp(property_name, "selectedIndex") == 0) {
        return KRYON_RUNTIME_PROP_INTEGER;
    }
    
    // Default to string
    return KRYON_RUNTIME_PROP_STRING;
}

// =============================================================================
// FORWARD DECLARATIONS
// =============================================================================

static KryonElement *load_element_from_binary(KryonRuntime *runtime, 
                                             const uint8_t *data, 
                                             size_t *offset, 
                                             size_t size,
                                             KryonElement *parent);
static bool load_element_properties(KryonElement *element,
                                   const uint8_t *data,
                                   size_t *offset,
                                   size_t size,
                                   uint32_t property_count);
static bool load_components_section(KryonRuntime* runtime, const uint8_t* data, size_t size, size_t* offset);
static bool load_property_value(KryonProperty *property,
                               const uint8_t *data,
                               size_t *offset,
                               size_t size);
static bool skip_styles_section(const uint8_t *data, size_t *offset, size_t size, uint32_t style_count);
static bool skip_functions_section(const uint8_t *data, size_t *offset, size_t size, uint32_t function_count);
static void register_event_directive_handlers(KryonRuntime *runtime, KryonElement *element);
static bool load_function_from_binary(KryonRuntime *runtime, const uint8_t *data, size_t *offset, size_t size, char** string_table, uint32_t string_count);
static void connect_function_handlers(KryonRuntime *runtime, KryonElement *element);

// =============================================================================
// UTILITY FUNCTIONS
// =============================================================================

static const char *get_element_type_name(uint16_t hex) {
    // Use centralized element mappings
    const char *name = kryon_get_widget_name(hex);
    if (name) {
        return name;
    }
    
    // Handle custom elements (hex >= 0x1000)
    if (hex >= 0x1000) {
        // For now, return a generic custom element name
        // TODO: In the future, custom element names should be stored in the KRB file
        static char custom_name[32];
        snprintf(custom_name, sizeof(custom_name), "CustomElement_0x%04X", hex);
        return custom_name;
    }
    
    return "Unknown";
}

static const char *get_property_name(uint16_t hex) {
    // Use centralized property mappings
    const char *name = kryon_get_property_name(hex);
    if (name) {
        return name;
    }
    
    // Handle custom component properties (hex >= 0x9000)
    if (hex >= 0x9000) {
        // For now, return a generic custom property name
        // TODO: In the future, custom property names should be stored in the KRB file
        static char custom_prop_name[32];
        snprintf(custom_prop_name, sizeof(custom_prop_name), "customProp_0x%04X", hex);
        return custom_prop_name;
    }
    
    return NULL;
}

static KryonRuntimePropertyType get_property_type(uint16_t hex) {
    // Use centralized property mappings with runtime type mapping
    const char *property_name = get_property_name(hex); // Use our updated function that handles custom properties
    if (property_name) {
        return get_runtime_property_type(property_name);
    }
    return KRYON_RUNTIME_PROP_STRING; // Default to string
}

static bool read_uint16_safe(const uint8_t *data, size_t *offset, size_t size, uint16_t *value) {
    return kryon_read_uint16(data, offset, size, value);
}

static bool read_uint32_safe(const uint8_t *data, size_t *offset, size_t size, uint32_t *value) {
    return kryon_read_uint32(data, offset, size, value);
}

static bool read_float_safe(const uint8_t *data, size_t *offset, size_t size, float *value) {
    if (*offset + sizeof(float) > size) {
        return false;
    }
    
    // Read as uint32 with endian conversion, then reinterpret as float
    uint32_t float_bits;
    if (!kryon_read_uint32(data, offset, size, &float_bits)) {
        return false;
    }
    
    memcpy(value, &float_bits, sizeof(float));
    return true;
}

static bool read_uint8_safe(const uint8_t *data, size_t *offset, size_t size, uint8_t *value) {
    return kryon_read_uint8(data, offset, size, value);
}

static bool read_int64_safe(const uint8_t *data, size_t *offset, size_t size, int64_t *value) {
    if (*offset + sizeof(int64_t) > size) return false;
    memcpy(value, data + *offset, sizeof(int64_t));
    *offset += sizeof(int64_t);
    return true;
}

static bool read_double_safe(const uint8_t *data, size_t *offset, size_t size, double *value) {
    if (*offset + sizeof(double) > size) return false;
    memcpy(value, data + *offset, sizeof(double));
    *offset += sizeof(double);
    return true;
}

// =============================================================================
// KRB LOADING IMPLEMENTATION
// =============================================================================

bool kryon_runtime_load_krb_data(KryonRuntime *runtime, const uint8_t *data, size_t size) {
    if (!runtime || !data || size < 128) { // Minimum size for complex format header
        return false;
    }
    
    size_t offset = 0;
    char** string_table = NULL;
    uint32_t string_count = 0;
    
    // Read and validate 128-byte complex header
    uint32_t magic;
    if (!read_uint32_safe(data, &offset, size, &magic)) {
        printf("DEBUG: Failed to read magic number\n");
        return false;
    }
    if (magic != 0x4B52594E) { // "KRYN"
        printf("DEBUG: Invalid magic number: 0x%08X (expected 0x4B52594E)\n", magic);
        return false;
    }
    
    // Read version fields (KRB v0.1 format)
    uint16_t version_major, version_minor, version_patch;
    if (!read_uint16_safe(data, &offset, size, &version_major) ||
        !read_uint16_safe(data, &offset, size, &version_minor) ||
        !read_uint16_safe(data, &offset, size, &version_patch)) {
        printf("DEBUG: Failed to read version\n");
        return false;
    }
    
    uint16_t flags;
    if (!read_uint16_safe(data, &offset, size, &flags)) {
        printf("DEBUG: Failed to read flags\n");
        return false;
    }
    
    printf("DEBUG: KRB file v%u.%u.%u, flags: 0x%04X\n", version_major, version_minor, version_patch, flags);
    
    // Read section counts from header
    uint32_t style_count, theme_count, widget_def_count, element_count, property_count;
    if (!read_uint32_safe(data, &offset, size, &style_count) ||
        !read_uint32_safe(data, &offset, size, &theme_count) ||
        !read_uint32_safe(data, &offset, size, &widget_def_count) ||
        !read_uint32_safe(data, &offset, size, &element_count) ||
        !read_uint32_safe(data, &offset, size, &property_count)) {
        printf("DEBUG: Failed to read section counts\n");
        return false;
    }
    
    // Read size and checksum fields
    uint32_t string_table_size, data_size, checksum;
    if (!read_uint32_safe(data, &offset, size, &string_table_size) ||
        !read_uint32_safe(data, &offset, size, &data_size) ||
        !read_uint32_safe(data, &offset, size, &checksum)) {
        printf("DEBUG: Failed to read size/checksum fields\n");
        return false;
    }
    
    // Skip compression byte and uncompressed size
    uint8_t compression;
    uint32_t uncompressed_size;
    if (!read_uint8_safe(data, &offset, size, &compression) ||
        !read_uint32_safe(data, &offset, size, &uncompressed_size)) {
        printf("DEBUG: Failed to read compression fields\n");
        return false;
    }
    
    // Read section offsets
    uint32_t style_offset, theme_offset, widget_def_offset, widget_inst_offset;
    uint32_t script_offset, resource_offset;
    if (!read_uint32_safe(data, &offset, size, &style_offset) ||
        !read_uint32_safe(data, &offset, size, &theme_offset) ||
        !read_uint32_safe(data, &offset, size, &widget_def_offset) ||
        !read_uint32_safe(data, &offset, size, &widget_inst_offset) ||
        !read_uint32_safe(data, &offset, size, &script_offset) ||
        !read_uint32_safe(data, &offset, size, &resource_offset)) {
        printf("DEBUG: Failed to read section offsets\n");
        return false;
    }
    
    // Skip reserved bytes (55 bytes)
    offset += 55;
    
    // Validate header size (should be 128 bytes now)
    if (offset != 128) {
        printf("DEBUG: Header size mismatch: %zu (expected 128)\n", offset);
        return false;
    }
    
    printf("DEBUG: Found %u styles, %u themes, %u widget defs, %u elements, %u properties in KRB file\n", 
           style_count, theme_count, widget_def_count, element_count, property_count);
    
    // Now we're at the start of the data sections after the 128-byte header
    // First should be the string table, then style definitions, etc.
    
    // Load string table
    printf("DEBUG: String table size from header: %u bytes (0x%08X)\n", string_table_size, string_table_size);
    if (string_table_size > 0) {
        if (string_table_size > 1000000) { // If > 1MB, something's wrong
            printf("DEBUG: String table size seems too large, possible endianness issue\n");
            return false;
        }
        
        size_t string_table_start = offset;
        runtime->string_table_offset = string_table_start;
        
        // Read string count
        if (!read_uint32_safe(data, &offset, size, &string_count)) {
            printf("DEBUG: Failed to read string count\n");
            return false;
        }
        
        printf("DEBUG: Loading %u strings from string table\n", string_count);
        
        // Allocate string table (handle 0 count case)
        if (string_count > 0) {
            string_table = kryon_alloc(string_count * sizeof(char*));
            if (!string_table) {
                printf("DEBUG: Failed to allocate string table\n");
                return false;
            }
            memset(string_table, 0, string_count * sizeof(char*));
        }
        
        // Read each string
        for (uint32_t i = 0; i < string_count; i++) {
            uint16_t str_len;
            if (!read_uint16_safe(data, &offset, size, &str_len)) {
                printf("DEBUG: Failed to read string length\n");
                goto cleanup;
            }
            
            if (offset + str_len > size) {
                printf("DEBUG: String data exceeds buffer size\n");
                goto cleanup;
            }
            
            string_table[i] = kryon_alloc(str_len + 1);
            if (!string_table[i]) {
                printf("DEBUG: Failed to allocate string %u\n", i);
                goto cleanup;
            }
            
            if (str_len > 0) {
                memcpy(string_table[i], data + offset, str_len);
            }
            string_table[i][str_len] = '\0';
            offset += str_len;
            
            printf("DEBUG: String[%u]: '%s'\n", i, string_table[i]);
        }
        
        // Verify we consumed the expected bytes
        if (offset - string_table_start != string_table_size) {
            printf("DEBUG: String table size mismatch: expected %u, got %zu\n", 
                   string_table_size, offset - string_table_start);
        }
    }
    
    // Navigate to widget instance section if we have elements
    if (element_count > 0 && widget_inst_offset > 0) {
        printf("DEBUG: Jumping to widget instance section at offset %u\n", widget_inst_offset);
        offset = widget_inst_offset;
    }
    
    // Load root element (should be the first one)
    if (element_count > 0) {
        runtime->root = load_element_from_binary(runtime, data, &offset, size, NULL);
        if (!runtime->root) {
            printf("DEBUG: Failed to load root element\n");
            return false;
        }
        printf("DEBUG: Successfully loaded root element: %s\n", runtime->root->type_name ? runtime->root->type_name : "unknown");
    } else {
        printf("DEBUG: No elements found in KRB file\n");
    }
    
    // Load Variables section if present (right before script section)
    if (script_offset > 0) {
        size_t vars_offset = script_offset;
        
        // Check for Variables section magic "VARS"
        uint32_t vars_magic;
        if (read_uint32_safe(data, &vars_offset, size, &vars_magic) && vars_magic == 0x56415253) {
            printf("DEBUG: Loading Variables section at offset %u\n", script_offset);
            
            // Read variable count
            uint32_t var_count;
            if (!read_uint32_safe(data, &vars_offset, size, &var_count)) {
                printf("DEBUG: Failed to read variable count\n");
                return false;
            }
            
            printf("DEBUG: Found %u variables in Variables section\n", var_count);
            
            // Initialize variable registry if needed
            if (!runtime->variable_names) {
                runtime->variable_names = kryon_alloc(var_count * sizeof(char*));
                runtime->variable_values = kryon_alloc(var_count * sizeof(char*));
                runtime->variable_capacity = var_count;
                runtime->variable_count = 0;
            }
            
            // Load each variable
            for (uint32_t i = 0; i < var_count; i++) {
                // Read variable name (string ref)
                uint32_t name_ref;
                if (!read_uint32_safe(data, &vars_offset, size, &name_ref)) {
                    printf("DEBUG: Failed to read variable name ref\n");
                    continue;
                }
                
                // Read variable type
                uint8_t var_type;
                if (!read_uint8_safe(data, &vars_offset, size, &var_type)) {
                    printf("DEBUG: Failed to read variable type\n");
                    continue;
                }
                
                // Read variable value based on type
                char *var_value = NULL;
                switch (var_type) {
                    case 0: { // String
                        uint8_t value_type;
                        if (read_uint8_safe(data, &vars_offset, size, &value_type) && value_type == KRYON_VALUE_STRING) {
                            uint32_t str_ref;
                            if (read_uint32_safe(data, &vars_offset, size, &str_ref) && str_ref < string_count) {
                                var_value = kryon_strdup(string_table[str_ref]);
                            }
                        }
                        break;
                    }
                    case 1: { // Integer
                        uint8_t value_type;
                        int64_t int_val;
                        if (read_uint8_safe(data, &vars_offset, size, &value_type) && 
                            value_type == KRYON_VALUE_INTEGER &&
                            read_int64_safe(data, &vars_offset, size, &int_val)) {
                            var_value = kryon_alloc(32);
                            snprintf(var_value, 32, "%lld", (long long)int_val);
                        }
                        break;
                    }
                    case 2: { // Float
                        uint8_t value_type;
                        double float_val;
                        if (read_uint8_safe(data, &vars_offset, size, &value_type) && 
                            value_type == KRYON_VALUE_FLOAT &&
                            read_double_safe(data, &vars_offset, size, &float_val)) {
                            var_value = kryon_alloc(32);
                            snprintf(var_value, 32, "%.6g", float_val);
                        }
                        break;
                    }
                    case 3: { // Boolean
                        uint8_t value_type;
                        uint8_t bool_val;
                        if (read_uint8_safe(data, &vars_offset, size, &value_type) && 
                            value_type == KRYON_VALUE_BOOLEAN &&
                            read_uint8_safe(data, &vars_offset, size, &bool_val)) {
                            var_value = kryon_strdup(bool_val ? "true" : "false");
                        }
                        break;
                    }
                    default:
                        // Skip unknown type
                        vars_offset += 5; // Skip value type + 4 bytes
                        break;
                }
                
                // Add to runtime variable registry
                if (name_ref < string_count && var_value && runtime->variable_count < runtime->variable_capacity) {
                    runtime->variable_names[runtime->variable_count] = kryon_strdup(string_table[name_ref]);
                    runtime->variable_values[runtime->variable_count] = var_value;
                    runtime->variable_count++;
                    printf("DEBUG: Loaded variable '%s' = '%s'\n", string_table[name_ref], var_value);
                } else {
                    kryon_free(var_value);
                }
            }
            
            // Update script_offset to point after Variables section
            script_offset = (uint32_t)vars_offset;
        }
    }
    
    // Load script section if present
    if (script_offset > 0) {
        printf("DEBUG: Loading script section at offset %u\n", script_offset);
        offset = script_offset;
        
        // Read function count
        uint32_t function_count;
        if (!read_uint32_safe(data, &offset, size, &function_count)) {
            printf("DEBUG: Failed to read function count\n");
            return false;
        }
        
        printf("DEBUG: Found %u functions in script section\n", function_count);
        
        // Load each function
        for (uint32_t i = 0; i < function_count; i++) {
            if (!load_function_from_binary(runtime, data, &offset, size, string_table, string_count)) {
                printf("DEBUG: Failed to load function %u\n", i);
                goto cleanup;
            }
        }
        
        // List all loaded functions for debugging
        printf("\n🔍 DEBUG: Available functions (%zu total):\n", runtime->function_count);
        for (size_t i = 0; i < runtime->function_count; i++) {
            printf("  [%zu] %s\n", i, runtime->function_names[i]);
        }
        printf("\n");
    }
    
    // Load component definitions if present
    // Components come after the script section in KRB files
    if (offset < size) {
        // Check for component magic "COMP"
        size_t comp_offset = offset;
        uint32_t comp_magic;
        if (read_uint32_safe(data, &comp_offset, size, &comp_magic) && comp_magic == 0x434F4D50) {
            printf("DEBUG: Loading components section at offset %zu\n", offset);
            
            // Reset offset to start of components section
            comp_offset = offset;
            
            // Load components until we reach the end or find non-component data
            while (comp_offset < size) {
                size_t section_start = comp_offset;
                
                // Try to load a component
                if (!load_components_section(runtime, data, size, &comp_offset)) {
                    // If component loading fails, we might be at the end of components
                    // or encountering other data
                    break;
                }
                
                // Make sure we made progress
                if (comp_offset <= section_start) {
                    printf("DEBUG: Component loading made no progress, stopping\n");
                    break;
                }
            }
            
            // List all loaded components for debugging
            printf("\n🔍 DEBUG: Available components (%zu total):\n", runtime->component_count);
            for (size_t i = 0; i < runtime->component_count; i++) {
                printf("  [%zu] %s (%zu params, %zu state vars)\n", i, 
                       runtime->components[i]->name,
                       runtime->components[i]->parameter_count,
                       runtime->components[i]->state_count);
            }
            printf("\n");
        }
    }
    
    // Cleanup string table
    if (string_table) {
        for (uint32_t i = 0; i < string_count; i++) {
            kryon_free(string_table[i]);
        }
        kryon_free(string_table);
    }
    
    return true;
    
cleanup:
    // Cleanup string table on error
    if (string_table) {
        for (uint32_t i = 0; i < string_count; i++) {
            kryon_free(string_table[i]);
        }
        kryon_free(string_table);
    }
    return false;
}

static KryonElement *load_element_from_binary(KryonRuntime *runtime, 
                                             const uint8_t *data, 
                                             size_t *offset, 
                                             size_t size,
                                             KryonElement *parent) {
    // Read widget instance header as per KRB v0.1 spec:
    // [Instance ID: u32] [Widget Type ID: u32] [Parent Instance ID: u32] [Style Reference ID: u32]
    // [Property Count: u16] [Child Count: u16] [Event Handler Count: u16] [Widget Flags: u32]
    
    if (*offset + 24 > size) { // Minimum header size for widget instance
        return NULL;
    }
    
    // Read widget instance header
    uint32_t instance_id, widget_type_id, parent_id, style_ref_id;
    uint16_t property_count, child_count, event_handler_count;
    uint32_t widget_flags;
    
    if (!read_uint32_safe(data, offset, size, &instance_id) ||
        !read_uint32_safe(data, offset, size, &widget_type_id) ||
        !read_uint32_safe(data, offset, size, &parent_id) ||
        !read_uint32_safe(data, offset, size, &style_ref_id) ||
        !read_uint16_safe(data, offset, size, &property_count) ||
        !read_uint16_safe(data, offset, size, &child_count) ||
        !read_uint16_safe(data, offset, size, &event_handler_count) ||
        !read_uint32_safe(data, offset, size, &widget_flags)) {
        printf("DEBUG: Failed to read widget instance header\n");
        return NULL;
    }
    
    // Create element using the widget type ID
    KryonElement *element = kryon_element_create(runtime, (uint16_t)widget_type_id, parent);
    if (!element) {
        return NULL;
    }
    
    // Set element type name with null check
    const char* type_name = kryon_get_widget_type_name((uint16_t)widget_type_id);
    if (type_name) {
        element->type_name = kryon_alloc(strlen(type_name) + 1);
        if (element->type_name) {
            strcpy(element->type_name, type_name);
        }
    } else {
        printf("❌ ERROR: Unknown widget type ID: %u\n", widget_type_id);
        element->type_name = kryon_alloc(strlen("Unknown") + 1);
        if (element->type_name) {
            strcpy(element->type_name, "Unknown");
        }
    }
    
    // Load properties
    if (property_count > 0) {
        printf("DEBUG: About to load %u properties for element %s\n", property_count, element->type_name);
        if (!load_element_properties(element, data, offset, size, property_count)) {
            kryon_element_destroy(runtime, element);
            return NULL;
        }
        printf("DEBUG: Successfully loaded %zu properties for element %s\n", element->property_count, element->type_name);
    }
    
    // Special handling for event directive elements
    if (widget_type_id == KRYON_ELEMENT_EVENT_DIRECTIVE) {
        register_event_directive_handlers(runtime, element);
    }
    
    // Connect function handlers for elements with onClick etc.
    connect_function_handlers(runtime, element);
    
    if (child_count > 0) {
        printf("DEBUG: Element %s has %u children\n", element->type_name, child_count);
    }
    
    // Load children
    printf("DEBUG: About to load %u children for element %s\n", child_count, element->type_name);
    for (uint32_t i = 0; i < child_count; i++) {
        printf("DEBUG: Loading child %u/%u (offset=%zu)\n", i+1, child_count, *offset);
        KryonElement *child = load_element_from_binary(runtime, data, offset, size, element);
        if (!child) {
            printf("DEBUG: Failed to load child %u\n", i+1);
            kryon_element_destroy(runtime, element);
            return NULL;
        }
        printf("DEBUG: Successfully loaded child %u: %s\n", i+1, child->type_name);
    }
    
    return element;
}

static bool load_element_properties(KryonElement *element,
                                   const uint8_t *data,
                                   size_t *offset,
                                   size_t size,
                                   uint32_t property_count) {
    // Allocate property array
    element->property_capacity = property_count;
    element->properties = kryon_alloc(property_count * sizeof(KryonProperty*));
    if (!element->properties) {
        return false;
    }
    
    for (uint32_t i = 0; i < property_count; i++) {
        if (*offset + 2 > size) {
            return false;
        }
        
        // Read property header
        uint16_t property_id;
        if (!read_uint16_safe(data, offset, size, &property_id)) {
            printf("DEBUG: Failed to read property ID\n");
            return false;
        }
        
        // Create property
        KryonProperty *property = kryon_alloc(sizeof(KryonProperty));
        if (!property) {
            return false;
        }
        
        property->id = property_id;
        const char* prop_name = get_property_name(property_id);
        if (prop_name) {
            property->name = kryon_alloc(strlen(prop_name) + 1);
            if (property->name) {
                strcpy(property->name, prop_name);
            }
        } else {
            printf("❌ ERROR: Unknown property ID: 0x%04X\n", property_id);
            property->name = kryon_alloc(strlen("unknown") + 1);
            if (property->name) {
                strcpy(property->name, "unknown");
            }
        }
        property->type = get_property_type(property_id);
        
        printf("DEBUG: Loading property %s (id=0x%04X, type=%d)\n", 
               property->name, property_id, property->type);
        
        // Handle special properties
        if (property_id == kryon_get_property_hex("id")) { // "id" property
            // Read the property value
            if (!load_property_value(property, data, offset, size)) {
                kryon_free(property->name);
                kryon_free(property);
                return false;
            }
            
            // Set element ID
            if (property->type == KRYON_RUNTIME_PROP_STRING && property->value.string_value) {
                element->element_id = kryon_alloc(strlen(property->value.string_value) + 1);
                if (element->element_id) {
                    strcpy(element->element_id, property->value.string_value);
                }
            }
        } else if (property_id == kryon_get_property_hex("class")) { // "class" property
            // Read the property value
            if (!load_property_value(property, data, offset, size)) {
                kryon_free(property->name);
                kryon_free(property);
                return false;
            }
            
            // Parse class names
            if (property->type == KRYON_RUNTIME_PROP_STRING && property->value.string_value) {
                // For simplicity, support single class name for now
                element->class_count = 1;
                element->class_names = kryon_alloc(sizeof(char*));
                if (element->class_names) {
                    element->class_names[0] = kryon_alloc(strlen(property->value.string_value) + 1);
                    if (element->class_names[0]) {
                        strcpy(element->class_names[0], property->value.string_value);
                    }
                }
            }
        } else {
            // Read property value
            if (!load_property_value(property, data, offset, size)) {
                kryon_free(property->name);
                kryon_free(property);
                return false;
            }
        }
        
        element->properties[element->property_count++] = property;
    }
    
    // Properties loaded successfully
    
    return true;
}

static bool load_property_value(KryonProperty *property,
                               const uint8_t *data,
                               size_t *offset,
                               size_t size) {
    // Load value based on the expected property type (no type tags in KRB)
    switch (property->type) {
        case KRYON_RUNTIME_PROP_STRING:
            {
                // For string properties, read as string (length + data)
                uint16_t string_length;
                if (!read_uint16_safe(data, offset, size, &string_length)) {
                    printf("DEBUG: Failed to read string length\n");
                    return false;
                }
                
                if (*offset + string_length > size) {
                    printf("DEBUG: String data exceeds buffer size\n");
                    return false;
                }
                
                property->value.string_value = kryon_alloc(string_length + 1);
                if (!property->value.string_value) {
                    printf("DEBUG: Failed to allocate string memory\n");
                    return false;
                }
                
                if (string_length > 0) {
                    memcpy(property->value.string_value, data + *offset, string_length);
                }
                property->value.string_value[string_length] = '\0';
                *offset += string_length;
                
                printf("DEBUG: Read string property '%s' = '%s'\n", property->name, property->value.string_value);
            }
            break;
            
        case KRYON_RUNTIME_PROP_INTEGER:
            {
                uint32_t int_value;
                if (!read_uint32_safe(data, offset, size, &int_value)) {
                    printf("DEBUG: Failed to read integer value\n");
                    return false;
                }
                property->value.int_value = (int64_t)int_value;
                printf("DEBUG: Read integer property '%s' = %lld\n", property->name, (long long)property->value.int_value);
            }
            break;
            
        case KRYON_RUNTIME_PROP_FLOAT:
            {
                // For float properties, read as uint32 and reinterpret as float
                uint32_t float_bits;
                if (!read_uint32_safe(data, offset, size, &float_bits)) {
                    printf("DEBUG: Failed to read float value\n"); 
                    return false;
                }
                float float_value;
                memcpy(&float_value, &float_bits, sizeof(float));
                property->value.float_value = (double)float_value;
                printf("DEBUG: Read float property '%s' = %f\n", property->name, float_value);
            }
            break;
            
        case KRYON_RUNTIME_PROP_BOOLEAN:
            {
                uint8_t bool_value;
                if (!read_uint8_safe(data, offset, size, &bool_value)) {
                    printf("DEBUG: Failed to read boolean value\n");
                    return false;
                }
                property->value.bool_value = bool_value != 0;
                printf("DEBUG: Read boolean property '%s' = %s\n", property->name, property->value.bool_value ? "true" : "false");
            }
            break;
            
        case KRYON_RUNTIME_PROP_COLOR:
            {
                // For color properties, read as uint32 RGBA value
                uint32_t color_value;
                if (!read_uint32_safe(data, offset, size, &color_value)) {
                    printf("DEBUG: Failed to read color value\n");
                    return false;
                }
                property->value.color_value = color_value;
                printf("DEBUG: Read color property '%s' = 0x%08X\n", property->name, color_value);
            }
            break;
            
        case KRYON_RUNTIME_PROP_FUNCTION:
            {
                // For function properties (like onClick handlers), read as string (function name)
                uint16_t string_length;
                if (!read_uint16_safe(data, offset, size, &string_length)) {
                    printf("DEBUG: Failed to read function name length\n");
                    return false;
                }
                
                if (*offset + string_length > size) {
                    printf("DEBUG: Function name data exceeds buffer size\n");
                    return false;
                }
                
                property->value.string_value = kryon_alloc(string_length + 1);
                if (!property->value.string_value) {
                    printf("DEBUG: Failed to allocate function name string\n");
                    return false;
                }
                
                memcpy(property->value.string_value, data + *offset, string_length);
                property->value.string_value[string_length] = '\0';
                *offset += string_length;
                
                printf("DEBUG: Read function property '%s' = '%s'\n", property->name, property->value.string_value);
            }
            break;
            
        default:
            printf("DEBUG: Unknown property type %d for property '%s' - skipping\n", property->type, property->name);
            // Skip unknown properties instead of failing
            break;
    }
    
    return true;
}

// =============================================================================
// PROPERTY ACCESS
// =============================================================================

bool kryon_element_set_property(KryonElement *element, uint16_t property_id, const void *value) {
    if (!element || !value) {
        return false;
    }
    
    // Find existing property
    for (size_t i = 0; i < element->property_count; i++) {
        if (element->properties[i]->id == property_id) {
            KryonProperty *prop = element->properties[i];
            
            // Update value based on type
            switch (prop->type) {
                case KRYON_RUNTIME_PROP_STRING:
                    kryon_free(prop->value.string_value);
                    prop->value.string_value = kryon_alloc(strlen((const char*)value) + 1);
                    if (prop->value.string_value) {
                        strcpy(prop->value.string_value, (const char*)value);
                    }
                    break;
                    
                case KRYON_RUNTIME_PROP_INTEGER:
                    prop->value.int_value = *(int64_t*)value;
                    break;
                    
                case KRYON_RUNTIME_PROP_FLOAT:
                    prop->value.float_value = *(double*)value;
                    break;
                    
                case KRYON_RUNTIME_PROP_BOOLEAN:
                    prop->value.bool_value = *(bool*)value;
                    break;
                    
                case KRYON_RUNTIME_PROP_COLOR:
                    prop->value.color_value = *(uint32_t*)value;
                    break;
                    
                default:
                    return false;
            }
            
            // Mark element for update
            kryon_element_invalidate_render(element);
            return true;
        }
    }
    
    // Property not found, add new one
    if (element->property_count >= element->property_capacity) {
        size_t new_capacity = element->property_capacity ? element->property_capacity * 2 : 4;
        KryonProperty **new_properties = kryon_realloc(element->properties,
                                                      new_capacity * sizeof(KryonProperty*));
        if (!new_properties) {
            return false;
        }
        element->properties = new_properties;
        element->property_capacity = new_capacity;
    }
    
    // Create new property
    KryonProperty *prop = kryon_alloc(sizeof(KryonProperty));
    if (!prop) {
        return false;
    }
    
    prop->id = property_id;
    const char* prop_name = get_property_name(property_id);
    if (prop_name) {
        prop->name = kryon_alloc(strlen(prop_name) + 1);
        if (prop->name) {
            strcpy(prop->name, prop_name);
        }
    } else {
        printf("❌ ERROR: Unknown property ID: 0x%04X\n", property_id);
        prop->name = kryon_alloc(strlen("unknown") + 1);
        if (prop->name) {
            strcpy(prop->name, "unknown");
        }
    }
    prop->type = get_property_type(property_id);
    
    // Set value based on type
    switch (prop->type) {
        case KRYON_RUNTIME_PROP_STRING:
            prop->value.string_value = kryon_alloc(strlen((const char*)value) + 1);
            if (prop->value.string_value) {
                strcpy(prop->value.string_value, (const char*)value);
            }
            break;
            
        case KRYON_RUNTIME_PROP_INTEGER:
            prop->value.int_value = *(int64_t*)value;
            break;
            
        case KRYON_RUNTIME_PROP_FLOAT:
            prop->value.float_value = *(double*)value;
            break;
            
        case KRYON_RUNTIME_PROP_BOOLEAN:
            prop->value.bool_value = *(bool*)value;
            break;
            
        case KRYON_RUNTIME_PROP_COLOR:
            prop->value.color_value = *(uint32_t*)value;
            break;
            
        default:
            kryon_free(prop->name);
            kryon_free(prop);
            return false;
    }
    
    element->properties[element->property_count++] = prop;
    kryon_element_invalidate_render(element);
    
    return true;
}

const void *kryon_element_get_property(const KryonElement *element, uint16_t property_id) {
    if (!element) {
        return NULL;
    }
    
    for (size_t i = 0; i < element->property_count; i++) {
        if (element->properties[i]->id == property_id) {
            KryonProperty *prop = element->properties[i];
            
            switch (prop->type) {
                case KRYON_RUNTIME_PROP_STRING:
                    return prop->value.string_value;
                case KRYON_RUNTIME_PROP_INTEGER:
                    return &prop->value.int_value;
                case KRYON_RUNTIME_PROP_FLOAT:
                    return &prop->value.float_value;
                case KRYON_RUNTIME_PROP_BOOLEAN:
                    return &prop->value.bool_value;
                case KRYON_RUNTIME_PROP_COLOR:
                    return &prop->value.color_value;
                default:
                    return NULL;
            }
        }
    }
    
    return NULL;
}

// =============================================================================
// EVENT DIRECTIVE HANDLING
// =============================================================================

/**
 * Register event handlers for event directive elements
 */
static void register_event_directive_handlers(KryonRuntime *runtime, KryonElement *element) {
    if (!runtime || !element || !runtime->event_system) {
        return;
    }
    
    printf("DEBUG: Registering event handlers for event directive\n");
    
    // Process each property as an event handler
    for (size_t i = 0; i < element->property_count; i++) {
        KryonProperty *prop = element->properties[i];
        if (!prop->name || !prop->value.string_value) {
            continue;
        }
        
        printf("DEBUG: Found event handler property: %s -> %s\n", 
               prop->name, prop->value.string_value);
        
        // Determine event type from property name
        if (strncmp(prop->name, "keyboard:", 9) == 0) {
            // Keyboard event: "keyboard:Ctrl+I" -> "Ctrl+I"
            const char *shortcut = prop->name + 9;
            const char *handler_name = prop->value.string_value;
            
            // TODO: Look up actual handler function from script system
            // For now, just register a placeholder handler
            printf("DEBUG: Would register keyboard handler for '%s' -> '%s'\n", 
                   shortcut, handler_name);
            
            // kryon_event_register_keyboard_handler(runtime->event_system, 
            //                                       shortcut, 
            //                                       script_handler, 
            //                                       handler_name);
            
        } else if (strncmp(prop->name, "mouse:", 6) == 0) {
            // Mouse event: "mouse:LeftClick" -> "LeftClick"
            const char *mouse_event = prop->name + 6;
            const char *handler_name = prop->value.string_value;
            
            printf("DEBUG: Would register mouse handler for '%s' -> '%s'\n", 
                   mouse_event, handler_name);
            
            // kryon_event_register_mouse_handler(runtime->event_system, 
            //                                    mouse_event, 
            //                                    script_handler, 
            //                                    handler_name);
        }
    }
}

// =============================================================================
// SECTION SKIPPING FUNCTIONS
// =============================================================================

/**
 * Skip styles section by reading and discarding style data
 */
static bool skip_styles_section(const uint8_t *data, size_t *offset, size_t size, uint32_t style_count) {
    for (uint32_t i = 0; i < style_count; i++) {
        // Skip style header ("STYL" magic)
        if (*offset + 4 > size) return false;
        *offset += 4;
        
        // Skip style name reference
        if (*offset + 4 > size) return false;
        *offset += 4;
        
        // Read property count
        uint32_t prop_count;
        if (!read_uint32_safe(data, offset, size, &prop_count)) {
            return false;
        }
        
        // Skip all properties
        for (uint32_t j = 0; j < prop_count; j++) {
            // Skip property ID
            if (*offset + 2 > size) return false;
            *offset += 2;
            
            // Skip property value (read type first)
            uint8_t value_type;
            if (!read_uint8_safe(data, offset, size, &value_type)) {
                return false;
            }
            
            // Skip value data based on type
            switch (value_type) {
                case 0: // String
                    {
                        uint16_t str_len;
                        if (!read_uint16_safe(data, offset, size, &str_len)) return false;
                        if (*offset + str_len > size) return false;
                        *offset += str_len;
                    }
                    break;
                case 1: // Integer
                case 2: // Float  
                case 4: // Color
                    if (*offset + 4 > size) return false;
                    *offset += 4;
                    break;
                case 3: // Boolean
                    if (*offset + 1 > size) return false;
                    *offset += 1;
                    break;
                default:
                    return false; // Unknown type
            }
        }
    }
    
    printf("DEBUG: Successfully skipped %u styles\n", style_count);
    return true;
}

/**
 * Skip functions section by reading and discarding function data
 */
static bool skip_functions_section(const uint8_t *data, size_t *offset, size_t size, uint32_t function_count) {
    for (uint32_t i = 0; i < function_count; i++) {
        // Skip function header ("FUNC" magic)
        if (*offset + 4 > size) return false;
        *offset += 4;
        
        // Skip language reference
        if (*offset + 4 > size) return false;
        *offset += 4;
        
        // Skip function name reference
        if (*offset + 4 > size) return false;
        *offset += 4;
        
        // Read parameter count
        uint32_t param_count;
        if (!read_uint32_safe(data, offset, size, &param_count)) {
            return false;
        }
        
        // Skip parameter references
        for (uint32_t j = 0; j < param_count; j++) {
            if (*offset + 4 > size) return false;
            *offset += 4;
        }
        
        // Skip function code reference
        if (*offset + 4 > size) return false;
        *offset += 4;
    }
    
    printf("DEBUG: Successfully skipped %u functions\n", function_count);
    return true;
}

/**
 * Load a function from the script section
 */
static bool load_function_from_binary(KryonRuntime *runtime, const uint8_t *data, size_t *offset, size_t size, char** string_table, uint32_t string_count) {
    if (!runtime || !data || !offset) {
        return false;
    }
    
    // Read function header ("FUNC" magic)
    uint32_t func_magic;
    if (!read_uint32_safe(data, offset, size, &func_magic)) {
        printf("DEBUG: Failed to read function magic\n");
        return false;
    }
    if (func_magic != 0x46554E43) { // "FUNC"
        printf("DEBUG: Invalid function magic: 0x%08X\n", func_magic);
        return false;
    }
    
    // Read language reference (string table index)
    uint32_t lang_ref;
    if (!read_uint32_safe(data, offset, size, &lang_ref)) {
        printf("DEBUG: Failed to read language reference\n");
        return false;
    }
    
    // Read function name reference (string table index)
    uint32_t name_ref;
    if (!read_uint32_safe(data, offset, size, &name_ref)) {
        printf("DEBUG: Failed to read name reference\n");
        return false;
    }
    
    // Read parameter count
    uint32_t param_count;
    if (!read_uint32_safe(data, offset, size, &param_count)) {
        printf("DEBUG: Failed to read parameter count\n");
        return false;
    }
    
    // Skip parameter references for now
    for (uint32_t i = 0; i < param_count; i++) {
        uint32_t param_ref;
        if (!read_uint32_safe(data, offset, size, &param_ref)) {
            printf("DEBUG: Failed to read parameter reference\n");
            return false;
        }
    }
    
    // Read function code reference (string table index - contains bytecode)
    uint32_t code_ref;
    if (!read_uint32_safe(data, offset, size, &code_ref)) {
        printf("DEBUG: Failed to read code reference\n");
        return false;
    }
    
    // Get strings from string table (1-based indices)
    const char* lang_str = (lang_ref > 0 && lang_ref <= string_count) ? string_table[lang_ref - 1] : NULL;
    const char* name_str = (name_ref > 0 && name_ref <= string_count) ? string_table[name_ref - 1] : NULL;
    const char* code_str = (code_ref > 0 && code_ref <= string_count) ? string_table[code_ref - 1] : NULL;
    
    printf("DEBUG: Found function: lang='%s', name='%s', params=%u\n",
           lang_str ? lang_str : "(null)",
           name_str ? name_str : "(null)", 
           param_count);
    
    // Check if this is a Lua function
    if (lang_str && strcmp(lang_str, "lua") == 0 && code_str) {
        // Create Lua VM if not exists
        if (!runtime->script_vm) {
            printf("DEBUG: Creating Lua VM for runtime\n");
            runtime->script_vm = kryon_vm_create(KRYON_VM_LUA, NULL);
            if (!runtime->script_vm) {
                printf("ERROR: Failed to create Lua VM\n");
                return false;
            }
        }
        
        // Convert hex string back to binary bytecode
        size_t hex_len = strlen(code_str);
        size_t bytecode_len = hex_len / 2;
        uint8_t* bytecode = kryon_alloc(bytecode_len);
        if (!bytecode) {
            printf("ERROR: Failed to allocate bytecode buffer\n");
            return false;
        }
        
        // Convert hex string to binary
        for (size_t i = 0; i < bytecode_len; i++) {
            char hex_byte[3] = {code_str[i*2], code_str[i*2 + 1], '\0'};
            bytecode[i] = (uint8_t)strtoul(hex_byte, NULL, 16);
        }
        
        printf("DEBUG: Converted %zu hex chars to %zu bytes of bytecode\n", hex_len, bytecode_len);
        
        // Load bytecode into Lua VM
        char* source_name_copy = name_str ? kryon_alloc(strlen(name_str) + 1) : NULL;
        if (source_name_copy && name_str) {
            strcpy(source_name_copy, name_str);
        }
        
        KryonScript script = {
            .vm_type = KRYON_VM_LUA,
            .format = KRYON_SCRIPT_BYTECODE,
            .data = bytecode,
            .size = bytecode_len,
            .source_name = source_name_copy,
            .entry_point = NULL
        };
        
        KryonVMResult result = kryon_vm_load_script(runtime->script_vm, &script);
        
        // Clean up bytecode buffer
        kryon_free(bytecode);
        kryon_free(source_name_copy);
        
        if (result != KRYON_VM_SUCCESS) {
            printf("ERROR: Failed to load Lua bytecode for function '%s' (result=%d)\n", name_str, result);
            const char* error = kryon_vm_get_error(runtime->script_vm);
            if (error) {
                printf("ERROR: %s\n", error);
            }
            return false;
        }
        
        printf("✅ Successfully loaded Lua function '%s' (%zu bytes of bytecode)\n", name_str, bytecode_len);
        
        // Add to runtime's function registry for debugging
        if (!runtime->function_names) {
            runtime->function_names = kryon_alloc(16 * sizeof(char*));
            runtime->function_capacity = 16;
            runtime->function_count = 0;
        }
        
        // Initialize variable registry if needed
        if (!runtime->variable_names) {
            runtime->variable_names = kryon_alloc(16 * sizeof(char*));
            runtime->variable_values = kryon_alloc(16 * sizeof(char*));
            runtime->variable_capacity = 16;
            runtime->variable_count = 0;
        }
        
        if (runtime->function_count < runtime->function_capacity && name_str) {
            runtime->function_names[runtime->function_count] = kryon_alloc(strlen(name_str) + 1);
            if (runtime->function_names[runtime->function_count]) {
                strcpy(runtime->function_names[runtime->function_count], name_str);
                runtime->function_count++;
            }
        }
    }
    
    return true;
}

/**
 * Connect Lua function handlers to element event properties
 */
static void connect_function_handlers(KryonRuntime *runtime, KryonElement *element) {
    if (!runtime || !element || !runtime->script_vm) {
        return;
    }
    
    // Look for function properties (onClick, onHover, etc.)
    for (size_t i = 0; i < element->property_count; i++) {
        KryonProperty *prop = element->properties[i];
        if (!prop || prop->type != KRYON_RUNTIME_PROP_FUNCTION) {
            continue;
        }
        
        // Check if this is an event handler property
        if (prop->name && strncmp(prop->name, "on", 2) == 0 && prop->value.string_value) {
            const char* event_name = prop->name + 2; // Skip "on" prefix
            const char* function_name = prop->value.string_value;
            
            printf("DEBUG: Connecting event handler: %s -> %s\n", prop->name, function_name);
            
            // Determine event type from property name
            KryonEventType event_type = 0;
            if (strcmp(prop->name, "onClick") == 0) {
                event_type = KRYON_EVENT_MOUSE_BUTTON_DOWN;
            } else if (strcmp(prop->name, "onHover") == 0) {
                event_type = KRYON_EVENT_MOUSE_MOVED;
            } else if (strcmp(prop->name, "onKeyDown") == 0) {
                event_type = KRYON_EVENT_KEY_DOWN;
            } else if (strcmp(prop->name, "onKeyUp") == 0) {
                event_type = KRYON_EVENT_KEY_UP;
            }
            
            if (event_type != 0) {
                // Create a wrapper event handler that calls the Lua function
                // For now, just log that we would connect it
                printf("DEBUG: Would register %s handler for Lua function '%s'\n", 
                       prop->name, function_name);
                
                // TODO: Create actual event handler that:
                // 1. Receives the event
                // 2. Calls kryon_vm_call_function(runtime->script_vm, function_name, ...)
                // 3. Passes element reference and event data to Lua
                
                // Example pseudo-code:
                // kryon_element_add_event_handler(element, event_type, 
                //                                lua_event_wrapper, function_name);
            }
        }
    }
}

// =============================================================================
// COMPONENT LOADING
// =============================================================================

static bool load_components_section(KryonRuntime* runtime, const uint8_t* data, size_t size, size_t* offset) {
    uint32_t comp_magic;
    if (!read_uint32_safe(data, offset, size, &comp_magic)) {
        return false;
    }
    
    if (comp_magic != 0x434F4D50) { // "COMP"
        return false;
    }
    
    // Read component name
    uint32_t name_ref;
    if (!read_uint32_safe(data, offset, size, &name_ref)) {
        return false;
    }
    
    // Create component definition
    KryonComponentDefinition* comp_def = kryon_alloc(sizeof(KryonComponentDefinition));
    if (!comp_def) {
        return false;
    }
    
    memset(comp_def, 0, sizeof(KryonComponentDefinition));
    
    // Resolve component name from string table
    const char* string_table = (const char*)(data + runtime->string_table_offset);
    comp_def->name = kryon_strdup(string_table + name_ref);
    
    // Read parameter count
    uint32_t param_count;
    if (!read_uint32_safe(data, offset, size, &param_count)) {
        kryon_free(comp_def->name);
        kryon_free(comp_def);
        return false;
    }
    
    comp_def->parameter_count = param_count;
    
    // Allocate parameter arrays
    if (param_count > 0) {
        comp_def->parameters = kryon_alloc(param_count * sizeof(char*));
        comp_def->param_defaults = kryon_alloc(param_count * sizeof(char*));
        
        if (!comp_def->parameters || !comp_def->param_defaults) {
            kryon_free(comp_def->name);
            kryon_free(comp_def->parameters);
            kryon_free(comp_def->param_defaults);
            kryon_free(comp_def);
            return false;
        }
        
        // Read parameters and defaults
        for (size_t i = 0; i < param_count; i++) {
            uint32_t param_ref, default_ref;
            
            if (!read_uint32_safe(data, offset, size, &param_ref) ||
                !read_uint32_safe(data, offset, size, &default_ref)) {
                // Cleanup on error
                for (size_t j = 0; j < i; j++) {
                    kryon_free(comp_def->parameters[j]);
                    kryon_free(comp_def->param_defaults[j]);
                }
                kryon_free(comp_def->name);
                kryon_free(comp_def->parameters);
                kryon_free(comp_def->param_defaults);
                kryon_free(comp_def);
                return false;
            }
            
            comp_def->parameters[i] = kryon_strdup(string_table + param_ref);
            comp_def->param_defaults[i] = default_ref ? kryon_strdup(string_table + default_ref) : NULL;
        }
    }
    
    // Read state variable count
    uint32_t state_count;
    if (!read_uint32_safe(data, offset, size, &state_count)) {
        // Cleanup
        for (size_t i = 0; i < param_count; i++) {
            kryon_free(comp_def->parameters[i]);
            kryon_free(comp_def->param_defaults[i]);
        }
        kryon_free(comp_def->name);
        kryon_free(comp_def->parameters);
        kryon_free(comp_def->param_defaults);
        kryon_free(comp_def);
        return false;
    }
    
    comp_def->state_count = state_count;
    
    // Allocate state variables array
    if (state_count > 0) {
        comp_def->state_vars = kryon_alloc(state_count * sizeof(KryonComponentStateVar));
        if (!comp_def->state_vars) {
            // Cleanup
            for (size_t i = 0; i < param_count; i++) {
                kryon_free(comp_def->parameters[i]);
                kryon_free(comp_def->param_defaults[i]);
            }
            kryon_free(comp_def->name);
            kryon_free(comp_def->parameters);
            kryon_free(comp_def->param_defaults);
            kryon_free(comp_def);
            return false;
        }
        
        // Read state variables (assuming they follow the variable format)
        for (size_t i = 0; i < state_count; i++) {
            uint32_t var_name_ref;
            uint8_t var_type;
            
            if (!read_uint32_safe(data, offset, size, &var_name_ref) ||
                !read_uint8_safe(data, offset, size, &var_type)) {
                // Cleanup
                for (size_t j = 0; j < i; j++) {
                    kryon_free(comp_def->state_vars[j].name);
                    kryon_free(comp_def->state_vars[j].default_value);
                }
                for (size_t j = 0; j < param_count; j++) {
                    kryon_free(comp_def->parameters[j]);
                    kryon_free(comp_def->param_defaults[j]);
                }
                kryon_free(comp_def->name);
                kryon_free(comp_def->parameters);
                kryon_free(comp_def->param_defaults);
                kryon_free(comp_def->state_vars);
                kryon_free(comp_def);
                return false;
            }
            
            comp_def->state_vars[i].name = kryon_strdup(string_table + var_name_ref);
            comp_def->state_vars[i].type = var_type;
            
            // Read default value based on type
            if (var_type == KRYON_VALUE_STRING) {
                uint32_t value_ref;
                if (!read_uint32_safe(data, offset, size, &value_ref)) {
                    comp_def->state_vars[i].default_value = NULL;
                } else {
                    comp_def->state_vars[i].default_value = kryon_strdup(string_table + value_ref);
                }
            } else if (var_type == KRYON_VALUE_INTEGER) {
                int64_t int_val;
                if (!read_int64_safe(data, offset, size, &int_val)) {
                    comp_def->state_vars[i].default_value = kryon_strdup("0");
                } else {
                    char int_str[32];
                    snprintf(int_str, sizeof(int_str), "%lld", (long long)int_val);
                    comp_def->state_vars[i].default_value = kryon_strdup(int_str);
                }
            } else if (var_type == KRYON_VALUE_FLOAT) {
                double float_val;
                if (!read_double_safe(data, offset, size, &float_val)) {
                    comp_def->state_vars[i].default_value = kryon_strdup("0.0");
                } else {
                    char float_str[32];
                    snprintf(float_str, sizeof(float_str), "%g", float_val);
                    comp_def->state_vars[i].default_value = kryon_strdup(float_str);
                }
            } else {
                comp_def->state_vars[i].default_value = kryon_strdup("");
            }
        }
    }
    
    // Add component to runtime registry
    if (runtime->component_count >= runtime->component_capacity) {
        size_t new_capacity = runtime->component_capacity ? runtime->component_capacity * 2 : 4;
        KryonComponentDefinition** new_components = kryon_realloc(runtime->components, 
                                                                  new_capacity * sizeof(KryonComponentDefinition*));
        if (!new_components) {
            // Cleanup component definition
            for (size_t i = 0; i < state_count; i++) {
                kryon_free(comp_def->state_vars[i].name);
                kryon_free(comp_def->state_vars[i].default_value);
            }
            for (size_t i = 0; i < param_count; i++) {
                kryon_free(comp_def->parameters[i]);
                kryon_free(comp_def->param_defaults[i]);
            }
            kryon_free(comp_def->name);
            kryon_free(comp_def->parameters);
            kryon_free(comp_def->param_defaults);
            kryon_free(comp_def->state_vars);
            kryon_free(comp_def);
            return false;
        }
        runtime->components = new_components;
        runtime->component_capacity = new_capacity;
    }
    
    runtime->components[runtime->component_count] = comp_def;
    runtime->component_count++;
    
    printf("✅ Loaded component definition: %s (%zu params, %zu state vars)\n", 
           comp_def->name, comp_def->parameter_count, comp_def->state_count);
    
    return true;
}