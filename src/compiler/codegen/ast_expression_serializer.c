/**
 * @file ast_expression_serializer.c
 * @brief AST Expression to String Serializer Implementation
 */

#include "ast_expression_serializer.h"
#include "memory.h"
#include "lexer.h"
#include "codegen.h"
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <math.h>

// Helper function to get operator string from token type
static const char* get_operator_string(KryonTokenType op) {
    switch (op) {
        case KRYON_TOKEN_PLUS: return "+";
        case KRYON_TOKEN_MINUS: return "-";
        case KRYON_TOKEN_MULTIPLY: return "*";
        case KRYON_TOKEN_DIVIDE: return "/";
        case KRYON_TOKEN_MODULO: return "%";
        case KRYON_TOKEN_EQUALS: return "==";
        case KRYON_TOKEN_NOT_EQUALS: return "!=";
        case KRYON_TOKEN_LESS_THAN: return "<";
        case KRYON_TOKEN_LESS_EQUAL: return "<=";
        case KRYON_TOKEN_GREATER_THAN: return ">";
        case KRYON_TOKEN_GREATER_EQUAL: return ">=";
        case KRYON_TOKEN_LOGICAL_AND: return "&&";
        case KRYON_TOKEN_LOGICAL_OR: return "||";
        default: return "?";
    }
}

// Helper function to evaluate constant AST node to string
static char *evaluate_const_node_to_string(const KryonASTNode *node) {
    if (!node) return NULL;
    
    switch (node->type) {
        case KRYON_AST_LITERAL:
            if (node->data.literal.value.type == KRYON_VALUE_INTEGER) {
                char *result = malloc(32);
                if (result) {
                    snprintf(result, 32, "%lld", (long long)node->data.literal.value.data.int_value);
                }
                return result;
            } else if (node->data.literal.value.type == KRYON_VALUE_FLOAT) {
                char *result = malloc(32);
                if (result) {
                    snprintf(result, 32, "%.6g", node->data.literal.value.data.float_value);
                }
                return result;
            } else if (node->data.literal.value.type == KRYON_VALUE_STRING) {
                return kryon_strdup(node->data.literal.value.data.string_value ? 
                                   node->data.literal.value.data.string_value : "");
            } else if (node->data.literal.value.type == KRYON_VALUE_BOOLEAN) {
                return kryon_strdup(node->data.literal.value.data.bool_value ? "true" : "false");
            }
            break;
        default:
            break;
    }
    return NULL;
}

// Helper function to lookup constant value by name
const char *lookup_constant_value(KryonCodeGenerator *codegen, const char *name) {
    if (!codegen || !name) return NULL;
    
    // Search in constant table
    for (size_t i = 0; i < codegen->const_count; i++) {
        if (strcmp(codegen->const_table[i].name, name) == 0) {
            // Convert AST constant value to string
            return evaluate_const_node_to_string(codegen->const_table[i].value);
        }
    }
    return NULL;  // Not found in constants
}

char* kryon_ast_expression_to_string(const KryonASTNode* node, KryonCodeGenerator *codegen) {
    if (!node) return NULL;
    
    switch (node->type) {
        case KRYON_AST_BINARY_OP:
            return kryon_ast_binary_op_to_string(node, codegen);
            
        case KRYON_AST_MEMBER_ACCESS:
            return kryon_ast_member_access_to_string(node, codegen);
            
        case KRYON_AST_VARIABLE:
            return kryon_ast_variable_to_string(node, codegen);
            
        case KRYON_AST_LITERAL:
            return kryon_ast_literal_to_string(node, codegen);
            
        case KRYON_AST_TEMPLATE:
            // For template segments, rebuild the template string
            if (node->data.template.segment_count == 1 && 
                node->data.template.segments[0] &&
                node->data.template.segments[0]->type == KRYON_AST_LITERAL &&
                node->data.template.segments[0]->data.literal.value.type == KRYON_VALUE_STRING) {
                // Single literal segment, return as-is
                return kryon_strdup(node->data.template.segments[0]->data.literal.value.data.string_value);
            }
            // For complex templates, return a simple representation
            return kryon_strdup("[template]");
            
        case KRYON_AST_UNARY_OP:
            // Handle unary operations like -value, !value
            if (node->data.unary_op.operand) {
                char* operand_str = kryon_ast_expression_to_string(node->data.unary_op.operand, codegen);
                if (operand_str) {
                    const char* op_str = (node->data.unary_op.operator == KRYON_TOKEN_MINUS) ? "-" : 
                                        (node->data.unary_op.operator == KRYON_TOKEN_LOGICAL_NOT) ? "!" : "?";
                    size_t len = strlen(op_str) + strlen(operand_str) + 1;
                    char* result = kryon_alloc(len);
                    if (result) {
                        snprintf(result, len, "%s%s", op_str, operand_str);
                    }
                    kryon_free(operand_str);
                    return result;
                }
            }
            return NULL;
            
        case KRYON_AST_FUNCTION_CALL:
            // Handle function calls like getValue()
            if (node->data.function_call.function_name) {
                size_t len = strlen(node->data.function_call.function_name) + 3; // name + "()"
                char* result = kryon_alloc(len);
                if (result) {
                    snprintf(result, len, "%s()", node->data.function_call.function_name);
                }
                return result;
            }
            return NULL;
            
        case KRYON_AST_ARRAY_ACCESS:
            // Handle array access like array[index]
            if (node->data.array_access.array && node->data.array_access.index) {
                char* array_str = kryon_ast_expression_to_string(node->data.array_access.array, codegen);
                char* index_str = kryon_ast_expression_to_string(node->data.array_access.index, codegen);
                if (array_str && index_str) {
                    size_t len = strlen(array_str) + strlen(index_str) + 4; // array + "[" + index + "]"
                    char* result = kryon_alloc(len);
                    if (result) {
                        snprintf(result, len, "%s[%s]", array_str, index_str);
                    }
                    kryon_free(array_str);
                    kryon_free(index_str);
                    return result;
                }
                if (array_str) kryon_free(array_str);
                if (index_str) kryon_free(index_str);
            }
            return NULL;
            
        case KRYON_AST_IDENTIFIER:
            // Handle plain identifiers (variable names without $)
            if (node->data.identifier.name) {
                // Try to resolve as constant first
                const char *const_value = lookup_constant_value(codegen, node->data.identifier.name);
                if (const_value) {
                    return kryon_strdup(const_value);  // Return constant value
                }
                // Fallback to identifier name (for non-const variables)
                return kryon_strdup(node->data.identifier.name);
            }
            return NULL;
            
        case KRYON_AST_TERNARY_OP:
            // Handle ternary operations like condition ? true_expr : false_expr
            if (node->data.ternary_op.condition && 
                node->data.ternary_op.true_expr && 
                node->data.ternary_op.false_expr) {
                char* condition_str = kryon_ast_expression_to_string(node->data.ternary_op.condition, codegen);
                
                if (condition_str) {
                    // Try to evaluate the condition at compile time
                    // Enhanced boolean detection to handle various formats
                    bool is_true = false;
                    bool is_false = false;
                    
                    // Check for "true"/"false" (case-insensitive)
                    if (strcasecmp(condition_str, "true") == 0 || strcmp(condition_str, "1") == 0) {
                        is_true = true;
                    } else if (strcasecmp(condition_str, "false") == 0 || strcmp(condition_str, "0") == 0) {
                        is_false = true;
                    } else {
                        // Try to parse as numeric boolean (any non-zero is true)
                        char *endptr;
                        double num_value = strtod(condition_str, &endptr);
                        if (*endptr == '\0') { // Successfully parsed as number
                            if (num_value != 0.0) {
                                is_true = true;
                            } else {
                                is_false = true;
                            }
                        }
                    }
                    
                    if (is_true) {
                        // Condition is true - return the true expression
                        kryon_free(condition_str);
                        return kryon_ast_expression_to_string(node->data.ternary_op.true_expr, codegen);
                    } else if (is_false) {
                        // Condition is false - return the false expression
                        kryon_free(condition_str);
                        return kryon_ast_expression_to_string(node->data.ternary_op.false_expr, codegen);
                    } else {
                        // Condition is not a compile-time constant - build runtime expression
                        char* true_str = kryon_ast_expression_to_string(node->data.ternary_op.true_expr, codegen);
                        char* false_str = kryon_ast_expression_to_string(node->data.ternary_op.false_expr, codegen);
                        
                        if (true_str && false_str) {
                            size_t len = strlen(condition_str) + strlen(true_str) + strlen(false_str) + 8; // " ? " + " : " + null
                            char* result = kryon_alloc(len);
                            if (result) {
                                snprintf(result, len, "%s ? %s : %s", condition_str, true_str, false_str);
                            }
                            kryon_free(condition_str);
                            kryon_free(true_str);
                            kryon_free(false_str);
                            return result;
                        }
                        if (true_str) kryon_free(true_str);
                        if (false_str) kryon_free(false_str);
                    }
                    kryon_free(condition_str);
                }
            }
            return NULL;
            
        default:
            // For unsupported node types, return a placeholder
            printf("Warning: Unsupported AST node type %d in expression serializer\n", node->type);
            return kryon_strdup("[unsupported]");
    }
}

char* kryon_ast_binary_op_to_string(const KryonASTNode* node, KryonCodeGenerator *codegen) {
    if (!node || node->type != KRYON_AST_BINARY_OP) return NULL;
    
    char* left_str = kryon_ast_expression_to_string(node->data.binary_op.left, codegen);
    char* right_str = kryon_ast_expression_to_string(node->data.binary_op.right, codegen);
    
    if (!left_str || !right_str) {
        if (left_str) kryon_free(left_str);
        if (right_str) kryon_free(right_str);
        return NULL;
    }
    
    // Try to evaluate the expression at compile time if both operands are numeric
    char *end_left, *end_right;
    double left_num = strtod(left_str, &end_left);
    double right_num = strtod(right_str, &end_right);
    
    // Check if both strings were fully converted to numbers (no trailing characters)
    if (*end_left == '\0' && *end_right == '\0') {
        // Both operands are numbers - evaluate at compile time
        double result_num;
        bool can_evaluate = true;
        
        switch (node->data.binary_op.operator) {
            case KRYON_TOKEN_PLUS:
                result_num = left_num + right_num;
                break;
            case KRYON_TOKEN_MINUS:
                result_num = left_num - right_num;
                break;
            case KRYON_TOKEN_MULTIPLY:
                result_num = left_num * right_num;
                break;
            case KRYON_TOKEN_DIVIDE:
                if (right_num != 0.0) {
                    result_num = left_num / right_num;
                } else {
                    can_evaluate = false; // Avoid division by zero
                }
                break;
            case KRYON_TOKEN_MODULO:
                if (right_num != 0.0) {
                    result_num = fmod(left_num, right_num);
                } else {
                    can_evaluate = false; // Avoid division by zero
                }
                break;
            default:
                can_evaluate = false; // Don't evaluate comparison/logical ops at compile time
                break;
        }
        
        if (can_evaluate) {
            kryon_free(left_str);
            kryon_free(right_str);
            
            // Return the computed result
            char* result = malloc(32);
            if (result) {
                // Check if result is a whole number to avoid unnecessary decimal places
                if (result_num == floor(result_num)) {
                    snprintf(result, 32, "%.0f", result_num);
                } else {
                    snprintf(result, 32, "%.6g", result_num);
                }
            }
            return result;
        }
    }
    
    // Fallback: return the expression string
    const char* op_str = get_operator_string(node->data.binary_op.operator);
    
    // Calculate length: left + " " + operator + " " + right + null terminator
    size_t len = strlen(left_str) + strlen(op_str) + strlen(right_str) + 4;
    char* result = kryon_alloc(len);
    
    if (result) {
        snprintf(result, len, "%s %s %s", left_str, op_str, right_str);
    }
    
    kryon_free(left_str);
    kryon_free(right_str);
    return result;
}

char* kryon_ast_member_access_to_string(const KryonASTNode* node, KryonCodeGenerator *codegen) {
    if (!node || node->type != KRYON_AST_MEMBER_ACCESS) return NULL;
    
    char* object_str = kryon_ast_expression_to_string(node->data.member_access.object, codegen);
    const char* member_str = node->data.member_access.member;
    
    if (!object_str || !member_str) {
        if (object_str) kryon_free(object_str);
        return NULL;
    }
    
    // Calculate length: object + "." + member + null terminator
    size_t len = strlen(object_str) + strlen(member_str) + 2;
    char* result = kryon_alloc(len);
    
    if (result) {
        snprintf(result, len, "%s.%s", object_str, member_str);
    }
    
    kryon_free(object_str);
    return result;
}

char* kryon_ast_variable_to_string(const KryonASTNode* node, KryonCodeGenerator *codegen) {
    if (!node || node->type != KRYON_AST_VARIABLE) return NULL;
    
    const char* var_name = node->data.variable.name;
    if (!var_name) return NULL;
    
    // Variable names should include the $ prefix
    size_t len = strlen(var_name) + 2; // "$" + name + null terminator
    char* result = kryon_alloc(len);
    
    if (result) {
        snprintf(result, len, "$%s", var_name);
    }
    
    return result;
}

char* kryon_ast_literal_to_string(const KryonASTNode* node, KryonCodeGenerator *codegen) {
    if (!node || node->type != KRYON_AST_LITERAL) return NULL;
    
    const KryonASTValue* value = &node->data.literal.value;
    
    switch (value->type) {
        case KRYON_VALUE_STRING:
            if (value->data.string_value) {
                // Return string with quotes
                size_t len = strlen(value->data.string_value) + 3; // "string"
                char* result = kryon_alloc(len);
                if (result) {
                    snprintf(result, len, "\"%s\"", value->data.string_value);
                }
                return result;
            }
            return kryon_strdup("\"\"");
            
        case KRYON_VALUE_INTEGER: {
            char* result = kryon_alloc(32); // Enough for 64-bit int
            if (result) {
                snprintf(result, 32, "%lld", (long long)value->data.int_value);
            }
            return result;
        }
        
        case KRYON_VALUE_FLOAT: {
            char* result = kryon_alloc(32); // Enough for double
            if (result) {
                snprintf(result, 32, "%.6g", value->data.float_value);
            }
            return result;
        }
        
        case KRYON_VALUE_BOOLEAN:
            return kryon_strdup(value->data.bool_value ? "true" : "false");
            
        case KRYON_VALUE_NULL:
            return kryon_strdup("null");
            
        case KRYON_VALUE_COLOR: {
            char* result = kryon_alloc(16); // Enough for #RRGGBBAA
            if (result) {
                snprintf(result, 16, "#%08X", value->data.color_value);
            }
            return result;
        }
        
        case KRYON_VALUE_UNIT: {
            char* result = kryon_alloc(32);
            if (result) {
                snprintf(result, 32, "%.6g%s", value->data.unit_value.value, value->data.unit_value.unit);
            }
            return result;
        }
        
        default:
            return kryon_strdup("[unknown]");
    }
}