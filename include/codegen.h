/**
 * @file codegen.h
 * @brief Kryon Code Generator - Converts AST to KRB Binary
 * 
 * Generates optimized KRB binary files from parsed AST with proper
 * element/property hex mapping and cross-platform compatibility.
 * 
 * @version 1.0.0
 * @author Kryon Labs
 */

#ifndef KRYON_INTERNAL_CODEGEN_H
#define KRYON_INTERNAL_CODEGEN_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "parser.h"
#include "krb_format.h"

// =============================================================================
// FORWARD DECLARATIONS
// =============================================================================

typedef struct KryonCodeGenerator KryonCodeGenerator;
typedef struct KryonCodeGenConfig KryonCodeGenConfig;
typedef struct KryonCodeGenStats KryonCodeGenStats;

// =============================================================================
// CODE GENERATION CONFIGURATION
// =============================================================================

/**
 * @brief Code generation optimization levels
 */
typedef enum {
    KRYON_OPTIMIZE_NONE = 0,     // No optimization, preserve all structure
    KRYON_OPTIMIZE_SIZE,         // Optimize for smaller binary size
    KRYON_OPTIMIZE_SPEED,        // Optimize for faster loading/parsing
    KRYON_OPTIMIZE_BALANCED      // Balance between size and speed
} KryonOptimizationLevel;

/**
 * @brief Code generation configuration
 */
struct KryonCodeGenConfig {
    KryonOptimizationLevel optimization;  // Optimization level
    bool enable_compression;              // Enable binary compression
    bool preserve_debug_info;             // Include debug information
    bool validate_output;                 // Validate generated binary
    bool inline_styles;                   // Inline style definitions
    bool deduplicate_strings;             // Remove duplicate strings
    bool remove_unused_styles;            // Remove unreferenced styles
    uint32_t target_version;              // Target KRB format version
};

// =============================================================================
// CODE GENERATION STATISTICS
// =============================================================================

/**
 * @brief Code generation statistics
 */
struct KryonCodeGenStats {
    // Input statistics
    size_t input_ast_nodes;        // Number of AST nodes processed
    size_t input_elements;         // Number of elements
    size_t input_properties;       // Number of properties
    size_t input_styles;           // Number of style blocks
    
    // Output statistics
    size_t output_binary_size;     // Final binary size in bytes
    size_t output_elements;        // Elements in binary
    size_t output_properties;      // Properties in binary
    size_t output_strings;         // String literals in binary
    size_t output_styles;          // Style blocks in binary
    size_t output_themes;          // Theme definitions in binary
    size_t output_variables;       // Variable definitions in binary
    size_t output_functions;       // Function definitions in binary
    size_t output_metadata;        // Metadata directives in binary
    
    // Optimization statistics
    size_t strings_deduplicated;   // Number of strings deduplicated
    size_t styles_inlined;         // Number of styles inlined
    size_t styles_removed;         // Number of unused styles removed
    size_t properties_folded;      // Properties constant-folded
    
    // Performance metrics
    double generation_time;        // Time spent generating (seconds)
    double optimization_time;      // Time spent optimizing (seconds)
    double validation_time;        // Time spent validating (seconds)
    
    // Memory usage
    size_t peak_memory_usage;      // Peak memory during generation
    size_t string_table_size;      // Size of string table
    size_t metadata_size;          // Size of metadata section
};

// =============================================================================
// CODE GENERATOR STRUCTURE
// =============================================================================

/**
 * @brief Code generator state
 */
struct KryonCodeGenerator {
    // Configuration
    KryonCodeGenConfig config;     // Generation configuration
    
    // Input
    const KryonASTNode *ast_root;  // Root AST node to generate from
    
    // Output
    KryonKrbFile *krb_file;        // Generated KRB file structure
    uint8_t *binary_data;          // Raw binary data
    size_t binary_size;            // Size of binary data
    size_t binary_capacity;        // Capacity of binary buffer
    
    // String table for deduplication
    char **string_table;           // Array of unique strings
    size_t string_count;           // Number of strings in table
    size_t string_capacity;        // Capacity of string table
    
    // Constant symbol table for @const definitions
    struct {
        char *name;                // Constant name
        const KryonASTNode *value; // Constant value (AST node)
    } *const_table;                // Array of constants
    size_t const_count;            // Number of constants in table
    size_t const_capacity;         // Capacity of constant table
    
    // Element/Property mapping
    uint16_t *element_map;         // Element type to hex mapping
    uint16_t *property_map;        // Property name to hex mapping
    size_t element_map_size;       // Size of element mapping
    size_t property_map_size;      // Size of property mapping
    
    // Generation state
    size_t current_offset;         // Current write offset in binary
    uint32_t next_element_id;      // Next unique element ID
    
    // Component functions collected during expansion
    KryonASTNode **component_functions;    // Array of component functions
    size_t component_function_count;       // Number of component functions
    size_t component_function_capacity;    // Capacity of function array
    
    // Component instance state tracking
    struct {
        char *instance_id;                 // Unique instance ID
        char *component_name;              // Component type name
        char **variable_names;             // State variable names
        char **variable_values;            // State variable values (as strings)
        size_t variable_count;             // Number of state variables
    } *component_instances;                // Array of component instances
    size_t component_instance_count;       // Number of component instances
    size_t component_instance_capacity;    // Capacity of instance array
    uint32_t next_component_instance_id;   // Next unique component instance ID
    
    // Error tracking
    bool has_errors;               // Whether generation failed
    char **error_messages;         // Array of error messages
    size_t error_count;            // Number of errors
    size_t error_capacity;         // Error array capacity
    
    // Statistics
    KryonCodeGenStats stats;       // Generation statistics
};

// =============================================================================
// CODE GENERATOR API
// =============================================================================

/**
 * @brief Create code generator
 * @param config Generation configuration (NULL for defaults)
 * @return Pointer to code generator, or NULL on failure
 */
KryonCodeGenerator *kryon_codegen_create(const KryonCodeGenConfig *config);

/**
 * @brief Destroy code generator and free resources
 * @param codegen Code generator to destroy
 */
void kryon_codegen_destroy(KryonCodeGenerator *codegen);

/**
 * @brief Generate KRB binary from AST
 * @param codegen Code generator
 * @param ast_root Root AST node
 * @return true on success, false on error
 */
bool kryon_codegen_generate(KryonCodeGenerator *codegen, const KryonASTNode *ast_root);

/**
 * @brief Get generated binary data
 * @param codegen Code generator
 * @param out_size Output for binary size
 * @return Pointer to binary data (owned by generator)
 */
const uint8_t *kryon_codegen_get_binary(const KryonCodeGenerator *codegen, size_t *out_size);

/**
 * @brief Write binary to file
 * @param codegen Code generator
 * @param filename Output file path
 * @return true on success, false on error
 */
bool kryon_write_file(const KryonCodeGenerator *codegen, const char *filename);

/**
 * @brief Get generation errors
 * @param codegen Code generator
 * @param out_count Output for error count
 * @return Array of error messages
 */
const char **kryon_codegen_get_errors(const KryonCodeGenerator *codegen, size_t *out_count);

/**
 * @brief Get generation statistics
 * @param codegen Code generator
 * @return Pointer to statistics structure
 */
const KryonCodeGenStats *kryon_codegen_get_stats(const KryonCodeGenerator *codegen);

// =============================================================================
// CONFIGURATION API
// =============================================================================

/**
 * @brief Get default code generation configuration
 * @return Default configuration
 */
KryonCodeGenConfig kryon_codegen_default_config(void);

/**
 * @brief Get size-optimized configuration
 * @return Configuration optimized for binary size
 */
KryonCodeGenConfig kryon_codegen_size_config(void);

/**
 * @brief Get speed-optimized configuration
 * @return Configuration optimized for loading speed
 */
KryonCodeGenConfig kryon_codegen_speed_config(void);

/**
 * @brief Get debug-friendly configuration
 * @return Configuration with debug info preserved
 */
KryonCodeGenConfig kryon_codegen_debug_config(void);

// =============================================================================
// ELEMENT/PROPERTY MAPPING
// =============================================================================

/**
 * @brief Get hex code for element type
 * @param element_name Element type name ("Button", "Text", etc.)
 * @return Hex code for element type, or 0 if unknown
 */
uint16_t kryon_codegen_get_element_hex(const char *element_name);

/**
 * @brief Get hex code for property name
 * @param property_name Property name ("text", "backgroundColor", etc.)
 * @return Hex code for property, or 0 if unknown
 */
uint16_t kryon_codegen_get_property_hex(const char *property_name);



// =============================================================================
// BINARY GENERATION
// =============================================================================

/**
 * @brief Generate binary header
 * @param codegen Code generator
 * @return true on success, false on error
 */
bool kryon_write_header(KryonCodeGenerator *codegen);

/**
 * @brief Generate metadata section
 * @param codegen Code generator
 * @param ast_root Root AST node
 * @return true on success, false on error
 */
bool kryon_write_metadata(KryonCodeGenerator *codegen, const KryonASTNode *ast_root);

/**
 * @brief Generate string table
 * @param codegen Code generator
 * @return true on success, false on error
 */
bool kryon_write_string_table(KryonCodeGenerator *codegen);

/**
 * @brief Generate style definitions
 * @param codegen Code generator
 * @param ast_root Root AST node
 * @return true on success, false on error
 */
bool kryon_codegen_write_styles(KryonCodeGenerator *codegen, const KryonASTNode *ast_root);

/**
 * @brief Generate element tree
 * @param codegen Code generator
 * @param ast_root Root AST node
 * @return true on success, false on error
 */
bool kryon_write_elements(KryonCodeGenerator *codegen, const KryonASTNode *ast_root);

// =============================================================================
// UTILITY FUNCTIONS (for use by separated modules)
// =============================================================================

// add_string_to_table moved to string_table.h

/**
 * @brief Write uint32 to binary
 * @param codegen Code generator instance  
 * @param value Value to write
 * @return true on success, false on error
 */
bool write_uint32(KryonCodeGenerator *codegen, uint32_t value);

/**
 * @brief Write uint16 to binary
 * @param codegen Code generator instance
 * @param value Value to write 
 * @return true on success, false on error
 */
bool write_uint16(KryonCodeGenerator *codegen, uint16_t value);

/**
 * @brief Write uint8 to binary
 * @param codegen Code generator instance
 * @param value Value to write
 * @return true on success, false on error
 */
bool write_uint8(KryonCodeGenerator *codegen, uint8_t value);

/**
 * @brief Write binary data
 * @param codegen Code generator instance
 * @param data Data to write
 * @param size Size of data
 * @return true on success, false on error
 */
bool write_binary_data(KryonCodeGenerator *codegen, const void *data, size_t size);

/**
 * @brief Report code generation error
 * @param codegen Code generator instance
 * @param message Error message
 */
void codegen_error(KryonCodeGenerator *codegen, const char *message);

/**
 * @brief Write value node to binary
 * @param codegen Code generator instance
 * @param value Value to write
 * @return true on success, false on error
 */
bool write_value_node(KryonCodeGenerator *codegen, const KryonASTValue *value);


/**
 * @brief Expand component instance
 * @param codegen Code generator instance
 * @param component_instance Component instance node
 * @param ast_root Root AST node
 * @return Expanded AST node or NULL on error
 */
KryonASTNode *expand_component_instance(KryonCodeGenerator *codegen, const KryonASTNode *component_instance, const KryonASTNode *ast_root);

/**
 * @brief Write property node to binary
 * @param codegen Code generator instance
 * @param property Property node to write
 * @return true on success, false on error
 */
bool write_property_node(KryonCodeGenerator *codegen, const KryonASTNode *property);

/**
 * @brief Count expanded children of element
 * @param codegen Code generator instance
 * @param element Element node
 * @return Number of expanded children
 */
uint16_t count_expanded_children(KryonCodeGenerator *codegen, const KryonASTNode *element);

/**
 * @brief Add constant to table
 * @param codegen Code generator instance
 * @param name Constant name
 * @param value Constant value node
 * @return true on success, false on error
 */
bool add_constant_to_table(KryonCodeGenerator *codegen, const char *name, const KryonASTNode *value);

/**
 * @brief Find constant value by name
 * @param codegen Code generator instance
 * @param name Constant name
 * @return Constant value node or NULL if not found
 */
const KryonASTNode *find_constant_value(KryonCodeGenerator *codegen, const char *name);

/**
 * @brief Write element node to binary
 * @param codegen Code generator instance
 * @param element Element node to write
 * @param ast_root Root AST node
 * @return true on success, false on error
 */
bool write_element_node(KryonCodeGenerator *codegen, const KryonASTNode *element, const KryonASTNode *ast_root);

/**
 * @brief Write element instance to binary (from element_serializer.c)
 * @param codegen Code generator instance
 * @param element Element to write
 * @param ast_root Root AST node
 * @return true on success, false on error
 */
bool kryon_write_element_instance(KryonCodeGenerator *codegen, const KryonASTNode *element, const KryonASTNode *ast_root);

/**
 * @brief Add component instance state to tracking
 * @param codegen Code generator instance
 * @param component_name Component type name
 * @param variable_names Array of state variable names
 * @param variable_values Array of state variable values
 * @param variable_count Number of state variables
 * @return Component instance ID, or NULL on error
 */
char *add_component_instance(KryonCodeGenerator *codegen, const char *component_name, 
                            char **variable_names, char **variable_values, size_t variable_count);

/**
 * @brief Write component instance state to Variables section
 * @param codegen Code generator instance
 * @return true on success, false on error
 */
bool write_component_instance_variables(KryonCodeGenerator *codegen);

// =============================================================================
// VALIDATION
// =============================================================================

/**
 * @brief Validate generated binary
 * @param codegen Code generator
 * @return true if binary is valid, false on error
 */
bool kryon_codegen_validate_binary(const KryonCodeGenerator *codegen);

/**
 * @brief Check binary format version compatibility
 * @param binary_data Binary data to check
 * @param size Size of binary data
 * @param min_version Minimum supported version
 * @param max_version Maximum supported version
 * @return true if compatible, false otherwise
 */
bool kryon_codegen_check_version(const uint8_t *binary_data, size_t size,
                                uint32_t min_version, uint32_t max_version);

// =============================================================================
// DEBUGGING AND INTROSPECTION
// =============================================================================

/**
 * @brief Print binary structure for debugging
 * @param binary_data Binary data
 * @param size Size of binary data
 * @param file Output file (NULL for stdout)
 */
void kryon_codegen_print_binary(const uint8_t *binary_data, size_t size, FILE *file);

/**
 * @brief Disassemble binary to human-readable format
 * @param binary_data Binary data
 * @param size Size of binary data
 * @param output_file Output file path
 * @return true on success, false on error
 */
bool kryon_codegen_disassemble(const uint8_t *binary_data, size_t size, const char *output_file);

/**
 * @brief Compare two binary files for differences
 * @param file1_data First binary
 * @param file1_size Size of first binary
 * @param file2_data Second binary
 * @param file2_size Size of second binary
 * @param diff_file Output file for differences (NULL to skip)
 * @return 0 if identical, positive if different, negative on error
 */
int kryon_codegen_compare_binaries(const uint8_t *file1_data, size_t file1_size,
                                  const uint8_t *file2_data, size_t file2_size,
                                  const char *diff_file);

#ifdef __cplusplus
}
#endif

#endif // KRYON_INTERNAL_CODEGEN_H