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

#include "runtime.h"
#include "krb_format.h"
#include "events.h"
#include "memory.h"
#include "binary_io.h"
#include "parser.h"
#include "component_state.h"
#include "../../shared/kryon_mappings.h"
#include "../../shared/krb_schema.h"
#include <string.h>
#include <stdlib.h>

// =============================================================================
// FORWARD DECLARATIONS
// =============================================================================

static KryonElement *load_element_from_binary(KryonRuntime *runtime,
                                             const uint8_t *data,
                                             size_t *offset,
                                             size_t size,
                                             KryonElement *parent);
static KryonElement *load_element_simple(KryonRuntime *runtime,
                                         const uint8_t *data,
                                         size_t *offset,
                                         size_t size,
                                         KryonElement *parent,
                                         char** string_table,
                                         uint32_t string_count,
                                         uint32_t *next_id);
static bool load_element_properties(KryonElement *element,
                                   const uint8_t *data,
                                   size_t *offset,
                                   size_t size,
                                   uint32_t property_count,
                                   KryonRuntime *runtime);
static bool load_components_section(KryonRuntime* runtime, const uint8_t* data, size_t size, size_t* offset, char** string_table, uint32_t string_count);
static bool load_property_value(KryonProperty *property,
                               const uint8_t *data,
                               size_t *offset,
                               size_t size,
                               KryonRuntime *runtime,
                               KryonElement *element);
static bool skip_styles_section(const uint8_t *data, size_t *offset, size_t size, uint32_t style_count);
static void register_event_directive_handlers(KryonRuntime *runtime, KryonElement *element);
static bool load_function_from_binary(KryonRuntime *runtime, const uint8_t *data, size_t *offset, size_t size, char** string_table, uint32_t string_count);
static void connect_function_handlers(KryonRuntime *runtime, KryonElement *element);
static void reprocess_property_bindings(KryonRuntime *runtime, KryonElement *element);
static char *resolve_template_property(KryonRuntime *runtime, KryonProperty *property);
static void kryon_free_template_segments(KryonProperty *property);
static void propagate_component_scope_up(KryonElement* element, const char* scope_id);
static void assign_component_scope_to_element(KryonElement* element, const char* binding_path);


static bool ensure_script_function_capacity(KryonRuntime* runtime, size_t required_count) {
    if (!runtime) {
        return false;
    }

    if (required_count <= runtime->function_capacity) {
        return true;
    }

    size_t new_capacity = runtime->function_capacity ? runtime->function_capacity * 2 : 8;
    while (new_capacity < required_count) {
        new_capacity *= 2;
    }

    KryonScriptFunction* new_storage = kryon_realloc(runtime->script_functions, new_capacity * sizeof(KryonScriptFunction));
    if (!new_storage) {
        printf("❌ Failed to allocate script function storage (requested %zu entries)\n", new_capacity);
        return false;
    }

    // Zero-initialize new slots to keep cleanup logic simple
    if (new_capacity > runtime->function_capacity) {
        size_t old_capacity = runtime->function_capacity;
        memset(new_storage + old_capacity, 0, (new_capacity - old_capacity) * sizeof(KryonScriptFunction));
    }

    runtime->script_functions = new_storage;
    runtime->function_capacity = new_capacity;
    return true;
}

static void reset_script_function(KryonScriptFunction* fn) {
    if (!fn) {
        return;
    }

    kryon_free(fn->name);
    kryon_free(fn->language);
    kryon_free(fn->code);
    if (fn->parameters) {
        for (uint16_t i = 0; i < fn->param_count; i++) {
            kryon_free(fn->parameters[i]);
        }
        kryon_free(fn->parameters);
    }
    memset(fn, 0, sizeof(*fn));
}



static void propagate_component_scope_up(KryonElement* element, const char* scope_id) {
    if (!scope_id) {
        return;
    }

    for (KryonElement* current = element; current; current = current->parent) {
        if (!current->component_scope_id) {
            current->component_scope_id = kryon_strdup(scope_id);
        } else if (strcmp(current->component_scope_id, scope_id) != 0) {
            break;
        }
    }
}

static void assign_component_scope_to_element(KryonElement* element, const char* binding_path) {
    if (!element || !binding_path) {
        return;
    }

    const char prefix[] = "comp_";
    if (strncmp(binding_path, prefix, sizeof(prefix) - 1) != 0) {
        return;
    }

    const char* dot = strchr(binding_path, '.');
    if (!dot || dot == binding_path) {
        return;
    }

    size_t prefix_len = (size_t)(dot - binding_path);
    char* scope_id = kryon_alloc(prefix_len + 1);
    if (!scope_id) {
        return;
    }
    memcpy(scope_id, binding_path, prefix_len);
    scope_id[prefix_len] = '\0';

    if (!element->component_scope_id) {
        element->component_scope_id = scope_id;
    } else {
        kryon_free(scope_id);
    }

    propagate_component_scope_up(element->parent, element->component_scope_id);
}

// =============================================================================
// UTILITY FUNCTIONS
// =============================================================================

static const char *get_element_type_name(uint16_t hex) {
    // Use centralized element mappings
    const char *name = kryon_get_element_name(hex);
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
    // Use centralized property type hints and map to runtime types
    KryonValueTypeHint type_hint = kryon_get_property_type_hint(hex);
    
    switch (type_hint) {
        case KRYON_TYPE_HINT_STRING:        return KRYON_RUNTIME_PROP_STRING;
        case KRYON_TYPE_HINT_INTEGER:       return KRYON_RUNTIME_PROP_INTEGER;
        case KRYON_TYPE_HINT_FLOAT:         return KRYON_RUNTIME_PROP_FLOAT;
        case KRYON_TYPE_HINT_BOOLEAN:       return KRYON_RUNTIME_PROP_BOOLEAN;
        case KRYON_TYPE_HINT_COLOR:         return KRYON_RUNTIME_PROP_COLOR;
        case KRYON_TYPE_HINT_UNIT:          return KRYON_RUNTIME_PROP_FLOAT; // Units stored as floats
        case KRYON_TYPE_HINT_DIMENSION:     return KRYON_RUNTIME_PROP_FLOAT; // Dimensions as floats
        case KRYON_TYPE_HINT_SPACING:       return KRYON_RUNTIME_PROP_FLOAT; // Spacing as floats
        case KRYON_TYPE_HINT_REFERENCE:     return KRYON_RUNTIME_PROP_FUNCTION; // Function references
        case KRYON_TYPE_HINT_ARRAY:         return KRYON_RUNTIME_PROP_ARRAY;
        default:                            return KRYON_RUNTIME_PROP_STRING;
    }
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
    
    // Set loading flag to prevent @for processing during KRB loading
    runtime->is_loading = true;
    
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
    uint32_t style_count, theme_count, element_def_count, element_count, property_count;
    if (!read_uint32_safe(data, &offset, size, &style_count) ||
        !read_uint32_safe(data, &offset, size, &theme_count) ||
        !read_uint32_safe(data, &offset, size, &element_def_count) ||
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
    uint32_t style_offset, theme_offset, element_def_offset, element_inst_offset;
    uint32_t script_offset, string_table_offset;
    if (!read_uint32_safe(data, &offset, size, &style_offset) ||
        !read_uint32_safe(data, &offset, size, &theme_offset) ||
        !read_uint32_safe(data, &offset, size, &element_def_offset) ||
        !read_uint32_safe(data, &offset, size, &element_inst_offset) ||
        !read_uint32_safe(data, &offset, size, &script_offset) ||
        !read_uint32_safe(data, &offset, size, &string_table_offset)) {
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
           style_count, theme_count, element_def_count, element_count, property_count);
    printf("DEBUG: Section offsets - Style: %u, Script: %u, String table: %u\n", 
           style_offset, script_offset, string_table_offset);
    
    // Load string table from its stored offset
    printf("DEBUG: String table size from header: %u bytes (0x%08X), offset: %u\n", 
           string_table_size, string_table_size, string_table_offset);
    if (string_table_size > 0 && string_table_offset > 0) {
        if (string_table_size > 1000000) { // If > 1MB, something's wrong
            printf("DEBUG: String table size seems too large, possible endianness issue\n");
            return false;
        }
        
        // Jump to string table location
        offset = string_table_offset;
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
    
    runtime->string_table = string_table;
    runtime->string_table_count = string_count;
    
    // Skip early Variables section loading - rely on later loading that works correctly
    
    // Navigate to widget instance section if we have elements
    if (element_count > 0 && element_inst_offset > 0) {
        printf("DEBUG: Jumping to widget instance section at offset %u\n", element_inst_offset);
        offset = element_inst_offset;
    }
    
    // Load root element (should be the first one) - variables are now available for binding
    if (element_count > 0) {
        runtime->root = load_element_from_binary(runtime, data, &offset, size, NULL);
        if (!runtime->root) {
            printf("DEBUG: Failed to load root element\n");
            return false;
        }
        printf("DEBUG: Successfully loaded root element\n");
        if (runtime->root && runtime->root->type_name) {
            printf("DEBUG: Root element type: %s\n", runtime->root->type_name);
        }
        printf("DEBUG: Element loading complete, about to load variables\n");
    } else {
        printf("DEBUG: No elements found in KRB file\n");
    }
    
    
    // Variables loading moved earlier in the loading process
        
    
    // ======================================================================
    // PHASE 1: Load Variables section FIRST (before any functions) 
    // Variables must be available before @onload functions execute
    // ======================================================================
    if (script_offset > 0) {
        printf("🔄 PHASE 1: Loading Variables section BEFORE functions at offset %u\n", script_offset);
        
        size_t vars_offset = script_offset;
        uint32_t vars_magic;
        if (read_uint32_safe(data, &vars_offset, size, &vars_magic)) {
            printf("DEBUG: Magic at offset %u: 0x%08X (expected 0x56415253)\n", script_offset, vars_magic);
            
            if (vars_magic == 0x56415253) { // "VARS" 
                uint32_t var_count;
                if (read_uint32_safe(data, &vars_offset, size, &var_count)) {
                    printf("DEBUG: Variable count: %u\n", var_count);
                    
                    if (var_count <= 10 && var_count > 0) {
                        runtime->variable_names = kryon_alloc(var_count * sizeof(char*));
                        runtime->variable_values = kryon_alloc(var_count * sizeof(char*));
                        runtime->variable_capacity = var_count;
                        runtime->variable_count = 0;
                        
                        for (uint32_t i = 0; i < var_count; i++) {
                            uint32_t name_ref;
                            if (!read_uint32_safe(data, &vars_offset, size, &name_ref)) break;
                            
                            uint8_t var_type;
                            if (!read_uint8_safe(data, &vars_offset, size, &var_type)) break;
                            
                            uint8_t var_flags;
                            if (!read_uint8_safe(data, &vars_offset, size, &var_flags)) break;
                            
                            uint8_t value_type;
                            if (!read_uint8_safe(data, &vars_offset, size, &value_type)) break;
                            
                            const char* var_name = (name_ref < string_count) ? string_table[name_ref] : "unknown";
                            
                            // Load typed variable values without conversion
                            if (value_type == KRYON_VALUE_STRING) {
                                uint32_t value_ref;
                                if (read_uint32_safe(data, &vars_offset, size, &value_ref)) {
                                    if (value_ref < string_count && string_table[value_ref]) {
                                        const char* string_value = string_table[value_ref];

                                        // Store variable as-is (including @ref: references)
                                        // Resolution will happen dynamically in kryon_runtime_get_variable()
                                        runtime->variable_names[runtime->variable_count] = kryon_strdup(var_name);
                                        runtime->variable_values[runtime->variable_count] = kryon_strdup(string_value);
                                        runtime->variable_count++;
                                        printf("DEBUG: Loaded STRING variable '%s' = '%s'\n", var_name, string_value);
                                    }
                                }
                            } else if (value_type == KRYON_VALUE_INTEGER) {
                                int32_t int_value;
                                uint32_t raw_value;
                                if (read_uint32_safe(data, &vars_offset, size, &raw_value)) {
                                    // Reinterpret as signed integer
                                    memcpy(&int_value, &raw_value, sizeof(int32_t));

                                    // Store integer as string until we implement typed variables properly
                                    char int_buffer[32];
                                    snprintf(int_buffer, sizeof(int_buffer), "%d", int_value);
                                    runtime->variable_names[runtime->variable_count] = kryon_strdup(var_name);
                                    runtime->variable_values[runtime->variable_count] = kryon_strdup(int_buffer);
                                    runtime->variable_count++;
                                    printf("DEBUG: Loaded INTEGER variable '%s' = %d\n", var_name, int_value);
                                }
                            } else if (value_type == KRYON_VALUE_BOOLEAN) {
                                uint8_t bool_value;
                                if (read_uint8_safe(data, &vars_offset, size, &bool_value)) {
                                    runtime->variable_names[runtime->variable_count] = kryon_strdup(var_name);
                                    runtime->variable_values[runtime->variable_count] = kryon_strdup(bool_value ? "true" : "false");
                                    runtime->variable_count++;
                                    printf("DEBUG: Loaded BOOLEAN variable '%s' = %s\n", var_name, bool_value ? "true" : "false");
                                }
                            } else {
                                printf("DEBUG: Skipped unknown variable type %d for '%s'\n", value_type, var_name);
                            }
                        }
                        
                        printf("✅ PHASE 1: Loaded %zu variables before function loading\n", runtime->variable_count);
                        printf("🔄 Reprocessing property bindings with loaded variables\n");
                        reprocess_property_bindings(runtime, runtime->root);
                    }
                }
            }
        }
    }
    
    // ======================================================================
    // PHASE 2: Load script section if present (functions AFTER variables)
    // ======================================================================
    if (script_offset > 0) {
        printf("🔄 PHASE 2: Loading Functions section AFTER variables at offset %u\n", script_offset);
        
        // Script offset points to Variables section, we need to find Functions section after it
        // Look for FUNC magic (0x46554E43 = "FUNC")
        size_t search_offset = script_offset;
        size_t func_offset = 0;
        bool found_func = false;
        
        printf("DEBUG: Searching for FUNC magic starting from offset %zu\n", search_offset);
        
        for (size_t i = search_offset; i <= size - 4; i++) {
            uint32_t magic = (data[i] << 24) | (data[i+1] << 16) | (data[i+2] << 8) | data[i+3];
            if (magic == 0x46554E43) { // "FUNC"
                printf("DEBUG: Found FUNC magic at offset %zu\n", i);
                func_offset = i;
                found_func = true;
                break;
            }
        }
        
        if (!found_func) {
            printf("DEBUG: No FUNC section found, skipping function loading\n");
        } else {
            offset = func_offset + 4; // Skip FUNC magic
            
            // Read function count
            uint32_t function_count;
            if (!read_uint32_safe(data, &offset, size, &function_count)) {
                printf("DEBUG: Failed to read function count\n");
                return false;
            }
            
            printf("DEBUG: Found %u functions in FUNC section\n", function_count);
            
            if (function_count > 100) {
                printf("DEBUG: Function count seems too high (%u), skipping function loading\n", function_count);
            } else {
                // Load each function
                for (uint32_t i = 0; i < function_count; i++) {
                    if (!load_function_from_binary(runtime, data, &offset, size, string_table, string_count)) {
                        printf("DEBUG: Failed to load function %u\n", i);
                        break; // Don't fail entire loading, just skip remaining functions
                    }
                }
            }
        }
        
        // List all loaded functions for debugging
        printf("\n🔍 DEBUG: Available functions (%zu total):\n", runtime->function_count);
        for (size_t i = 0; i < runtime->function_count; i++) {
            KryonScriptFunction* fn = &runtime->script_functions[i];
            printf("  [%zu] %s\n", i, fn->name ? fn->name : "<unnamed>");
        }
        printf("\n");
        
        // Clear loading flag BEFORE @for processing to allow template expansion
        runtime->is_loading = false;
        
        // Process @for and @if directives AFTER @onload functions have executed
        // This ensures variables populated by @onload are available for directive processing
        if (runtime->root) {
            printf("🔄 Processing @for directives after @onload execution\n");
            process_for_directives(runtime, runtime->root);
            printf("🔄 Completed @for directive processing\n");

            printf("🔄 Processing @if directives after @onload execution\n");
            process_if_directives(runtime, runtime->root);
            printf("🔄 Completed @if directive processing\n");
        }

        // Process @if directives in all component ui_templates
        for (size_t i = 0; i < runtime->component_count; i++) {
            if (runtime->components[i] && runtime->components[i]->ui_template) {
                printf("🔄 Processing @if directives for component '%s' ui_template\n", runtime->components[i]->name);
                process_if_directives(runtime, runtime->components[i]->ui_template);
                printf("✅ Completed @if processing for component '%s'\n", runtime->components[i]->name);
            }
        }
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
                printf("🔄 Attempting to load component at offset %zu\n", comp_offset);
                if (!load_components_section(runtime, data, size, &comp_offset, string_table, string_count)) {
                    // If component loading fails, we might be at the end of components
                    // or encountering other data
                    printf("⚠️  Component loading failed, stopping component section processing\n");
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
    
    // Variables are now loaded in PHASE 1 above (before functions)
    // Loading flag was cleared before @for processing above
    
    // Cleanup string table
    //if (string_table) {
    //    for (uint32_t i = 0; i < string_count; i++) {
    //        kryon_free(string_table[i]);
    //    }
    //    kryon_free(string_table);
    //}
    
    return true;
    
cleanup:
    // Clear loading flag on error (if not already cleared)
    runtime->is_loading = false;
    
    // Cleanup string table on error
    if (string_table) {
        for (uint32_t i = 0; i < string_count; i++) {
            kryon_free(string_table[i]);
        }
        kryon_free(string_table);
    }
    return false;
}

/**
 * Load element from simplified format used in component bodies
 * Format: type(uint16) + prop_count(uint16) + props + child_count(uint16) + children
 */
static KryonElement *load_element_simple(KryonRuntime *runtime,
                                         const uint8_t *data,
                                         size_t *offset,
                                         size_t size,
                                         KryonElement *parent,
                                         char** string_table,
                                         uint32_t string_count,
                                         uint32_t *next_id) {
    printf("🔄 Loading simplified element at offset %zu\n", *offset);

    // Read element type (uint16)
    uint16_t element_type;
    if (!read_uint16_safe(data, offset, size, &element_type)) {
        printf("❌ Failed to read element type\n");
        return NULL;
    }

    printf("🔍 Element type: 0x%04X\n", element_type);

    // Create element
    KryonElement *element = kryon_alloc(sizeof(KryonElement));
    if (!element) {
        return NULL;
    }
    memset(element, 0, sizeof(KryonElement));

    // Assign auto-incrementing ID
    element->id = (*next_id)++;
    element->type = element_type;
    element->parent = parent;
    element->visible = 1;

    // Get element type name
    element->type_name = kryon_strdup(kryon_get_element_name(element_type));

    // Read property count
    uint16_t prop_count;
    if (!read_uint16_safe(data, offset, size, &prop_count)) {
        printf("❌ Failed to read property count\n");
        kryon_free(element);
        return NULL;
    }

    printf("🔍 Property count: %u\n", prop_count);

    // Allocate properties
    if (prop_count > 0) {
        element->properties = kryon_alloc(prop_count * sizeof(KryonProperty));
        if (!element->properties) {
            kryon_free((void*)element->type_name);
            kryon_free(element);
            return NULL;
        }
        element->property_capacity = prop_count;
    }

    // Load properties
    if (!load_element_properties(element, data, offset, size, prop_count, runtime)) {
        printf("❌ Failed to load element properties\n");
        kryon_free(element->properties);
        kryon_free((void*)element->type_name);
        kryon_free(element);
        return NULL;
    }

    // Read child count
    uint16_t child_count;
    if (!read_uint16_safe(data, offset, size, &child_count)) {
        printf("❌ Failed to read child count\n");
        kryon_free(element->properties);
        kryon_free((void*)element->type_name);
        kryon_free(element);
        return NULL;
    }

    printf("🔍 Child count: %u\n", child_count);

    // Allocate children
    if (child_count > 0) {
        element->children = kryon_alloc(child_count * sizeof(KryonElement*));
        if (!element->children) {
            kryon_free(element->properties);
            kryon_free((void*)element->type_name);
            kryon_free(element);
            return NULL;
        }
        element->child_capacity = child_count;

        // Load children recursively
        for (size_t i = 0; i < child_count; i++) {
            element->children[i] = load_element_simple(runtime, data, offset, size, element,
                                                       string_table, string_count, next_id);
            if (!element->children[i]) {
                printf("❌ Failed to load child %zu\n", i);
                // Cleanup
                for (size_t j = 0; j < i; j++) {
                    // TODO: Free children recursively
                    kryon_free(element->children[j]);
                }
                kryon_free(element->children);
                kryon_free(element->properties);
                kryon_free((void*)element->type_name);
                kryon_free(element);
                return NULL;
            }
            element->child_count++;
        }
    }

    printf("✅ Loaded simplified element: %s (id=%u, %u props, %u children)\n",
           element->type_name, element->id, element->property_count, element->child_count);

    return element;
}


static KryonElement *load_element_from_binary(KryonRuntime *runtime,
                                             const uint8_t *data, 
                                             size_t *offset, 
                                             size_t size,
                                             KryonElement *parent) {
    // Use centralized schema-based element header validation
    KRBElementHeader header;
    
    printf("🔄 Loading element at offset %zu\n", *offset);
    
    // Validate and read element header using schema
    if (!krb_validate_element_header(data, size, offset, &header)) {
        printf("❌ Failed to validate element header at offset %zu\n", *offset);
        return NULL;
    }
    
    printf("🏗️  Loaded element header: id=%u type=0x%x props=%u children=%u events=%u\n", 
           header.instance_id, header.element_type, header.property_count, 
           header.child_count, header.event_count);
    
    // Create element using the element type ID from header
    KryonElement *element = kryon_element_create(runtime, (uint16_t)header.element_type, parent);
    if (!element) {
        return NULL;
    }
    

    const char* type_name = kryon_get_syntax_name((uint16_t)header.element_type);
    if (!type_name) {
        // If it's not a syntax keyword, it must be an element.
        type_name = kryon_get_element_name((uint16_t)header.element_type);
    }

    if (type_name) {
        element->type_name = kryon_alloc(strlen(type_name) + 1);
        if (element->type_name) {
            strcpy(element->type_name, type_name);
        }
    } else {
        printf("❌ ERROR: Unknown element type ID: %u\n", header.element_type);
        element->type_name = kryon_alloc(strlen("Unknown") + 1);
        if (element->type_name) {
            strcpy(element->type_name, "Unknown");
        }
    }
    
    // Load properties using schema-validated count
    if (header.property_count > 0) {
        printf("DEBUG: About to load %u properties for element %s\n", header.property_count, element->type_name);
        if (!load_element_properties(element, data, offset, size, header.property_count, runtime)) {
            kryon_element_destroy(runtime, element);
            return NULL;
        }
        // printf("DEBUG: Successfully loaded %zu properties for element %s\n", element->property_count, element->type_name);
    }
    
    // Special handling for syntax directives

    if (element->type_name && strcmp(element->type_name, "event") == 0) {
        register_event_directive_handlers(runtime, element);
    } else if (element->type_name && strcmp(element->type_name, "for") == 0) {
        // @for directives need special reactive processing
        element->needs_render = false; // Don't render template directly
    } else if (element->type_name && strcmp(element->type_name, "if") == 0) {
        // @if directives need special reactive processing
        element->needs_render = false; // Don't render template directly
        printf("🔍 DEBUG: Loaded @if directive element with condition\n");
    }
    
    // Check if this element represents a component instance
    printf("🔍 DEBUG: Element has element_type=0x%04X, checking if >= 0x2000 for component instance\n", 
           header.element_type);
    if (element->type_name) {
        printf("🔍 DEBUG: Element type name: %s\n", element->type_name);
    } else {
        printf("🔍 DEBUG: Element type name: (null)\n");
    }
    if (header.element_type >= 0x2000) { // Custom component type IDs start at 0x2000
        // Find the component definition
        for (size_t i = 0; i < runtime->component_count; i++) {
            if (runtime->components[i] && runtime->components[i]->name) {
                // Match component type name
                if (element->type_name && strstr(element->type_name, runtime->components[i]->name)) {
                    printf("🔗 Creating component instance for '%s'\n", runtime->components[i]->name);
                    
                    // Create component instance using the new state system
                    char element_id_str[32];
                    snprintf(element_id_str, sizeof(element_id_str), "%u", element->id);
                    KryonComponentInstance *comp_inst = kryon_component_instance_create(runtime, runtime->components[i], element_id_str, NULL);
                    if (comp_inst) {
                        printf("    Created component instance with ID: %s\n", comp_inst->component_id);

                        // Initialize state variables in the new typed system
                        for (size_t j = 0; j < comp_inst->definition->state_count; j++) {
                            const char* default_val = comp_inst->definition->state_vars[j].default_value ?
                                                     comp_inst->definition->state_vars[j].default_value : "0";
                            kryon_component_state_set_string(comp_inst->state_table,
                                                             comp_inst->definition->state_vars[j].name,
                                                             default_val);
                            printf("    Initialized state '%s' to '%s'\n",
                                   comp_inst->definition->state_vars[j].name, default_val);
                        }
                        
                                                
                        element->component_instance = comp_inst;
                        printf("✅ Successfully created component instance for '%s'\n", runtime->components[i]->name);
                    }
                    break;
                }
            }
        }
    }
    
    // Connect function handlers for elements with onClick etc.
    connect_function_handlers(runtime, element);
    
    if (header.child_count > 0) {
        printf("DEBUG: Element %s has %u children\n", element->type_name, header.child_count);
    }

    if (element->type_name) {
        printf("   element %s component_instance=%p parent=%p\n", element->type_name, (void*)element->component_instance, (void*)parent);
    }
    
    // Load children using schema-validated count
    printf("DEBUG: About to load %u children for element %s\n", header.child_count, element->type_name);
    for (uint32_t i = 0; i < header.child_count; i++) {
        printf("DEBUG: Loading child %u/%u (offset=%zu)\n", i+1, header.child_count, *offset);
        
        // MEMORY VALIDATION: Check string table integrity before child loading
        // printf("🔍 PRE-CHILD DEBUG: string_table=%p, count=%zu\n", 
        //        (void*)runtime->string_table, runtime->string_table_count);
        // if (runtime->string_table && runtime->string_table_count > 0) {
        //     for (size_t j = 0; j < runtime->string_table_count && j < 5; j++) {
        //         printf("🔍 PRE-CHILD DEBUG: string_table[%zu] = %p -> '%s'\n", 
        //                j, (void*)runtime->string_table[j], 
        //                runtime->string_table[j] ? runtime->string_table[j] : "NULL");
        //     }
        // }
        
        KryonElement *child = load_element_from_binary(runtime, data, offset, size, element);
        if (!child) {
            printf("DEBUG: Failed to load child %u\n", i+1);
            kryon_element_destroy(runtime, element);
            return NULL;
        }

        if (element->component_instance && !child->component_instance) {
            child->component_instance = element->component_instance;
        }
        if (!child->component_scope_id && element->component_scope_id) {
            child->component_scope_id = kryon_strdup(element->component_scope_id);
        }
        // printf("DEBUG: Successfully loaded child %u: %s\n", i+1, child->type_name ? child->type_name : "(null)");
    }
    
    return element;
}

static bool load_element_properties(KryonElement *element,
                                   const uint8_t *data,
                                   size_t *offset,
                                   size_t size,
                                   uint32_t property_count,
                                   KryonRuntime *runtime) {
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
        
        // Use centralized schema-based property header validation
        KRBPropertyHeader prop_header;
        if (!krb_validate_property_header(data, size, offset, &prop_header)) {
            printf("❌ Failed to validate property header at offset %zu\n", *offset);
            return false;
        }
        
        // Skip debug output to avoid printf crashes
        // printf("🔧 Loaded property header: id=0x%04x type=%u\n", (unsigned int)prop_header.property_id, (unsigned int)prop_header.value_type);
        
        // Create property
        KryonProperty *property = kryon_alloc(sizeof(KryonProperty));
        if (!property) {
            return false;
        }
        
        // Initialize all fields to prevent garbage values
        memset(property, 0, sizeof(KryonProperty));
        
        property->id = prop_header.property_id;
        property->type = prop_header.value_type;  // Use actual type from binary header
        const char* prop_name = get_property_name(prop_header.property_id);
        if (prop_name) {
            property->name = kryon_alloc(strlen(prop_name) + 1);
            if (property->name) {
                strcpy(property->name, prop_name);
            }
        } else {
            printf("❌ ERROR: Unknown property ID: 0x%04X\n", prop_header.property_id);
            property->name = kryon_alloc(strlen("unknown") + 1);
            if (property->name) {
                strcpy(property->name, "unknown");
            }
        }
        // DO NOT overwrite property->type here - use the actual type from binary header
        // The old code: property->type = get_property_type(prop_header.property_id);
        // This was incorrectly overwriting REFERENCE types with expected types
        
        // Disable debug output to avoid crashes
        // printf("DEBUG: Loading property %s (id=0x%04X, type=%d)\n", 
        //        property->name ? property->name : "(null)", prop_header.property_id, property->type);
        
        // Handle special properties
        if (prop_header.property_id == kryon_get_property_hex("id")) { // "id" property
            // Read the property value
            if (!load_property_value(property, data, offset, size, runtime, element)) {
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
        } else if (prop_header.property_id == kryon_get_property_hex("class")) { // "class" property
            // Read the property value
            if (!load_property_value(property, data, offset, size, runtime, element)) {
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
            // Note: Type prefix overrides are NOT needed here because we already have 
            // a valid property header with the correct value_type.
            // The original override logic was incorrectly checking the property VALUE 
            // (e.g., color 0x059669FF starts with 0x05) and mistaking it for a type prefix.
            // This caused colors starting with 0x05 to be misidentified as REFERENCE types.
            
            // Read property value
            printf("🔍 PROPERTY DEBUG: About to call load_property_value for property type=%d\n", property->type);
            printf("🔍 PROPERTY DEBUG: About to check property pointer\n");
            if (!property) {
                printf("❌ PROPERTY DEBUG: Property pointer is NULL!\n");
                return false;
            }
            printf("🔍 PROPERTY DEBUG: Property pointer is valid\n");
            printf("🔍 PROPERTY DEBUG: Property name exists: %s\n", property->name ? "yes" : "no");
            if (!load_property_value(property, data, offset, size, runtime, element)) {
                kryon_free(property->name);
                kryon_free(property);
                return false;
            }
        }
        
        // Validate property array integrity before adding
        printf("🔍 ARRAY DEBUG: About to add property %u to element\n", i);
        printf("🔍 ARRAY DEBUG: Current property_count: %zu\n", element->property_count);
        printf("🔍 ARRAY DEBUG: Property array capacity: %u\n", property_count);
        
        if (element->property_count >= property_count) {
            printf("❌ ARRAY DEBUG: Property array overflow!\n");
            return false;
        }
        
        element->properties[element->property_count++] = property;
        printf("🔍 ARRAY DEBUG: Successfully added property, new count: %zu\n", element->property_count);
    }
    
    // Properties loaded successfully
    
    return true;
}

static bool load_property_value(KryonProperty *property,
                               const uint8_t *data,
                               size_t *offset,
                               size_t size,
                               KryonRuntime *runtime,
                               KryonElement *element) {
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
                
                printf("DEBUG: Read string property '%s' = '%s'\n", property->name ? property->name : "(null)", property->value.string_value);
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
                printf("DEBUG: Read integer property '%s' = %lld\n", property->name ? property->name : "(null)", (long long)property->value.int_value);
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
                
                // Check if this is a sentinel value indicating a reactive variable reference
                if (float_value == -99999.0f && runtime) {
                    // This property has a reactive variable reference that needs to be resolved
                    // For now, we need to implement a mapping from property to variable name
                    // As a temporary solution, assume common patterns
                    const char* var_value = NULL;
                    
                    if (property->name && strcmp(property->name, "posY") == 0) {
                        // posY with sentinel likely refers to $root.height or similar
                        var_value = kryon_runtime_get_variable(runtime, "root.height");
                        if (var_value) {
                            float resolved_value = (float)atof(var_value);
                            property->value.float_value = (double)resolved_value;
                            printf("🔄 Resolved float reactive variable '%s' to %f (from root.height='%s')\n", 
                                   property->name ? property->name : "(null)", resolved_value, var_value);
                        } else {
                            property->value.float_value = 0.0; // Default fallback
                            printf("⚠️  Failed to resolve reactive variable for '%s', using 0.0\n", property->name);
                        }
                    } else if (property->name && strcmp(property->name, "posX") == 0) {
                        // posX with sentinel likely refers to $root.width or similar
                        var_value = kryon_runtime_get_variable(runtime, "root.width");
                        if (var_value) {
                            float resolved_value = (float)atof(var_value);
                            property->value.float_value = (double)resolved_value;
                            printf("🔄 Resolved float reactive variable '%s' to %f (from root.width='%s')\n", 
                                   property->name ? property->name : "(null)", resolved_value, var_value);
                        } else {
                            property->value.float_value = 0.0; // Default fallback
                            printf("⚠️  Failed to resolve reactive variable for '%s', using 0.0\n", property->name);
                        }
                    } else {
                        // Generic sentinel handling - mark for runtime reactive binding
                        property->value.float_value = 0.0; // Default fallback
                        property->is_bound = true;
                        property->binding_path = kryon_strdup("unknown_float_variable"); // TODO: store actual path
                        printf("🔗 Float property '%s' marked as reactive (sentinel detected)\n", property->name);
                    }
                } else {
                    property->value.float_value = (double)float_value;
                    printf("DEBUG: Read float property '%s' = %f\n", property->name ? property->name : "(null)", float_value);
                }
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
                printf("DEBUG: Read boolean property '%s' = %s\n", property->name ? property->name : "(null)", property->value.bool_value ? "true" : "false");
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
                printf("DEBUG: Read color property '%s' = 0x%08X\n", property->name ? property->name : "(null)", color_value);
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
                
                printf("DEBUG: Read function property '%s' = '%s'\n", property->name ? property->name : "(null)", property->value.string_value);
            }
            break;
            
        case KRYON_RUNTIME_PROP_ARRAY:
            {
                uint16_t count;
                if (!read_uint16_safe(data, offset, size, &count)) {
                    return false;
                }
                property->value.array_value.count = count;
                property->value.array_value.values = kryon_alloc(count * sizeof(char*));
                for (uint16_t i = 0; i < count; i++) {
                    uint32_t string_ref;
                    if (!read_uint32_safe(data, offset, size, &string_ref)) {
                        // cleanup
                        for(uint16_t j = 0; j < i; j++) {
                            kryon_free(property->value.array_value.values[j]);
                        }
                        kryon_free(property->value.array_value.values);
                        return false;
                    }
                    
                    if (string_ref < runtime->string_table_count) {
                        property->value.array_value.values[i] = kryon_strdup(runtime->string_table[string_ref]);
                    } else {
                        property->value.array_value.values[i] = NULL;
                    }
                }
            }
            break;
            
        case KRYON_RUNTIME_PROP_REFERENCE:
            {
                // Read the property type prefix
                uint8_t prop_type;
                if (!read_uint8_safe(data, offset, size, &prop_type)) {
                    printf("DEBUG: Failed to read REFERENCE property type prefix\n");
                    return false;
                }
                
                if (prop_type != KRYON_RUNTIME_PROP_REFERENCE) {
                    printf("DEBUG: Expected REFERENCE type prefix, got %d\n", prop_type);
                    return false;
                }
                
                // Read variable name reference (uint32_t pointing to string table)
                uint32_t variable_name_ref;
                if (!read_uint32_safe(data, offset, size, &variable_name_ref)) {
                    printf("DEBUG: Failed to read variable name reference\n");
                    return false;
                }
                
                // Safely access string table with comprehensive validation
                // printf("🔍 REFERENCE DEBUG: Processing reference property\n");
                // printf("🔍 REFERENCE DEBUG: variable_name_ref value is available\n");
                // printf("🔍 REFERENCE DEBUG: runtime structure is valid\n");
                
                const char* variable_name = NULL;
                
                // Validate string table and reference bounds
                if (!runtime->string_table) {
                    // printf("❌ REFERENCE DEBUG: String table is NULL!\n");
                    property->value.string_value = kryon_strdup("no_string_table");
                    property->is_bound = false;
                    property->binding_path = NULL;
                } else if (variable_name_ref >= runtime->string_table_count) {
                    // printf("❌ REFERENCE DEBUG: Invalid reference %u >= %zu\n", 
                    //        variable_name_ref, runtime->string_table_count);
                    property->value.string_value = kryon_strdup("invalid_ref");
                    property->is_bound = false;
                    property->binding_path = NULL;
                } else {
                    // Safe string table access with validation
                    char* string_ptr = runtime->string_table[variable_name_ref];
                    // printf("🔍 REFERENCE DEBUG: string_table[%u] = %p\n", variable_name_ref, (void*)string_ptr);
                    
                    if (!string_ptr) {
                        // printf("❌ REFERENCE DEBUG: String pointer is NULL at index %u\n", variable_name_ref);
                        property->value.string_value = kryon_strdup("null_ptr");
                        property->is_bound = false;
                        property->binding_path = NULL;
                    } else {
                        // Validate the string by checking if it's a reasonable pointer
                        // Use simple validation instead of strnlen which may crash on corrupted memory
                        // String appears valid
                        variable_name = string_ptr;
                        // printf("✅ REFERENCE DEBUG: Valid string '%s' (ref=%u)\n", 
                        //        variable_name, variable_name_ref);
                        
                        // Strip template syntax from variable names - compiler should only store clean names
                        const char* clean_var_name = (variable_name[0] == '$') ? variable_name + 1 : variable_name;
                        
                        // Set up reactive binding with clean variable name
                        property->value.string_value = kryon_strdup(variable_name);  // Temporary fallback for debugging
                        property->is_bound = true;
                        property->binding_path = kryon_strdup(clean_var_name);  // Store clean name for runtime lookup
                        
                        printf("🔗 Property '%s' bound to variable '%s'\n", 
                               property->name ? property->name : "(null)", variable_name);
                        assign_component_scope_to_element(element, property->binding_path);

                        printf("🔗 Property '%s' bound to variable '%s'\n", 
                               property->name ? property->name : "(null)", variable_name);
                    }
                }
            }
            break;
            
        case KRYON_RUNTIME_PROP_TEMPLATE:
            {
                // Read the property type prefix
                uint8_t prop_type;
                if (!read_uint8_safe(data, offset, size, &prop_type)) {
                    printf("DEBUG: Failed to read TEMPLATE property type prefix\n");
                    return false;
                }
                
                if (prop_type != KRYON_RUNTIME_PROP_TEMPLATE) {
                    printf("DEBUG: Expected TEMPLATE type prefix, got %d\n", prop_type);
                    return false;
                }
                
                // Read segment count
                uint16_t segment_count;
                if (!read_uint16_safe(data, offset, size, &segment_count)) {
                    printf("DEBUG: Failed to read template segment count\n");
                    return false;
                }
                
                printf("📝 Loading template with %u segments\n", segment_count);
                
                // Allocate segments array
                property->value.template_value.segment_count = segment_count;
                property->value.template_value.segments = kryon_alloc(segment_count * sizeof(KryonTemplateSegment));
                if (!property->value.template_value.segments) {
                    printf("DEBUG: Failed to allocate template segments\n");
                    return false;
                }
                
                // Load each segment
                for (uint16_t i = 0; i < segment_count; i++) {
                    // Read segment type
                    uint8_t segment_type;
                    if (!read_uint8_safe(data, offset, size, &segment_type)) {
                        printf("DEBUG: Failed to read segment %u type\n", i);
                        return false;
                    }
                    
                    KryonTemplateSegment *segment = &property->value.template_value.segments[i];
                    segment->type = (KryonTemplateSegmentType)segment_type;
                    
                    if (segment_type == KRYON_TEMPLATE_SEGMENT_LITERAL) {
                        // Read literal text (length + data)
                        uint16_t text_len;
                        if (!read_uint16_safe(data, offset, size, &text_len)) {
                            printf("DEBUG: Failed to read literal text length for segment %u\n", i);
                            return false;
                        }
                        
                        if (*offset + text_len > size) {
                            printf("DEBUG: Literal text data exceeds buffer size for segment %u\n", i);
                            return false;
                        }
                        
                        segment->data.literal_text = kryon_alloc(text_len + 1);
                        if (!segment->data.literal_text) {
                            printf("DEBUG: Failed to allocate literal text for segment %u\n", i);
                            return false;
                        }
                        
                        if (text_len > 0) {
                            memcpy(segment->data.literal_text, data + *offset, text_len);
                        }
                        segment->data.literal_text[text_len] = '\0';
                        *offset += text_len;
                        
                        printf("  📝 Loaded literal segment %u: '%s'\n", i, segment->data.literal_text);
                        
                    } else if (segment_type == KRYON_TEMPLATE_SEGMENT_VARIABLE) {
                        // Read variable name reference
                        uint32_t var_name_ref;
                        if (!read_uint32_safe(data, offset, size, &var_name_ref)) {
                            printf("DEBUG: Failed to read variable name reference for segment %u\n", i);
                            return false;
                        }
                        
                        // Convert string table reference to variable name
                        if (var_name_ref < runtime->string_table_count) {
                            segment->data.variable_name = kryon_strdup(runtime->string_table[var_name_ref]);
                            printf("  🔗 Loaded variable segment %u: '%s' (string_ref=%u)\n", 
                                   i, segment->data.variable_name, var_name_ref);
                        } else {
                            printf("DEBUG: Invalid variable name reference %u for segment %u\n", var_name_ref, i);
                            segment->data.variable_name = kryon_strdup("");
                        }
                        
                    } else {
                        printf("DEBUG: Unknown template segment type %u for segment %u\n", segment_type, i);
                        return false;
                    }
                }
                
                printf("✅ Template property loaded with %u segments\n", segment_count);
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
    
    // Initialize all fields to prevent garbage values
    memset(prop, 0, sizeof(KryonProperty));
    
    prop->id = property_id;
    const char* prop_name = get_property_name(property_id);
    if (prop_name) {
        prop->name = kryon_alloc(strlen(prop_name) + 1);
        if (prop->name) {
            strcpy(prop->name, prop_name);
        }
        // Debug: log property mapping for alignment properties
        if (property_id == 0x0606 || property_id == 0x0607) {
            printf("🔍 KRB PROPERTY LOAD: hex=0x%04X → name='%s'\n", property_id, prop_name);
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
 * Load a function from the script section
 */
static bool load_function_from_binary(KryonRuntime *runtime, const uint8_t *data, size_t *offset, size_t size, char** string_table, uint32_t string_count) {
    if (!runtime || !data || !offset) {
        return false;
    }

    // Read function header using KRB schema format
    uint32_t func_magic;
    if (!read_uint32_safe(data, offset, size, &func_magic)) {
        printf("DEBUG: Failed to read function magic\n");
        return false;
    }
    
    if (func_magic != KRB_MAGIC_FUNC) {
        printf("DEBUG: Invalid function magic: 0x%08X (expected 0x%08X)\n", func_magic, KRB_MAGIC_FUNC);
        return false;
    }
    
    uint32_t lang_ref;
    if (!read_uint32_safe(data, offset, size, &lang_ref)) {
        printf("DEBUG: Failed to read language reference\n");
        return false;
    }

    uint32_t name_ref;
    if (!read_uint32_safe(data, offset, size, &name_ref)) {
        printf("DEBUG: Failed to read name reference\n");
        return false;
    }
    
    // Read parameter count as uint16_t (KRB schema format)
    uint16_t param_count;
    if (!read_uint16_safe(data, offset, size, &param_count)) {
        printf("DEBUG: Failed to read parameter count\n");
        return false;
    }
    
    printf("DEBUG: Function header - magic=0x%08X, lang_ref=%u, name_ref=%u, param_count=%u (offset now %zu)\n", 
           func_magic, lang_ref, name_ref, param_count, *offset);

    // Ensure capacity for the new function entry
    size_t function_index = runtime->function_count;
    if (!ensure_script_function_capacity(runtime, function_index + 1)) {
        printf("❌ Failed to reserve slot for function index %zu\n", function_index);
        return false;
    }

    KryonScriptFunction* function = &runtime->script_functions[function_index];
    memset(function, 0, sizeof(*function));
    function->param_count = param_count;

    const char* language_str = (lang_ref < string_count && string_table && string_table[lang_ref]) ? string_table[lang_ref] : "";
    const char* name_str = (name_ref < string_count && string_table && string_table[name_ref]) ? string_table[name_ref] : NULL;

    function->language = language_str ? kryon_strdup(language_str) : NULL;
    function->name = name_str ? kryon_strdup(name_str) : kryon_strdup("anonymous");

    if (param_count > 0) {
        function->parameters = kryon_alloc(param_count * sizeof(char*));
        if (!function->parameters) {
            printf("❌ Failed to allocate parameter list for function '%s' (%u params)\n", function->name ? function->name : "<null>", param_count);
            reset_script_function(function);
            return false;
        }
        memset(function->parameters, 0, param_count * sizeof(char*));
    }

    for (uint16_t i = 0; i < param_count; i++) {
        uint32_t param_ref;
        if (!read_uint32_safe(data, offset, size, &param_ref)) {
            printf("DEBUG: Failed to read parameter reference %u\n", i);
            reset_script_function(function);
            return false;
        }

        if (function->parameters) {
            const char* param_name = (param_ref < string_count && string_table && string_table[param_ref]) ? string_table[param_ref] : NULL;
            function->parameters[i] = param_name ? kryon_strdup(param_name) : kryon_strdup("param");
        }
    }

    // Read flags field (KRB schema format)
    uint16_t flags;
    if (!read_uint16_safe(data, offset, size, &flags)) {
        printf("DEBUG: Failed to read function flags\n");
        reset_script_function(function);
        return false;
    }
    printf("DEBUG: Read flags=0x%04X at offset %zu\n", flags, *offset - 2);

    // Read function code reference (string table index - contains script source)
    uint32_t code_ref;
    printf("DEBUG: About to read code_ref at offset %zu\n", *offset);
    if (!read_uint32_safe(data, offset, size, &code_ref)) {
        printf("DEBUG: Failed to read code reference\n");
        reset_script_function(function);
        return false;
    }
    printf("DEBUG: Read code_ref=%u at offset %zu\n", code_ref, *offset - 4);

    const char* code_str = (code_ref < string_count && string_table && string_table[code_ref]) ? string_table[code_ref] : NULL;
    function->code = code_str ? kryon_strdup(code_str) : kryon_strdup("");

    if (!function->name || !function->language || !function->code) {
        printf("❌ Failed to duplicate strings for function (name_ref=%u, code_ref=%u)\n", name_ref, code_ref);
        reset_script_function(function);
        return false;
    }

    runtime->function_count++;
    return true;
}

static void connect_function_handlers(KryonRuntime *runtime, KryonElement *element) {
    (void)runtime;
    (void)element;
}

// =============================================================================
// COMPONENT LOADING
// =============================================================================

static bool load_components_section(KryonRuntime* runtime, const uint8_t* data, size_t size, size_t* offset, char** string_table, uint32_t string_count) {
    printf("📖 Reading component at offset %zu\n", *offset);
    
    uint32_t comp_magic;
    if (!read_uint32_safe(data, offset, size, &comp_magic)) {
        printf("❌ Failed to read component magic\n");
        return false;
    }
    
    printf("🔍 Component magic: 0x%08X (expected 0x434F4D50)\n", comp_magic);
    
    if (comp_magic != 0x434F4D50) { // "COMP"
        printf("❌ Invalid component magic: 0x%08X\n", comp_magic);
        return false;
    }
    
    // Read component name
    uint32_t name_ref;
    if (!read_uint32_safe(data, offset, size, &name_ref)) {
        printf("❌ Failed to read component name reference\n");
        return false;
    }
    
    printf("🔍 Component name ref: %u (string_count=%u)\n", name_ref, string_count);
    
    // Create component definition
    KryonComponentDefinition* comp_def = kryon_alloc(sizeof(KryonComponentDefinition));
    if (!comp_def) {
        return false;
    }
    
    memset(comp_def, 0, sizeof(KryonComponentDefinition));
    
    // String table indices are already 0-based (no conversion needed)
    uint32_t string_index = name_ref;
    if (string_index >= string_count || !string_table[string_index]) {
        printf("❌ Invalid component name reference: ref=%u, string_index=%u, string_count=%u, string_exists=%s\n", 
               name_ref, string_index, string_count, (string_index < string_count && string_table[string_index]) ? "yes" : "no");
        kryon_free(comp_def);
        return false;
    }
    comp_def->name = kryon_strdup(string_table[string_index]);
    printf("✅ Component name: '%s'\n", comp_def->name);
    
    // Read parameter count
    uint16_t param_count;
    if (!read_uint16_safe(data, offset, size, &param_count)) {
        printf("❌ Failed to read parameter count\n");
        kryon_free(comp_def->name);
        kryon_free(comp_def);
        return false;
    }
    
    printf("🔍 Parameter count: %u\n", param_count);
    
    comp_def->parameter_count = param_count;
    
    // Allocate parameter arrays
    printf("🔍 Allocating arrays for %u parameters\n", param_count);
    if (param_count > 0) {
        comp_def->parameters = kryon_alloc(param_count * sizeof(char*));
        comp_def->param_defaults = kryon_alloc(param_count * sizeof(char*));
        
        if (!comp_def->parameters || !comp_def->param_defaults) {
            printf("❌ Failed to allocate parameter arrays\n");
            kryon_free(comp_def->name);
            kryon_free(comp_def->parameters);
            kryon_free(comp_def->param_defaults);
            kryon_free(comp_def);
            return false;
        }
        
        // Read parameters and defaults
        printf("🔍 Reading %u parameters\n", param_count);
        for (size_t i = 0; i < param_count; i++) {
            printf("🔍 Reading parameter %zu/%u\n", i+1, param_count);
            
            // Read parameter name reference
            uint32_t param_ref;
            if (!read_uint32_safe(data, offset, size, &param_ref)) {
                printf("❌ Failed to read param_ref for parameter %zu\n", i);
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
            
            // Read has_default flag
            uint8_t has_default;
            if (!read_uint8_safe(data, offset, size, &has_default)) {
                printf("❌ Failed to read has_default for parameter %zu\n", i);
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
            
            printf("🔍 Parameter %zu: param_ref=%u, has_default=%u\n", i, param_ref, has_default);
            
            // Validate parameter name reference
            uint32_t param_index = param_ref;
            if (param_index >= string_count || !string_table[param_index]) {
                printf("❌ Invalid parameter name reference: ref=%u, index=%u\n", param_ref, param_index);
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
            
            // Set parameter name
            comp_def->parameters[i] = kryon_strdup(string_table[param_index]);
            printf("🔍 Parameter name: '%s'\n", comp_def->parameters[i]);
            
            // Read default value if present
            if (has_default) {
                uint32_t default_ref;
                if (!read_uint32_safe(data, offset, size, &default_ref)) {
                    printf("❌ Failed to read default_ref for parameter %zu\n", i);
                    // Cleanup on error
                    for (size_t j = 0; j <= i; j++) {
                        kryon_free(comp_def->parameters[j]);
                        kryon_free(comp_def->param_defaults[j]);
                    }
                    kryon_free(comp_def->name);
                    kryon_free(comp_def->parameters);
                    kryon_free(comp_def->param_defaults);
                    kryon_free(comp_def);
                    return false;
                }
                
                uint32_t default_index = default_ref;
                comp_def->param_defaults[i] = (default_index < string_count && string_table[default_index]) ?
                                             kryon_strdup(string_table[default_index]) : NULL;
                printf("🔍 Parameter default: '%s'\n", comp_def->param_defaults[i] ? comp_def->param_defaults[i] : "(null)");
            } else {
                comp_def->param_defaults[i] = NULL;
                printf("🔍 Parameter default: (none)\n");
            }
        }
    }
    
    // Read state variable count
    printf("🔍 About to read state_count at offset %zu\n", *offset);
    uint16_t state_count;
    if (!read_uint16_safe(data, offset, size, &state_count)) {
        printf("❌ Failed to read state_count\n");
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
    printf("✅ Successfully read state_count: %u\n", state_count);

    // Allocate state variables array
    if (state_count > 0) {
        printf("🔍 Allocating %u state variables\n", state_count);
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

            printf("🔍 Reading state variable %zu at offset %zu\n", i, *offset);
            uint8_t var_flags;
            if (!read_uint32_safe(data, offset, size, &var_name_ref)) {
                printf("❌ Failed to read name_ref for state variable %zu\n", i);
                // ... cleanup ...
            }
            if (!read_uint8_safe(data, offset, size, &var_type)) {
                printf("❌ Failed to read var_type for state variable %zu\n", i);
                // ... cleanup ...
            }
            if (!read_uint8_safe(data, offset, size, &var_flags)) {
                printf("❌ Failed to read var_flags for state variable %zu\n", i);
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
            
            printf("🔍 State var %zu: name_ref=%u, type=%u\n", i, var_name_ref, var_type);

            if (var_name_ref >= string_count || !string_table[var_name_ref]) {
                printf("❌ Invalid string ref: var_name_ref=%u >= string_count=%u\n", var_name_ref, string_count);
                // Cleanup on error
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
            
            comp_def->state_vars[i].name = kryon_strdup(string_table[var_name_ref]);
            comp_def->state_vars[i].type = var_type;

            // Get base type (strip reactive/computed/static prefix)
            uint8_t base_type = var_type;
            if (var_type >= 0x10 && var_type <= 0x15) {  // REACTIVE types
                base_type = var_type - 0x10;
            } else if (var_type >= 0x20 && var_type <= 0x23) {  // COMPUTED types
                base_type = var_type - 0x20;
            }
            printf("🔍 State var %zu base_type=%u (from var_type=%u)\n", i, base_type, var_type);

            // Read value type tag (writer always writes this)
            uint8_t value_type_tag;
            if (!read_uint8_safe(data, offset, size, &value_type_tag)) {
                printf("❌ Failed to read value type tag for state variable %zu\n", i);
                // Cleanup...
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
            printf("🔍 Read value_type_tag=%u for state var %zu\n", value_type_tag, i);

            // Read default value based on value type tag
            if (value_type_tag == KRYON_VALUE_STRING) {
                uint32_t value_ref;
                printf("🔍 Reading string value_ref at offset %zu (size=%zu)\n", *offset, size);
                if (!read_uint32_safe(data, offset, size, &value_ref)) {
                    printf("❌ Failed to read string value_ref (offset=%zu, size=%zu)\n", *offset, size);
                    comp_def->state_vars[i].default_value = NULL;
                } else {
                    printf("✅ Read string value_ref=%u\n", value_ref);
                    comp_def->state_vars[i].default_value = (value_ref < string_count && string_table[value_ref]) ?
                                                      kryon_strdup(string_table[value_ref]) : NULL;
                }
            } else if (value_type_tag == KRYON_VALUE_INTEGER) {
                int64_t int_val;
                if (!read_int64_safe(data, offset, size, &int_val)) {
                    comp_def->state_vars[i].default_value = kryon_strdup("0");
                } else {
                    char int_str[32];
                    snprintf(int_str, sizeof(int_str), "%lld", (long long)int_val);
                    comp_def->state_vars[i].default_value = kryon_strdup(int_str);
                }
            } else if (value_type_tag == KRYON_VALUE_FLOAT) {
                double float_val;
                if (!read_double_safe(data, offset, size, &float_val)) {
                    comp_def->state_vars[i].default_value = kryon_strdup("0.0");
                } else {
                    char float_str[32];
                    snprintf(float_str, sizeof(float_str), "%g", float_val);
                    comp_def->state_vars[i].default_value = kryon_strdup(float_str);
                }
            } else if (value_type_tag == KRYON_VALUE_BOOLEAN) {
                uint8_t bool_val;
                printf("🔍 Reading boolean value at offset %zu\n", *offset);
                if (!read_uint8_safe(data, offset, size, &bool_val)) {
                    printf("❌ Failed to read boolean value\n");
                    comp_def->state_vars[i].default_value = kryon_strdup("false");
                } else {
                    printf("✅ Read boolean value: %u\n", bool_val);
                    comp_def->state_vars[i].default_value = kryon_strdup(bool_val ? "true" : "false");
                }
            } else {
                printf("⚠️ Unknown value_type_tag=%u, skipping value read\n", value_type_tag);
                comp_def->state_vars[i].default_value = kryon_strdup("");
            }
        }
    }
    
    // Read function count
    uint16_t function_count;
    if (!read_uint16_safe(data, offset, size, &function_count)) {
        printf("❌ Failed to read function count\n");
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
    
    printf("🔍 Function count: %u\n", function_count);
    comp_def->function_count = function_count;
    
    // For now, skip function loading - functions are loaded separately in the script section
    // TODO: Implement proper component function loading if needed
    for (size_t i = 0; i < function_count; i++) {
        // Skip function data for now
        uint32_t func_name_ref, func_lang_ref, func_code_ref;
        uint8_t has_default;
        
        if (!read_uint32_safe(data, offset, size, &func_name_ref) ||
            !read_uint32_safe(data, offset, size, &func_lang_ref) ||
            !read_uint8_safe(data, offset, size, &has_default)) {
            printf("❌ Failed to skip function %zu\n", i);
            break;
        }
        
        if (has_default) {
            if (!read_uint32_safe(data, offset, size, &func_code_ref)) {
                printf("❌ Failed to skip function code %zu\n", i);
                break;
            }
        }
    }
    
    // Check if component has body
    uint8_t has_body;
    printf("🔍 Reading has_body at offset %zu\n", *offset);
    if (!read_uint8_safe(data, offset, size, &has_body)) {
        printf("❌ Failed to read has_body flag\n");
        // Cleanup
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
    
    printf("🔍 Component has body: %s\n", has_body ? "yes" : "no");
    
    if (has_body) {
        // Load component body element as ui_template
        printf("🔄 Loading component body element as ui_template\n");

        // Component bodies use simplified format: type(uint16) + props + children
        // Use a starting ID for component body elements (e.g., 10000 to avoid conflicts)
        uint32_t next_id = 10000;

        KryonElement* ui_element = load_element_simple(runtime, data, offset, size, NULL,
                                                        string_table, string_count, &next_id);

        if (ui_element) {
            comp_def->ui_template = ui_element;
            printf("✅ Component ui_template loaded successfully: %s\n",
                   ui_element->type_name ? ui_element->type_name : "unknown");

            // Process @if directives in component ui_template
            printf("🔄 Processing @if directives in component ui_template (is_loading=%d)\n", runtime->is_loading);
            process_if_directives(runtime, ui_element);
            printf("🔄 Completed @if directive processing for component\n");
        } else {
            printf("❌ Failed to load component ui_template\n");
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


static void reprocess_property_bindings(KryonRuntime *runtime, KryonElement *element) {
    if (!runtime || !element) {
        return;
    }
    
    // Temporarily disabled to fix segfault - need to fix NULL pointer access
    return;
    
    // printf("🔍 DEBUG: reprocess_property_bindings called for element with %zu properties\n", element->property_count);
    
    // Process this element's properties for proper reactive binding
    for (size_t i = 0; i < element->property_count; i++) {
        KryonProperty *property = element->properties[i];
        // printf("🔍 DEBUG: Processing property %zu: %p\n", i, (void*)property);
        if (property && property->is_bound && property->binding_path) {
            const char *var_name = property->binding_path;
            // printf("🔍 DEBUG: Property is bound to variable: '%s'\n", var_name ? var_name : "(null)");
            
            // Look for the variable in runtime
            for (size_t j = 0; j < runtime->variable_count; j++) {
                if (runtime->variable_names[j] && var_name && strcmp(runtime->variable_names[j], var_name) == 0) {
                    // Found a variable match - update the property value based on type
                    // Skip REFERENCE properties - they should keep their variable names intact
                    if (property->type == KRYON_RUNTIME_PROP_REFERENCE) {
            printf("   REF property %s binding=%s\n", property->name ? property->name : "<noname>", property->binding_path ? property->binding_path : "(null)");

                        printf("🔄 DEBUG: Skipping REFERENCE property '%s' in reprocessing\n", 
                               property->name ? property->name : "(null)");
                        break; // Found match but skip processing for REFERENCE
                    } else if (property->type == KRYON_RUNTIME_PROP_STRING) {
                        kryon_free(property->value.string_value);
                        if (runtime->variable_values[j]) {
                            property->value.string_value = kryon_strdup(runtime->variable_values[j]);
                            printf("🔄 Resolved bound variable '%s' -> '%s' = '%s'\n", 
                                   property->name ? property->name : "(null)", var_name, property->value.string_value);
                        } else {
                            property->value.string_value = kryon_strdup("");
                            printf("🔄 Resolved bound variable '%s' -> '%s' = empty string\n", 
                                   property->name ? property->name : "(null)", var_name);
                        }
                    } else if (property->type == KRYON_RUNTIME_PROP_TEMPLATE) {
                        // Resolve template by concatenating segments and substituting variables
                        char *resolved_text = resolve_template_property(runtime, property);
                        if (resolved_text) {
                            // Convert template to resolved string
                            kryon_free_template_segments(property);
                            property->type = KRYON_RUNTIME_PROP_STRING;
                            property->value.string_value = resolved_text;
                            property->is_bound = false; // No longer bound after resolution
                            printf("🔄 Resolved template '%s' to '%s'\n", property->name ? property->name : "(null)", resolved_text);
                        }
                    }
                    // TODO: Add other property types (FLOAT, INTEGER, etc.) when needed
                    break;
                }
            }
        }
    }
    
    // Recursively process children
    for (size_t i = 0; i < element->child_count; i++) {
        if (element->children[i]) {
            reprocess_property_bindings(runtime, element->children[i]);
        }
    }
}

// =============================================================================
// TEMPLATE RESOLUTION
// =============================================================================

/**
 * @brief Resolve template property by substituting variables with their values
 * @param runtime Runtime context with variables
 * @param property Template property to resolve
 * @return Resolved string, or NULL on error (caller must free)
 */
static char *resolve_template_property(KryonRuntime *runtime, KryonProperty *property) {
    if (!runtime || !property || property->type != KRYON_RUNTIME_PROP_TEMPLATE) {
        return NULL;
    }
    
    // Calculate total length needed for resolved string
    size_t total_len = 0;
    for (size_t i = 0; i < property->value.template_value.segment_count; i++) {
        KryonTemplateSegment *segment = &property->value.template_value.segments[i];
        
        if (segment->type == KRYON_TEMPLATE_SEGMENT_LITERAL) {
            if (segment->data.literal_text) {
                total_len += strlen(segment->data.literal_text);
            }
        } else if (segment->type == KRYON_TEMPLATE_SEGMENT_VARIABLE) {
            // Look up variable value
            const char *var_name = segment->data.variable_name;
            const char *var_value = NULL;
            
            for (size_t j = 0; j < runtime->variable_count; j++) {
                if (runtime->variable_names[j] && var_name && 
                    strcmp(runtime->variable_names[j], var_name) == 0) {
                    var_value = runtime->variable_values[j];
                    break;
                }
            }
            
            if (var_value) {
                total_len += strlen(var_value);
            }
            // If variable not found, contribute 0 to length (empty substitution)
        }
    }
    
    // Allocate result string
    char *result = kryon_alloc(total_len + 1);
    if (!result) {
        return NULL;
    }
    
    // Build resolved string by concatenating segments
    result[0] = '\0';
    for (size_t i = 0; i < property->value.template_value.segment_count; i++) {
        KryonTemplateSegment *segment = &property->value.template_value.segments[i];
        
        if (segment->type == KRYON_TEMPLATE_SEGMENT_LITERAL) {
            if (segment->data.literal_text) {
                strcat(result, segment->data.literal_text);
            }
        } else if (segment->type == KRYON_TEMPLATE_SEGMENT_VARIABLE) {
            // Look up and substitute variable value
            const char *var_name = segment->data.variable_name;
            const char *var_value = NULL;
            
            for (size_t j = 0; j < runtime->variable_count; j++) {
                if (runtime->variable_names[j] && var_name && 
                    strcmp(runtime->variable_names[j], var_name) == 0) {
                    var_value = runtime->variable_values[j];
                    break;
                }
            }
            
            if (var_value) {
                strcat(result, var_value);
                printf("  🔗 Substituted variable '%s' = '%s'\n", var_name, var_value);
            } else {
                printf("  ⚠️  Variable '%s' not found, using empty substitution\n", var_name ? var_name : "(null)");
            }
        }
    }
    
    return result;
}

/**
 * @brief Free template segments memory
 * @param property Template property to free segments for
 */
static void kryon_free_template_segments(KryonProperty *property) {
    if (!property || property->type != KRYON_RUNTIME_PROP_TEMPLATE) {
        return;
    }
    
    for (size_t i = 0; i < property->value.template_value.segment_count; i++) {
        KryonTemplateSegment *segment = &property->value.template_value.segments[i];
        
        if (segment->type == KRYON_TEMPLATE_SEGMENT_LITERAL) {
            kryon_free(segment->data.literal_text);
        } else if (segment->type == KRYON_TEMPLATE_SEGMENT_VARIABLE) {
            kryon_free(segment->data.variable_name);
        }
    }
    
    kryon_free(property->value.template_value.segments);
    property->value.template_value.segments = NULL;
    property->value.template_value.segment_count = 0;
}
