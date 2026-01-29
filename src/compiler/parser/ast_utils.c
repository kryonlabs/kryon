/**

 * @file ast_utils.c
 * @brief AST Utility Functions
 * 
 * 0BSD License
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted.
 * 
 * Additional AST functions for traversal, validation, and pretty printing.
 */
#include "lib9.h"


#include "parser.h"
#include "memory.h"

// =============================================================================
// AST PRINTING UTILITIES
// =============================================================================

static void print_indent(FILE *file, int indent) {
    for (int i = 0; i < indent; i++) {
        fprintf(file, "  ");
    }
}

static void print_ast_value(FILE *file, const KryonASTValue *value) {
    switch (value->type) {
        case KRYON_VALUE_STRING:
            fprintf(file, "\"%s\"", value->data.string_value ? value->data.string_value : "(null)");
            break;
        case KRYON_VALUE_INTEGER:
            fprintf(file, "%lld", (long long)value->data.int_value);
            break;
        case KRYON_VALUE_FLOAT:
            fprintf(file, "%.2f", value->data.float_value);
            break;
        case KRYON_VALUE_BOOLEAN:
            fprintf(file, "%s", value->data.bool_value ? "true" : "false");
            break;
        case KRYON_VALUE_NULL:
            fprintf(file, "null");
            break;
        case KRYON_VALUE_COLOR:
            fprintf(file, "#%08X", value->data.color_value);
            break;
        case KRYON_VALUE_UNIT:
            fprintf(file, "%.2f%s", value->data.unit_value.value, value->data.unit_value.unit);
            break;
        default:
            fprintf(file, "(unknown)");
            break;
    }
}

void kryon_ast_print(const KryonASTNode *node, FILE *file, int indent) {
    if (!node) {
        return;
    }
    
    if (!file) {
        file = stdout;
    }
    
    print_indent(file, indent);
    
    switch (node->type) {
        case KRYON_AST_ROOT:
            fprintf(file, "Root [%zu children]\n", node->data.element.child_count);
            for (size_t i = 0; i < node->data.element.child_count; i++) {
                kryon_ast_print(node->data.element.children[i], file, indent + 1);
            }
            break;
            
        case KRYON_AST_ELEMENT:
            fprintf(file, "Element: %s [%zu children, %zu properties]\n",
                   node->data.element.element_type ? node->data.element.element_type : "(null)",
                   node->data.element.child_count,
                   node->data.element.property_count);
            
            // Print properties
            for (size_t i = 0; i < node->data.element.property_count; i++) {
                kryon_ast_print(node->data.element.properties[i], file, indent + 1);
            }
            
            // Print children
            for (size_t i = 0; i < node->data.element.child_count; i++) {
                kryon_ast_print(node->data.element.children[i], file, indent + 1);
            }
            break;
            
        case KRYON_AST_PROPERTY:
            fprintf(file, "Property: %s = ",
                   node->data.property.name ? node->data.property.name : "(null)");
            if (node->data.property.value) {
                if (node->data.property.value->type == KRYON_AST_LITERAL) {
                    print_ast_value(file, &node->data.property.value->data.literal.value);
                    fprintf(file, "\n");
                } else {
                    fprintf(file, "\n");
                    kryon_ast_print(node->data.property.value, file, indent + 1);
                }
            } else {
                fprintf(file, "(null)\n");
            }
            break;
            
        case KRYON_AST_STYLE_BLOCK:
            fprintf(file, "Style: %s [%zu properties]\n",
                   node->data.style.name ? node->data.style.name : "(null)",
                   node->data.style.property_count);
            for (size_t i = 0; i < node->data.style.property_count; i++) {
                kryon_ast_print(node->data.style.properties[i], file, indent + 1);
            }
            break;
            
        case KRYON_AST_LITERAL:
            fprintf(file, "Literal: ");
            print_ast_value(file, &node->data.literal.value);
            fprintf(file, "\n");
            break;
            
        case KRYON_AST_VARIABLE:
            fprintf(file, "Variable: $%s\n",
                   node->data.variable.name ? node->data.variable.name : "(null)");
            break;
            
        case KRYON_AST_TEMPLATE:
            fprintf(file, "Template: [%zu segments]\n", node->data.template.segment_count);
            for (size_t i = 0; i < node->data.template.segment_count; i++) {
                if (node->data.template.segments[i]) {
                    kryon_ast_print(node->data.template.segments[i], file, indent + 1);
                }
            }
            break;
            
        case KRYON_AST_FUNCTION_CALL:
            fprintf(file, "FunctionCall: %s [%zu args]\n",
                   node->data.function_call.function_name ? node->data.function_call.function_name : "(null)",
                   node->data.function_call.argument_count);
            for (size_t i = 0; i < node->data.function_call.argument_count; i++) {
                kryon_ast_print(node->data.function_call.arguments[i], file, indent + 1);
            }
            break;
            
        case KRYON_AST_MEMBER_ACCESS:
            fprintf(file, "MemberAccess: .%s\n",
                   node->data.member_access.member ? node->data.member_access.member : "(null)");
            if (node->data.member_access.object) {
                kryon_ast_print(node->data.member_access.object, file, indent + 1);
            }
            break;
            
        case KRYON_AST_ERROR:
            fprintf(file, "Error: %s\n",
                   node->data.error.message ? node->data.error.message : "(null)");
            break;
            
        default:
            fprintf(file, "%s [node_id: %zu]\n", kryon_ast_node_type_name(node->type), node->node_id);
            break;
    }
}

// =============================================================================
// AST VALIDATION
// =============================================================================


static bool validate_element_node(const KryonASTNode *node, char **errors, size_t *error_count, size_t max_errors) {
    // ROOT nodes don't have element_type, only actual elements do
    if (node->type != KRYON_AST_ROOT && !node->data.element.element_type) {
        if (*error_count < max_errors) {
            errors[(*error_count)++] = kryon_alloc(64);
            if (errors[*error_count - 1]) {
                snprint(errors[*error_count - 1], 64, "Element node missing element_type");
            }
        }
        return false;
    }
    
    // Validate children
    for (size_t i = 0; i < node->data.element.child_count; i++) {
        if (!node->data.element.children[i]) {
            if (*error_count < max_errors) {
                errors[(*error_count)++] = kryon_alloc(64);
                if (errors[*error_count - 1]) {
                    snprint(errors[*error_count - 1], 64, "Element has null child at index %zu", i);
                }
            }
        }
    }
    
    // Validate properties
    for (size_t i = 0; i < node->data.element.property_count; i++) {
        if (!node->data.element.properties[i]) {
            if (*error_count < max_errors) {
                errors[(*error_count)++] = kryon_alloc(64);
                if (errors[*error_count - 1]) {
                    snprint(errors[*error_count - 1], 64, "Element has null property at index %zu", i);
                }
            }
        }
    }
    
    return true;
}


static bool validate_property_node(const KryonASTNode *node, char **errors, size_t *error_count, size_t max_errors) {
    if (!node->data.property.name) {
        if (*error_count < max_errors) {
            errors[(*error_count)++] = kryon_alloc(64);
            if (errors[*error_count - 1]) {
                snprint(errors[*error_count - 1], 64, "Property node missing name");
            }
        }
        return false;
    }
    
    if (!node->data.property.value) {
        if (*error_count < max_errors) {
            errors[(*error_count)++] = kryon_alloc(64);
            if (errors[*error_count - 1]) {
                snprint(errors[*error_count - 1], 64, "Property '%s' missing value", node->data.property.name);
            }
        }
        return false;
    }
    
    return true;
}

size_t kryon_ast_validate(const KryonASTNode *node, char **errors, size_t max_errors) {
    if (!node || !errors || max_errors == 0) {
        return 0;
    }
    
    size_t error_count = 0;
    
    switch (node->type) {
        case KRYON_AST_ELEMENT:
        case KRYON_AST_ROOT:
            validate_element_node(node, errors, &error_count, max_errors);
            
            // Recursively validate children
            for (size_t i = 0; i < node->data.element.child_count && error_count < max_errors; i++) {
                if (node->data.element.children[i]) {
                    error_count += kryon_ast_validate(node->data.element.children[i],
                                                     errors + error_count,
                                                     max_errors - error_count);
                }
            }
            
            // Recursively validate properties
            for (size_t i = 0; i < node->data.element.property_count && error_count < max_errors; i++) {
                if (node->data.element.properties[i]) {
                    error_count += kryon_ast_validate(node->data.element.properties[i],
                                                     errors + error_count,
                                                     max_errors - error_count);
                }
            }
            break;
            
        case KRYON_AST_PROPERTY:
            validate_property_node(node, errors, &error_count, max_errors);
            if (node->data.property.value && error_count < max_errors) {
                error_count += kryon_ast_validate(node->data.property.value,
                                                 errors + error_count,
                                                 max_errors - error_count);
            }
            break;
            
        case KRYON_AST_STYLE_BLOCK:
            if (!node->data.style.name) {
                if (error_count < max_errors) {
                    errors[error_count++] = kryon_alloc(64);
                    if (errors[error_count - 1]) {
                        snprint(errors[error_count - 1], 64, "Style block missing name");
                    }
                }
            }
            
            // Validate style properties
            for (size_t i = 0; i < node->data.style.property_count && error_count < max_errors; i++) {
                if (node->data.style.properties[i]) {
                    error_count += kryon_ast_validate(node->data.style.properties[i],
                                                     errors + error_count,
                                                     max_errors - error_count);
                }
            }
            break;
            
        case KRYON_AST_TEMPLATE:
            for (size_t i = 0; i < node->data.template.segment_count && error_count < max_errors; i++) {
                if (node->data.template.segments[i]) {
                    error_count += kryon_ast_validate(node->data.template.segments[i],
                                                     errors + error_count,
                                                     max_errors - error_count);
                }
            }
            break;
            
        case KRYON_AST_FUNCTION_CALL:
            if (!node->data.function_call.function_name) {
                if (error_count < max_errors) {
                    errors[error_count++] = kryon_alloc(64);
                    if (errors[error_count - 1]) {
                        snprint(errors[error_count - 1], 64, "Function call missing name");
                    }
                }
            }
            
            // Validate arguments
            for (size_t i = 0; i < node->data.function_call.argument_count && error_count < max_errors; i++) {
                if (node->data.function_call.arguments[i]) {
                    error_count += kryon_ast_validate(node->data.function_call.arguments[i],
                                                     errors + error_count,
                                                     max_errors - error_count);
                }
            }
            break;
            
        case KRYON_AST_MEMBER_ACCESS:
            if (node->data.member_access.object && error_count < max_errors) {
                error_count += kryon_ast_validate(node->data.member_access.object,
                                                 errors + error_count,
                                                 max_errors - error_count);
            }
            break;
            
        default:
            // Most node types don't need special validation
            break;
    }
    
    return error_count;
}

// =============================================================================
// AST TRAVERSAL
// =============================================================================

void kryon_ast_traverse(const KryonASTNode *root, KryonASTVisitor visitor, void *user_data) {
    if (!root || !visitor) {
        return;
    }
    
    // Visit current node
    if (!visitor(root, user_data)) {
        return; // Visitor requested stop
    }
    
    // Traverse children based on node type
    switch (root->type) {
        case KRYON_AST_ROOT:
        case KRYON_AST_ELEMENT:
            // Traverse child elements
            for (size_t i = 0; i < root->data.element.child_count; i++) {
                if (root->data.element.children[i]) {
                    kryon_ast_traverse(root->data.element.children[i], visitor, user_data);
                }
            }
            
            // Traverse properties
            for (size_t i = 0; i < root->data.element.property_count; i++) {
                if (root->data.element.properties[i]) {
                    kryon_ast_traverse(root->data.element.properties[i], visitor, user_data);
                }
            }
            break;
            
        case KRYON_AST_PROPERTY:
            if (root->data.property.value) {
                kryon_ast_traverse(root->data.property.value, visitor, user_data);
            }
            break;
            
        case KRYON_AST_STYLE_BLOCK:
            for (size_t i = 0; i < root->data.style.property_count; i++) {
                if (root->data.style.properties[i]) {
                    kryon_ast_traverse(root->data.style.properties[i], visitor, user_data);
                }
            }
            break;
            
        case KRYON_AST_TEMPLATE:
            for (size_t i = 0; i < root->data.template.segment_count; i++) {
                if (root->data.template.segments[i]) {
                    kryon_ast_traverse(root->data.template.segments[i], visitor, user_data);
                }
            }
            break;
            
        case KRYON_AST_BINARY_OP:
            if (root->data.binary_op.left) {
                kryon_ast_traverse(root->data.binary_op.left, visitor, user_data);
            }
            if (root->data.binary_op.right) {
                kryon_ast_traverse(root->data.binary_op.right, visitor, user_data);
            }
            break;
            
        case KRYON_AST_UNARY_OP:
            if (root->data.unary_op.operand) {
                kryon_ast_traverse(root->data.unary_op.operand, visitor, user_data);
            }
            break;
            
        case KRYON_AST_FUNCTION_CALL:
            for (size_t i = 0; i < root->data.function_call.argument_count; i++) {
                if (root->data.function_call.arguments[i]) {
                    kryon_ast_traverse(root->data.function_call.arguments[i], visitor, user_data);
                }
            }
            break;
            
        case KRYON_AST_MEMBER_ACCESS:
            if (root->data.member_access.object) {
                kryon_ast_traverse(root->data.member_access.object, visitor, user_data);
            }
            break;
            
        case KRYON_AST_ARRAY_ACCESS:
            if (root->data.array_access.array) {
                kryon_ast_traverse(root->data.array_access.array, visitor, user_data);
            }
            if (root->data.array_access.index) {
                kryon_ast_traverse(root->data.array_access.index, visitor, user_data);
            }
            break;
            
        default:
            // Leaf nodes don't have children to traverse
            break;
    }
}

// =============================================================================
// AST SEARCH UTILITIES
// =============================================================================

typedef struct {
    KryonASTNodeType target_type;
    const KryonASTNode **results;
    size_t max_results;
    size_t found_count;
} TypeSearchData;

static bool type_search_visitor(const KryonASTNode *node, void *user_data) {
    TypeSearchData *data = (TypeSearchData*)user_data;
    
    if (node->type == data->target_type && data->found_count < data->max_results) {
        data->results[data->found_count++] = node;
    }
    
    return data->found_count < data->max_results; // Continue if we haven't filled results
}

size_t kryon_ast_find_by_type(const KryonASTNode *root, KryonASTNodeType type,
                             const KryonASTNode **results, size_t max_results) {
    if (!root || !results || max_results == 0) {
        return 0;
    }
    
    TypeSearchData data = {
        .target_type = type,
        .results = results,
        .max_results = max_results,
        .found_count = 0
    };
    
    kryon_ast_traverse(root, type_search_visitor, &data);
    return data.found_count;
}

typedef struct {
    const char *target_element_type;
    const KryonASTNode **results;
    size_t max_results;
    size_t found_count;
} ElementSearchData;

static bool element_search_visitor(const KryonASTNode *node, void *user_data) {
    ElementSearchData *data = (ElementSearchData*)user_data;
    
    if (node->type == KRYON_AST_ELEMENT && data->found_count < data->max_results) {
        if (node->data.element.element_type &&
            strcmp(node->data.element.element_type, data->target_element_type) == 0) {
            data->results[data->found_count++] = node;
        }
    }
    
    return data->found_count < data->max_results;
}

size_t kryon_ast_find_elements(const KryonASTNode *root, const char *element_type,
                              const KryonASTNode **results, size_t max_results) {
    if (!root || !element_type || !results || max_results == 0) {
        return 0;
    }
    
    ElementSearchData data = {
        .target_element_type = element_type,
        .results = results,
        .max_results = max_results,
        .found_count = 0
    };
    
    kryon_ast_traverse(root, element_search_visitor, &data);
    return data.found_count;
}

// =============================================================================
// AST VALUE UTILITIES
// =============================================================================

char *kryon_ast_value_to_string(const KryonASTValue *value) {
    if (!value) {
        return NULL;
    }
    
    char *result = NULL;
    size_t len = 0;
    
    switch (value->type) {
        case KRYON_VALUE_STRING:
            if (value->data.string_value) {
                len = strlen(value->data.string_value) + 3; // quotes + null
                result = kryon_alloc(len);
                if (result) {
                    snprint(result, len, "\"%s\"", value->data.string_value);
                }
            } else {
                result = kryon_alloc(8);
                if (result) {
                    strcpy(result, "\"\"");
                }
            }
            break;
            
        case KRYON_VALUE_INTEGER:
            result = kryon_alloc(32);
            if (result) {
                snprint(result, 32, "%lld", (long long)value->data.int_value);
            }
            break;
            
        case KRYON_VALUE_FLOAT:
            result = kryon_alloc(32);
            if (result) {
                snprint(result, 32, "%.6g", value->data.float_value);
            }
            break;
            
        case KRYON_VALUE_BOOLEAN:
            result = kryon_alloc(8);
            if (result) {
                strcpy(result, value->data.bool_value ? "true" : "false");
            }
            break;
            
        case KRYON_VALUE_NULL:
            result = kryon_alloc(8);
            if (result) {
                strcpy(result, "null");
            }
            break;
            
        case KRYON_VALUE_COLOR:
            result = kryon_alloc(16);
            if (result) {
                snprint(result, 16, "#%08X", value->data.color_value);
            }
            break;
            
        case KRYON_VALUE_UNIT:
            result = kryon_alloc(32);
            if (result) {
                snprint(result, 32, "%.6g%s", value->data.unit_value.value, value->data.unit_value.unit);
            }
            break;
            
        default:
            result = kryon_alloc(16);
            if (result) {
                strcpy(result, "(unknown)");
            }
            break;
    }
    
    return result;
}
