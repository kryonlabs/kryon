/**
 * Kryon .kry AST (Abstract Syntax Tree)
 *
 * Internal AST representation for .kry file parsing.
 * The parser first builds this AST, then converts it to IR components.
 *
 * This is an internal implementation detail - external code should
 * use ir_kry_parse() which returns IRComponent* directly.
 */

#ifndef IR_KRY_AST_H
#define IR_KRY_AST_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Node Types
// ============================================================================

typedef enum {
    KRY_NODE_COMPONENT,         // Component block (App, Container, Text, etc.)
    KRY_NODE_PROPERTY,          // Property assignment (text = "Hello")
    KRY_NODE_STATE,             // State declaration (state count: int = 0)
    KRY_NODE_EXPRESSION         // Inline expression ({ value + 1 })
} KryNodeType;

// ============================================================================
// Value Types
// ============================================================================

typedef enum {
    KRY_VALUE_STRING,           // "string literal"
    KRY_VALUE_NUMBER,           // 42, 3.14
    KRY_VALUE_IDENTIFIER,       // variableName, center, etc.
    KRY_VALUE_EXPRESSION        // { code block }
} KryValueType;

// ============================================================================
// Forward Declarations
// ============================================================================

typedef struct KryNode KryNode;
typedef struct KryValue KryValue;
typedef struct KryParser KryParser;

// ============================================================================
// Value Structure
// ============================================================================

struct KryValue {
    KryValueType type;
    union {
        char* string_value;
        double number_value;
        char* identifier;
        char* expression;       // Raw expression code
    };
    bool is_percentage;         // True if number has % suffix (e.g., 100%)
};

// ============================================================================
// AST Node Structure
// ============================================================================

struct KryNode {
    KryNodeType type;

    // Tree structure
    KryNode* parent;
    KryNode* first_child;
    KryNode* last_child;
    KryNode* prev_sibling;
    KryNode* next_sibling;

    // Node data
    char* name;                 // Component or property name
    KryValue* value;            // Property value (NULL for components)
    bool is_component_definition; // True if this is a component definition (component Foo {...})
    char* arguments;            // Raw arguments for component instantiation (e.g., "5" or "initialValue=10")

    // Source location (for error messages)
    uint32_t line;
    uint32_t column;
};

// ============================================================================
// Memory Management (chunk-based allocation like markdown parser)
// ============================================================================

#define KRY_CHUNK_SIZE 4096

typedef struct KryChunk {
    uint8_t data[KRY_CHUNK_SIZE];
    struct KryChunk* next;
    size_t used;
} KryChunk;

// ============================================================================
// Parser State
// ============================================================================

struct KryParser {
    const char* source;         // Source text
    size_t length;              // Source length
    size_t pos;                 // Current position

    // Current location
    uint32_t line;
    uint32_t column;

    // Memory management
    KryChunk* first_chunk;
    KryChunk* current_chunk;

    // AST root
    KryNode* root;

    // Error handling
    bool has_error;
    char* error_message;
    uint32_t error_line;
    uint32_t error_column;
};

// ============================================================================
// Parser Functions
// ============================================================================

// Create parser
KryParser* kry_parser_create(const char* source, size_t length);

// Free parser and all allocated memory
void kry_parser_free(KryParser* parser);

// Parse source and return AST root
KryNode* kry_parse(KryParser* parser);

// Memory allocation (from chunks)
void* kry_alloc(KryParser* parser, size_t size);
char* kry_strdup(KryParser* parser, const char* str);
char* kry_strndup(KryParser* parser, const char* str, size_t len);

// Node creation
KryNode* kry_node_create(KryParser* parser, KryNodeType type);
void kry_node_append_child(KryNode* parent, KryNode* child);

// Value creation
KryValue* kry_value_create_string(KryParser* parser, const char* str);
KryValue* kry_value_create_number(KryParser* parser, double num);
KryValue* kry_value_create_identifier(KryParser* parser, const char* id);
KryValue* kry_value_create_expression(KryParser* parser, const char* expr);

// Error handling
void kry_parser_error(KryParser* parser, const char* message);

#ifdef __cplusplus
}
#endif

#endif // IR_KRY_AST_H
