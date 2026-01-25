/**
 * Kryon .kry to IR Converter - Expression Conversion
 *
 * Functions for converting KryExprNode and KryValue to IRExpression.
 */

#ifndef KRY_TO_IR_EXPRESSIONS_H
#define KRY_TO_IR_EXPRESSIONS_H

#include "kry_ast.h"
#include "kry_expression.h"
#include "ir_core.h"
#include "ir_expression.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Convert KryBinaryOp to IRBinaryOp
 */
IRBinaryOp kry_convert_binary_op(KryBinaryOp op);

/**
 * Convert KryUnaryOp to IRUnaryOp
 */
IRUnaryOp kry_convert_unary_op(KryUnaryOp op);

/**
 * Convert KryExprNode (from expression parser) to IRExpression
 *
 * @param node Parsed expression node
 * @return New IRExpression, or NULL on failure. Caller must free.
 */
IRExpression* kry_convert_expr_to_ir(KryExprNode* node);

/**
 * Convert KryValue to IRExpression
 *
 * @param value KryValue from AST
 * @return New IRExpression, or NULL on failure. Caller must free.
 */
IRExpression* kry_value_to_ir_expr(KryValue* value);

#ifdef __cplusplus
}
#endif

#endif // KRY_TO_IR_EXPRESSIONS_H
