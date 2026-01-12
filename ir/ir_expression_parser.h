/**
 * IR Expression Parser
 *
 * Parses expression strings into IRExpression AST nodes.
 * Supports:
 *   - Member access: obj.prop, obj.prop.nested
 *   - Computed member: obj[key], obj[0]
 *   - Method calls: obj.method(arg1, arg2)
 *   - Function calls: func(arg1, arg2)
 *   - Ternary: condition ? then : else
 *   - Binary operators: +, -, *, /, %, ==, !=, <, >, <=, >=, &&, ||
 *   - Unary operators: !, -
 *   - Literals: integers, floats, strings, booleans, null
 *   - Grouping: (expr)
 *   - Arrays: [1, 2, 3]
 *   - Objects: {key: value, key2: value2}
 */

#ifndef IR_EXPRESSION_PARSER_H
#define IR_EXPRESSION_PARSER_H

#include "ir_expression.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Parse expression string into AST
 *
 * @param source Expression source code (e.g., "user.profile.name", "count + 1")
 * @return IRExpression* Parsed expression, or NULL on error
 *
 * @note Caller is responsible for freeing the returned expression with ir_expr_free()
 *
 * @example
 *   IRExpression* expr = ir_parse_expression("user.profile.name");
 *   // expr->type == EXPR_MEMBER_ACCESS
 *   ir_expr_free(expr);
 */
IRExpression* ir_parse_expression(const char* source);

/**
 * Parse expression with error reporting
 *
 * @param source Expression source code
 * @param error_message Output parameter for error message (caller must free)
 * @param error_position Output parameter for error position in source
 * @return IRExpression* Parsed expression, or NULL on error
 */
IRExpression* ir_parse_expression_ex(const char* source,
                                     char** error_message,
                                     uint32_t* error_position);

/**
 * Get parser version string
 */
const char* ir_expression_parser_version(void);

#ifdef __cplusplus
}
#endif

#endif // IR_EXPRESSION_PARSER_H
