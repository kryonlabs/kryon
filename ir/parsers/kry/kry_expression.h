/**
 * KRY Expression Transpiler
 *
 * Transpiles KRY expressions to Lua, JavaScript, C, or Hare.
 *
 * Targets:
 *   KRY_TARGET_LUA        - Lua 5.x (1-indexed arrays, `and`/`or`/`not`)
 *   KRY_TARGET_JAVASCRIPT - ES6 JavaScript
 *   KRY_TARGET_C          - Native C99 (designated initializers, NULL)
 *   KRY_TARGET_HARE       - Hare (void instead of null, match for ternary)
 *
 * Supported Expressions:
 *   Literals      strings, numbers, booleans, null
 *   Operators     + - * / % == != < > <= >= && || ! =
 *   Ternary       condition ? then : else
 *   Access        obj.prop, arr[index], func(args)
 *   Literals      [1, 2, 3], {key: val}
 *   Arrow funcs   x => x * 2 (all targets - C uses registry)
 *   Templates     `Hello ${name}!` (template strings with interpolation)
 */

#ifndef KRY_EXPRESSION_H
#define KRY_EXPRESSION_H

#include <stddef.h>
#include <stdbool.h>

// Forward declaration for arrow registry
struct KryArrowRegistry;

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Expression AST Node Types
// ============================================================================

typedef enum {
    KRY_EXPR_LITERAL,      // "string", 42, true, false, null, undefined
    KRY_EXPR_IDENTIFIER,   // variableName
    KRY_EXPR_BINARY_OP,    // a + b, a == b, a && b
    KRY_EXPR_UNARY_OP,     // -a, !a, typeof a
    KRY_EXPR_PROPERTY_ACCESS,  // obj.prop, obj["prop"]
    KRY_EXPR_ELEMENT_ACCESS,   // arr[index]
    KRY_EXPR_CALL,         // func(arg1, arg2)
    KRY_EXPR_ARRAY,        // [1, 2, 3]
    KRY_EXPR_OBJECT,       // {key: val}
    KRY_EXPR_ARROW_FUNC,   // x => x * 2
    KRY_EXPR_MEMBER_EXPR,  // arr.length, arr.push(x)
    KRY_EXPR_CONDITIONAL,  // condition ? consequent : alternate
    KRY_EXPR_TEMPLATE      // Template strings: `Hello ${name}!`
} KryExprType;

// Forward declaration
struct KryExprNode;

// Literal value types
typedef enum {
    KRY_LITERAL_STRING,
    KRY_LITERAL_NUMBER,
    KRY_LITERAL_BOOLEAN,
    KRY_LITERAL_NULL,
    KRY_LITERAL_UNDEFINED
} KryLiteralType;

// Binary operator types
typedef enum {
    KRY_BIN_OP_ADD,        // +
    KRY_BIN_OP_SUB,        // -
    KRY_BIN_OP_MUL,        // *
    KRY_BIN_OP_DIV,        // /
    KRY_BIN_OP_MOD,        // %
    KRY_BIN_OP_EQ,         // ==
    KRY_BIN_OP_NEQ,        // !=
    KRY_BIN_OP_LT,         // <
    KRY_BIN_OP_GT,         // >
    KRY_BIN_OP_LTE,        // <=
    KRY_BIN_OP_GTE,        // >=
    KRY_BIN_OP_AND,        // &&
    KRY_BIN_OP_OR,         // ||
    KRY_BIN_OP_ASSIGN      // =
} KryBinaryOp;

// Unary operator types
typedef enum {
    KRY_UNARY_OP_NEGATE,   // -
    KRY_UNARY_OP_NOT,      // !
    KRY_UNARY_OP_TYPEOF    // typeof
} KryUnaryOp;

// Expression AST node
typedef struct KryExprNode {
    KryExprType type;

    union {
        // KRY_EXPR_LITERAL
        struct {
            KryLiteralType literal_type;
            union {
                char* string_val;
                double number_val;
                bool bool_val;
            };
        } literal;

        // KRY_EXPR_IDENTIFIER
        char* identifier;

        // KRY_EXPR_BINARY_OP
        struct {
            struct KryExprNode* left;
            KryBinaryOp op;
            struct KryExprNode* right;
        } binary_op;

        // KRY_EXPR_UNARY_OP
        struct {
            KryUnaryOp op;
            struct KryExprNode* operand;
        } unary_op;

        // KRY_EXPR_PROPERTY_ACCESS
        struct {
            struct KryExprNode* object;
            char* property;       // Property name (for obj.prop)
            bool is_computed;     // True for obj["prop"]
        } property_access;

        // KRY_EXPR_ELEMENT_ACCESS
        struct {
            struct KryExprNode* array;
            struct KryExprNode* index;
        } element_access;

        // KRY_EXPR_CALL
        struct {
            struct KryExprNode* callee;
            struct KryExprNode** args;
            size_t arg_count;
        } call;

        // KRY_EXPR_ARRAY
        struct {
            struct KryExprNode** elements;
            size_t element_count;
        } array;

        // KRY_EXPR_OBJECT
        struct {
            char** keys;
            struct KryExprNode** values;
            size_t prop_count;
        } object;

        // KRY_EXPR_ARROW_FUNC
        struct {
            char** params;
            size_t param_count;
            struct KryExprNode* body;
            bool is_expression_body;  // true for x => x*2, false for x => { return x; }
        } arrow_func;

        // KRY_EXPR_MEMBER_EXPR (for array methods like .length, .push, etc.)
        struct {
            struct KryExprNode* object;
            char* member;
        } member_expr;

        // KRY_EXPR_CONDITIONAL (ternary: condition ? consequent : alternate)
        struct {
            struct KryExprNode* condition;
            struct KryExprNode* consequent;
            struct KryExprNode* alternate;
        } conditional;

        // KRY_EXPR_TEMPLATE (template strings: `Hello ${name}!`)
        struct {
            char** parts;             // Literal string parts
            size_t part_count;        // Number of literal parts
            struct KryExprNode** expressions;  // Interpolated expressions
            size_t expr_count;        // Number of expressions
        } template_str;
    };
} KryExprNode;

// ============================================================================
// Transpiler Options
// ============================================================================

typedef enum {
    KRY_TARGET_LUA,
    KRY_TARGET_JAVASCRIPT,
    KRY_TARGET_C,    // Native C code generation
    KRY_TARGET_HARE  // Hare code generation
} KryExprTarget;

typedef struct {
    KryExprTarget target;
    bool pretty_print;
    int indent_level;
    struct KryArrowRegistry* arrow_registry;  // For C target: deferred arrow generation
    const char* context_hint;                 // Optional context hint: "click", "map", etc.
} KryExprOptions;

// ============================================================================
// API Functions
// ============================================================================

/**
 * Parse an expression string into an AST
 *
 * @param source The expression source code
 * @param error_output Optional error message output (set if parsing fails)
 * @return Parsed AST node, or NULL on error
 */
KryExprNode* kry_expr_parse(const char* source, char** error_output);

/**
 * Free an expression AST node
 */
void kry_expr_free(KryExprNode* node);

/**
 * Transpile expression AST to target platform code
 *
 * @param node The expression AST
 * @param options Transpilation options
 * @param output_length Optional output length (can be NULL)
 * @return Transpiled code (caller must free), or NULL on error
 */
char* kry_expr_transpile(KryExprNode* node, KryExprOptions* options, size_t* output_length);

/**
 * Convenience function: Parse and transpile in one step
 *
 * @param source Expression source code
 * @param target Target platform (Lua or JavaScript)
 * @param output_length Optional output length (can be NULL)
 * @return Transpiled code (caller must free), or NULL on error
 */
char* kry_expr_transpile_source(const char* source, KryExprTarget target, size_t* output_length);

/**
 * Get the last error message from the transpiler
 *
 * @return Error message, or NULL if no error
 */
const char* kry_expr_get_error(void);

/**
 * Clear the last error message
 */
void kry_expr_clear_error(void);

#ifdef __cplusplus
}
#endif

#endif // KRY_EXPRESSION_H
