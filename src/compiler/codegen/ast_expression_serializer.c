/**
 * @file ast_expression_serializer.c
 * @brief AST Expression to String Serializer Implementation
 */

#include "ast_expression_serializer.h"
#include "memory.h"
#include "lexer.h"
#include <stdio.h>
#include <string.h>

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

char* kryon_ast_expression_to_string(const KryonASTNode* node) {
    if (!node) return NULL;
    
    switch (node->type) {
        case KRYON_AST_BINARY_OP:
            return kryon_ast_binary_op_to_string(node);
            
        case KRYON_AST_MEMBER_ACCESS:
            return kryon_ast_member_access_to_string(node);
            
        case KRYON_AST_VARIABLE:
            return kryon_ast_variable_to_string(node);
            
        case KRYON_AST_LITERAL:
            return kryon_ast_literal_to_string(node);
            
        case KRYON_AST_UNARY_OP:
            // Handle unary operations like -value, !value
            if (node->data.unary_op.operand) {
                char* operand_str = kryon_ast_expression_to_string(node->data.unary_op.operand);
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
                char* array_str = kryon_ast_expression_to_string(node->data.array_access.array);
                char* index_str = kryon_ast_expression_to_string(node->data.array_access.index);
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
            
        default:
            // For unsupported node types, return a placeholder
            printf("Warning: Unsupported AST node type %d in expression serializer\n", node->type);
            return kryon_strdup("[unsupported]");
    }
}

char* kryon_ast_binary_op_to_string(const KryonASTNode* node) {
    if (!node || node->type != KRYON_AST_BINARY_OP) return NULL;
    
    char* left_str = kryon_ast_expression_to_string(node->data.binary_op.left);
    char* right_str = kryon_ast_expression_to_string(node->data.binary_op.right);
    
    if (!left_str || !right_str) {
        if (left_str) kryon_free(left_str);
        if (right_str) kryon_free(right_str);
        return NULL;
    }
    
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

char* kryon_ast_member_access_to_string(const KryonASTNode* node) {
    if (!node || node->type != KRYON_AST_MEMBER_ACCESS) return NULL;
    
    char* object_str = kryon_ast_expression_to_string(node->data.member_access.object);
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

char* kryon_ast_variable_to_string(const KryonASTNode* node) {
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

char* kryon_ast_literal_to_string(const KryonASTNode* node) {
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