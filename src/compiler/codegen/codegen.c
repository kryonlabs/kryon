/**
 * @file codegen.c
 * @brief Kryon Code Generator Implementation
 * 
 * 0BSD License
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted.
 * 
 * Generates optimized KRB binary files from parsed AST with proper
 * element/property hex mapping and cross-platform compatibility.
 */

#include "internal/codegen.h"
#include "internal/memory.h"
#include "internal/krb_format.h"
#include "internal/binary_io.h"
#include "internal/color_utils.h"    
#include "internal/script_vm.h"
#include "../../shared/kryon_mappings.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <assert.h>

// KRB compression types
#define KRYON_KRB_COMPRESSION_NONE 0

// =============================================================================
// FORWARD DECLARATIONS
// =============================================================================

static bool generate_binary_from_ast(KryonCodeGenerator *codegen, const KryonASTNode *node);
static bool write_element_node(KryonCodeGenerator *codegen, const KryonASTNode *element);
static bool write_property_node(KryonCodeGenerator *codegen, const KryonASTNode *property);
static bool write_style_node(KryonCodeGenerator *codegen, const KryonASTNode *style);
static bool write_theme_node(KryonCodeGenerator *codegen, const KryonASTNode *theme);
static bool write_variable_node(KryonCodeGenerator *codegen, const KryonASTNode *variable);
static bool write_single_variable(KryonCodeGenerator *codegen, const KryonASTNode *variable);
static bool write_function_node(KryonCodeGenerator *codegen, const KryonASTNode *function);
static bool write_component_node(KryonCodeGenerator *codegen, const KryonASTNode *component);
static bool write_metadata_node(KryonCodeGenerator *codegen, const KryonASTNode *metadata);
static bool write_value_node(KryonCodeGenerator *codegen, const KryonASTValue *value);
static bool write_property_value_node(KryonCodeGenerator *codegen, const KryonASTValue *value, uint16_t property_hex);
static bool write_complex_krb_format(KryonCodeGenerator *codegen, const KryonASTNode *ast_root);
static bool write_style_definition(KryonCodeGenerator *codegen, const KryonASTNode *style);
static bool write_widget_instance(KryonCodeGenerator *codegen, const KryonASTNode *element);

static uint32_t add_string_to_table(KryonCodeGenerator *codegen, const char *str);
static bool expand_binary_buffer(KryonCodeGenerator *codegen, size_t additional_size);
static uint32_t count_elements_recursive(const KryonASTNode *node);
static uint32_t count_properties_recursive(const KryonASTNode *node);
static uint16_t get_or_create_custom_element_hex(const char *element_name);

static bool write_uint8(KryonCodeGenerator *codegen, uint8_t value);
static bool write_uint16(KryonCodeGenerator *codegen, uint16_t value);
static bool write_uint32(KryonCodeGenerator *codegen, uint32_t value);
static bool write_binary_data(KryonCodeGenerator *codegen, const void *data, size_t size);
static bool update_header_uint32(KryonCodeGenerator *codegen, size_t offset, uint32_t value);

static void codegen_error(KryonCodeGenerator *codegen, const char *message);

// =============================================================================
// ELEMENT AND PROPERTY MAPPINGS
// =============================================================================

// Using centralized element mappings

// Custom element registry for dynamic element types
static struct {
    char *name;
    uint16_t hex;
} custom_element_registry[256]; // Support up to 256 custom elements
static size_t custom_element_count = 0;
static uint16_t next_custom_element_hex = 0x1000; // Start custom elements at 0x1000

// Using centralized property mappings

// =============================================================================
// CONFIGURATION
// =============================================================================

KryonCodeGenConfig kryon_codegen_default_config(void) {
    KryonCodeGenConfig config = {
        .optimization = KRYON_OPTIMIZE_BALANCED,
        .enable_compression = true,
        .preserve_debug_info = false,
        .validate_output = true,
        .inline_styles = true,
        .deduplicate_strings = true,
        .remove_unused_styles = true,
        .target_version = 0x00010000  // Version 1.0.0
    };
    return config;
}

KryonCodeGenConfig kryon_codegen_size_config(void) {
    KryonCodeGenConfig config = kryon_codegen_default_config();
    config.optimization = KRYON_OPTIMIZE_SIZE;
    config.enable_compression = true;
    config.inline_styles = true;
    config.preserve_debug_info = false;
    return config;
}

KryonCodeGenConfig kryon_codegen_speed_config(void) {
    KryonCodeGenConfig config = kryon_codegen_default_config();
    config.optimization = KRYON_OPTIMIZE_SPEED;
    config.enable_compression = false;
    config.inline_styles = false;
    config.preserve_debug_info = false;
    return config;
}

KryonCodeGenConfig kryon_codegen_debug_config(void) {
    KryonCodeGenConfig config = kryon_codegen_default_config();
    config.optimization = KRYON_OPTIMIZE_NONE;
    config.enable_compression = false;
    config.preserve_debug_info = true;
    config.validate_output = true;
    return config;
}

// =============================================================================
// CODE GENERATOR LIFECYCLE
// =============================================================================

KryonCodeGenerator *kryon_codegen_create(const KryonCodeGenConfig *config) {
    KryonCodeGenerator *codegen = malloc(sizeof(KryonCodeGenerator));
    if (!codegen) {
        return NULL;
    }
    
    // Initialize structure
    memset(codegen, 0, sizeof(KryonCodeGenerator));
    
    // Set configuration
    if (config) {
        codegen->config = *config;
    } else {
        codegen->config = kryon_codegen_default_config();
    }
    
    // Initialize binary buffer
    codegen->binary_capacity = 4096; // Start with 4KB
    codegen->binary_data = malloc(codegen->binary_capacity);
    if (!codegen->binary_data) {
        free(codegen);
        return NULL;
    }
    
    // Initialize string table
    codegen->string_capacity = 64;
    codegen->string_table = malloc(codegen->string_capacity * sizeof(char*));
    if (!codegen->string_table) {
        free(codegen->binary_data);
        free(codegen);
        return NULL;
    }
    
    // Initialize other fields
    codegen->next_element_id = 1;
    
    return codegen;
}

void kryon_codegen_destroy(KryonCodeGenerator *codegen) {
    if (!codegen) {
        return;
    }
    
    // Free binary data
    free(codegen->binary_data);
    
    // Free string table
    if (codegen->string_table) {
        for (size_t i = 0; i < codegen->string_count; i++) {
            free(codegen->string_table[i]);
        }
        free(codegen->string_table);
    }
    
    // Free error messages
    if (codegen->error_messages) {
        for (size_t i = 0; i < codegen->error_count; i++) {
            free(codegen->error_messages[i]);
        }
        free(codegen->error_messages);
    }
    
    // Free mappings
    free(codegen->element_map);
    free(codegen->property_map);
    
    free(codegen);
}

// =============================================================================
// MAIN GENERATION FUNCTIONS
// =============================================================================

bool kryon_codegen_generate(KryonCodeGenerator *codegen, const KryonASTNode *ast_root) {
    if (!codegen || !ast_root) {
        return false;
    }
    
    clock_t start_time = clock();
    
    // Reset generator state
    codegen->ast_root = ast_root;
    codegen->binary_size = 0;
    codegen->current_offset = 0;
    codegen->has_errors = false;
    codegen->string_count = 0;
    codegen->next_element_id = 1;
    
    // Write complex KRB format (v0.1 spec)
    if (!write_complex_krb_format(codegen, ast_root)) {
        codegen_error(codegen, "Failed to write complex KRB format");
        return false;
    }
    
    // Validate output if requested
    if (codegen->config.validate_output) {
        if (!kryon_codegen_validate_binary(codegen)) {
            codegen_error(codegen, "Binary validation failed");
            return false;
        }
    }
    
    // Calculate statistics
    clock_t end_time = clock();
    codegen->stats.generation_time = ((double)(end_time - start_time)) / CLOCKS_PER_SEC;
    codegen->stats.output_binary_size = codegen->binary_size;
    
    return !codegen->has_errors;
}

bool kryon_write_header(KryonCodeGenerator *codegen) {
    // KRB magic number "KRYN"
    if (!write_uint32(codegen, 0x4B52594E)) {
        return false;
    }
    
    // Version
    if (!write_uint32(codegen, codegen->config.target_version)) {
        return false;
    }
    
    // Flags
    uint32_t flags = 0;
    if (codegen->config.enable_compression) flags |= 0x01;
    if (codegen->config.preserve_debug_info) flags |= 0x02;
    if (!write_uint32(codegen, flags)) {
        return false;
    }
    
    // Reserve space for section offsets (will be filled later)
    // String table offset
    if (!write_uint32(codegen, 0)) return false;
    // Metadata offset  
    if (!write_uint32(codegen, 0)) return false;
    // Elements offset
    if (!write_uint32(codegen, 0)) return false;
    
    return true;
}

bool kryon_write_string_table(KryonCodeGenerator *codegen) {
    if (!codegen) return false;
    
    // Write actual string count
    if (!write_uint32(codegen, (uint32_t)codegen->string_count)) {
        return false;
    }
    
    // Write each string
    for (size_t i = 0; i < codegen->string_count; i++) {
        const char *str = codegen->string_table[i];
        if (!str) {
            codegen_error(codegen, "NULL string in string table");
            return false;
        }
        
        // Write string length
        uint16_t len = (uint16_t)strlen(str);
        if (!write_uint16(codegen, len)) {
            return false;
        }
        
        // Write string data
        if (!write_binary_data(codegen, str, len)) {
            return false;
        }
    }
    
    return true;
}

bool kryon_write_metadata(KryonCodeGenerator *codegen, const KryonASTNode *ast_root) {
    // Write metadata section header
    if (!write_uint32(codegen, 0x4D455441)) { // "META"
        return false;
    }
    
    // Metadata size (placeholder, filled later)
    size_t metadata_size_offset = codegen->current_offset;
    if (!write_uint32(codegen, 0)) {
        return false;
    }
    
    // Write basic metadata
    if (!write_uint32(codegen, codegen->stats.input_elements)) {
        return false;
    }
    
    if (!write_uint32(codegen, codegen->stats.input_properties)) {
        return false;
    }
    
    // Update metadata size
    size_t metadata_end = codegen->current_offset;
    uint32_t metadata_size = (uint32_t)(metadata_end - metadata_size_offset - 4);
    if (!update_header_uint32(codegen, metadata_size_offset, metadata_size)) {
        codegen_error(codegen, "Failed to write metadata size to header");
        return false;
    }
    
    return true;
}

bool kryon_write_elements(KryonCodeGenerator *codegen, const KryonASTNode *ast_root) {
    if (!ast_root) {
        return false;
    }
    
    // Write elements section header
    if (!write_uint32(codegen, 0x454C454D)) { // "ELEM"
        return false;
    }
    
    // Element count (placeholder)
    size_t element_count_offset = codegen->current_offset;
    if (!write_uint32(codegen, 0)) {
        return false;
    }
    
    uint32_t element_count = 0;
    
    // Process root children
    if (ast_root->type == KRYON_AST_ROOT) {
        for (size_t i = 0; i < ast_root->data.element.child_count; i++) {
            const KryonASTNode *child = ast_root->data.element.children[i];
            if (child && (child->type == KRYON_AST_ELEMENT || child->type == KRYON_AST_EVENT_DIRECTIVE)) {
                if (write_element_node(codegen, child)) {
                    element_count++;
                    codegen->stats.output_elements++;
                } else {
                    return false;
                }
            } else if (child && child->type == KRYON_AST_STYLE_BLOCK) {
                if (!write_style_node(codegen, child)) {
                    return false;
                }
            }
        }
    }
    
    // Update element count
    if (!update_header_uint32(codegen, element_count_offset, element_count)) {
        codegen_error(codegen, "Failed to write element count to header");
        return false;
    }
    
    return true;
}

static bool write_element_node(KryonCodeGenerator *codegen, const KryonASTNode *element) {
    if (!element || (element->type != KRYON_AST_ELEMENT && element->type != KRYON_AST_EVENT_DIRECTIVE)) {
        return false;
    }
    
    // Get element type hex code
    uint16_t element_hex;
    if (element->type == KRYON_AST_EVENT_DIRECTIVE) {
        element_hex = KRYON_ELEMENT_EVENT_DIRECTIVE;
    } else {
        element_hex = kryon_codegen_get_element_hex(element->data.element.element_type);
        if (element_hex == 0) {
            codegen_error(codegen, "Unknown element type");
            return false;
        }
    }
    
    // Write element header
    if (!write_uint16(codegen, element_hex)) {
        return false;
    }
    
    // Element ID
    if (!write_uint32(codegen, codegen->next_element_id++)) {
        return false;
    }
    
    // Property count
    if (!write_uint32(codegen, (uint32_t)element->data.element.property_count)) {
        return false;
    }
    
    // Write properties
    for (size_t i = 0; i < element->data.element.property_count; i++) {
        if (!element->data.element.properties[i]) {
            codegen_error(codegen, "Property array contains NULL pointers - parser memory allocation issue");
            return false;
        }
        if (!write_property_node(codegen, element->data.element.properties[i])) {
            return false;
        }
    }
    
    // Child count
    if (!write_uint32(codegen, (uint32_t)element->data.element.child_count)) {
        return false;
    }
    
    // Write child elements recursively
    for (size_t i = 0; i < element->data.element.child_count; i++) {
        if (!write_element_node(codegen, element->data.element.children[i])) {
            return false;
        }
    }
    
    return true;
}


/**
 * @brief Retrieves the data type hint for a property from the centralized mappings.
 * @param property_hex The hex code of the property.
 * @return The KryonValueTypeHint for the property, or KRYON_TYPE_HINT_ANY if not found.
 */
static KryonValueTypeHint get_property_type_hint(uint16_t property_hex) {
    return kryon_get_property_type_hint(property_hex);
}



static bool write_property_node(KryonCodeGenerator *codegen, const KryonASTNode *property) {
    if (!property || property->type != KRYON_AST_PROPERTY) {
        return false;
    }
    
    // Debug: Show which property we're processing
    if (property->data.property.name) {
        printf("🔧 Processing property: '%s'\n", property->data.property.name);
    }
    
    // Get property hex code
    uint16_t property_hex = kryon_codegen_get_property_hex(property->data.property.name);
    if (property_hex == 0) {
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), "Unknown property type: '%s'", 
                property->data.property.name ? property->data.property.name : "NULL");
        codegen_error(codegen, error_msg);
        return false;
    }
    
    // Write property header
    if (!write_uint16(codegen, property_hex)) {
        return false;
    }
    
    // Write property value with type-aware conversion
    if (property->data.property.value) {
        if (property->data.property.value->type == KRYON_AST_LITERAL) {
            return write_property_value_node(codegen, &property->data.property.value->data.literal.value, property_hex);
        } else if (property->data.property.value->type == KRYON_AST_VARIABLE) {
            // Handle variable references as string values
            KryonASTValue var_value;
            var_value.type = KRYON_VALUE_STRING;
            // Create a variable reference string like "$counterValue"  
            static char var_ref[256];
            snprintf(var_ref, sizeof(var_ref), "$%s", property->data.property.value->data.variable.name);
            var_value.data.string_value = var_ref;
            return write_property_value_node(codegen, &var_value, property_hex);
        } else {
            // For now, only handle literals and variables
            codegen_error(codegen, "Non-literal/non-variable property values not yet supported");
            return false;
        }
    }
    
    return true;
}

static bool write_style_node(KryonCodeGenerator *codegen, const KryonASTNode *style) {
    if (!style || style->type != KRYON_AST_STYLE_BLOCK) {
        return false;
    }
    
    // Write style header
    if (!write_uint32(codegen, 0x5354594C)) { // "STYL"
        return false;
    }
    
    // Style name (as string reference)
    uint32_t name_ref = add_string_to_table(codegen, style->data.style.name);
    if (!write_uint32(codegen, name_ref)) {
        return false;
    }
    
    // Property count
    if (!write_uint32(codegen, (uint32_t)style->data.style.property_count)) {
        return false;
    }
    
    // Write style properties
    for (size_t i = 0; i < style->data.style.property_count; i++) {
        if (!write_property_node(codegen, style->data.style.properties[i])) {
            return false;
        }
    }
    
    return true;
}

static bool write_theme_node(KryonCodeGenerator *codegen, const KryonASTNode *theme) {
    if (!theme || theme->type != KRYON_AST_THEME_DEFINITION) {
        return false;
    }
    
    // Write theme header
    if (!write_uint32(codegen, 0x54484D45)) { // "THME"
        return false;
    }
    
    // Theme group name (as string reference)
    uint32_t name_ref = add_string_to_table(codegen, theme->data.theme.group_name);
    if (!write_uint32(codegen, name_ref)) {
        return false;
    }
    
    // Variable count
    if (!write_uint32(codegen, (uint32_t)theme->data.theme.variable_count)) {
        return false;
    }
    
    // Write variables (as properties)
    for (size_t i = 0; i < theme->data.theme.variable_count; i++) {
        if (!write_property_node(codegen, theme->data.theme.variables[i])) {
            return false;
        }
    }
    
    return true;
}

static bool write_variable_node(KryonCodeGenerator *codegen, const KryonASTNode *variable) {
    if (!variable || variable->type != KRYON_AST_VARIABLE_DEFINITION) {
        return false;
    }
    
    // Check if this is a variables block (contains children) or single variable
    if (variable->data.variable_def.name && 
        strcmp(variable->data.variable_def.name, "__variables_block__") == 0) {
        // Handle @variables { ... } block - write all child variables
        for (size_t i = 0; i < variable->data.element.child_count; i++) {
            const KryonASTNode *child_var = variable->data.element.children[i];
            if (child_var && child_var->type == KRYON_AST_VARIABLE_DEFINITION) {
                if (!write_single_variable(codegen, child_var)) {
                    return false;
                }
            }
        }
        return true;
    } else {
        // Handle single @var
        return write_single_variable(codegen, variable);
    }
}

static bool write_single_variable(KryonCodeGenerator *codegen, const KryonASTNode *variable) {
    if (!variable || variable->type != KRYON_AST_VARIABLE_DEFINITION) {
        return false;
    }
    
    // Variable name (as string reference)
    uint32_t name_ref = add_string_to_table(codegen, variable->data.variable_def.name);
    if (!write_uint32(codegen, name_ref)) {
        return false;
    }
    
    // Variable type (for now, infer from value or default to string)
    uint8_t var_type = 0; // String by default
    if (variable->data.variable_def.value) {
        switch (variable->data.variable_def.value->type) {
            case KRYON_AST_LITERAL:
                switch (variable->data.variable_def.value->data.literal.value.type) {
                    case KRYON_VALUE_INTEGER: var_type = 1; break;
                    case KRYON_VALUE_FLOAT: var_type = 2; break;
                    case KRYON_VALUE_BOOLEAN: var_type = 3; break;
                    default: var_type = 0; break; // String
                }
                break;
            default: var_type = 0; break; // String
        }
    }
    if (!write_uint8(codegen, var_type)) {
        return false;
    }
    
    // Variable value
    if (variable->data.variable_def.value && variable->data.variable_def.value->type == KRYON_AST_LITERAL) {
        return write_value_node(codegen, &variable->data.variable_def.value->data.literal.value);
    } else {
        // Write null value
        if (!write_uint8(codegen, KRYON_VALUE_NULL)) {
            return false;
        }
        return write_uint32(codegen, 0); // No data
    }
}

static bool write_function_node(KryonCodeGenerator *codegen, const KryonASTNode *function) {
    if (!function || function->type != KRYON_AST_FUNCTION_DEFINITION) {
        return false;
    }
    
    // Write function header
    if (!write_uint32(codegen, 0x46554E43)) { // "FUNC"
        return false;
    }
    
    // Function language (as string reference)
    uint32_t lang_ref = add_string_to_table(codegen, function->data.function_def.language);
    if (!write_uint32(codegen, lang_ref)) {
        return false;
    }
    
    // Function name (as string reference)
    uint32_t name_ref = add_string_to_table(codegen, function->data.function_def.name);
    if (!write_uint32(codegen, name_ref)) {
        return false;
    }
    
    // Parameter count
    if (!write_uint32(codegen, (uint32_t)function->data.function_def.parameter_count)) {
        return false;
    }
    
    // Write parameters
    for (size_t i = 0; i < function->data.function_def.parameter_count; i++) {
        uint32_t param_ref = add_string_to_table(codegen, function->data.function_def.parameters[i]);
        if (!write_uint32(codegen, param_ref)) {
            return false;
        }
    }
    
    // Function code - use pre-compiled bytecode from string table if Lua
    char *bytecode_to_store = NULL;
    if (function->data.function_def.language && 
        strcmp(function->data.function_def.language, "lua") == 0 &&
        function->data.function_def.code) {
        
        printf("🔍 DEBUG: Finding pre-compiled Lua bytecode for '%s'\n", function->data.function_def.name);
        
        // Re-compile to get the same bytecode string (this should be optimized later)
        KryonVM* temp_vm = kryon_vm_create(KRYON_VM_LUA, NULL);
        if (temp_vm) {
            char* wrapped_code = kryon_alloc(strlen(function->data.function_def.code) + 
                                           strlen(function->data.function_def.name) + 50);
            if (wrapped_code) {
                snprintf(wrapped_code, strlen(function->data.function_def.code) + 
                        strlen(function->data.function_def.name) + 50,
                        "function %s()\n%s\nend", 
                        function->data.function_def.name, 
                        function->data.function_def.code);
                
                KryonScript script = {0};
                KryonVMResult result = kryon_vm_compile(temp_vm, wrapped_code, 
                                                       function->data.function_def.name, &script);
                
                if (result == KRYON_VM_SUCCESS && script.data && script.size > 0) {
                    char* bytecode_str = kryon_alloc(script.size * 2 + 1);
                    if (bytecode_str) {
                        for (size_t i = 0; i < script.size; i++) {
                            snprintf(bytecode_str + i * 2, 3, "%02x", ((uint8_t*)script.data)[i]);
                        }
                        bytecode_to_store = bytecode_str;
                        printf("✅ Using pre-compiled Lua bytecode (%zu bytes)\n", script.size);
                    } else {
                        bytecode_to_store = function->data.function_def.code;
                    }
                    if (script.data) kryon_free(script.data);
                } else {
                    bytecode_to_store = function->data.function_def.code;
                }
                kryon_free(wrapped_code);
            } else {
                bytecode_to_store = function->data.function_def.code;
            }
            kryon_vm_destroy(temp_vm);
        } else {
            bytecode_to_store = function->data.function_def.code;
        }
    } else {
        bytecode_to_store = function->data.function_def.code;
    }
    
    printf("🔍 DEBUG: About to add bytecode to string table: '%s' (first 50 chars)\n", 
           bytecode_to_store ? (strlen(bytecode_to_store) > 50 ? 
               strndup(bytecode_to_store, 50) : bytecode_to_store) : "NULL");
    
    uint32_t code_ref = add_string_to_table(codegen, bytecode_to_store);
    printf("🔍 DEBUG: String table reference for bytecode: %u\n", code_ref);
    if (!write_uint32(codegen, code_ref)) {
        return false;
    }
    
    // Clean up bytecode string if it was allocated for hex encoding
    if (bytecode_to_store != function->data.function_def.code) {
        kryon_free((void*)bytecode_to_store);
    }
    
    return true;
}

static bool write_component_node(KryonCodeGenerator *codegen, const KryonASTNode *component) {
    if (!component || component->type != KRYON_AST_COMPONENT) {
        return false;
    }
    
    // Write component header
    if (!write_uint32(codegen, 0x434F4D50)) { // "COMP"
        return false;
    }
    
    // Component name (as string reference)
    uint32_t name_ref = add_string_to_table(codegen, component->data.component.name);
    if (!write_uint32(codegen, name_ref)) {
        return false;
    }
    
    // Parameter count
    if (!write_uint32(codegen, (uint32_t)component->data.component.parameter_count)) {
        return false;
    }
    
    // Write parameters and their defaults
    for (size_t i = 0; i < component->data.component.parameter_count; i++) {
        uint32_t param_ref = add_string_to_table(codegen, component->data.component.parameters[i]);
        if (!write_uint32(codegen, param_ref)) {
            return false;
        }
        
        // Write default value (0 if no default)
        uint32_t default_ref = 0;
        if (component->data.component.param_defaults[i]) {
            default_ref = add_string_to_table(codegen, component->data.component.param_defaults[i]);
        }
        if (!write_uint32(codegen, default_ref)) {
            return false;
        }
    }
    
    // State variable count
    if (!write_uint32(codegen, (uint32_t)component->data.component.state_count)) {
        return false;
    }
    
    // Write state variables
    for (size_t i = 0; i < component->data.component.state_count; i++) {
        if (!write_variable_node(codegen, component->data.component.state_vars[i])) {
            return false;
        }
    }
    
    // Function count
    if (!write_uint32(codegen, (uint32_t)component->data.component.function_count)) {
        return false;
    }
    
    // Write component functions
    for (size_t i = 0; i < component->data.component.function_count; i++) {
        if (!write_function_node(codegen, component->data.component.functions[i])) {
            return false;
        }
    }
    
    // Write component body (UI tree)
    if (component->data.component.body) {
        if (!write_uint8(codegen, 1)) { // Has body
            return false;
        }
        if (!write_element_node(codegen, component->data.component.body)) {
            return false;
        }
    } else {
        if (!write_uint8(codegen, 0)) { // No body
            return false;
        }
    }
    
    return true;
}

static bool write_metadata_node(KryonCodeGenerator *codegen, const KryonASTNode *metadata) {
    if (!metadata || metadata->type != KRYON_AST_METADATA_DIRECTIVE) {
        return false;
    }
    
    // Write metadata header
    if (!write_uint32(codegen, 0x4D455441)) { // "META"
        return false;
    }
    
    // Property count
    if (!write_uint32(codegen, (uint32_t)metadata->data.element.property_count)) {
        return false;
    }
    
    // Write properties
    for (size_t i = 0; i < metadata->data.element.property_count; i++) {
        if (!write_property_node(codegen, metadata->data.element.properties[i])) {
            return false;
        }
    }
    
    return true;
}

static bool write_value_node(KryonCodeGenerator *codegen, const KryonASTValue *value) {
    if (!value) {
        return false;
    }
    
    // Don't write type tags - values are interpreted based on property type hints
    switch (value->type) {
        case KRYON_VALUE_STRING: {
            // For simple format, write string inline: length + data
            const char *str = value->data.string_value ? value->data.string_value : "";
            uint16_t len = (uint16_t)strlen(str);
            if (!write_uint16(codegen, len)) {
                return false;
            }
            return write_binary_data(codegen, str, len);
        }
        
        case KRYON_VALUE_INTEGER:
            return write_uint32(codegen, (uint32_t)value->data.int_value);
        
        case KRYON_VALUE_FLOAT: {
            // Convert double to 32-bit float for binary
            float f = (float)value->data.float_value;
            // Write as uint32 with endian conversion (reinterpret float bits as uint32)
            uint32_t float_bits;
            memcpy(&float_bits, &f, sizeof(float));
            return write_uint32(codegen, float_bits);
        }
        
        case KRYON_VALUE_BOOLEAN:
            return write_uint8(codegen, value->data.bool_value ? 1 : 0);
        
        case KRYON_VALUE_NULL:
            // No additional data for null
            return true;
        
        case KRYON_VALUE_COLOR:
            return write_uint32(codegen, value->data.color_value);
        
        case KRYON_VALUE_UNIT:
            // Write value and unit string
            if (!write_binary_data(codegen, &value->data.unit_value.value, sizeof(double))) {
                return false;
            }
            uint32_t unit_ref = add_string_to_table(codegen, value->data.unit_value.unit);
            return write_uint32(codegen, unit_ref);
        
        default:
            codegen_error(codegen, "Unsupported value type");
            return false;
    }
}

// =============================================================================
// UTILITY FUNCTIONS
// =============================================================================

uint16_t kryon_codegen_get_element_hex(const char *element_name) {
    if (!element_name) {
        return 0;
    }
    
    // Use centralized element mappings
    uint16_t hex = kryon_get_widget_hex(element_name);
    if (hex != 0) {
        return hex;
    }
    
    // If not found in built-ins, check/create custom element
    return get_or_create_custom_element_hex(element_name);
}

uint16_t kryon_codegen_get_property_hex(const char *property_name) {
    if (!property_name) {
        return 0;
    }
    
    // Use centralized property mappings
    return kryon_get_property_hex(property_name);
    
    return 0; // Unknown property
}

static uint16_t get_or_create_custom_element_hex(const char *element_name) {
    if (!element_name) {
        return 0;
    }
    
    // Check if this custom element already exists
    for (size_t i = 0; i < custom_element_count; i++) {
        if (custom_element_registry[i].name && 
            strcmp(custom_element_registry[i].name, element_name) == 0) {
            return custom_element_registry[i].hex;
        }
    }
    
    // Create new custom element entry
    if (custom_element_count >= 256) {
        // Registry full - this is a reasonable limit for custom elements
        return 0;
    }
    
    // Allocate memory for the element name
    custom_element_registry[custom_element_count].name = malloc(strlen(element_name) + 1);
    if (!custom_element_registry[custom_element_count].name) {
        return 0;
    }
    
    strcpy(custom_element_registry[custom_element_count].name, element_name);
    custom_element_registry[custom_element_count].hex = next_custom_element_hex++;
    
    printf("🔧 Registered custom element: %s -> 0x%04X\n", 
           element_name, custom_element_registry[custom_element_count].hex);
    
    return custom_element_registry[custom_element_count++].hex;
}

static uint32_t add_string_to_table(KryonCodeGenerator *codegen, const char *str) {
    if (!str) {
        return 0;
    }
    
    // Check if string already exists
    for (size_t i = 0; i < codegen->string_count; i++) {
        if (strcmp(codegen->string_table[i], str) == 0) {
            return (uint32_t)i + 1; // 1-based index
        }
    }
    
    // Add new string
    if (codegen->string_count >= codegen->string_capacity) {
        size_t new_capacity = codegen->string_capacity * 2;
        char **new_table = realloc(codegen->string_table, 
                                        new_capacity * sizeof(char*));
        if (!new_table) {
            return 0;
        }
        codegen->string_table = new_table;
        codegen->string_capacity = new_capacity;
    }
    
    codegen->string_table[codegen->string_count] = strdup(str);
    if (!codegen->string_table[codegen->string_count]) {
        return 0;
    }
    
    return (uint32_t)(++codegen->string_count); // 1-based index
}

static bool expand_binary_buffer(KryonCodeGenerator *codegen, size_t additional_size) {
    size_t required_size = codegen->binary_size + additional_size;
    
    if (required_size <= codegen->binary_capacity) {
        return true; // No expansion needed
    }
    
    // Double capacity until we have enough space
    size_t new_capacity = codegen->binary_capacity;
    while (new_capacity < required_size) {
        new_capacity *= 2;
    }
    
    uint8_t *new_buffer = realloc(codegen->binary_data, new_capacity);
    if (!new_buffer) {
        codegen_error(codegen, "Failed to expand binary buffer");
        return false;
    }
    
    codegen->binary_data = new_buffer;
    codegen->binary_capacity = new_capacity;
    
    return true;
}

// Helper functions using centralized binary_io
static bool write_uint8(KryonCodeGenerator *codegen, uint8_t value) {
    if (!expand_binary_buffer(codegen, sizeof(uint8_t))) return false;
    size_t offset = codegen->binary_size;
    if (!kryon_write_uint8(&codegen->binary_data, &offset, codegen->binary_capacity, value)) return false;
    codegen->binary_size = offset;
    codegen->current_offset = offset;
    return true;
}

static bool write_uint16(KryonCodeGenerator *codegen, uint16_t value) {
    if (!expand_binary_buffer(codegen, sizeof(uint16_t))) return false;
    size_t offset = codegen->binary_size;
    if (!kryon_write_uint16(&codegen->binary_data, &offset, codegen->binary_capacity, value)) return false;
    codegen->binary_size = offset;
    codegen->current_offset = offset;
    return true;
}

static bool write_uint32(KryonCodeGenerator *codegen, uint32_t value) {
    if (!expand_binary_buffer(codegen, sizeof(uint32_t))) return false;
    size_t offset = codegen->binary_size;
    if (!kryon_write_uint32(&codegen->binary_data, &offset, codegen->binary_capacity, value)) return false;
    codegen->binary_size = offset;
    codegen->current_offset = offset;
    return true;
}

static bool write_binary_data(KryonCodeGenerator *codegen, const void *data, size_t size) {
    if (!expand_binary_buffer(codegen, size)) return false;
    size_t offset = codegen->binary_size;
    if (!kryon_write_bytes(&codegen->binary_data, &offset, codegen->binary_capacity, data, size)) return false;
    codegen->binary_size = offset;
    codegen->current_offset = offset;
    return true;
}

static bool update_header_uint32(KryonCodeGenerator *codegen, size_t offset, uint32_t value) {
    if (offset + sizeof(uint32_t) > codegen->binary_capacity) {
        return false;
    }
    uint8_t *buffer = codegen->binary_data;
    return kryon_write_uint32(&buffer, &offset, codegen->binary_capacity, value);
}

static void codegen_error(KryonCodeGenerator *codegen, const char *message) {
    if (!codegen || !message) {
        return;
    }
    
    codegen->has_errors = true;
    
    // Expand error array if needed
    if (codegen->error_count >= codegen->error_capacity) {
        size_t new_capacity = codegen->error_capacity ? codegen->error_capacity * 2 : 8;
        char **new_messages = realloc(codegen->error_messages,
                                           new_capacity * sizeof(char*));
        if (!new_messages) {
            return;
        }
        codegen->error_messages = new_messages;
        codegen->error_capacity = new_capacity;
    }
    
    codegen->error_messages[codegen->error_count++] = strdup(message);
}


// =============================================================================
// PUBLIC API IMPLEMENTATIONS
// =============================================================================

const uint8_t *kryon_codegen_get_binary(const KryonCodeGenerator *codegen, size_t *out_size) {
    if (!codegen) {
        if (out_size) *out_size = 0;
        return NULL;
    }
    
    if (out_size) {
        *out_size = codegen->binary_size;
    }
    
    return codegen->binary_data;
}

bool kryon_write_file(const KryonCodeGenerator *codegen, const char *filename) {
    if (!codegen || !filename || codegen->binary_size == 0) {
        return false;
    }
    
    FILE *file = fopen(filename, "wb");
    if (!file) {
        return false;
    }
    
    size_t written = fwrite(codegen->binary_data, 1, codegen->binary_size, file);
    fclose(file);
    
    return written == codegen->binary_size;
}

const char **kryon_codegen_get_errors(const KryonCodeGenerator *codegen, size_t *out_count) {
    if (!codegen || !out_count) {
        if (out_count) *out_count = 0;
        return NULL;
    }
    
    *out_count = codegen->error_count;
    return (const char**)codegen->error_messages;
}

const KryonCodeGenStats *kryon_codegen_get_stats(const KryonCodeGenerator *codegen) {
    return codegen ? &codegen->stats : NULL;
}

static bool write_complex_krb_format(KryonCodeGenerator *codegen, const KryonASTNode *ast_root) {
    if (!ast_root) {
        return false;
    }
    
    // First, count sections for header values
    uint32_t style_count = 0;
    uint32_t theme_count = 0; // For future theme support
    uint32_t widget_def_count = 0; // For future widget definitions
    uint32_t element_count = 0;
    uint32_t total_properties = 0;
    
    if (ast_root->type == KRYON_AST_ROOT) {
        for (size_t i = 0; i < ast_root->data.element.child_count; i++) {
            const KryonASTNode *child = ast_root->data.element.children[i];
            if (child) {
                switch (child->type) {
                    case KRYON_AST_STYLE_BLOCK:
                        style_count++;
                        break;
                    case KRYON_AST_ELEMENT:
                        element_count++;
                        total_properties += child->data.element.property_count;
                        break;
                    case KRYON_AST_FUNCTION_DEFINITION:
                        // Function definitions will be written to script section
                        break;
                    case KRYON_AST_VARIABLE_DEFINITION:
                        // Variable definitions will be handled
                        break;
                    default:
                        break;
                }
            }
        }
    } else if (ast_root->type == KRYON_AST_ELEMENT) {
        // Handle case where AST root is directly an element
        element_count = count_elements_recursive(ast_root);
        total_properties = count_properties_recursive(ast_root);
    }
    
    // Write complete 128-byte header as per KRB v0.1 spec
    
    // 0x00-0x03: Magic number "KRYN"
    if (!write_uint32(codegen, 0x4B52594E)) return false;
    
    // 0x04-0x05: Version Major
    if (!write_uint16(codegen, 0)) return false;
    
    // 0x06-0x07: Version Minor  
    if (!write_uint16(codegen, 1)) return false;
    
    // 0x08-0x09: Version Patch
    if (!write_uint16(codegen, 0)) return false;
    
    // 0x0A-0x0B: Flags
    uint16_t flags = 0;
    if (codegen->config.enable_compression) flags |= 0x0008;
    if (codegen->config.preserve_debug_info) flags |= 0x0010;
    flags |= 0x0001; // Has styles
    if (!write_uint16(codegen, flags)) return false;
    
    // 0x0C-0x0F: Style Definition Count
    if (!write_uint32(codegen, style_count)) return false;
    
    // 0x10-0x13: Theme Variable Count
    if (!write_uint32(codegen, theme_count)) return false;
    
    // 0x14-0x17: Widget Definition Count
    if (!write_uint32(codegen, widget_def_count)) return false;
    
    // 0x18-0x1B: Widget Instance Count
    if (!write_uint32(codegen, element_count)) return false;
    
    // 0x1C-0x1F: Property Count
    if (!write_uint32(codegen, total_properties)) return false;
    
    // 0x20-0x23: String Table Size (placeholder, will be updated later)
    size_t string_table_size_offset = codegen->current_offset;
    if (!write_uint32(codegen, 0)) return false;
    
    // 0x24-0x27: Data Size (placeholder, will be updated later)
    size_t data_size_offset = codegen->current_offset;
    if (!write_uint32(codegen, 0)) return false;
    
    // 0x28-0x2B: Checksum (placeholder, will be calculated at end)
    size_t checksum_offset = codegen->current_offset;
    if (!write_uint32(codegen, 0)) return false;
    
    // 0x2C: Compression
    if (!write_uint8(codegen, KRYON_KRB_COMPRESSION_NONE)) return false;
    
    // 0x2D-0x30: Uncompressed Size
    if (!write_uint32(codegen, 0)) return false; // Will be same as data size for no compression
    
    // 0x31-0x34: Style Definition Offset (placeholder)
    size_t style_offset_pos = codegen->current_offset;
    if (!write_uint32(codegen, 0)) return false;
    
    // 0x35-0x38: Theme Variable Offset (placeholder)
    size_t theme_offset_pos = codegen->current_offset;
    if (!write_uint32(codegen, 0)) return false;
    
    // 0x39-0x3C: Widget Definition Offset (placeholder)
    size_t widget_def_offset_pos = codegen->current_offset;
    if (!write_uint32(codegen, 0)) return false;
    
    // 0x3D-0x40: Widget Instance Offset (placeholder)
    size_t widget_inst_offset_pos = codegen->current_offset;
    if (!write_uint32(codegen, 0)) return false;
    
    // 0x41-0x44: Script Offset (placeholder)
    size_t script_offset_pos = codegen->current_offset;
    if (!write_uint32(codegen, 0)) return false;
    
    // 0x45-0x48: Resource Offset (unused)
    if (!write_uint32(codegen, 0)) return false;
    
    // Debug: Check current offset before reserved bytes
    size_t offset_before_reserved = codegen->current_offset;
    
    // 0x49-0x7F: Reserved bytes (need to calculate exactly how many)
    // We need to reach 128 bytes total, so calculate remaining
    size_t bytes_needed = 128 - offset_before_reserved;
    printf("DEBUG: Writing %zu reserved bytes (offset %zu, need to reach 128)\n", bytes_needed, offset_before_reserved);
    
    for (size_t i = 0; i < bytes_needed; i++) {
        if (!write_uint8(codegen, 0)) return false;
    }
    
    // Debug: Check final offset
    printf("DEBUG: Final header size: %zu bytes\n", codegen->current_offset);
    
    // Header is now complete (128 bytes). Start writing sections.
    
    // IMPORTANT: Pre-process functions to add bytecode strings to string table before writing it
    if (ast_root->type == KRYON_AST_ROOT) {
        for (size_t i = 0; i < ast_root->data.element.child_count; i++) {
            const KryonASTNode *child = ast_root->data.element.children[i];
            if (child && child->type == KRYON_AST_FUNCTION_DEFINITION) {
                // Pre-compile Lua functions to add bytecode strings to string table
                if (child->data.function_def.language && 
                    strcmp(child->data.function_def.language, "lua") == 0 &&
                    child->data.function_def.code) {
                    
                    printf("🔍 DEBUG: Pre-processing Lua function '%s' for string table\n", 
                           child->data.function_def.name);
                    
                    // Pre-add function metadata strings to string table
                    add_string_to_table(codegen, child->data.function_def.language);  // "lua"
                    add_string_to_table(codegen, child->data.function_def.name);     // function name
                    
                    KryonVM* temp_vm = kryon_vm_create(KRYON_VM_LUA, NULL);
                    if (temp_vm) {
                        char* wrapped_code = kryon_alloc(strlen(child->data.function_def.code) + 
                                                       strlen(child->data.function_def.name) + 50);
                        if (wrapped_code) {
                            snprintf(wrapped_code, strlen(child->data.function_def.code) + 
                                    strlen(child->data.function_def.name) + 50,
                                    "function %s()\n%s\nend", 
                                    child->data.function_def.name, 
                                    child->data.function_def.code);
                            
                            KryonScript script = {0};
                            KryonVMResult result = kryon_vm_compile(temp_vm, wrapped_code, 
                                                                   child->data.function_def.name, &script);
                            
                            if (result == KRYON_VM_SUCCESS && script.data && script.size > 0) {
                                char* bytecode_str = kryon_alloc(script.size * 2 + 1);
                                if (bytecode_str) {
                                    for (size_t j = 0; j < script.size; j++) {
                                        snprintf(bytecode_str + j * 2, 3, "%02x", ((uint8_t*)script.data)[j]);
                                    }
                                    // Add to string table during preprocessing
                                    add_string_to_table(codegen, bytecode_str);
                                    printf("✅ Pre-added Lua bytecode to string table (%zu bytes)\n", script.size);
                                    kryon_free(bytecode_str);
                                }
                                if (script.data) kryon_free(script.data);
                            }
                            kryon_free(wrapped_code);
                        }
                        kryon_vm_destroy(temp_vm);
                    }
                }
            }
        }
    }
    
    // Write String Table first (now includes bytecode strings)
    size_t string_table_start = codegen->current_offset;
    if (!kryon_write_string_table(codegen)) {
        return false;
    }
    size_t string_table_end = codegen->current_offset;
    
    // Update string table size in header
    uint32_t string_table_size = (uint32_t)(string_table_end - string_table_start);
    printf("DEBUG: String table size: %u bytes (start=%zu, end=%zu)\n", string_table_size, string_table_start, string_table_end);
    
    // Use proper endianness-aware write (fix for endianness issue)
    if (!update_header_uint32(codegen, string_table_size_offset, string_table_size)) {
        codegen_error(codegen, "Failed to write string table size to header");
        return false;
    }
    
    // Write Style Definition Table
    size_t style_section_start = codegen->current_offset;
    if (style_count > 0) {
        // Update style offset in header
        if (!update_header_uint32(codegen, style_offset_pos, (uint32_t)style_section_start)) {
            codegen_error(codegen, "Failed to write style offset to header");
            return false;
        }
        
        if (ast_root->type == KRYON_AST_ROOT) {
            for (size_t i = 0; i < ast_root->data.element.child_count; i++) {
                const KryonASTNode *child = ast_root->data.element.children[i];
                if (child && child->type == KRYON_AST_STYLE_BLOCK) {
                    if (!write_style_definition(codegen, child)) {
                        codegen_error(codegen, "Failed to write style definition");
                        return false;
                    }
                    codegen->stats.output_styles++;
                }
            }
        }
    }
    
    // Write Theme Variable Table (empty for now)
    size_t theme_section_start = codegen->current_offset;
    if (theme_count > 0) {
        if (!update_header_uint32(codegen, theme_offset_pos, (uint32_t)theme_section_start)) {
            codegen_error(codegen, "Failed to write theme offset to header");
            return false;
        }
        // TODO: Implement theme variable writing
    }
    
    // Write Widget Definition Table (empty for now - using built-in widgets)
    size_t widget_def_section_start = codegen->current_offset;
    if (widget_def_count > 0) {
        if (!update_header_uint32(codegen, widget_def_offset_pos, (uint32_t)widget_def_section_start)) {
            codegen_error(codegen, "Failed to write widget definition offset to header");
            return false;
        }
        // TODO: Implement widget definition writing
    }
    
    // Write Widget Instance Table (the actual UI elements)
    size_t widget_inst_section_start = codegen->current_offset;
    if (!update_header_uint32(codegen, widget_inst_offset_pos, (uint32_t)widget_inst_section_start)) {
        codegen_error(codegen, "Failed to write widget instance offset to header");
        return false;
    }
    
    if (ast_root->type == KRYON_AST_ROOT) {
        for (size_t i = 0; i < ast_root->data.element.child_count; i++) {
            const KryonASTNode *child = ast_root->data.element.children[i];
            if (child && child->type == KRYON_AST_ELEMENT) {
                if (!write_widget_instance(codegen, child)) {
                    codegen_error(codegen, "Failed to write widget instance");
                    return false;
                }
                codegen->stats.output_elements++;
            }
        }
    } else if (ast_root->type == KRYON_AST_ELEMENT) {
        // Handle case where AST root is directly an element (e.g., App)
        if (!write_widget_instance(codegen, ast_root)) {
            codegen_error(codegen, "Failed to write root widget instance");
            return false;
        }
        codegen->stats.output_elements++;
    }
    
    // Write Variables Section
    size_t var_section_start = codegen->current_offset;
    
    // Count variables first
    uint32_t total_var_count = 0;
    if (ast_root->type == KRYON_AST_ROOT) {
        for (size_t i = 0; i < ast_root->data.element.child_count; i++) {
            const KryonASTNode *child = ast_root->data.element.children[i];
            if (child && child->type == KRYON_AST_VARIABLE_DEFINITION) {
                if (child->data.variable_def.name && 
                    strcmp(child->data.variable_def.name, "__variables_block__") == 0) {
                    // Count variables in block
                    total_var_count += child->data.element.child_count;
                } else {
                    // Single variable
                    total_var_count++;
                }
            }
        }
    }
    
    // Write Variables section header
    if (!write_uint32(codegen, 0x56415253)) { // "VARS" magic
        codegen_error(codegen, "Failed to write Variables section magic");
        return false;
    }
    
    // Write variable count
    if (!write_uint32(codegen, total_var_count)) {
        codegen_error(codegen, "Failed to write variable count");
        return false;
    }
    
    // Write variables
    if (ast_root->type == KRYON_AST_ROOT) {
        for (size_t i = 0; i < ast_root->data.element.child_count; i++) {
            const KryonASTNode *child = ast_root->data.element.children[i];
            if (child && child->type == KRYON_AST_VARIABLE_DEFINITION) {
                if (!write_variable_node(codegen, child)) {
                    codegen_error(codegen, "Failed to write variable definition");
                    return false;
                }
            }
        }
    }
    
    // Write Script Section (functions)
    size_t script_section_start = codegen->current_offset;
    if (!update_header_uint32(codegen, script_offset_pos, (uint32_t)script_section_start)) {
        codegen_error(codegen, "Failed to write script offset to header");
        return false;
    }
    
    // Count functions and components
    uint32_t function_count = 0;
    uint32_t component_count = 0;
    if (ast_root->type == KRYON_AST_ROOT) {
        for (size_t i = 0; i < ast_root->data.element.child_count; i++) {
            const KryonASTNode *child = ast_root->data.element.children[i];
            if (child && child->type == KRYON_AST_FUNCTION_DEFINITION) {
                function_count++;
            } else if (child && child->type == KRYON_AST_COMPONENT) {
                component_count++;
            }
        }
    }
    
    // TEMPORARY: Add a test Lua function if we have an onClick handler
    bool has_onclick = false;
    // Disable test function for now due to segfault
    has_onclick = false;
    
    if (false && ast_root->type == KRYON_AST_ROOT) {
        for (size_t i = 0; i < ast_root->data.element.child_count; i++) {
            const KryonASTNode *elem = ast_root->data.element.children[i];
            if (elem && elem->type == KRYON_AST_ELEMENT) {
                for (size_t j = 0; j < elem->data.element.property_count; j++) {
                    const KryonASTNode *prop = elem->data.element.properties[j];
                    if (prop && prop->type == KRYON_AST_PROPERTY && 
                        prop->data.property.name && 
                        strcmp(prop->data.property.name, "onClick") == 0) {
                        has_onclick = true;
                        break;
                    }
                }
                // Check children recursively
                for (size_t j = 0; j < elem->data.element.child_count; j++) {
                    const KryonASTNode *child = elem->data.element.children[j];
                    if (child && child->type == KRYON_AST_ELEMENT) {
                        for (size_t k = 0; k < child->data.element.property_count; k++) {
                            const KryonASTNode *prop = child->data.element.properties[k];
                            if (prop && prop->type == KRYON_AST_PROPERTY && 
                                prop->data.property.name && 
                                strcmp(prop->data.property.name, "onClick") == 0) {
                                has_onclick = true;
                                break;
                            }
                        }
                    }
                }
            }
        }
    }
    
    if (has_onclick) {
        function_count++; // Add one for our test function
    }
    
    // Write total count (functions + components)
    uint32_t total_count = function_count + component_count;
    if (!write_uint32(codegen, total_count)) {
        codegen_error(codegen, "Failed to write script section count");
        return false;
    }
    
    // Write each function
    if (ast_root->type == KRYON_AST_ROOT) {
        for (size_t i = 0; i < ast_root->data.element.child_count; i++) {
            const KryonASTNode *child = ast_root->data.element.children[i];
            if (child && child->type == KRYON_AST_FUNCTION_DEFINITION) {
                if (!write_function_node(codegen, child)) {
                    codegen_error(codegen, "Failed to write function definition");
                    return false;
                }
            }
        }
    }
    
    // Write each component  
    if (ast_root->type == KRYON_AST_ROOT) {
        for (size_t i = 0; i < ast_root->data.element.child_count; i++) {
            const KryonASTNode *child = ast_root->data.element.children[i];
            if (child && child->type == KRYON_AST_COMPONENT) {
                if (!write_component_node(codegen, child)) {
                    codegen_error(codegen, "Failed to write component definition");
                    return false;
                }
            }
        }
    }
    
    // TEMPORARY: Write test Lua function if we detected onClick
    if (has_onclick) {
        printf("DEBUG: Writing test Lua function to KRB\n");
        
        // Add required strings to string table
        uint32_t lang_ref = add_string_to_table(codegen, "lua");
        uint32_t name_ref = add_string_to_table(codegen, "handleClick");
        const char* lua_code = "kryon.log('Button clicked!')\nlocal text = kryon.getText()\nkryon.setText('Clicked: ' .. text)";
        
        // Write function header ("FUNC" magic)
        if (!write_uint32(codegen, 0x46554E43)) { // "FUNC"
            codegen_error(codegen, "Failed to write function magic");
            return false;
        }
        
        // Write language reference (string table index)
        if (!write_uint32(codegen, lang_ref)) {
            codegen_error(codegen, "Failed to write language reference");
            return false;
        }
        
        // Write function name reference (string table index)
        if (!write_uint32(codegen, name_ref)) {
            codegen_error(codegen, "Failed to write name reference");
            return false;
        }
        
        // Write parameter count (0 for this test function)
        if (!write_uint32(codegen, 0)) {
            codegen_error(codegen, "Failed to write parameter count");
            return false;
        }
        
        // Compile Lua code to bytecode
        KryonVM* temp_vm = kryon_vm_create(KRYON_VM_LUA, NULL);
        if (!temp_vm) {
            codegen_error(codegen, "Failed to create temporary Lua VM");
            return false;
        }
        
        KryonScript bytecode_script;
        KryonVMResult result = kryon_vm_compile(temp_vm, lua_code, "handleClick", &bytecode_script);
        if (result != KRYON_VM_SUCCESS) {
            kryon_vm_destroy(temp_vm);
            codegen_error(codegen, "Failed to compile Lua function to bytecode");
            return false;
        }
        
        // Convert bytecode to hex string
        size_t hex_len = bytecode_script.size * 2;
        char* hex_string = malloc(hex_len + 1);
        if (!hex_string) {
            kryon_vm_destroy(temp_vm);
            codegen_error(codegen, "Failed to allocate hex string buffer");
            return false;
        }
        
        for (size_t i = 0; i < bytecode_script.size; i++) {
            sprintf(hex_string + i * 2, "%02x", bytecode_script.data[i]);
        }
        hex_string[hex_len] = '\0';
        
        printf("DEBUG: Compiled Lua function to %zu bytes of bytecode\n", bytecode_script.size);
        
        // Add bytecode hex string to string table
        uint32_t code_ref = add_string_to_table(codegen, hex_string);
        
        // Write function code reference (string table index)
        if (!write_uint32(codegen, code_ref)) {
            free(hex_string);
            kryon_vm_destroy(temp_vm);
            codegen_error(codegen, "Failed to write code reference");
            return false;
        }
        
        // Cleanup
        free(hex_string);
        if (bytecode_script.data) {
            free((void*)bytecode_script.data);
        }
        if (bytecode_script.source_name) {
            free((void*)bytecode_script.source_name);
        }
        kryon_vm_destroy(temp_vm);
        
        printf("DEBUG: Successfully wrote Lua function to KRB\n");
    }
    
    // Update data size in header
    size_t total_data_size = codegen->current_offset;
    if (!update_header_uint32(codegen, data_size_offset, (uint32_t)total_data_size)) {
        codegen_error(codegen, "Failed to write data size to header");
        return false;
    }
    
    // Calculate and write checksum
    uint32_t checksum = kryon_krb_calculate_checksum(codegen->binary_data, codegen->current_offset);
    if (!update_header_uint32(codegen, checksum_offset, checksum)) {
        codegen_error(codegen, "Failed to write checksum to header");
        return false;
    }
    
    return true;
}

// =============================================================================
// COMPLEX FORMAT HELPER FUNCTIONS
// =============================================================================

static bool write_style_definition(KryonCodeGenerator *codegen, const KryonASTNode *style) {
    if (!style || style->type != KRYON_AST_STYLE_BLOCK) {
        return false;
    }
    
    // Style Definition Structure as per KRB v0.1 spec:
    // [Style ID: u32]
    // [Style Name String ID: u16]
    // [Parent Style ID: u32]
    // [Property Count: u16]
    // [Property Definitions]
    // [Style Flags: u16]
    // [Reserved: 4 bytes]
    
    // Style ID (auto-increment)
    if (!write_uint32(codegen, codegen->next_element_id++)) return false;
    
    // Style Name String ID
    uint32_t name_string_id = add_string_to_table(codegen, style->data.style.name);
    if (!write_uint16(codegen, (uint16_t)name_string_id)) return false;
    
    // Parent Style ID (0 = no parent)
    if (!write_uint32(codegen, 0)) return false;
    
    // Property Count  
    if (!write_uint16(codegen, (uint16_t)style->data.style.property_count)) return false;
    
    // Write style properties
    for (size_t i = 0; i < style->data.style.property_count; i++) {
        if (!write_property_node(codegen, style->data.style.properties[i])) {
            return false;
        }
    }
    
    // Style Flags (0 for now)
    if (!write_uint16(codegen, 0)) return false;
    
    // Reserved bytes
    if (!write_uint32(codegen, 0)) return false;
    
    return true;
}

static bool write_widget_instance(KryonCodeGenerator *codegen, const KryonASTNode *element) {
    if (!element || element->type != KRYON_AST_ELEMENT) {
        return false;
    }
    
    // Widget Instance Structure as per KRB v0.1 spec:
    // [Instance ID: u32]
    // [Widget Type ID: u32]
    // [Parent Instance ID: u32]
    // [Style Reference ID: u32]
    // [Property Count: u16]
    // [Child Count: u16]
    // [Event Handler Count: u16]
    // [Widget Flags: u32]
    // [Properties]
    // [Child Instance IDs]
    // [Event Handlers]
    
    // Instance ID
    uint32_t instance_id = codegen->next_element_id++;
    if (!write_uint32(codegen, instance_id)) return false;
    
    // Widget Type ID (use our widget mappings)
    uint16_t widget_type = kryon_codegen_get_element_hex(element->data.element.element_type);
    if (!write_uint32(codegen, (uint32_t)widget_type)) return false;
    
    // Parent Instance ID (0 for root elements)
    if (!write_uint32(codegen, 0)) return false;
    
    // Style Reference ID (0 = no style)
    if (!write_uint32(codegen, 0)) return false;
    
    // Property Count
    if (!write_uint16(codegen, (uint16_t)element->data.element.property_count)) return false;
    
    // Child Count  
    if (!write_uint16(codegen, (uint16_t)element->data.element.child_count)) return false;
    
    // Event Handler Count (0 for now)
    if (!write_uint16(codegen, 0)) return false;
    
    // Widget Flags (0 for now)
    if (!write_uint32(codegen, 0)) return false;
    
    // Write properties
    for (size_t i = 0; i < element->data.element.property_count; i++) {
        if (!write_property_node(codegen, element->data.element.properties[i])) {
            return false;
        }
    }
    
    // Write child instances recursively
    for (size_t i = 0; i < element->data.element.child_count; i++) {
        if (!write_widget_instance(codegen, element->data.element.children[i])) {
            return false;
        }
    }
    
    // Event handlers (none for now)
    
    return true;
}

// CRC32 checksum function is implemented in krb_file.c

bool kryon_codegen_validate_binary(const KryonCodeGenerator *codegen) {
    if (!codegen || codegen->binary_size < 12) { // header + version + flags
        return false;
    }
    
    // Check magic number using centralized binary_io (handles endianness)
    size_t offset = 0;
    uint32_t magic;
    if (!kryon_read_uint32(codegen->binary_data, &offset, codegen->binary_size, &magic)) {
        return false;
    }
    
    if (magic != 0x4B52594E) { // "KRYN"
        return false;
    }
    
    // Basic validation passed
    return true;
}

// =============================================================================
// PROPERTY VALUE CONVERSION
// =============================================================================

// =============================================================================
// PROPERTY VALUE CONVERSION
// =============================================================================

/**
 * @brief Writes a property's value, performing type conversions based on property hints.
 *
 * This function intelligently converts string values to colors, integers to floats, etc.,
 * based on the expected type of the property, making the KRY language more flexible.
 *
 * @param codegen The code generator instance.
 * @param value The AST value node to write.
 * @param property_hex The hex code of the property being written.
 * @return true on success, false on failure.
 */
 static bool write_property_value_node(KryonCodeGenerator *codegen, const KryonASTValue *value, uint16_t property_hex) {
    if (!value) {
        return false;
    }

    // Use the helper function you correctly added
    KryonValueTypeHint hint = get_property_type_hint(property_hex);
    
    // Debug: Show type hint resolution
    printf("🔍 Property 0x%04X has type hint %d, value type %d\n", property_hex, hint, value->type);

    // Perform type conversions based on the property's expected type
    switch (hint) {
        case KRYON_TYPE_HINT_COLOR:
            // If this property expects a color and we got a string, parse it to uint32
            if (value->type == KRYON_VALUE_STRING) {
                // Use the color parsing utility to convert to hex
                uint32_t color_value = kryon_color_parse_string(value->data.string_value);
                // Write directly as uint32 without type tag
                return write_uint32(codegen, color_value);
            }
            // If it's not a string, it might already be a color value. Fall through to default.
            break;

        case KRYON_TYPE_HINT_FLOAT:
            // If this property expects a float (like width, size) and we got an integer, convert it.
            if (value->type == KRYON_VALUE_INTEGER) {
                float float_value = (float)value->data.int_value;
                // Write directly as float without type tag
                
                // Use memcpy to safely get the bit representation of the float.
                uint32_t float_bits;
                memcpy(&float_bits, &float_value, sizeof(float));
                return write_uint32(codegen, float_bits);
            }
            // If not an integer, fall through to default.
            break;

        case KRYON_TYPE_HINT_DIMENSION:
        case KRYON_TYPE_HINT_SPACING:
            // If this property expects a dimension/spacing and we got a unit value, convert to float
            if (value->type == KRYON_VALUE_UNIT) {
                float float_value = (float)value->data.unit_value.value;
                // Write directly as float without type tag (ignore unit for now)
                
                // Use memcpy to safely get the bit representation of the float.
                uint32_t float_bits;
                memcpy(&float_bits, &float_value, sizeof(float));
                return write_uint32(codegen, float_bits);
            }
            // If property expects dimension but got integer, convert it
            if (value->type == KRYON_VALUE_INTEGER) {
                float float_value = (float)value->data.int_value;
                uint32_t float_bits;
                memcpy(&float_bits, &float_value, sizeof(float));
                return write_uint32(codegen, float_bits);
            }
            break;

        // Add more conversion cases here as needed (e.g., KRYON_TYPE_HINT_BOOL)

        default:
            // For KRYON_TYPE_HINT_ANY or if no conversion is applicable, do nothing special.
            break;
    }

    // Default behavior: write the value as-is without any conversion.
    return write_value_node(codegen, value);
}

// =============================================================================
// RECURSIVE COUNTING HELPERS
// =============================================================================

static uint32_t count_elements_recursive(const KryonASTNode *node) {
    if (!node || node->type != KRYON_AST_ELEMENT) {
        return 0;
    }
    
    uint32_t count = 1; // Count this element
    
    // Count child elements recursively
    for (size_t i = 0; i < node->data.element.child_count; i++) {
        if (node->data.element.children[i]) {
            count += count_elements_recursive(node->data.element.children[i]);
        }
    }
    
    return count;
}

static uint32_t count_properties_recursive(const KryonASTNode *node) {
    if (!node || node->type != KRYON_AST_ELEMENT) {
        return 0;
    }
    
    uint32_t count = node->data.element.property_count; // Count this element's properties
    
    // Count child element properties recursively
    for (size_t i = 0; i < node->data.element.child_count; i++) {
        if (node->data.element.children[i]) {
            count += count_properties_recursive(node->data.element.children[i]);
        }
    }
    
    return count;
}
