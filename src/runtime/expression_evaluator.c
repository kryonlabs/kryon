/**
 * @file expression_evaluator.c
 * @brief Runtime expression evaluation for Kryon
 * 
 * Evaluates parsed expression AST nodes at runtime, supporting arithmetic,
 * comparison, logical operations and variable resolution.
 */

#include "runtime.h"
#include "types.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>

// =============================================================================
// INTERNAL HELPERS
// =============================================================================

static double resolve_variable_as_number(KryonRuntime *runtime, const char *var_name) {
    if (!runtime || !var_name) return 0.0;
    
    const char *value = kryon_runtime_get_variable(runtime, var_name);
    if (!value) return 0.0;
    
    return strtod(value, NULL);
}

static const char *resolve_variable_as_string(KryonRuntime *runtime, const char *var_name) {
    if (!runtime || !var_name) return "";
    
    const char *value = kryon_runtime_get_variable(runtime, var_name);
    return value ? value : "";
}

static bool resolve_variable_as_boolean(KryonRuntime *runtime, const char *var_name) {
    if (!runtime || !var_name) return false;
    
    const char *value = kryon_runtime_get_variable(runtime, var_name);
    if (!value) return false;
    
    // Consider "true", "1", non-empty strings as true
    return (strcmp(value, "true") == 0 || 
            strcmp(value, "1") == 0 || 
            (strlen(value) > 0 && strcmp(value, "false") != 0 && strcmp(value, "0") != 0));
}

// =============================================================================
// EXPRESSION EVALUATION
// =============================================================================

KryonExpressionValue kryon_runtime_evaluate_expression(const KryonExpressionNode *expression, KryonRuntime *runtime) {
    KryonExpressionValue result = {0};
    
    if (!expression) {
        result.type = KRYON_EXPR_VALUE_NUMBER;
        result.data.number_value = 0.0;
        return result;
    }
    
    switch (expression->type) {
        case KRYON_EXPR_NODE_VALUE: {
            // Return the literal value
            return expression->data.value;
        }
        
        case KRYON_EXPR_NODE_BINARY_OP: {
            KryonExpressionValue left = kryon_runtime_evaluate_expression(expression->data.binary_op.left, runtime);
            KryonExpressionValue right = kryon_runtime_evaluate_expression(expression->data.binary_op.right, runtime);
            
            // Handle variable references
            if (left.type == KRYON_EXPR_VALUE_VARIABLE_REF) {
                double val = resolve_variable_as_number(runtime, left.data.variable_name);
                left.type = KRYON_EXPR_VALUE_NUMBER;
                left.data.number_value = val;
            }
            if (right.type == KRYON_EXPR_VALUE_VARIABLE_REF) {
                double val = resolve_variable_as_number(runtime, right.data.variable_name);
                right.type = KRYON_EXPR_VALUE_NUMBER;
                right.data.number_value = val;
            }
            
            switch (expression->data.binary_op.operator) {
                case KRYON_EXPR_OP_ADD:
                    result.type = KRYON_EXPR_VALUE_NUMBER;
                    result.data.number_value = kryon_expression_value_to_number(&left) + kryon_expression_value_to_number(&right);
                    break;
                    
                case KRYON_EXPR_OP_SUBTRACT:
                    result.type = KRYON_EXPR_VALUE_NUMBER;
                    result.data.number_value = kryon_expression_value_to_number(&left) - kryon_expression_value_to_number(&right);
                    break;
                    
                case KRYON_EXPR_OP_MULTIPLY:
                    result.type = KRYON_EXPR_VALUE_NUMBER;
                    result.data.number_value = kryon_expression_value_to_number(&left) * kryon_expression_value_to_number(&right);
                    break;
                    
                case KRYON_EXPR_OP_DIVIDE:
                    result.type = KRYON_EXPR_VALUE_NUMBER;
                    {
                        double right_val = kryon_expression_value_to_number(&right);
                        if (right_val != 0.0) {
                            result.data.number_value = kryon_expression_value_to_number(&left) / right_val;
                        } else {
                            result.data.number_value = 0.0; // Handle division by zero
                        }
                    }
                    break;
                    
                case KRYON_EXPR_OP_MODULO:
                    result.type = KRYON_EXPR_VALUE_NUMBER;
                    {
                        double right_val = kryon_expression_value_to_number(&right);
                        if (right_val != 0.0) {
                            result.data.number_value = fmod(kryon_expression_value_to_number(&left), right_val);
                        } else {
                            result.data.number_value = 0.0;
                        }
                    }
                    break;
                    
                case KRYON_EXPR_OP_EQUAL:
                    result.type = KRYON_EXPR_VALUE_BOOLEAN;
                    result.data.boolean_value = (fabs(kryon_expression_value_to_number(&left) - kryon_expression_value_to_number(&right)) < 1e-9);
                    break;
                    
                case KRYON_EXPR_OP_NOT_EQUAL:
                    result.type = KRYON_EXPR_VALUE_BOOLEAN;
                    result.data.boolean_value = (fabs(kryon_expression_value_to_number(&left) - kryon_expression_value_to_number(&right)) >= 1e-9);
                    break;
                    
                case KRYON_EXPR_OP_LESS_THAN:
                    result.type = KRYON_EXPR_VALUE_BOOLEAN;
                    result.data.boolean_value = kryon_expression_value_to_number(&left) < kryon_expression_value_to_number(&right);
                    break;
                    
                case KRYON_EXPR_OP_GREATER_THAN:
                    result.type = KRYON_EXPR_VALUE_BOOLEAN;
                    result.data.boolean_value = kryon_expression_value_to_number(&left) > kryon_expression_value_to_number(&right);
                    break;
                    
                case KRYON_EXPR_OP_LESS_EQUAL:
                    result.type = KRYON_EXPR_VALUE_BOOLEAN;
                    result.data.boolean_value = kryon_expression_value_to_number(&left) <= kryon_expression_value_to_number(&right);
                    break;
                    
                case KRYON_EXPR_OP_GREATER_EQUAL:
                    result.type = KRYON_EXPR_VALUE_BOOLEAN;
                    result.data.boolean_value = kryon_expression_value_to_number(&left) >= kryon_expression_value_to_number(&right);
                    break;
                    
                case KRYON_EXPR_OP_LOGICAL_AND:
                    result.type = KRYON_EXPR_VALUE_BOOLEAN;
                    result.data.boolean_value = kryon_expression_value_to_boolean(&left) && kryon_expression_value_to_boolean(&right);
                    break;
                    
                case KRYON_EXPR_OP_LOGICAL_OR:
                    result.type = KRYON_EXPR_VALUE_BOOLEAN;
                    result.data.boolean_value = kryon_expression_value_to_boolean(&left) || kryon_expression_value_to_boolean(&right);
                    break;
                    
                default:
                    result.type = KRYON_EXPR_VALUE_NUMBER;
                    result.data.number_value = 0.0;
                    break;
            }
            
            // Free temporary values
            kryon_expression_value_free(&left);
            kryon_expression_value_free(&right);
            break;
        }
        
        case KRYON_EXPR_NODE_UNARY_OP: {
            KryonExpressionValue operand = kryon_runtime_evaluate_expression(expression->data.unary_op.operand, runtime);
            
            // Handle variable reference
            if (operand.type == KRYON_EXPR_VALUE_VARIABLE_REF) {
                double val = resolve_variable_as_number(runtime, operand.data.variable_name);
                operand.type = KRYON_EXPR_VALUE_NUMBER;
                operand.data.number_value = val;
            }
            
            switch (expression->data.unary_op.operator) {
                case KRYON_EXPR_OP_NEGATE:
                    result.type = KRYON_EXPR_VALUE_NUMBER;
                    result.data.number_value = -kryon_expression_value_to_number(&operand);
                    break;
                    
                case KRYON_EXPR_OP_NOT:
                    result.type = KRYON_EXPR_VALUE_BOOLEAN;
                    result.data.boolean_value = !kryon_expression_value_to_boolean(&operand);
                    break;
                    
                default:
                    result.type = KRYON_EXPR_VALUE_NUMBER;
                    result.data.number_value = 0.0;
                    break;
            }
            
            kryon_expression_value_free(&operand);
            break;
        }
        
        case KRYON_EXPR_NODE_TERNARY_OP: {
            KryonExpressionValue condition = kryon_runtime_evaluate_expression(expression->data.ternary_op.condition, runtime);
            
            if (kryon_expression_value_to_boolean(&condition)) {
                result = kryon_runtime_evaluate_expression(expression->data.ternary_op.true_value, runtime);
            } else {
                result = kryon_runtime_evaluate_expression(expression->data.ternary_op.false_value, runtime);
            }
            
            kryon_expression_value_free(&condition);
            break;
        }
        
        default:
            result.type = KRYON_EXPR_VALUE_NUMBER;
            result.data.number_value = 0.0;
            break;
    }
    
    return result;
}

// =============================================================================
// VALUE CONVERSION FUNCTIONS
// =============================================================================

char *kryon_expression_value_to_string(const KryonExpressionValue *value) {
    if (!value) return strdup("");
    
    switch (value->type) {
        case KRYON_EXPR_VALUE_STRING:
            return strdup(value->data.string_value ? value->data.string_value : "");
            
        case KRYON_EXPR_VALUE_NUMBER: {
            char *result = malloc(32);
            if (result) {
                snprintf(result, 32, "%.6g", value->data.number_value);
            }
            return result;
        }
        
        case KRYON_EXPR_VALUE_BOOLEAN:
            return strdup(value->data.boolean_value ? "true" : "false");
            
        case KRYON_EXPR_VALUE_VARIABLE_REF:
            return strdup(value->data.variable_name ? value->data.variable_name : "");
            
        default:
            return strdup("");
    }
}

double kryon_expression_value_to_number(const KryonExpressionValue *value) {
    if (!value) return 0.0;
    
    switch (value->type) {
        case KRYON_EXPR_VALUE_NUMBER:
            return value->data.number_value;
            
        case KRYON_EXPR_VALUE_STRING:
            if (value->data.string_value) {
                return strtod(value->data.string_value, NULL);
            }
            return 0.0;
            
        case KRYON_EXPR_VALUE_BOOLEAN:
            return value->data.boolean_value ? 1.0 : 0.0;
            
        case KRYON_EXPR_VALUE_VARIABLE_REF:
            // This case should be handled by the caller
            return 0.0;
            
        default:
            return 0.0;
    }
}

bool kryon_expression_value_to_boolean(const KryonExpressionValue *value) {
    if (!value) return false;
    
    switch (value->type) {
        case KRYON_EXPR_VALUE_BOOLEAN:
            return value->data.boolean_value;
            
        case KRYON_EXPR_VALUE_NUMBER:
            return value->data.number_value != 0.0;
            
        case KRYON_EXPR_VALUE_STRING:
            if (!value->data.string_value) return false;
            return strlen(value->data.string_value) > 0 && 
                   strcmp(value->data.string_value, "false") != 0 &&
                   strcmp(value->data.string_value, "0") != 0;
            
        case KRYON_EXPR_VALUE_VARIABLE_REF:
            // This case should be handled by the caller
            return false;
            
        default:
            return false;
    }
}

void kryon_expression_value_free(KryonExpressionValue *value) {
    if (!value) return;
    
    switch (value->type) {
        case KRYON_EXPR_VALUE_STRING:
            free(value->data.string_value);
            value->data.string_value = NULL;
            break;
            
        case KRYON_EXPR_VALUE_VARIABLE_REF:
            free(value->data.variable_name);
            value->data.variable_name = NULL;
            break;
            
        default:
            // Other types don't need freeing
            break;
    }
}

void kryon_expression_node_free(KryonExpressionNode *node) {
    if (!node) return;
    
    switch (node->type) {
        case KRYON_EXPR_NODE_VALUE:
            kryon_expression_value_free(&node->data.value);
            break;
            
        case KRYON_EXPR_NODE_BINARY_OP:
            kryon_expression_node_free(node->data.binary_op.left);
            kryon_expression_node_free(node->data.binary_op.right);
            break;
            
        case KRYON_EXPR_NODE_UNARY_OP:
            kryon_expression_node_free(node->data.unary_op.operand);
            break;
            
        case KRYON_EXPR_NODE_TERNARY_OP:
            kryon_expression_node_free(node->data.ternary_op.condition);
            kryon_expression_node_free(node->data.ternary_op.true_value);
            kryon_expression_node_free(node->data.ternary_op.false_value);
            break;
    }
    
    free(node);
}