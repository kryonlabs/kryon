#ifndef IR_EXPRESSION_H
#define IR_EXPRESSION_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// ============================================================================
// UNIVERSAL EXPRESSION AST
// ============================================================================
// This defines a portable expression language that can be transpiled to
// any target language (Lua, JavaScript, C, etc.)

// Binary operators
typedef enum {
    BINARY_OP_ADD,        // +
    BINARY_OP_SUB,        // -
    BINARY_OP_MUL,        // *
    BINARY_OP_DIV,        // /
    BINARY_OP_MOD,        // %
    BINARY_OP_EQ,         // ==
    BINARY_OP_NEQ,        // !=
    BINARY_OP_LT,         // <
    BINARY_OP_LTE,        // <=
    BINARY_OP_GT,         // >
    BINARY_OP_GTE,        // >=
    BINARY_OP_AND,        // &&
    BINARY_OP_OR,         // ||
    BINARY_OP_CONCAT,     // string concatenation
} IRBinaryOp;

// Unary operators
typedef enum {
    UNARY_OP_NEG,         // -x
    UNARY_OP_NOT,         // !x
    UNARY_OP_TYPEOF,      // typeof x
} IRUnaryOp;

// Expression types
typedef enum {
    EXPR_LITERAL_INT,
    EXPR_LITERAL_FLOAT,
    EXPR_LITERAL_STRING,
    EXPR_LITERAL_BOOL,
    EXPR_LITERAL_NULL,
    EXPR_VAR_REF,         // {"var": "name"}
    EXPR_PROPERTY,        // {"prop": "obj", "field": "name"}
    EXPR_INDEX,           // {"index": expr, "at": n}
    EXPR_BINARY,          // {"op": "add", "left": ..., "right": ...}
    EXPR_UNARY,           // {"op": "neg", "operand": ...}
    EXPR_CALL,            // {"op": "call", "function": "name", "args": [...]}
    EXPR_TERNARY,         // {"op": "ternary", "condition": ..., "then": ..., "else": ...}
    // NEW: Enhanced expression types for Phase 1
    EXPR_MEMBER_ACCESS,    // Member access: obj.prop where obj is an expression
    EXPR_COMPUTED_MEMBER, // Computed member: obj[key] where key is an expression
    EXPR_METHOD_CALL,     // Method call: obj.method(args)
    EXPR_GROUP,           // Grouped expression: (expr) for precedence
    // NEW: Literal and function expression types
    EXPR_ARRAY_LITERAL,   // Array literal: [1, 2, 3]
    EXPR_OBJECT_LITERAL,  // Object literal: {key: value}
    EXPR_ARROW_FUNCTION,  // Arrow function: (x) => x * 2
} IRExprType;

// Forward declarations
struct IRExpression;
struct IRStatement;

// Expression structure
typedef struct IRExpression {
    IRExprType type;
    union {
        int64_t int_value;
        double float_value;
        char* string_value;
        bool bool_value;

        struct {
            char* name;
            char* scope;  // Optional: "Counter:value" → scope="Counter", name="value"
        } var_ref;

        struct {
            char* object;
            char* field;
        } property;

        struct {
            struct IRExpression* array;
            struct IRExpression* index;
        } index_access;

        struct {
            IRBinaryOp op;
            struct IRExpression* left;
            struct IRExpression* right;
        } binary;

        struct {
            IRUnaryOp op;
            struct IRExpression* operand;
        } unary;

        struct {
            char* function;
            struct IRExpression** args;
            int arg_count;
        } call;

        struct {
            struct IRExpression* condition;
            struct IRExpression* then_expr;
            struct IRExpression* else_expr;
        } ternary;

        // NEW: Enhanced expression types for Phase 1
        struct {
            struct IRExpression* object;    // The object being accessed
            char* property;                  // Property name (static)
            uint32_t property_hash;          // Pre-computed hash for fast lookup
        } member_access;

        struct {
            struct IRExpression* object;    // The object being accessed
            struct IRExpression* key;        // Dynamic key expression
        } computed_member;

        struct {
            struct IRExpression* receiver;  // Object receiving the call
            char* method_name;               // Method name
            struct IRExpression** args;     // Argument expressions
            int arg_count;
        } method_call;

        struct {
            struct IRExpression* inner;     // Inner expression
        } group;

        // NEW: Array literal: [1, 2, 3]
        struct {
            struct IRExpression** elements;
            int element_count;
        } array_literal;

        // NEW: Object literal: {key: value}
        struct {
            char** keys;
            struct IRExpression** values;
            int property_count;
        } object_literal;

        // NEW: Arrow function: (x) => x * 2
        struct {
            char** params;
            int param_count;
            struct IRExpression* body;
            bool is_expression_body;
        } arrow_function;
    };
} IRExpression;

// Statement types
typedef enum {
    STMT_ASSIGN,          // {"op": "assign", "target": "x", "expr": ...}
    STMT_ASSIGN_OP,       // {"op": "assign_add", "target": "x", "expr": ...}
    STMT_IF,              // {"op": "if", "condition": ..., "then": [...], "else": [...]}
    STMT_WHILE,           // {"op": "while", "condition": ..., "body": [...]}
    STMT_FOR_EACH,        // {"op": "for_each", "item": "x", "in": ..., "body": [...]}
    STMT_CALL,            // {"op": "call", "function": "f", "args": [...]}
    STMT_RETURN,          // {"op": "return", "value": ...}
    STMT_BREAK,           // {"op": "break"}
    STMT_CONTINUE,        // {"op": "continue"}
    STMT_DELETE,          // {"op": "delete", "target": ...}
} IRStmtType;

// Assignment operators for STMT_ASSIGN_OP
typedef enum {
    ASSIGN_OP_ADD,        // +=
    ASSIGN_OP_SUB,        // -=
    ASSIGN_OP_MUL,        // *=
    ASSIGN_OP_DIV,        // /=
} IRAssignOp;

// Statement structure
typedef struct IRStatement {
    IRStmtType type;
    union {
        struct {
            char* target;
            char* target_scope;  // Optional scope
            IRExpression* expr;
        } assign;

        struct {
            char* target;
            char* target_scope;
            IRAssignOp op;
            IRExpression* expr;
        } assign_op;

        struct {
            IRExpression* condition;
            struct IRStatement** then_body;
            int then_count;
            struct IRStatement** else_body;
            int else_count;
        } if_stmt;

        struct {
            IRExpression* condition;
            struct IRStatement** body;
            int body_count;
        } while_stmt;

        struct {
            char* item_name;
            IRExpression* iterable;
            struct IRStatement** body;
            int body_count;
        } for_each;

        struct {
            char* function;
            IRExpression** args;
            int arg_count;
        } call;

        struct {
            IRExpression* value;  // NULL for void return
        } return_stmt;

        struct {
            IRExpression* target;  // Expression identifying what to delete
        } delete_stmt;
    };
} IRStatement;

// ============================================================================
// CONSTRUCTOR FUNCTIONS
// ============================================================================

// Literal expressions
IRExpression* ir_expr_int(int64_t value);
IRExpression* ir_expr_float(double value);
IRExpression* ir_expr_string(const char* value);
IRExpression* ir_expr_bool(bool value);
IRExpression* ir_expr_null(void);

// Reference expressions
IRExpression* ir_expr_var(const char* name);
IRExpression* ir_expr_var_scoped(const char* scope, const char* name);
IRExpression* ir_expr_property(const char* object, const char* field);
IRExpression* ir_expr_index(IRExpression* array, IRExpression* index);

// Operator expressions
IRExpression* ir_expr_binary(IRBinaryOp op, IRExpression* left, IRExpression* right);
IRExpression* ir_expr_unary(IRUnaryOp op, IRExpression* operand);
IRExpression* ir_expr_ternary(IRExpression* condition, IRExpression* then_expr, IRExpression* else_expr);

// Call expression
IRExpression* ir_expr_call(const char* function, IRExpression** args, int arg_count);

// NEW: Enhanced expression constructors for Phase 1
IRExpression* ir_expr_member_access(IRExpression* object, const char* property);
IRExpression* ir_expr_computed_member(IRExpression* object, IRExpression* key);
IRExpression* ir_expr_method_call(IRExpression* receiver, const char* method_name, IRExpression** args, int arg_count);
IRExpression* ir_expr_group(IRExpression* inner);

// NEW: Literal and function expression constructors
IRExpression* ir_expr_array_literal(IRExpression** elements, int count);
IRExpression* ir_expr_object_literal(char** keys, IRExpression** values, int count);
IRExpression* ir_expr_arrow_function(char** params, int param_count, IRExpression* body, bool is_expr_body);

// Convenience binary operators
IRExpression* ir_expr_add(IRExpression* left, IRExpression* right);
IRExpression* ir_expr_sub(IRExpression* left, IRExpression* right);
IRExpression* ir_expr_mul(IRExpression* left, IRExpression* right);
IRExpression* ir_expr_div(IRExpression* left, IRExpression* right);
IRExpression* ir_expr_eq(IRExpression* left, IRExpression* right);
IRExpression* ir_expr_neq(IRExpression* left, IRExpression* right);
IRExpression* ir_expr_lt(IRExpression* left, IRExpression* right);
IRExpression* ir_expr_gt(IRExpression* left, IRExpression* right);
IRExpression* ir_expr_and(IRExpression* left, IRExpression* right);
IRExpression* ir_expr_or(IRExpression* left, IRExpression* right);

// ============================================================================
// STATEMENT CONSTRUCTORS
// ============================================================================

IRStatement* ir_stmt_assign(const char* target, IRExpression* expr);
IRStatement* ir_stmt_assign_scoped(const char* scope, const char* target, IRExpression* expr);
IRStatement* ir_stmt_assign_op(const char* target, IRAssignOp op, IRExpression* expr);
IRStatement* ir_stmt_if(IRExpression* condition, IRStatement** then_body, int then_count,
                        IRStatement** else_body, int else_count);
IRStatement* ir_stmt_while(IRExpression* condition, IRStatement** body, int body_count);
IRStatement* ir_stmt_for_each(const char* item, IRExpression* iterable,
                              IRStatement** body, int body_count);
IRStatement* ir_stmt_call(const char* function, IRExpression** args, int arg_count);
IRStatement* ir_stmt_return(IRExpression* value);
IRStatement* ir_stmt_break(void);
IRStatement* ir_stmt_continue(void);
IRStatement* ir_stmt_delete(IRExpression* target);

// ============================================================================
// MEMORY MANAGEMENT
// ============================================================================

void ir_expr_free(IRExpression* expr);
void ir_stmt_free(IRStatement* stmt);
void ir_stmt_array_free(IRStatement** stmts, int count);

// ============================================================================
// JSON SERIALIZATION
// ============================================================================

// Forward declaration of cJSON
struct cJSON;

// Serialize expression to JSON
struct cJSON* ir_expr_to_json(IRExpression* expr);

// Serialize statement to JSON
struct cJSON* ir_stmt_to_json(IRStatement* stmt);

// Parse expression from JSON
IRExpression* ir_expr_from_json(struct cJSON* json);

// Parse statement from JSON
IRStatement* ir_stmt_from_json(struct cJSON* json);

// Parse statement array from JSON array
IRStatement** ir_stmts_from_json(struct cJSON* json_array, int* out_count);

// ============================================================================
// DEBUG / PRINT
// ============================================================================

// Print expression as human-readable string (for debugging)
void ir_expr_print(IRExpression* expr);

// Print statement as human-readable string
void ir_stmt_print(IRStatement* stmt, int indent);

#endif // IR_EXPRESSION_H
