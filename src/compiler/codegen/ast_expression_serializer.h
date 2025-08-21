/**
 * @file ast_expression_serializer.h
 * @brief AST Expression to String Serializer
 * 
 * Converts complex AST expressions (binary operations, member access, etc.)
 * back to string representations for storing as reactive references.
 * 
 * This enables expressions like "$root.height - 200" to be stored as strings
 * in the KRB format and evaluated at runtime.
 */

#ifndef KRYON_AST_EXPRESSION_SERIALIZER_H
#define KRYON_AST_EXPRESSION_SERIALIZER_H

#include "parser.h"

// Forward declaration
typedef struct KryonCodeGenerator KryonCodeGenerator;

/**
 * @brief Convert an AST expression node to its string representation
 * @param node The AST node to serialize (should be an expression type)  
 * @param codegen Code generator context for constant resolution
 * @return Allocated string containing the expression, or NULL on error
 * @note Caller must free the returned string
 */
char* kryon_ast_expression_to_string(const KryonASTNode* node, KryonCodeGenerator *codegen);

/**
 * @brief Lookup a constant value by name from the const table
 * @param codegen Code generator context containing const table
 * @param name Constant name to lookup
 * @return Constant value as string, or NULL if not found
 * @note The returned string is managed by the const table and should not be freed
 */
const char *lookup_constant_value(KryonCodeGenerator *codegen, const char *name);

/**
 * @brief Convert a binary operation node to string
 * @param node Binary operation node
 * @param codegen Code generator context for constant resolution
 * @return Allocated string with format "left operator right"
 */
char* kryon_ast_binary_op_to_string(const KryonASTNode* node, KryonCodeGenerator *codegen);

/**
 * @brief Convert a member access node to string  
 * @param node Member access node
 * @param codegen Code generator context for constant resolution
 * @return Allocated string with format "object.member"
 */
char* kryon_ast_member_access_to_string(const KryonASTNode* node, KryonCodeGenerator *codegen);

/**
 * @brief Convert a variable node to string
 * @param node Variable node
 * @param codegen Code generator context (unused for variables - they remain reactive)  
 * @return Allocated string with format "$variableName"
 */
char* kryon_ast_variable_to_string(const KryonASTNode* node, KryonCodeGenerator *codegen);

/**
 * @brief Convert a literal node to string
 * @param node Literal node
 * @param codegen Code generator context (unused for literals)
 * @return Allocated string representation of the literal value
 */
char* kryon_ast_literal_to_string(const KryonASTNode* node, KryonCodeGenerator *codegen);

#endif // KRYON_AST_EXPRESSION_SERIALIZER_H