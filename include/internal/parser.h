/**
 * @file parser.h
 * @brief Kryon Parser - Converts tokens to Abstract Syntax Tree
 * 
 * Recursive descent parser for KRY language that creates AST nodes from
 * tokenized input with comprehensive error recovery and validation.
 * 
 * @version 1.0.0
 * @author Kryon Labs
 */

#ifndef KRYON_INTERNAL_PARSER_H
#define KRYON_INTERNAL_PARSER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "internal/lexer.h"

// =============================================================================
// FORWARD DECLARATIONS
// =============================================================================

typedef struct KryonParser KryonParser;
typedef struct KryonASTNode KryonASTNode;
typedef struct KryonASTElement KryonASTElement;
typedef struct KryonASTProperty KryonASTProperty;
typedef struct KryonASTStyle KryonASTStyle;
typedef struct KryonASTExpression KryonASTExpression;

// =============================================================================
// AST NODE TYPES
// =============================================================================

/**
 * @brief AST node types for KRY language constructs
 */
typedef enum {
    // Root and containers
    KRYON_AST_ROOT = 0,              // Root document node
    KRYON_AST_ELEMENT,               // UI element (Button, Text, etc.)
    KRYON_AST_PROPERTY,              // Element property
    KRYON_AST_STYLE_BLOCK,           // @style block
    KRYON_AST_STYLES_BLOCK,          // @styles block
    
    // Directives
    KRYON_AST_STORE_DIRECTIVE,       // @store
    KRYON_AST_WATCH_DIRECTIVE,       // @watch
    KRYON_AST_ON_MOUNT_DIRECTIVE,    // @on_mount
    KRYON_AST_ON_UNMOUNT_DIRECTIVE,  // @on_unmount
    KRYON_AST_IMPORT_DIRECTIVE,      // @import
    KRYON_AST_EXPORT_DIRECTIVE,      // @export
    KRYON_AST_INCLUDE_DIRECTIVE,     // @include
    KRYON_AST_METADATA_DIRECTIVE,    // @metadata
    KRYON_AST_EVENT_DIRECTIVE,       // @event
    
    // Component system
    KRYON_AST_COMPONENT,             // @component directive
    KRYON_AST_PROPS,                 // @props block
    KRYON_AST_SLOTS,                 // @slots block
    KRYON_AST_LIFECYCLE,             // @lifecycle block
    
    // Legacy (deprecated)
    KRYON_AST_DEFINE,                // Define custom widget (deprecated)
    KRYON_AST_PROPERTIES,            // Properties block (deprecated)
    KRYON_AST_SCRIPT,                // @script block
    
    // Expressions
    KRYON_AST_LITERAL,               // String, number, boolean literals
    KRYON_AST_VARIABLE,              // $variable
    KRYON_AST_TEMPLATE,              // ${expression}
    KRYON_AST_BINARY_OP,             // Binary operations
    KRYON_AST_UNARY_OP,              // Unary operations
    KRYON_AST_FUNCTION_CALL,         // Function calls
    KRYON_AST_MEMBER_ACCESS,         // Object.property
    KRYON_AST_ARRAY_ACCESS,          // array[index]
    
    // Special
    KRYON_AST_ERROR,                 // Error node for recovery
    
    KRYON_AST_COUNT
} KryonASTNodeType;

// =============================================================================
// AST VALUE TYPES
// =============================================================================

/**
 * @brief Value types for AST literals
 */
typedef enum {
    KRYON_VALUE_STRING,
    KRYON_VALUE_INTEGER,
    KRYON_VALUE_FLOAT,
    KRYON_VALUE_BOOLEAN,
    KRYON_VALUE_NULL,
    KRYON_VALUE_COLOR,               // #RRGGBBAA
    KRYON_VALUE_UNIT,                // number with unit (px, %, etc.)
} KryonValueType;

/**
 * @brief AST value with type information
 */
typedef struct {
    KryonValueType type;
    union {
        char *string_value;          // Allocated string
        int64_t int_value;
        double float_value;
        bool bool_value;
        uint32_t color_value;        // RGBA color
        struct {
            double value;
            char unit[8];            // px, %, em, etc.
        } unit_value;
    } data;
} KryonASTValue;

// =============================================================================
// AST NODE STRUCTURE
// =============================================================================

/**
 * @brief Generic AST node structure
 */
struct KryonASTNode {
    KryonASTNodeType type;           // Node type
    KryonSourceLocation location;    // Source location
    
    // Node-specific data (union based on type)
    union {
        struct {
            char *element_type;      // "Button", "Text", etc.
            KryonASTNode **children; // Child elements
            size_t child_count;
            size_t child_capacity;
            KryonASTNode **properties; // Element properties
            size_t property_count;
            size_t property_capacity;
        } element;
        
        struct {
            char *name;              // Property name
            KryonASTNode *value;     // Property value expression
        } property;
        
        struct {
            char *name;              // Style name
            KryonASTNode **properties; // Style properties
            size_t property_count;
            size_t property_capacity;
        } style;
        
        struct {
            KryonASTValue value;     // Literal value
        } literal;
        
        struct {
            char *name;              // Variable name (without $)
        } variable;
        
        struct {
            KryonASTNode *expression; // Template expression
        } template;
        
        struct {
            KryonTokenType operator; // Binary operator token
            KryonASTNode *left;      // Left operand
            KryonASTNode *right;     // Right operand
        } binary_op;
        
        struct {
            KryonTokenType operator; // Unary operator token
            KryonASTNode *operand;   // Operand
        } unary_op;
        
        struct {
            char *function_name;     // Function name
            KryonASTNode **arguments; // Function arguments
            size_t argument_count;
            size_t argument_capacity;
        } function_call;
        
        struct {
            KryonASTNode *object;    // Object being accessed
            char *member;            // Member name
        } member_access;
        
        struct {
            KryonASTNode *array;     // Array being indexed
            KryonASTNode *index;     // Index expression
        } array_access;
        
        struct {
            char *message;           // Error message
            const KryonToken *token; // Token that caused error
        } error;
        
        struct {
            char *name;              // Define widget name
            KryonASTNode *properties; // Properties block
            KryonASTNode **children; // Child elements
            size_t child_count;
            size_t child_capacity;
            KryonASTNode **scripts;  // Script blocks
            size_t script_count;
            size_t script_capacity;
        } define;
        
        struct {
            KryonASTNode **properties; // Property definitions
            size_t property_count;
            size_t property_capacity;
        } properties;
        
        struct {
            char *language;          // Script language (e.g., "lua")
            char *code;              // Script code
        } script;
    } data;
    
    // Metadata
    KryonASTNode *parent;            // Parent node (NULL for root)
    size_t node_id;                  // Unique node ID
};

// =============================================================================
// PARSER CONFIGURATION
// =============================================================================

/**
 * @brief Parser configuration options
 */
typedef struct {
    bool strict_mode;                // Strict syntax checking
    bool error_recovery;             // Attempt error recovery
    bool preserve_comments;          // Preserve comment nodes
    bool validate_properties;        // Validate property names/types
    size_t max_nesting_depth;        // Maximum element nesting
    size_t max_errors;               // Maximum errors before giving up
} KryonParserConfig;

// =============================================================================
// PARSER STRUCTURE
// =============================================================================

/**
 * @brief Kryon parser state
 */
struct KryonParser {
    // Input
    KryonLexer *lexer;              // Source lexer
    const KryonToken *tokens;       // Token array
    size_t token_count;             // Number of tokens
    
    // Current state
    size_t current_token;           // Current token index
    size_t nesting_depth;           // Current nesting depth
    
    // Configuration
    KryonParserConfig config;       // Parser configuration
    
    // AST
    KryonASTNode *root;             // Root AST node
    size_t next_node_id;            // Next unique node ID
    
    // Error tracking
    bool has_errors;                // Whether parser encountered errors
    char **error_messages;          // Array of error messages
    size_t error_count;             // Number of errors
    size_t error_capacity;          // Error array capacity
    bool panic_mode;                // Whether in panic recovery mode
    
    // Memory management
    KryonASTNode **all_nodes;       // All allocated nodes (for cleanup)
    size_t node_count;              // Number of allocated nodes
    size_t node_capacity;           // Node array capacity
    
    // Statistics
    double parsing_time;            // Time spent parsing (seconds)
    size_t nodes_created;           // Total nodes created
    size_t max_depth_reached;       // Maximum nesting depth reached
};

// =============================================================================
// PARSER API
// =============================================================================

/**
 * @brief Create a new parser
 * @param lexer Lexer with tokenized input
 * @param config Parser configuration (NULL for defaults)
 * @return Pointer to parser, or NULL on failure
 */
KryonParser *kryon_parser_create(KryonLexer *lexer, const KryonParserConfig *config);

/**
 * @brief Destroy parser and free all resources
 * @param parser Parser to destroy
 */
void kryon_parser_destroy(KryonParser *parser);

/**
 * @brief Parse tokens into AST
 * @param parser The parser
 * @return true on success, false on error
 */
bool kryon_parser_parse(KryonParser *parser);

/**
 * @brief Get the root AST node
 * @param parser The parser
 * @return Root node, or NULL if parsing failed
 */
const KryonASTNode *kryon_parser_get_root(const KryonParser *parser);

/**
 * @brief Get parser errors
 * @param parser The parser
 * @param out_count Output for error count
 * @return Array of error messages
 */
const char **kryon_parser_get_errors(const KryonParser *parser, size_t *out_count);

/**
 * @brief Get parser statistics
 * @param parser The parser
 * @param out_time Output for parsing time
 * @param out_nodes Output for nodes created
 * @param out_depth Output for max depth reached
 */
void kryon_parser_get_stats(const KryonParser *parser, double *out_time,
                           size_t *out_nodes, size_t *out_depth);

// =============================================================================
// AST NODE API
// =============================================================================

/**
 * @brief Create AST node
 * @param parser The parser (for memory management)
 * @param type Node type
 * @param location Source location
 * @return New AST node
 */
KryonASTNode *kryon_ast_create_node(KryonParser *parser, KryonASTNodeType type,
                                   const KryonSourceLocation *location);

/**
 * @brief Add child to AST node
 * @param parent Parent node
 * @param child Child node to add
 * @return true on success
 */
bool kryon_ast_add_child(KryonASTNode *parent, KryonASTNode *child);

/**
 * @brief Add property to element or style node
 * @param parent Parent node (element or style)
 * @param property Property node to add
 * @return true on success
 */
bool kryon_ast_add_property(KryonASTNode *parent, KryonASTNode *property);

/**
 * @brief Get node type name
 * @param type Node type
 * @return Type name string
 */
const char *kryon_ast_node_type_name(KryonASTNodeType type);

/**
 * @brief Print AST for debugging
 * @param node Root node to print
 * @param file Output file (NULL for stdout)
 * @param indent Indentation level
 */
void kryon_ast_print(const KryonASTNode *node, FILE *file, int indent);

/**
 * @brief Validate AST node structure
 * @param node Node to validate
 * @param errors Output buffer for error messages
 * @param max_errors Maximum errors to report
 * @return Number of errors found
 */
size_t kryon_ast_validate(const KryonASTNode *node, char **errors, size_t max_errors);

// =============================================================================
// AST VALUE API
// =============================================================================

/**
 * @brief Create string value
 * @param str String value (will be copied)
 * @return AST value
 */
KryonASTValue kryon_ast_value_string(const char *str);

/**
 * @brief Create integer value
 * @param value Integer value
 * @return AST value
 */
KryonASTValue kryon_ast_value_integer(int64_t value);

/**
 * @brief Create float value
 * @param value Float value
 * @return AST value
 */
KryonASTValue kryon_ast_value_float(double value);

/**
 * @brief Create boolean value
 * @param value Boolean value
 * @return AST value
 */
KryonASTValue kryon_ast_value_boolean(bool value);

/**
 * @brief Create null value
 * @return AST value
 */
KryonASTValue kryon_ast_value_null(void);

/**
 * @brief Create color value
 * @param rgba RGBA color value
 * @return AST value
 */
KryonASTValue kryon_ast_value_color(uint32_t rgba);

/**
 * @brief Create unit value
 * @param value Numeric value
 * @param unit Unit string (px, %, etc.)
 * @return AST value
 */
KryonASTValue kryon_ast_value_unit(double value, const char *unit);

/**
 * @brief Free AST value resources
 * @param value Value to free
 */
void kryon_ast_value_free(KryonASTValue *value);

/**
 * @brief Convert AST value to string
 * @param value The value
 * @return Allocated string (caller must free)
 */
char *kryon_ast_value_to_string(const KryonASTValue *value);

// =============================================================================
// PARSER CONFIGURATION
// =============================================================================

/**
 * @brief Get default parser configuration
 * @return Default configuration
 */
KryonParserConfig kryon_parser_default_config(void);

/**
 * @brief Get strict parser configuration
 * @return Strict configuration with all validation enabled
 */
KryonParserConfig kryon_parser_strict_config(void);

/**
 * @brief Get permissive parser configuration
 * @return Permissive configuration for error recovery
 */
KryonParserConfig kryon_parser_permissive_config(void);

// =============================================================================
// AST TRAVERSAL
// =============================================================================

/**
 * @brief AST visitor function type
 * @param node Current node being visited
 * @param user_data User-provided data
 * @return true to continue traversal, false to stop
 */
typedef bool (*KryonASTVisitor)(const KryonASTNode *node, void *user_data);

/**
 * @brief Traverse AST in depth-first order
 * @param root Root node to start from
 * @param visitor Visitor function
 * @param user_data User data passed to visitor
 */
void kryon_ast_traverse(const KryonASTNode *root, KryonASTVisitor visitor, void *user_data);

/**
 * @brief Find nodes by type
 * @param root Root node to search from
 * @param type Node type to find
 * @param results Output array for found nodes
 * @param max_results Maximum results to return
 * @return Number of nodes found
 */
size_t kryon_ast_find_by_type(const KryonASTNode *root, KryonASTNodeType type,
                             const KryonASTNode **results, size_t max_results);

/**
 * @brief Find element by type name
 * @param root Root node to search from
 * @param element_type Element type name ("Button", "Text", etc.)
 * @param results Output array for found elements
 * @param max_results Maximum results to return
 * @return Number of elements found
 */
size_t kryon_ast_find_elements(const KryonASTNode *root, const char *element_type,
                              const KryonASTNode **results, size_t max_results);

#ifdef __cplusplus
}
#endif

#endif // KRYON_INTERNAL_PARSER_H