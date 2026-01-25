/**
 * Kryon .kry to IR Converter - Expression Conversion
 */

#define _POSIX_C_SOURCE 200809L
#include "kry_to_ir_expressions.h"
#include "ir_expression.h"
#include <stdlib.h>
#include <string.h>

// ============================================================================
// Operator Conversion
// ============================================================================

IRBinaryOp kry_convert_binary_op(KryBinaryOp op) {
    switch (op) {
        case KRY_BIN_OP_ADD: return BINARY_OP_ADD;
        case KRY_BIN_OP_SUB: return BINARY_OP_SUB;
        case KRY_BIN_OP_MUL: return BINARY_OP_MUL;
        case KRY_BIN_OP_DIV: return BINARY_OP_DIV;
        case KRY_BIN_OP_MOD: return BINARY_OP_MOD;
        case KRY_BIN_OP_EQ:  return BINARY_OP_EQ;
        case KRY_BIN_OP_NEQ: return BINARY_OP_NEQ;
        case KRY_BIN_OP_LT:  return BINARY_OP_LT;
        case KRY_BIN_OP_GT:  return BINARY_OP_GT;
        case KRY_BIN_OP_LTE: return BINARY_OP_LTE;
        case KRY_BIN_OP_GTE: return BINARY_OP_GTE;
        case KRY_BIN_OP_AND: return BINARY_OP_AND;
        case KRY_BIN_OP_OR:  return BINARY_OP_OR;
        case KRY_BIN_OP_ASSIGN: return BINARY_OP_ADD; // Assignment handled separately
        default: return BINARY_OP_ADD;
    }
}

IRUnaryOp kry_convert_unary_op(KryUnaryOp op) {
    switch (op) {
        case KRY_UNARY_OP_NEGATE: return UNARY_OP_NEG;
        case KRY_UNARY_OP_NOT:    return UNARY_OP_NOT;
        case KRY_UNARY_OP_TYPEOF: return UNARY_OP_TYPEOF;
        default: return UNARY_OP_NEG;
    }
}

// ============================================================================
// KryExprNode to IRExpression Converter
// ============================================================================

IRExpression* kry_convert_expr_to_ir(KryExprNode* node) {
    if (!node) return NULL;

    switch (node->type) {
        case KRY_EXPR_LITERAL:
            switch (node->literal.literal_type) {
                case KRY_LITERAL_STRING:
                    return ir_expr_string(node->literal.string_val);
                case KRY_LITERAL_NUMBER:
                    return ir_expr_float(node->literal.number_val);
                case KRY_LITERAL_BOOLEAN:
                    return ir_expr_bool(node->literal.bool_val);
                case KRY_LITERAL_NULL:
                case KRY_LITERAL_UNDEFINED:
                    return ir_expr_null();
                default:
                    return NULL;
            }

        case KRY_EXPR_IDENTIFIER:
            return ir_expr_var(node->identifier);

        case KRY_EXPR_BINARY_OP: {
            IRExpression* left = kry_convert_expr_to_ir(node->binary_op.left);
            IRExpression* right = kry_convert_expr_to_ir(node->binary_op.right);
            if (!left || !right) {
                if (left) ir_expr_free(left);
                if (right) ir_expr_free(right);
                return NULL;
            }
            return ir_expr_binary(kry_convert_binary_op(node->binary_op.op), left, right);
        }

        case KRY_EXPR_UNARY_OP: {
            IRExpression* operand = kry_convert_expr_to_ir(node->unary_op.operand);
            if (!operand) return NULL;
            return ir_expr_unary(kry_convert_unary_op(node->unary_op.op), operand);
        }

        case KRY_EXPR_PROPERTY_ACCESS: {
            IRExpression* obj = kry_convert_expr_to_ir(node->property_access.object);
            if (!obj) return NULL;
            return ir_expr_member_access(obj, node->property_access.property);
        }

        case KRY_EXPR_ELEMENT_ACCESS: {
            IRExpression* arr = kry_convert_expr_to_ir(node->element_access.array);
            IRExpression* idx = kry_convert_expr_to_ir(node->element_access.index);
            if (!arr || !idx) {
                if (arr) ir_expr_free(arr);
                if (idx) ir_expr_free(idx);
                return NULL;
            }
            return ir_expr_index(arr, idx);
        }

        case KRY_EXPR_CALL: {
            // For function calls, extract function name from callee
            if (node->call.callee->type == KRY_EXPR_IDENTIFIER) {
                IRExpression** args = NULL;
                if (node->call.arg_count > 0) {
                    args = calloc(node->call.arg_count, sizeof(IRExpression*));
                    for (size_t i = 0; i < node->call.arg_count; i++) {
                        args[i] = kry_convert_expr_to_ir(node->call.args[i]);
                    }
                }
                return ir_expr_call(node->call.callee->identifier, args, (int)node->call.arg_count);
            } else if (node->call.callee->type == KRY_EXPR_PROPERTY_ACCESS) {
                // Method call: obj.method(args)
                IRExpression* receiver = kry_convert_expr_to_ir(node->call.callee->property_access.object);
                IRExpression** args = NULL;
                if (node->call.arg_count > 0) {
                    args = calloc(node->call.arg_count, sizeof(IRExpression*));
                    for (size_t i = 0; i < node->call.arg_count; i++) {
                        args[i] = kry_convert_expr_to_ir(node->call.args[i]);
                    }
                }
                return ir_expr_method_call(receiver, node->call.callee->property_access.property,
                                           args, (int)node->call.arg_count);
            }
            return NULL;
        }

        case KRY_EXPR_ARRAY: {
            int count = (int)node->array.element_count;
            IRExpression** elements = NULL;
            if (count > 0) {
                elements = calloc(count, sizeof(IRExpression*));
                bool failed = false;
                for (int i = 0; i < count; i++) {
                    elements[i] = kry_convert_expr_to_ir(node->array.elements[i]);
                    if (!elements[i]) {
                        failed = true;
                        break;
                    }
                }
                if (failed) {
                    for (int i = 0; i < count; i++) {
                        if (elements[i]) ir_expr_free(elements[i]);
                    }
                    free(elements);
                    return NULL;
                }
            }
            IRExpression* result = ir_expr_array_literal(elements, count);
            free(elements);
            return result;
        }

        case KRY_EXPR_OBJECT: {
            int count = (int)node->object.prop_count;
            char** keys = NULL;
            IRExpression** values = NULL;
            if (count > 0) {
                keys = calloc(count, sizeof(char*));
                values = calloc(count, sizeof(IRExpression*));
                bool failed = false;
                for (int i = 0; i < count; i++) {
                    keys[i] = strdup(node->object.keys[i] ? node->object.keys[i] : "");
                    values[i] = kry_convert_expr_to_ir(node->object.values[i]);
                    if (!values[i]) {
                        failed = true;
                        break;
                    }
                }
                if (failed) {
                    for (int i = 0; i < count; i++) {
                        free(keys[i]);
                        if (values[i]) ir_expr_free(values[i]);
                    }
                    free(keys);
                    free(values);
                    return NULL;
                }
            }
            IRExpression* result = ir_expr_object_literal(keys, values, count);
            // Free temporary arrays (keys and values are copied in ir_expr_object_literal)
            for (int i = 0; i < count; i++) {
                free(keys[i]);
            }
            free(keys);
            free(values);
            return result;
        }

        case KRY_EXPR_ARROW_FUNC: {
            int param_count = (int)node->arrow_func.param_count;
            char** params = NULL;
            if (param_count > 0) {
                params = calloc(param_count, sizeof(char*));
                for (int i = 0; i < param_count; i++) {
                    params[i] = strdup(node->arrow_func.params[i] ? node->arrow_func.params[i] : "");
                }
            }
            IRExpression* body = kry_convert_expr_to_ir(node->arrow_func.body);
            if (!body && node->arrow_func.body) {
                // Body conversion failed
                for (int i = 0; i < param_count; i++) free(params[i]);
                free(params);
                return NULL;
            }
            IRExpression* result = ir_expr_arrow_function(params, param_count, body, node->arrow_func.is_expression_body);
            // Free temporary params array (params are copied in ir_expr_arrow_function)
            for (int i = 0; i < param_count; i++) {
                free(params[i]);
            }
            free(params);
            return result;
        }

        case KRY_EXPR_MEMBER_EXPR: {
            IRExpression* obj = kry_convert_expr_to_ir(node->member_expr.object);
            if (!obj) return NULL;
            return ir_expr_member_access(obj, node->member_expr.member);
        }

        default:
            return NULL;
    }
}

// ============================================================================
// KryValue to IRExpression Converter
// ============================================================================

IRExpression* kry_value_to_ir_expr(KryValue* value) {
    if (!value) return NULL;

    switch (value->type) {
        case KRY_VALUE_IDENTIFIER:
            return ir_expr_var(value->identifier);
        case KRY_VALUE_STRING:
            return ir_expr_string(value->string_value);
        case KRY_VALUE_NUMBER:
            return ir_expr_float(value->number_value);
        case KRY_VALUE_EXPRESSION: {
            // Parse the expression string and convert to structured IR
            char* error = NULL;
            KryExprNode* parsed = kry_expr_parse(value->expression, &error);
            if (parsed) {
                IRExpression* result = kry_convert_expr_to_ir(parsed);
                kry_expr_free(parsed);
                if (result) {
                    return result;
                }
            }
            if (error) {
                free(error);
            }
            // Fallback: treat as variable reference (for backward compatibility)
            return ir_expr_var(value->expression);
        }
        case KRY_VALUE_ARRAY: {
            int count = (int)value->array.count;
            IRExpression** elements = NULL;
            if (count > 0) {
                elements = calloc(count, sizeof(IRExpression*));
                for (int i = 0; i < count; i++) {
                    elements[i] = kry_value_to_ir_expr(value->array.elements[i]);
                    if (!elements[i]) {
                        for (int j = 0; j < i; j++) ir_expr_free(elements[j]);
                        free(elements);
                        return NULL;
                    }
                }
            }
            IRExpression* result = ir_expr_array_literal(elements, count);
            free(elements);
            return result;
        }
        case KRY_VALUE_OBJECT: {
            int count = (int)value->object.count;
            char** keys = NULL;
            IRExpression** values_arr = NULL;
            if (count > 0) {
                keys = calloc(count, sizeof(char*));
                values_arr = calloc(count, sizeof(IRExpression*));
                for (int i = 0; i < count; i++) {
                    keys[i] = strdup(value->object.keys[i] ? value->object.keys[i] : "");
                    values_arr[i] = kry_value_to_ir_expr(value->object.values[i]);
                    if (!values_arr[i]) {
                        for (int j = 0; j <= i; j++) {
                            free(keys[j]);
                            if (values_arr[j]) ir_expr_free(values_arr[j]);
                        }
                        free(keys);
                        free(values_arr);
                        return NULL;
                    }
                }
            }
            IRExpression* result = ir_expr_object_literal(keys, values_arr, count);
            // Free temporary arrays (keys and values are copied in ir_expr_object_literal)
            for (int i = 0; i < count; i++) {
                free(keys[i]);
            }
            free(keys);
            free(values_arr);
            return result;
        }
        case KRY_VALUE_RANGE: {
            // Range expression: start..end
            // Convert to a __range__(start, end) call that transpiler handles
            IRExpression* start_expr = kry_value_to_ir_expr(value->range.start);
            IRExpression* end_expr = kry_value_to_ir_expr(value->range.end);
            if (!start_expr || !end_expr) {
                return NULL;
            }
            IRExpression* args[2] = {start_expr, end_expr};
            return ir_expr_call("__range__", args, 2);
        }
        default:
            return NULL;
    }
}
