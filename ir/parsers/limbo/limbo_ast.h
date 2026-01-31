/**
 * Limbo AST
 *
 * Abstract Syntax Tree for Limbo source code
 */

#ifndef IR_LIMBO_AST_H
#define IR_LIMBO_AST_H

#include "limbo_lexer.h"
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Forward declarations
 */
struct LimboNode;
struct LimboModule;
struct LimboFunction;
struct LimboStatement;
struct LimboExpression;

/**
 * Limbo node types
 */
typedef enum {
    NODE_MODULE,
    NODE_IMPORT,
    NODE_CON_DECLARATION,
    NODE_FUNCTION,
    NODE_VARIABLE,
    NODE_TYPE_DECLARATION,

    // Statements
    NODE_BLOCK,
    NODE_IF,
    NODE_WHILE,
    NODE_DO_WHILE,
    NODE_FOR,
    NODE_CASE,
    NODE_ALT,
    NODE_RETURN,
    NODE_BREAK,
    NODE_CONTINUE,
    NODE_EXPRESSION_STMT,

    // Expressions
    NODE_IDENTIFIER,
    NODE_LITERAL,
    NODE_BINARY_OP,
    NODE_UNARY_OP,
    NODE_CALL,
    NODE_INDEX,
    NODE_FIELD,
    NODE_LIST_LITERAL,
    NODE_ARRAY_LITERAL,
    NODE_TUPLE,

    // Types
    NODE_TYPE_BASIC,
    NODE_TYPE_FUNCTION,
    NODE_TYPE_LIST,
    NODE_TYPE_ARRAY,
    NODE_TYPE_CHANNEL,
    NODE_TYPE_ADT,

    NODE_UNKNOWN
} LimboNodeType;

/**
 * Limbo expression
 */
typedef struct LimboExpression {
    LimboNodeType type;
    LimboToken token;

    union {
        // Identifier
        struct {
            const char* name;
        } identifier;

        // Literals
        struct {
            const char* string_value;
        } string_literal;

        struct {
            long integer_value;
        } integer_literal;

        struct {
            double real_value;
        } real_literal;

        // Binary operation
        struct {
            struct LimboExpression* left;
            LimboTokenType op;
            struct LimboExpression* right;
        } binary_op;

        // Unary operation
        struct {
            LimboTokenType op;
            struct LimboExpression* operand;
        } unary_op;

        // Function call
        struct {
            struct LimboExpression* function;
            struct LimboExpression** args;
            size_t arg_count;
        } call;

        // Index expression
        struct {
            struct LimboExpression* container;
            struct LimboExpression* index;
        } index;

        // Field access
        struct {
            struct LimboExpression* object;
            const char* field;
        } field;

        // List/array literal
        struct {
            struct LimboExpression** elements;
            size_t element_count;
        } list_literal;

    } as;
} LimboExpression;

/**
 * Limbo statement
 */
typedef struct LimboStatement {
    LimboNodeType type;
    LimboToken token;

    union {
        // Block
        struct {
            struct LimboStatement** statements;
            size_t statement_count;
        } block;

        // If statement
        struct {
            LimboExpression* condition;
            struct LimboStatement* then_block;
            struct LimboStatement* else_block;
        } if_stmt;

        // While loop
        struct {
            LimboExpression* condition;
            struct LimboStatement* body;
        } while_stmt;

        // Do-while loop
        struct {
            struct LimboStatement* body;
            LimboExpression* condition;
        } do_while_stmt;

        // For loop
        struct {
            struct LimboStatement* init;
            LimboExpression* condition;
            struct LimboStatement* post;
            struct LimboStatement* body;
        } for_stmt;

        // Return statement
        struct {
            LimboExpression* value;
        } return_stmt;

        // Expression statement
        struct {
            LimboExpression* expr;
        } expression_stmt;

    } as;
} LimboStatement;

/**
 * Limbo function
 */
typedef struct LimboFunction {
    const char* name;
    LimboToken token;

    // Return type
    const char* return_type;

    // Parameters
    struct {
        const char** names;
        const char** types;
        size_t count;
    } parameters;

    // Function body
    LimboStatement* body;

    // Is this a method (has receiver)
    bool is_method;

    // For methods: receiver type and name
    const char* receiver_type;
    const char* receiver_name;
} LimboFunction;

/**
 * Limbo constant declaration
 */
typedef struct LimboConstant {
    const char* name;
    LimboToken token;
    const char* type;
    LimboExpression* value;
} LimboConstant;

/**
 * Limbo variable declaration
 */
typedef struct LimboVariable {
    const char* name;
    LimboToken token;
    const char* type;
    LimboExpression* initializer;
    bool is_const;
} LimboVariable;

/**
 * Limbo import declaration
 */
typedef struct LimboImport {
    const char* module_path;
    LimboToken token;
    const char* alias;  // Optional alias for "import module as alias"
} LimboImport;

/**
 * Limbo module
 */
typedef struct LimboModule {
    const char* name;
    LimboToken token;

    // Module content
    LimboImport** imports;
    size_t import_count;

    LimboConstant** constants;
    size_t constant_count;

    LimboFunction** functions;
    size_t function_count;

    LimboVariable** variables;
    size_t variable_count;

    // Interface (public declarations)
    struct {
        const char** function_names;
        size_t function_count;
    } interface;
} LimboModule;

/**
 * Create module node
 */
LimboModule* limbo_ast_module_create(const char* name, LimboToken token);

/**
 * Free module and all its children
 */
void limbo_ast_module_free(LimboModule* module);

/**
 * Create function node
 */
LimboFunction* limbo_ast_function_create(const char* name, LimboToken token);

/**
 * Free function
 */
void limbo_ast_function_free(LimboFunction* function);

/**
 * Create statement node
 */
LimboStatement* limbo_ast_statement_create(LimboNodeType type, LimboToken token);

/**
 * Free statement
 */
void limbo_ast_statement_free(LimboStatement* statement);

/**
 * Create expression node
 */
LimboExpression* limbo_ast_expression_create(LimboNodeType type, LimboToken token);

/**
 * Free expression
 */
void limbo_ast_expression_free(LimboExpression* expression);

#ifdef __cplusplus
}
#endif

#endif // IR_LIMBO_AST_H
