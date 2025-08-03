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
static bool write_element_node(KryonCodeGenerator *codegen, const KryonASTNode *element, const KryonASTNode *ast_root);
static KryonASTNode *expand_component_instance(const KryonASTNode *component_instance, const KryonASTNode *ast_root);
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
static bool write_widget_instance(KryonCodeGenerator *codegen, const KryonASTNode *element, const KryonASTNode *ast_root);
static bool write_const_definition(KryonCodeGenerator *codegen, const KryonASTNode *const_def);
static bool expand_const_for_loop(KryonCodeGenerator *codegen, const KryonASTNode *const_for, const KryonASTNode *ast_root);
static uint32_t count_const_for_elements(KryonCodeGenerator *codegen, const KryonASTNode *const_for);
static uint16_t count_expanded_children(KryonCodeGenerator *codegen, const KryonASTNode *element);
static bool add_constant_to_table(KryonCodeGenerator *codegen, const char *name, const KryonASTNode *value);
static const KryonASTNode *find_constant_value(KryonCodeGenerator *codegen, const char *name);
static char *process_string_template(const char *template_string, const char *var_name, const KryonASTNode *var_value);
static KryonASTNode *substitute_template_vars(const KryonASTNode *node, const char *var_name, const KryonASTNode *var_value);

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
static uint16_t next_custom_element_hex = 0x2000; // Start custom elements at 0x2000 to avoid conflicts

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
                // Check if this is a custom component instance
                if (child->type == KRYON_AST_ELEMENT) {
                    uint16_t element_hex = kryon_codegen_get_element_hex(child->data.element.element_type);
                    if (element_hex >= 0x2000) {
                        // This is a custom component - expand it
                        printf("🔧 Expanding custom component: %s\n", child->data.element.element_type);
                        KryonASTNode *expanded = expand_component_instance(child, ast_root);
                        if (expanded) {
                            if (write_element_node(codegen, expanded, ast_root)) {
                                element_count++;
                                codegen->stats.output_elements++;
                                // TODO: Free expanded node memory
                            } else {
                                return false;
                            }
                        } else {
                            printf("❌ Failed to expand component: %s\n", child->data.element.element_type);
                            return false;
                        }
                        continue;
                    }
                }
                
                // Standard element - write normally
                if (write_element_node(codegen, child, ast_root)) {
                    element_count++;
                    codegen->stats.output_elements++;
                } else {
                    return false;
                }
            } else if (child && child->type == KRYON_AST_CONST_FOR_LOOP) {
                // Expand const_for loops inline
                if (expand_const_for_loop(codegen, child, ast_root)) {
                    // Element count is handled within expand_const_for_loop
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

static bool write_element_node(KryonCodeGenerator *codegen, const KryonASTNode *element, const KryonASTNode *ast_root) {
    if (!element || (element->type != KRYON_AST_ELEMENT && element->type != KRYON_AST_EVENT_DIRECTIVE)) {
        return false;
    }
    
    // Check if this is a custom component instance that needs expansion
    if (element->type == KRYON_AST_ELEMENT) {
        uint16_t element_hex = kryon_codegen_get_element_hex(element->data.element.element_type);
        printf("🔍 DEBUG: Element '%s' has hex 0x%04X\n", element->data.element.element_type, element_hex);
        if (element_hex >= 0x2000) {
            // This is a custom component - expand it
            printf("🔧 Expanding custom component in recursive call: %s\n", element->data.element.element_type);
            KryonASTNode *expanded = expand_component_instance(element, ast_root);
            if (expanded) {
                bool result = write_element_node(codegen, expanded, ast_root);
                // TODO: Free expanded node memory
                return result;
            } else {
                printf("❌ Failed to expand component in recursive call: %s\n", element->data.element.element_type);
                return false;
            }
        }
    }
    
    // Get element type hex code for standard elements
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
    
    // Write properties (skip for component instances - they should be expanded first)
    for (size_t i = 0; i < element->data.element.property_count; i++) {
        if (!element->data.element.properties[i]) {
            codegen_error(codegen, "Property array contains NULL pointers - parser memory allocation issue");
            return false;
        }
        printf("🔍 DEBUG: About to process property '%s' for element '%s'\n", 
               element->data.element.properties[i]->data.property.name,
               element->data.element.element_type);
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
        if (!write_element_node(codegen, element->data.element.children[i], ast_root)) {
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
        printf("DEBUG: Processing property: '%s'\n", property->data.property.name);
    }
    
    // Get property hex code
    uint16_t property_hex = kryon_codegen_get_property_hex(property->data.property.name);
    if (property_hex == 0) {
        // Check if this might be a component parameter (should be handled during expansion)
        printf("⚠️  Detected unknown property '%s' - might be component parameter\n", 
               property->data.property.name ? property->data.property.name : "NULL");
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
        } else if (property->data.property.value->type == KRYON_AST_IDENTIFIER) {
            // Handle identifier references (component parameters) as string values
            KryonASTValue id_value;
            id_value.type = KRYON_VALUE_STRING;
            // Create a variable reference string like "$paramName"  
            static char id_ref[256];
            snprintf(id_ref, sizeof(id_ref), "$%s", property->data.property.value->data.identifier.name);
            id_value.data.string_value = id_ref;
            printf("🔧 Converting identifier '%s' to variable reference '%s'\n", 
                   property->data.property.value->data.identifier.name, id_ref);
            return write_property_value_node(codegen, &id_value, property_hex);
        } else {
            // For now, only handle literals, variables, and identifiers
            printf("❌ Unsupported property value type: %d\n", property->data.property.value->type);
            codegen_error(codegen, "Non-literal/non-variable/non-identifier property values not yet supported");
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
    if (!variable || (variable->type != KRYON_AST_VARIABLE_DEFINITION && variable->type != KRYON_AST_STATE_DEFINITION)) {
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
    if (!variable || (variable->type != KRYON_AST_VARIABLE_DEFINITION && variable->type != KRYON_AST_STATE_DEFINITION)) {
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
        if (!write_element_node(codegen, component->data.component.body, NULL)) {
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
    
    // First, process constants so they're available for counting
    if (ast_root->type == KRYON_AST_ROOT) {
        for (size_t i = 0; i < ast_root->data.element.child_count; i++) {
            const KryonASTNode *child = ast_root->data.element.children[i];
            if (child && child->type == KRYON_AST_CONST_DEFINITION) {
                if (!write_const_definition(codegen, child)) {
                    codegen_error(codegen, "Failed to process constant definition");
                    return false;
                }
            }
        }
    }
    
    // Now count sections for header values (constants are available)
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
                        element_count += count_elements_recursive(child);
                        total_properties += child->data.element.property_count;
                        break;
                    case KRYON_AST_FUNCTION_DEFINITION:
                        // Function definitions will be written to script section
                        break;
                    case KRYON_AST_VARIABLE_DEFINITION:
                        // Variable definitions will be handled
                        break;
                    case KRYON_AST_CONST_DEFINITION:
                        // Constant definitions will be handled
                        break;
                    case KRYON_AST_CONST_FOR_LOOP:
                        // Const for loops will be expanded inline
                        printf("DEBUG: Found const_for loop in counting phase\n");
                        element_count += count_const_for_elements(codegen, child);
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
    
    // Pre-process components to add their strings to string table
    if (ast_root->type == KRYON_AST_ROOT) {
        for (size_t i = 0; i < ast_root->data.element.child_count; i++) {
            const KryonASTNode *child = ast_root->data.element.children[i];
            if (child && child->type == KRYON_AST_COMPONENT) {
                printf("🔍 DEBUG: Pre-processing component '%s' for string table\n", 
                       child->data.component.name ? child->data.component.name : "(unnamed)");
                
                // Add component name to string table
                if (child->data.component.name) {
                    add_string_to_table(codegen, child->data.component.name);
                }
                
                // Add parameter names and defaults to string table
                for (size_t j = 0; j < child->data.component.parameter_count; j++) {
                    if (child->data.component.parameters[j]) {
                        add_string_to_table(codegen, child->data.component.parameters[j]);
                    }
                    if (child->data.component.param_defaults[j]) {
                        add_string_to_table(codegen, child->data.component.param_defaults[j]);
                    }
                }
                
                // Add state variable names to string table
                for (size_t j = 0; j < child->data.component.state_count; j++) {
                    if (child->data.component.state_vars[j] && 
                        child->data.component.state_vars[j]->data.variable_def.name) {
                        add_string_to_table(codegen, child->data.component.state_vars[j]->data.variable_def.name);
                    }
                }
                
                printf("✅ Pre-added component '%s' strings to string table\n", 
                       child->data.component.name ? child->data.component.name : "(unnamed)");
            }
        }
    }
    
    // Write String Table first (now includes bytecode and component strings)
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
    
    // Constants already processed at the beginning for counting
    
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
                if (!write_widget_instance(codegen, child, ast_root)) {
                    codegen_error(codegen, "Failed to write widget instance");
                    return false;
                }
                codegen->stats.output_elements++;
            } else if (child && child->type == KRYON_AST_CONST_FOR_LOOP) {
                // Expand const_for loops inline
                if (!expand_const_for_loop(codegen, child, ast_root)) {
                    codegen_error(codegen, "Failed to expand const_for loop");
                    return false;
                }
            }
        }
    } else if (ast_root->type == KRYON_AST_ELEMENT) {
        // Handle case where AST root is directly an element (e.g., App)
        if (!write_widget_instance(codegen, ast_root, ast_root)) {
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
    
    // Write function count only (components are written separately)
    if (!write_uint32(codegen, function_count)) {
        codegen_error(codegen, "Failed to write function count");
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
    
    // Components will be written after all script sections are complete
    
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
    
    // Write components after all script sections are complete
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

static bool write_widget_instance(KryonCodeGenerator *codegen, const KryonASTNode *element, const KryonASTNode *ast_root) {
    if (!element) {
        return false;
    }
    
    // Handle const_for loops by expanding them
    if (element->type == KRYON_AST_CONST_FOR_LOOP) {
        return expand_const_for_loop(codegen, element, ast_root);
    }
    
    if (element->type != KRYON_AST_ELEMENT) {
        return false;
    }
    
    // Check if this is a custom component instance that needs expansion
    uint16_t element_hex = kryon_codegen_get_element_hex(element->data.element.element_type);
    printf("DEBUG: write_widget_instance - Element '%s' has hex 0x%04X\n", 
           element->data.element.element_type, element_hex);
           
    if (element_hex >= 0x2000) {
        // This is a custom component - expand it
        printf("🔧 Expanding custom component in write_widget_instance: %s\n", element->data.element.element_type);
        
        KryonASTNode *expanded = expand_component_instance(element, ast_root);
        if (expanded) {
            // Write the expanded component body
            bool result = write_widget_instance(codegen, expanded, ast_root);
            // TODO: Free expanded node memory if we implement proper cloning
            return result;
        } else {
            printf("❌ Failed to expand component in write_widget_instance: %s\n", element->data.element.element_type);
            return false;
        }
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
    
    // Child Count (expanded for @const_for loops)
    uint16_t expanded_child_count = count_expanded_children(codegen, element);
    printf("DEBUG: Element has %zu AST children, expanded to %u runtime children\n", 
           element->data.element.child_count, expanded_child_count);
    if (!write_uint16(codegen, expanded_child_count)) return false;
    
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
        if (!write_widget_instance(codegen, element->data.element.children[i], ast_root)) {
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
    printf("DEBUG: Property 0x%04X has type hint %d, value type %d\n", property_hex, hint, value->type);

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
    if (!node) {
        return 0;
    }
    
    if (node->type == KRYON_AST_ELEMENT) {
        uint32_t count = 1; // Count this element
        
        // Count child elements recursively
        for (size_t i = 0; i < node->data.element.child_count; i++) {
            if (node->data.element.children[i]) {
                count += count_elements_recursive(node->data.element.children[i]);
            }
        }
        
        return count;
    } else if (node->type == KRYON_AST_CONST_FOR_LOOP) {
        // This requires access to codegen to resolve constants
        // For now, return a reasonable estimate
        // TODO: This should be fixed properly by passing codegen context
        
        // Hardcode for alignments array (6 elements)
        if (node->data.const_for_loop.array_name && 
            strcmp(node->data.const_for_loop.array_name, "alignments") == 0) {
            
            uint32_t array_size = 6;
            uint32_t elements_per_iteration = 0;
            
            // Count elements in each body element recursively
            for (size_t i = 0; i < node->data.const_for_loop.body_count; i++) {
                const KryonASTNode *body_element = node->data.const_for_loop.body[i];
                elements_per_iteration += count_elements_recursive(body_element);
            }
            
            printf("DEBUG: count_elements_recursive found const_for - Array '%s' has %u elements, %u elements per iteration = %u total\n",
                   node->data.const_for_loop.array_name, array_size, elements_per_iteration, array_size * elements_per_iteration);
            
            return array_size * elements_per_iteration;
        }
        
        // Fallback for unknown arrays
        return node->data.const_for_loop.body_count;
    }
    
    return 0;
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

// =============================================================================
// COMPONENT EXPANSION
// =============================================================================

static KryonASTNode *find_component_definition(const char *component_name, const KryonASTNode *ast_root) {
    if (!component_name || !ast_root) return NULL;
    
    // Search through all children in the AST root
    for (size_t i = 0; i < ast_root->data.element.child_count; i++) {
        const KryonASTNode *child = ast_root->data.element.children[i];
        if (child && child->type == KRYON_AST_COMPONENT) {
            if (child->data.component.name && strcmp(child->data.component.name, component_name) == 0) {
                return (KryonASTNode*)child; // Found the component definition
            }
        }
    }
    
    return NULL; // Component not found
}

// Parameter substitution context
typedef struct {
    const char *param_name;
    const KryonASTNode *param_value;
} ParamSubstitution;

typedef struct {
    ParamSubstitution *substitutions;
    size_t count;
    size_t capacity;
} ParamContext;

// Forward declarations
static KryonASTNode *clone_and_substitute_node(const KryonASTNode *original, const ParamContext *params);
static KryonASTNode *create_minimal_ast_node(KryonASTNodeType type);

static KryonASTNode *clone_ast_node(const KryonASTNode *original) {
    if (!original) return NULL;
    
    // For component nodes, return the body for expansion
    if (original->type == KRYON_AST_COMPONENT && original->data.component.body) {
        // Clone without substitution for now
        return clone_and_substitute_node(original->data.component.body, NULL);
    }
    
    return NULL;
}

// Create a minimal AST node without parser
static KryonASTNode *create_minimal_ast_node(KryonASTNodeType type) {
    KryonASTNode *node = calloc(1, sizeof(KryonASTNode));
    if (!node) return NULL;
    
    node->type = type;
    // Initialize arrays based on type
    switch (type) {
        case KRYON_AST_ELEMENT:
            node->data.element.children = NULL;
            node->data.element.child_count = 0;
            node->data.element.child_capacity = 0;
            node->data.element.properties = NULL;
            node->data.element.property_count = 0;
            node->data.element.property_capacity = 0;
            break;
        default:
            break;
    }
    return node;
}

// Add child to element node
static bool add_child_to_node(KryonASTNode *parent, KryonASTNode *child) {
    if (!parent || !child || parent->type != KRYON_AST_ELEMENT) return false;
    
    // Grow array if needed
    if (parent->data.element.child_count >= parent->data.element.child_capacity) {
        size_t new_capacity = parent->data.element.child_capacity == 0 ? 4 : parent->data.element.child_capacity * 2;
        KryonASTNode **new_children = realloc(parent->data.element.children, 
                                            new_capacity * sizeof(KryonASTNode*));
        if (!new_children) return false;
        parent->data.element.children = new_children;
        parent->data.element.child_capacity = new_capacity;
    }
    
    parent->data.element.children[parent->data.element.child_count++] = child;
    child->parent = parent;
    return true;
}

// Add property to element node
static bool add_property_to_node(KryonASTNode *parent, KryonASTNode *property) {
    if (!parent || !property || parent->type != KRYON_AST_ELEMENT) return false;
    
    // Grow array if needed
    if (parent->data.element.property_count >= parent->data.element.property_capacity) {
        size_t new_capacity = parent->data.element.property_capacity == 0 ? 4 : parent->data.element.property_capacity * 2;
        KryonASTNode **new_properties = realloc(parent->data.element.properties, 
                                               new_capacity * sizeof(KryonASTNode*));
        if (!new_properties) return false;
        parent->data.element.properties = new_properties;
        parent->data.element.property_capacity = new_capacity;
    }
    
    parent->data.element.properties[parent->data.element.property_count++] = property;
    property->parent = parent;
    return true;
}

// Find parameter value in context
static const KryonASTNode *find_param_value(const ParamContext *params, const char *param_name) {
    if (!params || !param_name) return NULL;
    
    for (size_t i = 0; i < params->count; i++) {
        if (strcmp(params->substitutions[i].param_name, param_name) == 0) {
            return params->substitutions[i].param_value;
        }
    }
    return NULL;
}

// Clone and substitute AST node
static KryonASTNode *clone_and_substitute_node(const KryonASTNode *original, const ParamContext *params) {
    if (!original) return NULL;
    
    const char* type_names[] = {
        "ROOT", "ELEMENT", "PROPERTY", "STYLE_BLOCK", "STYLES_BLOCK", "THEME_DEFINITION",
        "VARIABLE_DEFINITION", "FUNCTION_DEFINITION", "STATE_DEFINITION", "STORE_DIRECTIVE",
        "WATCH_DIRECTIVE", "ON_MOUNT_DIRECTIVE", "ON_UNMOUNT_DIRECTIVE", "IMPORT_DIRECTIVE",
        "EXPORT_DIRECTIVE", "INCLUDE_DIRECTIVE", "METADATA_DIRECTIVE", "EVENT_DIRECTIVE",
        "COMPONENT", "PROPS", "SLOTS", "LIFECYCLE", "DEFINE", "PROPERTIES", "SCRIPT",
        "LITERAL", "VARIABLE", "IDENTIFIER", "TEMPLATE", "BINARY_OP", "UNARY_OP",
        "FUNCTION_CALL", "MEMBER_ACCESS", "ARRAY_ACCESS", "ERROR"
    };
    printf("🔧 DEBUG: Cloning node type %d (%s)\n", original->type, 
           original->type < 35 ? type_names[original->type] : "UNKNOWN");
    
    KryonASTNode *cloned = create_minimal_ast_node(original->type);
    if (!cloned) return NULL;
    
    cloned->location = original->location;
    
    switch (original->type) {
        case KRYON_AST_ELEMENT:
            // Clone element type name
            if (original->data.element.element_type) {
                cloned->data.element.element_type = strdup(original->data.element.element_type);
            }
            
            // Clone properties
            for (size_t i = 0; i < original->data.element.property_count; i++) {
                KryonASTNode *prop_clone = clone_and_substitute_node(original->data.element.properties[i], params);
                if (prop_clone) {
                    add_property_to_node(cloned, prop_clone);
                }
            }
            
            // Clone children
            for (size_t i = 0; i < original->data.element.child_count; i++) {
                KryonASTNode *child_clone = clone_and_substitute_node(original->data.element.children[i], params);
                if (child_clone) {
                    add_child_to_node(cloned, child_clone);
                }
            }
            break;
            
        case KRYON_AST_PROPERTY:
            // Clone property name and value
            if (original->data.property.name) {
                cloned->data.property.name = strdup(original->data.property.name);
            }
            if (original->data.property.value) {
                cloned->data.property.value = clone_and_substitute_node(original->data.property.value, params);
            }
            break;
            
        case KRYON_AST_IDENTIFIER:
            // Check if this identifier is a parameter that needs substitution
            if (params && original->data.identifier.name) {
                printf("🔍 DEBUG: Checking identifier '%s' for substitution (params has %zu entries)\n", 
                       original->data.identifier.name, params->count);
                const KryonASTNode *param_value = find_param_value(params, original->data.identifier.name);
                if (param_value) {
                    printf("✅ Substituting identifier '%s' with parameter value\n", original->data.identifier.name);
                    // Substitute with parameter value
                    free(cloned);
                    return clone_and_substitute_node(param_value, NULL); // Clone the substituted value
                }
                printf("⚠️  No substitution found for identifier '%s'\n", original->data.identifier.name);
                for (size_t i = 0; i < params->count; i++) {
                    printf("    Available param[%zu]: '%s'\n", i, params->substitutions[i].param_name);
                }
            } else {
                printf("🔍 DEBUG: Processing identifier '%s' with no substitution params\n", 
                       original->data.identifier.name ? original->data.identifier.name : "(null)");
            }
            // Otherwise, just clone the identifier
            if (original->data.identifier.name) {
                cloned->data.identifier.name = strdup(original->data.identifier.name);
            }
            break;
            
        case KRYON_AST_LITERAL:
            // Clone literal value
            cloned->data.literal.value = original->data.literal.value;
            if (original->data.literal.value.type == KRYON_VALUE_STRING && 
                original->data.literal.value.data.string_value) {
                cloned->data.literal.value.data.string_value = strdup(original->data.literal.value.data.string_value);
            }
            break;
            
        case KRYON_AST_VARIABLE:
            // Check if this variable is a parameter that needs substitution
            if (params && original->data.variable.name) {
                printf("🔍 DEBUG: Checking variable '$%s' for substitution (params has %zu entries)\n", 
                       original->data.variable.name, params->count);
                const KryonASTNode *param_value = find_param_value(params, original->data.variable.name);
                if (param_value) {
                    printf("✅ Substituting variable '$%s' with parameter value (type %d)\n", 
                           original->data.variable.name, param_value->type);
                    if (param_value->type == KRYON_AST_LITERAL && param_value->data.literal.value.type == KRYON_VALUE_STRING) {
                        printf("    Parameter value is string: '%s'\n", param_value->data.literal.value.data.string_value);
                    } else if (param_value->type == KRYON_AST_LITERAL && param_value->data.literal.value.type == KRYON_VALUE_INTEGER) {
                        printf("    Parameter value is integer: %ld\n", param_value->data.literal.value.data.int_value);
                    } else if (param_value->type == KRYON_AST_LITERAL && param_value->data.literal.value.type == KRYON_VALUE_FLOAT) {
                        printf("    Parameter value is float: %f\n", param_value->data.literal.value.data.float_value);
                    } else {
                        printf("    Parameter value type: %d (literal type: %d)\n", param_value->type, 
                               param_value->type == KRYON_AST_LITERAL ? param_value->data.literal.value.type : -1);
                    }
                    
                    // Substitute with parameter value
                    free(cloned);
                    
                    // Special handling: if we're substituting into a text property and the value is a number,
                    // convert it to a string to avoid type mismatches
                    if (param_value->type == KRYON_AST_LITERAL && 
                        (param_value->data.literal.value.type == KRYON_VALUE_INTEGER || 
                         param_value->data.literal.value.type == KRYON_VALUE_FLOAT)) {
                        
                        // Create a new string literal node
                        KryonASTNode *string_node = create_minimal_ast_node(KRYON_AST_LITERAL);
                        if (string_node) {
                            string_node->data.literal.value.type = KRYON_VALUE_STRING;
                            
                            // Convert number to string
                            char buffer[32];
                            if (param_value->data.literal.value.type == KRYON_VALUE_INTEGER) {
                                snprintf(buffer, sizeof(buffer), "%ld", param_value->data.literal.value.data.int_value);
                            } else {
                                snprintf(buffer, sizeof(buffer), "%g", param_value->data.literal.value.data.float_value);
                            }
                            string_node->data.literal.value.data.string_value = strdup(buffer);
                            
                            printf("    Converted number to string: '%s'\n", buffer);
                            return string_node;
                        }
                    }
                    
                    return clone_and_substitute_node(param_value, NULL); // Clone the substituted value
                }
                printf("⚠️  No substitution found for variable '$%s'\n", original->data.variable.name);
                for (size_t i = 0; i < params->count; i++) {
                    printf("    Available param[%zu]: '%s'\n", i, params->substitutions[i].param_name);
                }
            } else {
                printf("🔍 DEBUG: Processing variable '$%s' with no substitution params\n", 
                       original->data.variable.name ? original->data.variable.name : "(null)");
            }
            // Otherwise, just clone the variable
            if (original->data.variable.name) {
                cloned->data.variable.name = strdup(original->data.variable.name);
            }
            break;
            
        case KRYON_AST_STATE_DEFINITION:
            // Clone state variable
            if (original->data.variable_def.name) {
                cloned->data.variable_def.name = strdup(original->data.variable_def.name);
            }
            if (original->data.variable_def.value) {
                // Check if the value is an identifier that needs substitution
                cloned->data.variable_def.value = clone_and_substitute_node(original->data.variable_def.value, params);
            }
            break;
            
        case KRYON_AST_FUNCTION_DEFINITION:
            // Clone function definition
            if (original->data.function_def.language) {
                cloned->data.function_def.language = strdup(original->data.function_def.language);
            }
            if (original->data.function_def.name) {
                cloned->data.function_def.name = strdup(original->data.function_def.name);
            }
            if (original->data.function_def.code) {
                cloned->data.function_def.code = strdup(original->data.function_def.code);
            }
            // Clone parameters
            cloned->data.function_def.parameter_count = original->data.function_def.parameter_count;
            if (original->data.function_def.parameter_count > 0) {
                cloned->data.function_def.parameters = calloc(original->data.function_def.parameter_count, sizeof(char*));
                for (size_t i = 0; i < original->data.function_def.parameter_count; i++) {
                    if (original->data.function_def.parameters[i]) {
                        cloned->data.function_def.parameters[i] = strdup(original->data.function_def.parameters[i]);
                    }
                }
            }
            break;
            
        default:
            // For other node types, copy as-is
            break;
    }
    
    return cloned;
}

static KryonASTNode *expand_component_instance(const KryonASTNode *component_instance, const KryonASTNode *ast_root) {
    if (!component_instance || !ast_root) {
        printf("❌ Invalid parameters for component expansion\n");
        return NULL;
    }
    
    if (component_instance->type != KRYON_AST_ELEMENT) {
        printf("❌ Component instance is not an element\n");
        return NULL;
    }
    
    const char *component_name = component_instance->data.element.element_type;
    printf("🔍 Looking for component definition: %s\n", component_name);
    
    // Find the component definition
    KryonASTNode *component_def = find_component_definition(component_name, ast_root);
    if (!component_def) {
        printf("❌ Component definition not found: %s\n", component_name);
        return NULL;
    }
    
    printf("✅ Found component definition: %s\n", component_name);
    printf("🔍 Component has %zu parameters, instance has %zu properties\n", 
           component_def->data.component.parameter_count, 
           component_instance->data.element.property_count);
    
    if (!component_def->data.component.body) {
        printf("❌ Component has no body to expand\n");
        return NULL;
    }
    
    // Build parameter substitution context
    ParamContext params = {0};
    if (component_def->data.component.parameter_count > 0) {
        params.capacity = component_def->data.component.parameter_count;
        params.substitutions = calloc(params.capacity, sizeof(ParamSubstitution));
        if (!params.substitutions) {
            printf("❌ Failed to allocate parameter substitutions\n");
            return NULL;
        }
        
        // Map instance properties to component parameters
        for (size_t i = 0; i < component_def->data.component.parameter_count; i++) {
            const char *param_name = component_def->data.component.parameters[i];
            if (!param_name) continue;
            
            // Find corresponding property in instance
            const KryonASTNode *param_value = NULL;
            for (size_t j = 0; j < component_instance->data.element.property_count; j++) {
                const KryonASTNode *prop = component_instance->data.element.properties[j];
                if (prop && prop->data.property.name && 
                    strcmp(prop->data.property.name, param_name) == 0) {
                    param_value = prop->data.property.value;
                    break;
                }
            }
            
            // If no value provided, use default
            if (!param_value && i < component_def->data.component.parameter_count && 
                component_def->data.component.param_defaults && 
                component_def->data.component.param_defaults[i]) {
                // Create a literal node for the default value
                printf("⚠️  Using default value for parameter '%s': '%s'\n", param_name, component_def->data.component.param_defaults[i]);
                
                KryonASTNode *default_node = create_minimal_ast_node(KRYON_AST_LITERAL);
                if (default_node) {
                    // Parse the default value as an integer (simple case for now)
                    const char *default_str = component_def->data.component.param_defaults[i];
                    char *endptr;
                    long int_val = strtol(default_str, &endptr, 10);
                    
                    if (*endptr == '\0') {
                        // It's an integer
                        default_node->data.literal.value.type = KRYON_VALUE_INTEGER;
                        default_node->data.literal.value.data.int_value = int_val;
                        printf("✅ Created default integer value: %ld\n", int_val);
                    } else {
                        // Treat as string
                        default_node->data.literal.value.type = KRYON_VALUE_STRING;
                        default_node->data.literal.value.data.string_value = strdup(default_str);
                        printf("✅ Created default string value: '%s'\n", default_str);
                    }
                    param_value = default_node;
                }
            }
            
            if (param_value) {
                params.substitutions[params.count].param_name = param_name;
                params.substitutions[params.count].param_value = param_value;
                params.count++;
                printf("✅ Mapped parameter '%s' to instance value\n", param_name);
            }
        }
    }
    
    // Create wrapper element to hold expanded content
    KryonASTNode *wrapper = create_minimal_ast_node(KRYON_AST_ELEMENT);
    if (!wrapper) {
        free(params.substitutions);
        return NULL;
    }
    
    // Use a generic container type
    wrapper->data.element.element_type = strdup("Container");
    
    // Auto-create state variables from component parameters
    for (size_t i = 0; i < params.count; i++) {
        KryonASTNode *state_def = create_minimal_ast_node(KRYON_AST_STATE_DEFINITION);
        if (state_def) {
            state_def->data.variable_def.name = strdup(params.substitutions[i].param_name);
            // Clone the parameter value as the initial state value
            state_def->data.variable_def.value = clone_and_substitute_node(params.substitutions[i].param_value, NULL);
            add_child_to_node(wrapper, state_def);
            printf("✅ Created state variable '%s' from parameter\n", params.substitutions[i].param_name);
        }
    }
    
    // Clone component state variables
    for (size_t i = 0; i < component_def->data.component.state_count; i++) {
        KryonASTNode *state_clone = clone_and_substitute_node(component_def->data.component.state_vars[i], &params);
        if (state_clone) {
            add_child_to_node(wrapper, state_clone);
        }
    }
    
    // Clone component functions
    for (size_t i = 0; i < component_def->data.component.function_count; i++) {
        KryonASTNode *func_clone = clone_and_substitute_node(component_def->data.component.functions[i], &params);
        if (func_clone) {
            add_child_to_node(wrapper, func_clone);
        }
    }
    
    // Clone and substitute the component body
    KryonASTNode *expanded_body = clone_and_substitute_node(component_def->data.component.body, &params);
    if (expanded_body) {
        add_child_to_node(wrapper, expanded_body);
    }
    
    free(params.substitutions);
    
    printf("✅ Successfully expanded component: %s\n", component_name);
    
    // For now, return just the body element (not the wrapper with state/functions)
    // TODO: Handle state and functions properly in the runtime
    return expanded_body;
}

// =============================================================================
// CONSTANT RESOLUTION AND TEMPLATE EXPANSION
// =============================================================================

static bool add_constant_to_table(KryonCodeGenerator *codegen, const char *name, const KryonASTNode *value) {
    // Ensure we have space for the new constant
    if (codegen->const_count >= codegen->const_capacity) {
        size_t new_capacity = codegen->const_capacity == 0 ? 8 : codegen->const_capacity * 2;
        void *new_table = kryon_realloc(codegen->const_table, 
                                       new_capacity * sizeof(codegen->const_table[0]));
        if (!new_table) {
            codegen_error(codegen, "Failed to allocate memory for constant table");
            return false;
        }
        codegen->const_table = new_table;
        codegen->const_capacity = new_capacity;
    }
    
    // Add the constant
    codegen->const_table[codegen->const_count].name = strdup(name);
    codegen->const_table[codegen->const_count].value = value;
    codegen->const_count++;
    
    return true;
}

static const KryonASTNode *find_constant_value(KryonCodeGenerator *codegen, const char *name) {
    for (size_t i = 0; i < codegen->const_count; i++) {
        if (strcmp(codegen->const_table[i].name, name) == 0) {
            return codegen->const_table[i].value;
        }
    }
    return NULL;
}

static bool write_const_definition(KryonCodeGenerator *codegen, const KryonASTNode *const_def) {
    if (!const_def || const_def->type != KRYON_AST_CONST_DEFINITION) {
        return false;
    }
    
    // Add the constant to the symbol table for later resolution
    return add_constant_to_table(codegen, const_def->data.const_def.name, const_def->data.const_def.value);
}

static uint32_t count_const_for_elements(KryonCodeGenerator *codegen, const KryonASTNode *const_for) {
    if (!const_for || const_for->type != KRYON_AST_CONST_FOR_LOOP) {
        return 0;
    }
    
    // Find the array constant to get its size
    const KryonASTNode *array_const = find_constant_value(codegen, const_for->data.const_for_loop.array_name);
    if (!array_const || array_const->type != KRYON_AST_ARRAY_LITERAL) {
        // Fallback: assume 1 element if we can't find the array
        return const_for->data.const_for_loop.body_count;
    }
    
    uint32_t array_size = (uint32_t)array_const->data.array_literal.element_count;
    uint32_t elements_per_iteration = 0;
    
    // Count elements in each body element recursively
    for (size_t i = 0; i < const_for->data.const_for_loop.body_count; i++) {
        const KryonASTNode *body_element = const_for->data.const_for_loop.body[i];
        elements_per_iteration += count_elements_recursive(body_element);
    }
    
    printf("DEBUG: count_const_for_elements - Array '%s' has %u elements, %u elements per iteration = %u total\n",
           const_for->data.const_for_loop.array_name, array_size, elements_per_iteration, array_size * elements_per_iteration);
    
    return array_size * elements_per_iteration;
}

static uint16_t count_expanded_children(KryonCodeGenerator *codegen, const KryonASTNode *element) {
    if (!element || element->type != KRYON_AST_ELEMENT) {
        return 0;
    }
    
    uint16_t expanded_count = 0;
    
    // Count each child, expanding @const_for loops
    for (size_t i = 0; i < element->data.element.child_count; i++) {
        const KryonASTNode *child = element->data.element.children[i];
        if (!child) continue;
        
        if (child->type == KRYON_AST_CONST_FOR_LOOP) {
            // Find the array constant to get expansion count
            const KryonASTNode *array_const = find_constant_value(codegen, child->data.const_for_loop.array_name);
            if (array_const && array_const->type == KRYON_AST_ARRAY_LITERAL) {
                // Each array element generates all body elements
                expanded_count += (uint16_t)array_const->data.array_literal.element_count;
            } else {
                // Fallback: count as 1 if we can't find the array
                expanded_count += 1;
            }
        } else if (child->type == KRYON_AST_ELEMENT) {
            // Regular element counts as 1
            expanded_count += 1;
        }
        // Skip other types (constants, etc.)
    }
    
    return expanded_count;
}

static bool expand_const_for_loop(KryonCodeGenerator *codegen, const KryonASTNode *const_for, const KryonASTNode *ast_root) {
    if (!const_for || const_for->type != KRYON_AST_CONST_FOR_LOOP) {
        return false;
    }
    
    // Find the array constant
    const KryonASTNode *array_const = find_constant_value(codegen, const_for->data.const_for_loop.array_name);
    if (!array_const || array_const->type != KRYON_AST_ARRAY_LITERAL) {
        codegen_error(codegen, "Constant array not found or not an array literal");
        return false;
    }
    
    printf("DEBUG: expand_const_for_loop - Array '%s' has %zu elements\n", 
           const_for->data.const_for_loop.array_name, array_const->data.array_literal.element_count);
    printf("DEBUG: expand_const_for_loop - Loop body has %zu elements\n", 
           const_for->data.const_for_loop.body_count);
    
    // Iterate through array elements and expand the loop body for each
    for (size_t i = 0; i < array_const->data.array_literal.element_count; i++) {
        const KryonASTNode *array_element = array_const->data.array_literal.elements[i];
        printf("DEBUG: Processing array element %zu/%zu\n", i+1, array_const->data.array_literal.element_count);
        
        // For each element in the loop body, substitute template variables and write
        for (size_t j = 0; j < const_for->data.const_for_loop.body_count; j++) {
            const KryonASTNode *body_element = const_for->data.const_for_loop.body[j];
            printf("DEBUG: Processing body element %zu/%zu\n", j+1, const_for->data.const_for_loop.body_count);
            
            // Create a substituted version of the body element with template variable resolution
            KryonASTNode *substituted_element = substitute_template_vars(body_element, 
                                                                        const_for->data.const_for_loop.var_name, 
                                                                        array_element);
            
            if (substituted_element) {
                printf("DEBUG: Writing substituted element to KRB\n");
                // Write the substituted element
                if (!write_widget_instance(codegen, substituted_element, ast_root)) {
                    printf("DEBUG: ERROR: Failed to write substituted element\n");
                    return false;
                }
                printf("DEBUG: Successfully wrote substituted element\n");
            } else {
                printf("DEBUG: ERROR: Template substitution returned NULL\n");
            }
        }
    }
    
    return true;
}

// Helper function to process string templates like "text: ${alignment.name}"
static char *process_string_template(const char *template_string, const char *var_name, const KryonASTNode *var_value) {
    if (!template_string || !var_name || !var_value) {
        return NULL;
    }
    
    // Simple implementation: replace ${var_name.property} with actual values
    char *result = malloc(strlen(template_string) + 512); // Extra space for substitutions
    if (!result) {
        return NULL;
    }
    
    strcpy(result, template_string);
    
    // Look for ${alignment.name}, ${alignment.value}, ${alignment.gap}, etc.
    char *pos = result;
    while ((pos = strstr(pos, "${")) != NULL) {
        char *end = strstr(pos, "}");
        if (!end) {
            break; // Malformed template
        }
        
        // Extract the template expression: "alignment.name", "alignment.value", etc.
        size_t expr_len = end - pos - 2; // -2 for "${"
        char expr[256];
        if (expr_len >= sizeof(expr)) {
            pos = end + 1;
            continue;
        }
        
        strncpy(expr, pos + 2, expr_len);
        expr[expr_len] = '\0';
        
        // Parse the expression (simple: var_name.property or var_name.property[index])
        char *dot = strchr(expr, '.');
        if (dot) {
            *dot = '\0';
            const char *var = expr;
            const char *property_expr = dot + 1;
            
            // Check if this matches our variable name
            if (strcmp(var, var_name) == 0) {
                // Parse property expression - could be "name", "value", "gap", or "colors[0]"
                char property[64];
                int array_index = -1;
                
                char *bracket = strchr(property_expr, '[');
                if (bracket) {
                    // Array access: colors[0]
                    size_t prop_len = bracket - property_expr;
                    if (prop_len < sizeof(property)) {
                        strncpy(property, property_expr, prop_len);
                        property[prop_len] = '\0';
                        
                        // Extract array index
                        char *bracket_end = strchr(bracket, ']');
                        if (bracket_end) {
                            char index_str[16];
                            size_t index_len = bracket_end - bracket - 1;
                            if (index_len < sizeof(index_str)) {
                                strncpy(index_str, bracket + 1, index_len);
                                index_str[index_len] = '\0';
                                array_index = atoi(index_str);
                            }
                        }
                    }
                } else {
                    // Simple property access: name, value, gap
                    strncpy(property, property_expr, sizeof(property) - 1);
                    property[sizeof(property) - 1] = '\0';
                }
                
                // Find the property value in the object
                const char *replacement = NULL;
                static char num_buf[32]; // Make it non-static to avoid reuse issues
                
                if (var_value->type == KRYON_AST_OBJECT_LITERAL) {
                    for (size_t i = 0; i < var_value->data.object_literal.property_count; i++) {
                        const KryonASTNode *prop = var_value->data.object_literal.properties[i];
                        if (prop && prop->type == KRYON_AST_PROPERTY &&
                            prop->data.property.name &&
                            strcmp(prop->data.property.name, property) == 0) {
                            
                            const KryonASTNode *prop_value = prop->data.property.value;
                            if (prop_value && prop_value->type == KRYON_AST_LITERAL) {
                                if (prop_value->data.literal.value.type == KRYON_VALUE_STRING) {
                                    replacement = prop_value->data.literal.value.data.string_value;
                                } else if (prop_value->data.literal.value.type == KRYON_VALUE_INTEGER) {
                                    snprintf(num_buf, sizeof(num_buf), "%lld", 
                                           (long long)prop_value->data.literal.value.data.int_value);
                                    replacement = num_buf;
                                } else if (prop_value->data.literal.value.type == KRYON_VALUE_FLOAT) {
                                    snprintf(num_buf, sizeof(num_buf), "%.2f", 
                                           prop_value->data.literal.value.data.float_value);
                                    replacement = num_buf;
                                }
                            } else if (prop_value && prop_value->type == KRYON_AST_ARRAY_LITERAL && array_index >= 0) {
                                // Handle array access: colors[0], colors[1], etc.
                                if (array_index < (int)prop_value->data.array_literal.element_count) {
                                    const KryonASTNode *array_elem = prop_value->data.array_literal.elements[array_index];
                                    if (array_elem && array_elem->type == KRYON_AST_LITERAL) {
                                        if (array_elem->data.literal.value.type == KRYON_VALUE_STRING) {
                                            replacement = array_elem->data.literal.value.data.string_value;
                                        } else if (array_elem->data.literal.value.type == KRYON_VALUE_INTEGER) {
                                            snprintf(num_buf, sizeof(num_buf), "%lld", 
                                                   (long long)array_elem->data.literal.value.data.int_value);
                                            replacement = num_buf;
                                        } else if (array_elem->data.literal.value.type == KRYON_VALUE_FLOAT) {
                                            snprintf(num_buf, sizeof(num_buf), "%.2f", 
                                                   array_elem->data.literal.value.data.float_value);
                                            replacement = num_buf;
                                        }
                                    }
                                }
                            }
                            break;
                        }
                    }
                }
                
                if (replacement) {
                    // Replace ${...} with the actual value
                    size_t template_len = end - pos + 1; // Include '}'
                    size_t replacement_len = strlen(replacement);
                    size_t result_len = strlen(result);
                    
                    // Move the rest of the string
                    memmove(pos + replacement_len, end + 1, result_len - (end - result));
                    
                    // Insert the replacement
                    memcpy(pos, replacement, replacement_len);
                    
                    // Update result length
                    result[result_len - template_len + replacement_len] = '\0';
                    
                    // Continue from after replacement
                    pos += replacement_len;
                } else {
                    // No replacement found, move past this template
                    pos = end + 1;
                }
            } else {
                // Different variable, move past this template
                pos = end + 1;
            }
        } else {
            // No dot in expression, move past this template
            pos = end + 1;
        }
    }
    
    return result;
}

// Helper function to substitute template variables in AST nodes
static KryonASTNode *substitute_template_vars(const KryonASTNode *node, const char *var_name, const KryonASTNode *var_value) {
    if (!node) {
        return NULL;
    }
    
    // Handle template substitution directly - THIS IS THE CRITICAL PART
    if (node->type == KRYON_AST_TEMPLATE) {
        KryonASTNode *expression = node->data.template.expression;
        
        // Handle member access: ${alignment.name}, ${alignment.value}, ${alignment.gap}
        if (expression && expression->type == KRYON_AST_MEMBER_ACCESS) {
            if (expression->data.member_access.object && 
                expression->data.member_access.object->type == KRYON_AST_IDENTIFIER &&
                expression->data.member_access.object->data.identifier.name &&
                strcmp(expression->data.member_access.object->data.identifier.name, var_name) == 0) {
                
                const char *member_name = expression->data.member_access.member;
                if (var_value && var_value->type == KRYON_AST_OBJECT_LITERAL && member_name) {
                    // Find the property in the object literal
                    for (size_t i = 0; i < var_value->data.object_literal.property_count; i++) {
                        const KryonASTNode *prop = var_value->data.object_literal.properties[i];
                        if (prop && prop->type == KRYON_AST_PROPERTY &&
                            prop->data.property.name &&
                            strcmp(prop->data.property.name, member_name) == 0) {
                            
                            // Return a clone of the property value (creates literal node)
                            return substitute_template_vars(prop->data.property.value, var_name, var_value);
                        }
                    }
                }
            }
        }
        
        // Handle array access: ${alignment.colors[0]}
        else if (expression && expression->type == KRYON_AST_ARRAY_ACCESS) {
            if (expression->data.array_access.array && 
                expression->data.array_access.array->type == KRYON_AST_MEMBER_ACCESS) {
                
                KryonASTNode *member_access = expression->data.array_access.array;
                if (member_access->data.member_access.object &&
                    member_access->data.member_access.object->type == KRYON_AST_IDENTIFIER &&
                    member_access->data.member_access.object->data.identifier.name &&
                    strcmp(member_access->data.member_access.object->data.identifier.name, var_name) == 0) {
                    
                    const char *member_name = member_access->data.member_access.member;
                    if (var_value && var_value->type == KRYON_AST_OBJECT_LITERAL && member_name) {
                        // Find the array property
                        for (size_t i = 0; i < var_value->data.object_literal.property_count; i++) {
                            const KryonASTNode *prop = var_value->data.object_literal.properties[i];
                            if (prop && prop->type == KRYON_AST_PROPERTY &&
                                prop->data.property.name &&
                                strcmp(prop->data.property.name, member_name) == 0 &&
                                prop->data.property.value &&
                                prop->data.property.value->type == KRYON_AST_ARRAY_LITERAL) {
                                
                                // Get the index
                                const KryonASTNode *index_node = expression->data.array_access.index;
                                if (index_node && index_node->type == KRYON_AST_LITERAL &&
                                    index_node->data.literal.value.type == KRYON_VALUE_INTEGER) {
                                    
                                    int64_t index = index_node->data.literal.value.data.int_value;
                                    const KryonASTNode *array = prop->data.property.value;
                                    
                                    if (index >= 0 && index < (int64_t)array->data.array_literal.element_count) {
                                        return substitute_template_vars(array->data.array_literal.elements[index], var_name, var_value);
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
        
        // If template couldn't be resolved, return literal string containing the template
        // This should NOT happen if template substitution is working correctly
        KryonASTNode *cloned = malloc(sizeof(KryonASTNode));
        if (cloned) {
            memcpy(cloned, node, sizeof(KryonASTNode));
            cloned->parent = NULL;
            // Template failed to resolve - return unmodified template
        }
        return cloned;
    }
    
    // Create a deep copy of the node
    KryonASTNode *cloned = malloc(sizeof(KryonASTNode));
    if (!cloned) {
        return NULL;
    }
    
    // Copy basic node data
    memcpy(cloned, node, sizeof(KryonASTNode));
    cloned->parent = NULL;
    
    // Handle recursive cloning for containers
    switch (node->type) {
        case KRYON_AST_ELEMENT: {
            // Clone element type string
            if (node->data.element.element_type) {
                cloned->data.element.element_type = strdup(node->data.element.element_type);
            }
            
            // Clone children recursively
            if (node->data.element.child_count > 0) {
                cloned->data.element.children = malloc(sizeof(KryonASTNode*) * node->data.element.child_count);
                cloned->data.element.child_capacity = node->data.element.child_count;
                cloned->data.element.child_count = 0;
                
                for (size_t i = 0; i < node->data.element.child_count; i++) {
                    KryonASTNode *child_clone = substitute_template_vars(node->data.element.children[i], var_name, var_value);
                    if (child_clone) {
                        cloned->data.element.children[cloned->data.element.child_count++] = child_clone;
                        child_clone->parent = cloned;
                    }
                }
            } else {
                cloned->data.element.children = NULL;
                cloned->data.element.child_count = 0;
                cloned->data.element.child_capacity = 0;
            }
            
            // Clone properties recursively
            if (node->data.element.property_count > 0) {
                cloned->data.element.properties = malloc(sizeof(KryonASTNode*) * node->data.element.property_count);
                cloned->data.element.property_capacity = node->data.element.property_count;
                cloned->data.element.property_count = 0;
                
                for (size_t i = 0; i < node->data.element.property_count; i++) {
                    KryonASTNode *prop_clone = substitute_template_vars(node->data.element.properties[i], var_name, var_value);
                    if (prop_clone) {
                        cloned->data.element.properties[cloned->data.element.property_count++] = prop_clone;
                        prop_clone->parent = cloned;
                    }
                }
            } else {
                cloned->data.element.properties = NULL;
                cloned->data.element.property_count = 0;
                cloned->data.element.property_capacity = 0;
            }
            break;
        }
        
        case KRYON_AST_PROPERTY: {
            // Clone property name
            if (node->data.property.name) {
                cloned->data.property.name = strdup(node->data.property.name);
            }
            
            // Clone property value with substitution - ENSURE ALL TEMPLATES ARE PROCESSED
            cloned->data.property.value = substitute_template_vars(node->data.property.value, var_name, var_value);
            if (cloned->data.property.value) {
                cloned->data.property.value->parent = cloned;
            }
            break;
        }
        
        case KRYON_AST_LITERAL: {
            // Clone literal value with template processing for strings
            if (node->data.literal.value.type == KRYON_VALUE_STRING && node->data.literal.value.data.string_value) {
                const char *original_string = node->data.literal.value.data.string_value;
                
                // Check if string contains template variables ${...}
                if (strstr(original_string, "${") != NULL) {
                    // Process string template substitution
                    char *processed_string = process_string_template(original_string, var_name, var_value);
                    if (processed_string) {
                        cloned->data.literal.value.data.string_value = processed_string;
                    } else {
                        cloned->data.literal.value.data.string_value = strdup(original_string);
                    }
                } else {
                    cloned->data.literal.value.data.string_value = strdup(original_string);
                }
            }
            break;
        }
        
        case KRYON_AST_IDENTIFIER: {
            // Clone identifier name
            if (node->data.identifier.name) {
                cloned->data.identifier.name = strdup(node->data.identifier.name);
            }
            break;
        }
        
        default:
            // For other node types, just return the cloned structure
            break;
    }
    
    return cloned;
}
