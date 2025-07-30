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
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <assert.h>

// =============================================================================
// FORWARD DECLARATIONS
// =============================================================================

static bool generate_binary_from_ast(KryonCodeGenerator *codegen, const KryonASTNode *node);
static bool write_element_node(KryonCodeGenerator *codegen, const KryonASTNode *element);
static bool write_property_node(KryonCodeGenerator *codegen, const KryonASTNode *property);
static bool write_style_node(KryonCodeGenerator *codegen, const KryonASTNode *style);
static bool write_value_node(KryonCodeGenerator *codegen, const KryonASTValue *value);

static uint32_t add_string_to_table(KryonCodeGenerator *codegen, const char *str);
static bool expand_binary_buffer(KryonCodeGenerator *codegen, size_t additional_size);
static bool write_binary_data(KryonCodeGenerator *codegen, const void *data, size_t size);
static bool write_uint8(KryonCodeGenerator *codegen, uint8_t value);
static bool write_uint16(KryonCodeGenerator *codegen, uint16_t value);
static bool write_uint32(KryonCodeGenerator *codegen, uint32_t value);

static void codegen_error(KryonCodeGenerator *codegen, const char *message);

// String duplication utility
static char *kryon_strdup(const char *str) {
    if (!str) return NULL;
    size_t len = strlen(str) + 1;
    char *copy = kryon_alloc(len);
    if (copy) {
        memcpy(copy, str, len);
    }
    return copy;
}

// =============================================================================
// ELEMENT AND PROPERTY MAPPINGS
// =============================================================================

// Basic element type mappings (expandable)
static struct {
    const char *name;
    uint16_t hex;
} element_mappings[] = {
    {"App", 0x0001},
    {"Container", 0x0002},
    {"Button", 0x0003},
    {"Text", 0x0004},
    {"Input", 0x0005},
    {"Image", 0x0006},
    {"List", 0x0007},
    {"Grid", 0x0008},
    {"Form", 0x0009},
    {"Modal", 0x000A},
    {"Tabs", 0x000B},
    {"Menu", 0x000C},
    {NULL, 0}
};

// Basic property mappings (expandable)
static struct {
    const char *name;
    uint16_t hex;
} property_mappings[] = {
    {"text", 0x0001},
    {"backgroundColor", 0x0002},
    {"borderColor", 0x0003},
    {"borderWidth", 0x0004},
    {"color", 0x0005},
    {"fontSize", 0x0006},
    {"fontWeight", 0x0007},
    {"fontFamily", 0x0008},
    {"width", 0x0009},
    {"height", 0x000A},
    {"posX", 0x000B},
    {"posY", 0x000C},
    {"padding", 0x000D},
    {"margin", 0x000E},
    {"display", 0x000F},
    {"flexDirection", 0x0010},
    {"alignItems", 0x0011},
    {"justifyContent", 0x0012},
    {"textAlignment", 0x0013},
    {"visible", 0x0014},
    {"enabled", 0x0015},
    {"class", 0x0016},
    {"id", 0x0017},
    {"style", 0x0018},
    {"onClick", 0x0019},
    {"onHover", 0x001A},
    {"onFocus", 0x001B},
    {"onChange", 0x001C},
    {NULL, 0}
};

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
    KryonCodeGenerator *codegen = kryon_alloc(sizeof(KryonCodeGenerator));
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
    codegen->binary_data = kryon_alloc(codegen->binary_capacity);
    if (!codegen->binary_data) {
        kryon_free(codegen);
        return NULL;
    }
    
    // Initialize string table
    codegen->string_capacity = 64;
    codegen->string_table = kryon_alloc(codegen->string_capacity * sizeof(char*));
    if (!codegen->string_table) {
        kryon_free(codegen->binary_data);
        kryon_free(codegen);
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
    kryon_free(codegen->binary_data);
    
    // Free string table
    if (codegen->string_table) {
        for (size_t i = 0; i < codegen->string_count; i++) {
            kryon_free(codegen->string_table[i]);
        }
        kryon_free(codegen->string_table);
    }
    
    // Free error messages
    if (codegen->error_messages) {
        for (size_t i = 0; i < codegen->error_count; i++) {
            kryon_free(codegen->error_messages[i]);
        }
        kryon_free(codegen->error_messages);
    }
    
    // Free mappings
    kryon_free(codegen->element_map);
    kryon_free(codegen->property_map);
    
    kryon_free(codegen);
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
    
    // Write KRB header
    if (!kryon_codegen_write_header(codegen)) {
        return false;
    }
    
    // Collect and write string table
    if (!kryon_codegen_write_string_table(codegen)) {
        return false;
    }
    
    // Write metadata
    if (!kryon_codegen_write_metadata(codegen, ast_root)) {
        return false;
    }
    
    // Write elements
    if (!kryon_codegen_write_elements(codegen, ast_root)) {
        return false;
    }
    
    // Validate output if requested
    if (codegen->config.validate_output) {
        if (!kryon_codegen_validate_binary(codegen)) {
            return false;
        }
    }
    
    // Calculate statistics
    clock_t end_time = clock();
    codegen->stats.generation_time = ((double)(end_time - start_time)) / CLOCKS_PER_SEC;
    codegen->stats.output_binary_size = codegen->binary_size;
    
    return !codegen->has_errors;
}

bool kryon_codegen_write_header(KryonCodeGenerator *codegen) {
    // KRB magic number "KRB\0"
    if (!write_uint32(codegen, 0x4B524200)) {
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

bool kryon_codegen_write_string_table(KryonCodeGenerator *codegen) {
    // For now, write an empty string table placeholder
    // In a full implementation, we would collect all strings first
    
    // String count
    if (!write_uint32(codegen, 0)) {
        return false;
    }
    
    return true;
}

bool kryon_codegen_write_metadata(KryonCodeGenerator *codegen, const KryonASTNode *ast_root) {
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
    memcpy(codegen->binary_data + metadata_size_offset, &metadata_size, sizeof(uint32_t));
    
    return true;
}

bool kryon_codegen_write_elements(KryonCodeGenerator *codegen, const KryonASTNode *ast_root) {
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
            if (child->type == KRYON_AST_ELEMENT) {
                if (write_element_node(codegen, child)) {
                    element_count++;
                    codegen->stats.output_elements++;
                } else {
                    return false;
                }
            } else if (child->type == KRYON_AST_STYLE_BLOCK) {
                if (!write_style_node(codegen, child)) {
                    return false;
                }
            }
        }
    }
    
    // Update element count
    memcpy(codegen->binary_data + element_count_offset, &element_count, sizeof(uint32_t));
    
    return true;
}

static bool write_element_node(KryonCodeGenerator *codegen, const KryonASTNode *element) {
    if (!element || element->type != KRYON_AST_ELEMENT) {
        return false;
    }
    
    // Get element type hex code
    uint16_t element_hex = kryon_codegen_get_element_hex(element->data.element.element_type);
    if (element_hex == 0) {
        codegen_error(codegen, "Unknown element type");
        return false;
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

static bool write_property_node(KryonCodeGenerator *codegen, const KryonASTNode *property) {
    if (!property || property->type != KRYON_AST_PROPERTY) {
        return false;
    }
    
    // Get property hex code
    uint16_t property_hex = kryon_codegen_get_property_hex(property->data.property.name);
    if (property_hex == 0) {
        codegen_error(codegen, "Unknown property type");
        return false;
    }
    
    // Write property header
    if (!write_uint16(codegen, property_hex)) {
        return false;
    }
    
    // Write property value
    if (property->data.property.value) {
        if (property->data.property.value->type == KRYON_AST_LITERAL) {
            return write_value_node(codegen, &property->data.property.value->data.literal.value);
        } else {
            // For now, only handle literals
            codegen_error(codegen, "Non-literal property values not yet supported");
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

static bool write_value_node(KryonCodeGenerator *codegen, const KryonASTValue *value) {
    if (!value) {
        return false;
    }
    
    // Write value type
    if (!write_uint8(codegen, (uint8_t)value->type)) {
        return false;
    }
    
    switch (value->type) {
        case KRYON_VALUE_STRING: {
            uint32_t string_ref = add_string_to_table(codegen, value->data.string_value);
            return write_uint32(codegen, string_ref);
        }
        
        case KRYON_VALUE_INTEGER:
            return write_uint32(codegen, (uint32_t)value->data.int_value);
        
        case KRYON_VALUE_FLOAT: {
            // Convert double to 32-bit float for binary
            float f = (float)value->data.float_value;
            return write_binary_data(codegen, &f, sizeof(float));
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
    
    for (int i = 0; element_mappings[i].name; i++) {
        if (strcmp(element_mappings[i].name, element_name) == 0) {
            return element_mappings[i].hex;
        }
    }
    
    return 0; // Unknown element
}

uint16_t kryon_codegen_get_property_hex(const char *property_name) {
    if (!property_name) {
        return 0;
    }
    
    for (int i = 0; property_mappings[i].name; i++) {
        if (strcmp(property_mappings[i].name, property_name) == 0) {
            return property_mappings[i].hex;
        }
    }
    
    return 0; // Unknown property
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
        char **new_table = kryon_realloc(codegen->string_table, 
                                        new_capacity * sizeof(char*));
        if (!new_table) {
            return 0;
        }
        codegen->string_table = new_table;
        codegen->string_capacity = new_capacity;
    }
    
    codegen->string_table[codegen->string_count] = kryon_strdup(str);
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
    
    uint8_t *new_buffer = kryon_realloc(codegen->binary_data, new_capacity);
    if (!new_buffer) {
        codegen_error(codegen, "Failed to expand binary buffer");
        return false;
    }
    
    codegen->binary_data = new_buffer;
    codegen->binary_capacity = new_capacity;
    
    return true;
}

static bool write_binary_data(KryonCodeGenerator *codegen, const void *data, size_t size) {
    if (!expand_binary_buffer(codegen, size)) {
        return false;
    }
    
    memcpy(codegen->binary_data + codegen->binary_size, data, size);
    codegen->binary_size += size;
    codegen->current_offset += size;
    
    return true;
}

static bool write_uint8(KryonCodeGenerator *codegen, uint8_t value) {
    return write_binary_data(codegen, &value, sizeof(uint8_t));
}

static bool write_uint16(KryonCodeGenerator *codegen, uint16_t value) {
    return write_binary_data(codegen, &value, sizeof(uint16_t));
}

static bool write_uint32(KryonCodeGenerator *codegen, uint32_t value) {
    return write_binary_data(codegen, &value, sizeof(uint32_t));
}

static void codegen_error(KryonCodeGenerator *codegen, const char *message) {
    if (!codegen || !message) {
        return;
    }
    
    codegen->has_errors = true;
    
    // Expand error array if needed
    if (codegen->error_count >= codegen->error_capacity) {
        size_t new_capacity = codegen->error_capacity ? codegen->error_capacity * 2 : 8;
        char **new_messages = kryon_realloc(codegen->error_messages,
                                           new_capacity * sizeof(char*));
        if (!new_messages) {
            return;
        }
        codegen->error_messages = new_messages;
        codegen->error_capacity = new_capacity;
    }
    
    codegen->error_messages[codegen->error_count++] = kryon_strdup(message);
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

bool kryon_codegen_write_file(const KryonCodeGenerator *codegen, const char *filename) {
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

bool kryon_codegen_validate_binary(const KryonCodeGenerator *codegen) {
    if (!codegen || codegen->binary_size < 8) {
        return false;
    }
    
    // Check magic number
    uint32_t magic = *(uint32_t*)codegen->binary_data;
    if (magic != 0x4B524200) { // "KRB\0"
        return false;
    }
    
    // Basic validation passed
    return true;
}