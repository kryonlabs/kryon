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
    KRY_NODE_EXPRESSION,        // Inline expression ({ value + 1 })
    KRY_NODE_VAR_DECL,          // Variable declaration (const x = ..., let y = ..., var z = ...)
    KRY_NODE_IMPORT,            // Import statement (import Math from "math")
    KRY_NODE_STATIC_BLOCK,      // Static block (static { ... })
    KRY_NODE_FOR_LOOP,          // For loop (for item in array { ... }) - compile-time expansion
    KRY_NODE_FOR_EACH,          // For each loop (for each item in collection { ... }) - runtime ForEach
    KRY_NODE_IF,                // If/else conditional (if condition { ... } else { ... })
    KRY_NODE_STYLE_BLOCK,       // Style block (style selector { property = value; })
    KRY_NODE_CODE_BLOCK,        // Platform-specific code block (@lua, @js)
    KRY_NODE_FUNCTION_DECL,     // Function declaration (func name(): type { ... })
    KRY_NODE_RETURN_STMT,       // Return statement in function (return expression)
    KRY_NODE_MODULE_RETURN,     // Module-level return: return { exports }
    KRY_NODE_STRUCT_DECL,       // Struct declaration: struct Habit { ... }
    KRY_NODE_STRUCT_INST,       // Struct instantiation: Habit { name = "..." }
    KRY_NODE_STRUCT_FIELD       // Struct field: name: string = "default"
} KryNodeType;

// ============================================================================
// Value Types
// ============================================================================

typedef enum {
    KRY_VALUE_STRING,           // "string literal"
    KRY_VALUE_NUMBER,           // 42, 3.14
    KRY_VALUE_IDENTIFIER,       // variableName, center, etc.
    KRY_VALUE_EXPRESSION,       // { code block }
    KRY_VALUE_ARRAY,            // [item1, item2, item3]
    KRY_VALUE_OBJECT            // {key: value, key2: value2}
} KryValueType;

// ============================================================================
// Forward Declarations
// ============================================================================

typedef struct KryNode KryNode;
typedef struct KryValue KryValue;
typedef struct KryParser KryParser;

// ============================================================================
// Error Collection Structures
// ============================================================================

// Error severity levels
typedef enum {
    KRY_ERROR_WARNING,    // Non-fatal issues (missing optional properties)
    KRY_ERROR_ERROR,      // Parse/semantic errors (syntax violations)
    KRY_ERROR_FATAL       // Unrecoverable errors (allocation failures)
} KryErrorLevel;

// Error categories
typedef enum {
    KRY_ERROR_SYNTAX,           // Syntax errors
    KRY_ERROR_SEMANTIC,         // Semantic errors
    KRY_ERROR_LIMIT_EXCEEDED,   // Array/object limits
    KRY_ERROR_BUFFER_OVERFLOW,  // Buffer overflow
    KRY_ERROR_CONVERSION,       // IR conversion errors
    KRY_ERROR_VALIDATION        // Post-validation errors
} KryErrorCategory;

// Individual error entry
typedef struct KryError {
    KryErrorLevel level;
    KryErrorCategory category;
    char* message;              // Allocated from chunk
    uint32_t line;
    uint32_t column;
    char* context;              // Source snippet (optional)
    struct KryError* next;      // Linked list
} KryError;

// Error collection
typedef struct {
    KryError* first;
    KryError* last;
    uint32_t error_count;
    uint32_t warning_count;
    bool has_fatal;
} KryErrorList;

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
        struct {
            KryValue** elements;
            size_t count;
        } array;
        struct {
            char** keys;
            KryValue** values;
            size_t count;
        } object;
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
    char* name;                 // Component or property name, variable name, or loop iterator
    KryValue* value;            // Property value (NULL for components)
    bool is_component_definition; // True if this is a component definition (component Foo {...})
    char* arguments;            // Raw arguments for component instantiation (e.g., "5" or "initialValue=10")
    char* var_type;             // Variable declaration type: "const", "let", or "var" (for KRY_NODE_VAR_DECL)
    char* state_type;           // State variable type: "int", "string", etc. (for KRY_NODE_STATE)

    // If/else support
    KryNode* else_branch;       // Else branch for KRY_NODE_IF (NULL if no else)

    // Code block support (for KRY_NODE_CODE_BLOCK)
    char* code_language;        // "lua", "js"
    char* code_source;          // Source code content

    // Import statement support (for KRY_NODE_IMPORT)
    char* import_module;        // Module path (e.g., "math", "components.calendar")
    char* import_name;          // Imported name (e.g., "Math", "*")

    // Function declaration support (for KRY_NODE_FUNCTION_DECL)
    char* func_name;            // Function name (e.g., "loadHabits")
    char* func_return_type;     // Return type (e.g., "int", "string", "array", "void")
    KryNode** func_params;      // Parameter declarations (array of VAR_DECL nodes)
    int param_count;            // Number of parameters
    KryNode* func_body;         // Function body (block containing statements)

    // Return statement support (for KRY_NODE_RETURN_STMT)
    KryNode* return_expr;       // Expression to return (NULL for void return)

    // Module-level return support (for KRY_NODE_MODULE_RETURN)
    char** export_names;        // Array of export names ["COLORS", "DEFAULT_COLOR", ...]
    int export_count;           // Number of exports

    // Struct declaration support (for KRY_NODE_STRUCT_DECL)
    char* struct_name;          // Struct type name (e.g., "Habit")
    KryNode** struct_fields;    // Array of KRY_NODE_STRUCT_FIELD
    int field_count;            // Number of fields in struct

    // Struct instance support (for KRY_NODE_STRUCT_INST)
    char* instance_type;        // Type being instantiated
    KryValue** field_values;    // Array of field values
    char** field_names;         // Array of field names
    int field_value_count;      // Number of fields in instance

    // Struct field support (for KRY_NODE_STRUCT_FIELD)
    char* field_type;           // Field type (string, int, float, bool, map)
    KryValue* field_default;    // Default value expression

    // Source location (for error messages)
    uint32_t line;
    uint32_t column;
};

// ============================================================================
// Memory Management (chunk-based allocation like markdown parser)
// ============================================================================

#define KRY_CHUNK_SIZE 32768  // Increased to support large plugin code blocks

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

    // Error collection
    KryErrorList errors;

    // Legacy error handling (for backward compatibility - points to first error)
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
KryValue* kry_value_create_array(KryParser* parser, KryValue** elements, size_t count);
KryValue* kry_value_create_object(KryParser* parser, char** keys, KryValue** values, size_t count);

// Error handling
void kry_parser_error(KryParser* parser, const char* message);
void kry_parser_add_error(KryParser* parser, KryErrorLevel level, KryErrorCategory category, const char* message);
void kry_parser_add_error_fmt(KryParser* parser, KryErrorLevel level, KryErrorCategory category, const char* fmt, ...);
bool kry_parser_should_stop(KryParser* parser);

// Code block creation
KryNode* kry_node_create_code_block(KryParser* parser, const char* language, const char* source);

// Import node helper
static inline bool kry_node_is_import(KryNode* node) {
    return node && node->type == KRY_NODE_IMPORT;
}

// Decorator creation

#ifdef __cplusplus
}
#endif

#endif // IR_KRY_AST_H
