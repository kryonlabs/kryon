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

#include "codegen.h"
#include "memory.h"
#include "krb_format.h"
#include "binary_io.h"
#include "color_utils.h"    
#include "script_vm.h"
#include "../../shared/kryon_mappings.h"
#include "../../shared/krb_schema.h"
#include "string_table.h"
#include "ast_expander.h"
#include "directive_serializer.h"
#include "element_serializer.h"
#include "event_serializer.h"
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
static bool write_style_node(KryonCodeGenerator *codegen, const KryonASTNode *style);
static bool write_theme_node(KryonCodeGenerator *codegen, const KryonASTNode *theme);
static bool write_metadata_node(KryonCodeGenerator *codegen, const KryonASTNode *metadata);
static bool write_property_value_node(KryonCodeGenerator *codegen, const KryonASTValue *value, uint16_t property_hex);
static bool write_complex_krb_format(KryonCodeGenerator *codegen, const KryonASTNode *ast_root);
static bool write_style_definition(KryonCodeGenerator *codegen, const KryonASTNode *style);
static bool write_const_definition(KryonCodeGenerator *codegen, const KryonASTNode *const_def);

static bool expand_binary_buffer(KryonCodeGenerator *codegen, size_t additional_size);
static uint32_t count_elements_recursive(const KryonASTNode *node);
static uint32_t count_properties_recursive(const KryonASTNode *node);
static uint16_t get_or_create_custom_element_hex(const char *element_name);

static bool update_header_uint32(KryonCodeGenerator *codegen, size_t offset, uint32_t value);


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
    
    // Zero-initialize the buffer to prevent garbage data
    memset(codegen->binary_data, 0, codegen->binary_capacity);
    
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

// kryon_write_string_table moved to string_table.c

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
    
    // Process root children
    if (ast_root->type == KRYON_AST_ROOT) {
        for (size_t i = 0; i < ast_root->data.element.child_count; i++) {
            const KryonASTNode *child = ast_root->data.element.children[i];
            if (child && (child->type == KRYON_AST_ELEMENT || child->type == KRYON_AST_EVENT_DIRECTIVE || child->type == KRYON_AST_FOR_DIRECTIVE)) {
                // Check if this is a custom component instance
                if (child->type == KRYON_AST_ELEMENT) {
                    uint16_t element_hex = kryon_codegen_get_element_hex(child->data.element.element_type);
                    if (element_hex >= 0x2000) {
                        // This is a custom component - expand it
                        printf("🔧 Expanding custom component: %s\n", child->data.element.element_type);
                        KryonASTNode *expanded = expand_component_instance(codegen, child, ast_root);
                        if (expanded) {
                            if (!kryon_write_element_node(codegen, expanded, ast_root)) {
                                return false;
                            }
                            // TODO: Free expanded node memory
                        } else {
                            printf("❌ Failed to expand component: %s\n", child->data.element.element_type);
                            return false;
                        }
                        continue;
                    }
                }
                
                // Standard element - write normally
                if (!kryon_write_element_node(codegen, child, ast_root)) {
                    return false;
                }
            } else if (child && child->type == KRYON_AST_CONST_FOR_LOOP) {
                // Expand const_for loops inline
                if (kryon_expand_const_for_loop(codegen, child, ast_root)) {
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
    
    // Update element count - use actual count from next_element_id (minus 1 since it starts at 1)
    uint32_t actual_element_count = codegen->next_element_id - 1;
    printf("🔧 DEBUG: Setting element count to %u (next_element_id=%u) at offset %zu\n", 
           actual_element_count, codegen->next_element_id, element_count_offset);
    if (!update_header_uint32(codegen, element_count_offset, actual_element_count)) {
        codegen_error(codegen, "Failed to write element count to header");
        return false;
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

bool write_value_node(KryonCodeGenerator *codegen, const KryonASTValue *value) {
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
    uint16_t hex = kryon_get_element_hex(element_name);
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

// add_string_to_table moved to string_table.c

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
    
    // Zero-initialize the newly allocated memory
    if (new_capacity > codegen->binary_capacity) {
        memset(new_buffer + codegen->binary_capacity, 0, new_capacity - codegen->binary_capacity);
    }
    
    codegen->binary_data = new_buffer;
    codegen->binary_capacity = new_capacity;
    
    return true;
}

// Helper functions using centralized binary_io
bool write_uint8(KryonCodeGenerator *codegen, uint8_t value) {
    if (!expand_binary_buffer(codegen, sizeof(uint8_t))) return false;
    size_t offset = codegen->binary_size;
    if (!kryon_write_uint8(&codegen->binary_data, &offset, codegen->binary_capacity, value)) return false;
    codegen->binary_size = offset;
    codegen->current_offset = offset;
    return true;
}

bool write_uint16(KryonCodeGenerator *codegen, uint16_t value) {
    if (!expand_binary_buffer(codegen, sizeof(uint16_t))) return false;
    size_t offset = codegen->binary_size;
    
    // Debug: Track writes near the corruption area (offset 2910 and 3008-3010)
    if ((offset >= 2905 && offset <= 2915) || (offset >= 3005 && offset <= 3015)) {
        printf("🔍 WRITE_DEBUG: write_uint16(0x%04x) at offset %zu (0x%zx)\n", 
               value, offset, offset);
    }
    
    if (!kryon_write_uint16(&codegen->binary_data, &offset, codegen->binary_capacity, value)) return false;
    codegen->binary_size = offset;
    codegen->current_offset = offset;
    return true;
}

bool write_uint32(KryonCodeGenerator *codegen, uint32_t value) {
    if (!expand_binary_buffer(codegen, sizeof(uint32_t))) return false;
    size_t offset = codegen->binary_size;
    
    // Debug: Track writes near the corruption area (offset 2910 and 3008-3010)  
    if ((offset >= 2905 && offset <= 2915) || (offset >= 3005 && offset <= 3015)) {
        printf("🔍 WRITE_DEBUG: write_uint32(0x%08x) at offset %zu (0x%zx)\n", 
               value, offset, offset);
        // Special debug for VARS magic
        if (value == 0x56415253) {
            printf("🔍 WRITING VARS MAGIC at offset %zu\n", offset);
        }
    }
    
    if (!kryon_write_uint32(&codegen->binary_data, &offset, codegen->binary_capacity, value)) return false;
    codegen->binary_size = offset;
    codegen->current_offset = offset;
    return true;
}

bool write_binary_data(KryonCodeGenerator *codegen, const void *data, size_t size) {
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

void codegen_error(KryonCodeGenerator *codegen, const char *message) {
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
    uint32_t element_def_count = 0; // For future element definitions
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
                        element_count += kryon_count_const_for_elements(codegen, child);
                        break;
                    case KRYON_AST_FOR_DIRECTIVE:
                        // For directives will be written to KRB for runtime expansion
                        printf("DEBUG: Found @for directive in counting phase\n");
                        element_count += count_elements_recursive(child);
                        total_properties += child->data.element.property_count;
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
    
    // 0x14-0x17: Element Definition Count
    if (!write_uint32(codegen, element_def_count)) return false;
    
    // 0x18-0x1B: Element Instance Count (placeholder, will be updated later)
    size_t element_count_offset = codegen->current_offset;
    if (!write_uint32(codegen, 0)) return false;
    
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
    
    // 0x39-0x3C: Element Definition Offset (placeholder)
    size_t element_def_offset_pos = codegen->current_offset;
    if (!write_uint32(codegen, 0)) return false;
    
    // 0x3D-0x40: Element Instance Offset (placeholder)
    size_t element_inst_offset_pos = codegen->current_offset;
    if (!write_uint32(codegen, 0)) return false;
    
    // 0x41-0x44: Script Offset (placeholder)
    size_t script_offset_pos = codegen->current_offset;
    if (!write_uint32(codegen, 0)) return false;
    
    // 0x45-0x48: Resource Offset (repurposed as string table offset)
    size_t string_table_offset_pos = codegen->current_offset;
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
                    
                    // Pre-add function metadata strings to string table (but not bytecode)
                    add_string_to_table(codegen, child->data.function_def.language);  // "lua"
                    add_string_to_table(codegen, child->data.function_def.name);     // function name
                    // Bytecode compilation moved to directive_serializer.c to avoid duplication
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
    
    // String table writing moved to end of compilation process
    
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
    
    // Write Element Definition Table (empty for now - using built-in elements)
    size_t element_def_section_start = codegen->current_offset;
    if (element_def_count > 0) {
        if (!update_header_uint32(codegen, element_def_offset_pos, (uint32_t)element_def_section_start)) {
            codegen_error(codegen, "Failed to write element definition offset to header");
            return false;
        }
        // TODO: Implement element definition writing
    }
    
    // Constants already processed at the beginning for counting
    
    // Write Element Instance Table (the actual UI elements)
    size_t element_inst_section_start = codegen->current_offset;
    if (!update_header_uint32(codegen, element_inst_offset_pos, (uint32_t)element_inst_section_start)) {
        codegen_error(codegen, "Failed to write element instance offset to header");
        return false;
    }
    
    if (ast_root->type == KRYON_AST_ROOT) {
        for (size_t i = 0; i < ast_root->data.element.child_count; i++) {
            const KryonASTNode *child = ast_root->data.element.children[i];
            if (child && child->type == KRYON_AST_ELEMENT) {
                if (!kryon_write_element_instance(codegen, child, ast_root)) {
                    codegen_error(codegen, "Failed to write element instance");
                    return false;
                }
                codegen->stats.output_elements++;
            } else if (child && child->type == KRYON_AST_CONST_FOR_LOOP) {
                // Expand const_for loops inline
                if (!kryon_expand_const_for_loop(codegen, child, ast_root)) {
                    codegen_error(codegen, "Failed to expand const_for loop");
                    return false;
                }
            }
            // Skip directive nodes - they are handled in their respective sections:
            // KRYON_AST_VARIABLE_DEFINITION -> Variables section
            // KRYON_AST_FUNCTION_DEFINITION -> Script section  
            // KRYON_AST_COMPONENT -> Component section
            // KRYON_AST_EVENT_DIRECTIVE -> Event handlers (future)
            // KRYON_AST_CONST_DEFINITION -> Already processed for constants table
        }
    } else if (ast_root->type == KRYON_AST_ELEMENT) {
        // Handle case where AST root is directly an element (e.g., App)
        if (!kryon_write_element_instance(codegen, ast_root, ast_root)) {
            codegen_error(codegen, "Failed to write root element instance");
            return false;
        }
        codegen->stats.output_elements++;
    }
    
    // Write Variables Section
    size_t var_section_start = codegen->current_offset;
    
    // Update header script_offset to point to Variables section (Variables come before Script)
    if (!update_header_uint32(codegen, script_offset_pos, (uint32_t)var_section_start)) {
        codegen_error(codegen, "Failed to write variables section offset to header");
        return false;
    }
    
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
    printf("🔍 DEBUG: About to write VARS section at offset %zu (0x%zx)\n", codegen->current_offset, codegen->current_offset);
    
    // Check if current offset is clean (should be aligned properly)
    if (codegen->current_offset >= 2) {
        printf("🔍 DEBUG: Bytes at offset %zu-2: %02x %02x\n", 
               codegen->current_offset, 
               codegen->binary_data[codegen->current_offset-2],
               codegen->binary_data[codegen->current_offset-1]);
    }
    
    printf("🔍 DEBUG: Last 16 bytes before VARS:\n");
    if (codegen->current_offset >= 16) {
        for (size_t i = codegen->current_offset - 16; i < codegen->current_offset; i++) {
            printf("%02x ", codegen->binary_data[i]);
            if ((i + 1) % 8 == 0) printf(" ");
        }
        printf("\n");
    }
    
    if (!write_uint32(codegen, 0x56415253)) { // "VARS" magic
        codegen_error(codegen, "Failed to write Variables section magic");
        return false;
    }
    
    printf("🔍 DEBUG: After writing VARS magic, current offset: %zu (0x%zx)\n", codegen->current_offset, codegen->current_offset);
    
    // Verify VARS magic was written correctly
    if (codegen->current_offset >= 4) {
        printf("🔍 DEBUG: VARS magic bytes written: %02x %02x %02x %02x\n",
               codegen->binary_data[codegen->current_offset-4],
               codegen->binary_data[codegen->current_offset-3],
               codegen->binary_data[codegen->current_offset-2],
               codegen->binary_data[codegen->current_offset-1]);
    }
    
    // Add component instance variables to the count
    size_t component_var_count = 0;
    for (size_t i = 0; i < codegen->component_instance_count; i++) {
        component_var_count += codegen->component_instances[i].variable_count;
    }
    
    uint32_t final_var_count = total_var_count + component_var_count;
    printf("📊 Writing Variables section: %u global + %zu component = %u total variables\n", 
           total_var_count, component_var_count, final_var_count);
    
    // Write variable count
    if (!write_uint32(codegen, final_var_count)) {
        codegen_error(codegen, "Failed to write variable count");
        return false;
    }
    
    // Write global variables
    if (ast_root->type == KRYON_AST_ROOT) {
        for (size_t i = 0; i < ast_root->data.element.child_count; i++) {
            const KryonASTNode *child = ast_root->data.element.children[i];
            if (child && child->type == KRYON_AST_VARIABLE_DEFINITION) {
                if (!kryon_write_variable_node(codegen, child)) {
                    codegen_error(codegen, "Failed to write variable definition");
                    return false;
                }
            }
        }
    }
    
    // Write component instance variables
    if (!write_component_instance_variables(codegen)) {
        codegen_error(codegen, "Failed to write component instance variables");
        return false;
    }
    
    // Write Script Section (functions)
    size_t script_section_start = codegen->current_offset;
    // Note: script_offset in header already points to Variables section (which comes before Script)
    
    // Count functions including those in components
    uint32_t function_count = 0;
    uint32_t component_function_count = 0;
    
    if (ast_root->type == KRYON_AST_ROOT) {
        for (size_t i = 0; i < ast_root->data.element.child_count; i++) {
            const KryonASTNode *child = ast_root->data.element.children[i];
            if (child && child->type == KRYON_AST_FUNCTION_DEFINITION) {
                function_count++;
            } else if (child && child->type == KRYON_AST_COMPONENT) {
                // Count functions in this component
                component_function_count += child->data.component.function_count;
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
    
    // Include component functions in the count
    uint32_t total_function_count = function_count + component_function_count;
    printf("DEBUG: Writing %u functions (%u root + %u component)\n", 
           total_function_count, function_count, component_function_count);
    
    // Write FUNC section header (magic + function count)
    if (!write_uint32(codegen, KRB_MAGIC_FUNC)) {
        codegen_error(codegen, "Failed to write FUNC magic");
        return false;
    }
    
    // Write total function count
    if (!write_uint32(codegen, total_function_count)) {
        codegen_error(codegen, "Failed to write function count");
        return false;
    }
    
    // Write root-level functions first
    if (ast_root->type == KRYON_AST_ROOT) {
        for (size_t i = 0; i < ast_root->data.element.child_count; i++) {
            const KryonASTNode *child = ast_root->data.element.children[i];
            if (child && child->type == KRYON_AST_FUNCTION_DEFINITION) {
                if (!kryon_write_function_node(codegen, child)) {
                    codegen_error(codegen, "Failed to write function definition");
                    return false;
                }
            }
        }
    }
    
    // Write component functions directly from component definitions
    if (ast_root->type == KRYON_AST_ROOT) {
        for (size_t i = 0; i < ast_root->data.element.child_count; i++) {
            const KryonASTNode *child = ast_root->data.element.children[i];
            if (child && child->type == KRYON_AST_COMPONENT) {
                // Write functions from this component
                for (size_t j = 0; j < child->data.component.function_count; j++) {
                    const KryonASTNode *func = child->data.component.functions[j];
                    if (func && func->type == KRYON_AST_FUNCTION_DEFINITION) {
                        printf("📝 Writing component function '%s' to script section\n", 
                               func->data.function_def.name);
                        if (!kryon_write_function_node(codegen, func)) {
                            codegen_error(codegen, "Failed to write component function definition");
                            return false;
                        }
                    }
                }
            }
        }
    }
    
    // Components will be written after all script sections are complete
    
    // Temporary test function removed - using proper directive serialization
    if (false && has_onclick) {
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
        printf("🔍 Scanning AST root for component definitions (%zu children)\n", ast_root->data.element.child_count);
        for (size_t i = 0; i < ast_root->data.element.child_count; i++) {
            const KryonASTNode *child = ast_root->data.element.children[i];
            if (child) {
                printf("    Child %zu: type=%d\n", i, child->type);
                if (child->type == KRYON_AST_COMPONENT) {
                    printf("📝 Found component definition to write: '%s'\n", child->data.component.name ? child->data.component.name : "unnamed");
                    if (!kryon_write_component_node(codegen, child)) {
                        codegen_error(codegen, "Failed to write component definition");
                        return false;
                    }
                }
            }
        }
    }
    
    // Write Event Handlers section (Phase 4)
    if (!kryon_write_event_section(codegen, ast_root)) {
        codegen_error(codegen, "Failed to write event handlers section");
        return false;
    }
    
    // Update data size in header
    size_t total_data_size = codegen->current_offset;
    if (!update_header_uint32(codegen, data_size_offset, (uint32_t)total_data_size)) {
        codegen_error(codegen, "Failed to write data size to header");
        return false;
    }
    
    // Write String Table at end (after all strings have been added)
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
    
    // Update string table offset in header to point to actual location
    if (!update_header_uint32(codegen, string_table_offset_pos, (uint32_t)string_table_start)) {
        codegen_error(codegen, "Failed to write string table offset to header");
        return false;
    }
    
    // Update element count with actual count (next_element_id - 1 since it starts at 1)
    uint32_t actual_element_count = codegen->next_element_id - 1;
    printf("🔧 DEBUG: Updating element count to %u (next_element_id=%u)\n", 
           actual_element_count, codegen->next_element_id);
    if (!update_header_uint32(codegen, element_count_offset, actual_element_count)) {
        codegen_error(codegen, "Failed to write element count to header");
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
        uint32_t iteration_count = 0;
        
        if (node->data.const_for_loop.is_range) {
            // Handle range-based loop
            int start = node->data.const_for_loop.range_start;
            int end = node->data.const_for_loop.range_end;
            iteration_count = (uint32_t)(end - start + 1);
            
            printf("DEBUG: count_elements_recursive found range const_for - Range %d..%d has %u iterations\n",
                   start, end, iteration_count);
        } else {
            // Handle array-based loop
            // Hardcode for alignments array (6 elements) - TODO: fix properly with codegen context
            if (node->data.const_for_loop.array_name && 
                strcmp(node->data.const_for_loop.array_name, "alignments") == 0) {
                iteration_count = 6;
            } else {
                // Fallback for unknown arrays
                iteration_count = 1;
            }
            
            printf("DEBUG: count_elements_recursive found array const_for - Array '%s' has %u elements\n",
                   node->data.const_for_loop.array_name ? node->data.const_for_loop.array_name : "NULL", 
                   iteration_count);
        }
        
        uint32_t elements_per_iteration = 0;
        
        // Count elements in each body element recursively
        for (size_t i = 0; i < node->data.const_for_loop.body_count; i++) {
            const KryonASTNode *body_element = node->data.const_for_loop.body[i];
            elements_per_iteration += count_elements_recursive(body_element);
        }
        
        uint32_t total = iteration_count * elements_per_iteration;
        printf("DEBUG: count_elements_recursive const_for total: %u iterations * %u elements per iteration = %u total\n",
               iteration_count, elements_per_iteration, total);
        
        return total;
    } else if (node->type == KRYON_AST_FOR_DIRECTIVE) {
        // Count @for directive itself as 1 element plus its children
        uint32_t count = 1; // Count the @for directive element
        
        // Count child elements recursively (the template elements to be repeated)
        for (size_t i = 0; i < node->data.element.child_count; i++) {
            if (node->data.element.children[i]) {
                count += count_elements_recursive(node->data.element.children[i]);
            }
        }
        
        printf("DEBUG: count_elements_recursive found @for directive with %zu children, total count: %u\n",
               node->data.element.child_count, count);
        
        return count;
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
    
    printf("🔍 Searching for component '%s' in AST with %zu children\n", 
           component_name, ast_root->data.element.child_count);
    
    // Search through all children in the AST root
    for (size_t i = 0; i < ast_root->data.element.child_count; i++) {
        const KryonASTNode *child = ast_root->data.element.children[i];
        printf("🔍 Child %zu: type=%d", i, child ? child->type : -1);
        if (child && child->type == KRYON_AST_COMPONENT) {
            printf(" (COMPONENT: name='%s')", child->data.component.name ? child->data.component.name : "NULL");
            if (child->data.component.name && strcmp(child->data.component.name, component_name) == 0) {
                printf(" ✅ MATCH!\n");
                return (KryonASTNode*)child; // Found the component definition
            }
        }
        printf("\n");
    }
    
    printf("❌ Component '%s' not found in AST\n", component_name);
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
static KryonASTNode *clone_and_substitute_node(const KryonASTNode *original, const ParamContext *params, const char *instance_id);
static KryonASTNode *create_minimal_ast_node(KryonASTNodeType type);

static KryonASTNode *clone_ast_node(const KryonASTNode *original) {
    if (!original) return NULL;
    
    // For component nodes, return the body for expansion
    if (original->type == KRYON_AST_COMPONENT && original->data.component.body) {
        // Clone without substitution for now
        return clone_and_substitute_node(original->data.component.body, NULL, NULL);
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
static KryonASTNode *clone_and_substitute_node(const KryonASTNode *original, const ParamContext *params, const char *instance_id) {
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
                KryonASTNode *prop_clone = clone_and_substitute_node(original->data.element.properties[i], params, instance_id);
                if (prop_clone) {
                    add_property_to_node(cloned, prop_clone);
                }
            }
            
            // Clone children
            for (size_t i = 0; i < original->data.element.child_count; i++) {
                KryonASTNode *child_clone = clone_and_substitute_node(original->data.element.children[i], params, instance_id);
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
                cloned->data.property.value = clone_and_substitute_node(original->data.property.value, params, instance_id);
            }
            break;
            
        case KRYON_AST_IDENTIFIER:
            // Check if this identifier is a parameter that needs substitution
            if (params && original->data.identifier.name) {
                printf("🔍 DEBUG: Checking identifier '%s' for substitution (params has %zu entries)\n", 
                       original->data.identifier.name, params->count);
                const KryonASTNode *param_value = find_param_value(params, original->data.identifier.name);
                if (param_value) {
                    printf("✅ Found parameter '%s' for substitution\n", original->data.identifier.name);
                    
                    // For reactive components: create VARIABLE node that references component instance variable
                    if (instance_id) {
                        printf("🔗 Creating reactive variable reference: %s.%s\n", instance_id, original->data.identifier.name);
                        
                        // Create a VARIABLE AST node instead of substituting with literal value
                        free(cloned);
                        cloned = create_minimal_ast_node(KRYON_AST_VARIABLE);
                        if (!cloned) return NULL;
                        
                        // Set variable name to "comp_N.parameter_name"
                        char *var_name = malloc(64);
                        if (var_name) {
                            snprintf(var_name, 64, "%s.%s", instance_id, original->data.identifier.name);
                            cloned->data.variable.name = var_name;
                            printf("🔗 Created VARIABLE node: '%s'\n", var_name);
                        }
                        return cloned;
                    } else {
                        // Non-reactive context: substitute with literal value (existing behavior)
                        printf("📝 Substituting identifier '%s' with literal value (non-reactive)\n", original->data.identifier.name);
                        free(cloned);
                        return clone_and_substitute_node(param_value, NULL, NULL); // Clone the substituted value
                    }
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
                    
                    // IMPORTANT: Component parameters are REACTIVE variables, not constants!
                    // They must maintain variable references for runtime updates, not be substituted
                    // at compile time. Return a variable reference for runtime resolution.
                    
                    printf("✅ Found template variable '$%s' - this is a REACTIVE parameter\n", 
                           original->data.variable.name);
                    
                    // Don't substitute - keep as variable reference for reactive updates
                    cloned->data.variable.name = strdup(original->data.variable.name);
                    printf("    Created reactive variable reference for '$%s'\n", original->data.variable.name);
                    return cloned;
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
                cloned->data.variable_def.value = clone_and_substitute_node(original->data.variable_def.value, params, instance_id);
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

KryonASTNode *expand_component_instance(KryonCodeGenerator *codegen, const KryonASTNode *component_instance, const KryonASTNode *ast_root) {
    if (!component_instance || !ast_root) {
        printf("❌ Invalid parameters for component expansion\n");
        return NULL;
    }
    
    // Pre-calculate the instance ID for reactive variable references
    char *instance_id = NULL;
    if (codegen) {
        instance_id = malloc(32);
        if (instance_id) {
            snprintf(instance_id, 32, "comp_%u", codegen->next_component_instance_id);
            printf("🔧 Pre-calculated instance ID: '%s'\n", instance_id);
        }
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
    
    // Prepare arrays for component instance state tracking
    char **variable_names = NULL;
    char **variable_values = NULL;
    size_t total_variables = params.count + component_def->data.component.state_count;
    size_t variable_index = 0;
    
    if (total_variables > 0) {
        variable_names = malloc(total_variables * sizeof(char*));
        variable_values = malloc(total_variables * sizeof(char*));
    }
    
    // Auto-create state variables from component parameters
    for (size_t i = 0; i < params.count; i++) {
        KryonASTNode *state_def = create_minimal_ast_node(KRYON_AST_STATE_DEFINITION);
        if (state_def) {
            state_def->data.variable_def.name = strdup(params.substitutions[i].param_name);
            // Clone the parameter value as the initial state value
            state_def->data.variable_def.value = clone_and_substitute_node(params.substitutions[i].param_value, NULL, NULL);
            add_child_to_node(wrapper, state_def);
            printf("✅ Created state variable '%s' from parameter\n", params.substitutions[i].param_name);
            
            // Capture variable for component instance tracking
            if (variable_names && variable_values && variable_index < total_variables) {
                variable_names[variable_index] = strdup(params.substitutions[i].param_name);
                
                // Convert parameter value to string for storage
                if (params.substitutions[i].param_value && params.substitutions[i].param_value->type == KRYON_AST_LITERAL) {
                    const KryonASTValue *value = &params.substitutions[i].param_value->data.literal.value;
                    char value_str[64];
                    switch (value->type) {
                        case KRYON_VALUE_INTEGER:
                            snprintf(value_str, sizeof(value_str), "%lld", (long long)value->data.int_value);
                            break;
                        case KRYON_VALUE_FLOAT:
                            snprintf(value_str, sizeof(value_str), "%f", value->data.float_value);
                            break;
                        case KRYON_VALUE_STRING:
                            snprintf(value_str, sizeof(value_str), "%s", value->data.string_value ? value->data.string_value : "");
                            break;
                        case KRYON_VALUE_BOOLEAN:
                            snprintf(value_str, sizeof(value_str), "%s", value->data.bool_value ? "true" : "false");
                            break;
                        default:
                            snprintf(value_str, sizeof(value_str), "undefined");
                            break;
                    }
                    variable_values[variable_index] = strdup(value_str);
                } else {
                    variable_values[variable_index] = strdup("undefined");
                }
                variable_index++;
            }
        }
    }
    
    // Clone component state variables
    for (size_t i = 0; i < component_def->data.component.state_count; i++) {
        KryonASTNode *state_clone = clone_and_substitute_node(component_def->data.component.state_vars[i], &params, instance_id);
        if (state_clone) {
            add_child_to_node(wrapper, state_clone);
            
            // Capture variable for component instance tracking
            if (variable_names && variable_values && variable_index < total_variables && 
                component_def->data.component.state_vars[i] && component_def->data.component.state_vars[i]->type == KRYON_AST_STATE_DEFINITION) {
                
                const char *var_name = component_def->data.component.state_vars[i]->data.variable_def.name;
                if (var_name) {
                    variable_names[variable_index] = strdup(var_name);
                    
                    // Get default value (for now, just mark as state variable)
                    variable_values[variable_index] = strdup("@state");
                    variable_index++;
                }
            }
        }
    }
    
    // Clone component functions
    for (size_t i = 0; i < component_def->data.component.function_count; i++) {
        KryonASTNode *func_clone = clone_and_substitute_node(component_def->data.component.functions[i], &params, instance_id);
        if (func_clone) {
            add_child_to_node(wrapper, func_clone);
            // Functions will be collected during write phase when we have access to codegen
        }
    }
    
    // Instance ID already pre-calculated at function start
    
    // Clone and substitute the component body with reactive context
    KryonASTNode *expanded_body = clone_and_substitute_node(component_def->data.component.body, &params, instance_id);
    if (expanded_body) {
        add_child_to_node(wrapper, expanded_body);
    }
    
    free(params.substitutions);
    
    // Register component instance with state tracking (will increment next_component_instance_id)
    if (codegen && variable_names && variable_values) {
        char *registered_instance_id = add_component_instance(codegen, component_name, variable_names, variable_values, variable_index);
        if (registered_instance_id) {
            printf("🔧 Registered component instance '%s' with %zu variables\n", registered_instance_id, variable_index);
            free(registered_instance_id);
        }
    }
    
    // Clean up temporary arrays (the add_component_instance function copies the data)
    if (variable_names && variable_values) {
        for (size_t i = 0; i < variable_index; i++) {
            free(variable_names[i]);
            free(variable_values[i]);
        }
    }
    
    free(variable_names);
    free(variable_values);
    
    // Clean up pre-calculated instance ID
    free(instance_id);
    
    printf("✅ Successfully expanded component: %s\n", component_name);
    
    // Return just the body element (component state is now tracked separately)
    return expanded_body;
}

// =============================================================================
// CONSTANT RESOLUTION AND TEMPLATE EXPANSION
// =============================================================================

bool add_constant_to_table(KryonCodeGenerator *codegen, const char *name, const KryonASTNode *value) {
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

const KryonASTNode *find_constant_value(KryonCodeGenerator *codegen, const char *name) {
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




// =============================================================================
// COMPONENT INSTANCE MANAGEMENT
// =============================================================================

char *add_component_instance(KryonCodeGenerator *codegen, const char *component_name, 
                            char **variable_names, char **variable_values, size_t variable_count) {
    if (!codegen || !component_name) {
        return NULL;
    }
    
    // Ensure we have capacity for component instances
    if (codegen->component_instance_count >= codegen->component_instance_capacity) {
        size_t new_capacity = codegen->component_instance_capacity == 0 ? 4 : codegen->component_instance_capacity * 2;
        void *new_instances = realloc(codegen->component_instances, 
                                     new_capacity * sizeof(*codegen->component_instances));
        if (!new_instances) {
            return NULL;
        }
        codegen->component_instances = new_instances;
        codegen->component_instance_capacity = new_capacity;
    }
    
    // Generate unique instance ID
    char *instance_id = malloc(32);
    if (!instance_id) {
        return NULL;
    }
    snprintf(instance_id, 32, "comp_%u", codegen->next_component_instance_id++);
    
    // Initialize the new component instance
    size_t index = codegen->component_instance_count++;
    codegen->component_instances[index].instance_id = strdup(instance_id);
    codegen->component_instances[index].component_name = strdup(component_name);
    codegen->component_instances[index].variable_count = variable_count;
    
    // Copy variable names and values
    if (variable_count > 0) {
        codegen->component_instances[index].variable_names = malloc(variable_count * sizeof(char*));
        codegen->component_instances[index].variable_values = malloc(variable_count * sizeof(char*));
        
        for (size_t i = 0; i < variable_count; i++) {
            codegen->component_instances[index].variable_names[i] = variable_names[i] ? strdup(variable_names[i]) : NULL;
            codegen->component_instances[index].variable_values[i] = variable_values[i] ? strdup(variable_values[i]) : NULL;
        }
    } else {
        codegen->component_instances[index].variable_names = NULL;
        codegen->component_instances[index].variable_values = NULL;
    }
    
    printf("🔧 Added component instance '%s' of type '%s' with %zu variables\n", 
           instance_id, component_name, variable_count);
    
    return instance_id;
}

bool write_component_instance_variables(KryonCodeGenerator *codegen) {
    if (!codegen) {
        return false;
    }
    
    printf("📝 Writing component instance variables (%zu instances)\n", codegen->component_instance_count);
    
    // Count total variables across all component instances
    size_t total_variables = 0;
    for (size_t i = 0; i < codegen->component_instance_count; i++) {
        total_variables += codegen->component_instances[i].variable_count;
    }
    
    if (total_variables == 0) {
        printf("   No component instance variables to write\n");
        return true;
    }
    
    // Write each component instance's variables with instance-scoped names
    for (size_t i = 0; i < codegen->component_instance_count; i++) {
        const void *instance_ptr = &codegen->component_instances[i];
        const char *instance_id = codegen->component_instances[i].instance_id;
        const char *component_name = codegen->component_instances[i].component_name;
        char **variable_names = codegen->component_instances[i].variable_names;
        char **variable_values = codegen->component_instances[i].variable_values;
        size_t variable_count = codegen->component_instances[i].variable_count;
        
        for (size_t j = 0; j < variable_count; j++) {
            if (variable_names[j] && variable_values[j]) {
                // Create instance-scoped variable name: "comp_0.value", "comp_1.value", etc.
                char scoped_name[128];
                snprintf(scoped_name, sizeof(scoped_name), "%s.%s", instance_id, variable_names[j]);
                
                // Add the scoped variable to the string table and write to Variables section
                uint32_t name_ref = add_string_to_table(codegen, scoped_name);
                uint32_t value_ref = add_string_to_table(codegen, variable_values[j]);
                
                // Write variable entry to Variables section
                if (!write_uint32(codegen, name_ref) || 
                    !write_uint8(codegen, 0) ||              // var_type (0 for component variables)
                    !write_uint8(codegen, 0) ||              // var_flags (0 for simple variables)
                    !write_uint8(codegen, KRYON_VALUE_STRING) ||  // value_type
                    !write_uint32(codegen, value_ref)) {
                    return false;
                }
                
                printf("   Wrote variable '%s' = '%s'\n", scoped_name, variable_values[j]);
            }
        }
    }
    
    return true;
}
