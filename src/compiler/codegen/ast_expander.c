/**
 * @file ast_expander.c
 * @brief AST expansion for components and const_for loops
 * 
 * Handles expansion of custom components and const_for loops during code generation.
 * Provides parameter substitution and template processing.
 */

#include "codegen.h"
#include "memory.h"
#include "parser.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// Parameter substitution context
typedef struct {
    const char *param_name;
    const KryonASTNode *param_value;
} ParamContext;

// Forward declarations
static KryonASTNode *find_component_definition(const char *component_name, const KryonASTNode *ast_root);
static KryonASTNode *clone_and_substitute_node(const KryonASTNode *original, const ParamContext *params, size_t param_count);
static const KryonASTNode *find_param_value(const ParamContext *params, size_t param_count, const char *param_name);
static KryonASTNode *create_minimal_ast_node(KryonASTNodeType type);
static bool add_child_to_node(KryonASTNode *parent, KryonASTNode *child);
static bool add_property_to_node(KryonASTNode *parent, KryonASTNode *property);
// Make this function public so codegen.c can use it
KryonASTNode *kryon_substitute_template_vars(const KryonASTNode *node, const char *var_name, const KryonASTNode *var_value);
static char *process_string_template(const char *template_string, const char *var_name, const KryonASTNode *var_value);

KryonASTNode *kryon_find_component_definition(const char *component_name, const KryonASTNode *ast_root) {
    if (!component_name || !ast_root) return NULL;
    
    printf("🔍 Searching for component '%s' in AST with %zu children\n", 
           component_name, ast_root->data.element.child_count);
    
    // Search through all children in the AST root
    for (size_t i = 0; i < ast_root->data.element.child_count; i++) {
        const KryonASTNode *child = ast_root->data.element.children[i];
        printf("🔍 Child %zu: type=%d", i, child ? child->type : -1);
        if (child && child->type == KRYON_AST_COMPONENT) {
            printf(" (COMPONENT: name='%s')", child->data.component.name ? child->data.component.name : "NULL");
            if (child->data.component.name && strcmp(child->data.component.name, component_name) == 0) {
                printf(" ✅ MATCH!\n");
                return (KryonASTNode*)child; // Found the component definition
            }
        }
        printf("\n");
    }
    
    printf("❌ Component '%s' not found in AST\n", component_name);
    return NULL; // Component not found
}

static KryonASTNode *find_component_definition(const char *component_name, const KryonASTNode *ast_root) {
    return kryon_find_component_definition(component_name, ast_root);
}

KryonASTNode *kryon_expand_component_instance(const KryonASTNode *component_instance, const KryonASTNode *ast_root) {
    if (!component_instance || component_instance->type != KRYON_AST_ELEMENT) {
        return NULL;
    }
    
    const char *component_name = component_instance->data.element.element_type;
    printf("🔍 Looking for component definition: %s\n", component_name);
    
    KryonASTNode *component_def = find_component_definition(component_name, ast_root);
    if (!component_def) {
        printf("❌ Component definition not found: %s\n", component_name);
        return NULL;
    }
    
    printf("✅ Found component definition: %s\n", component_name);
    printf("🔍 Component has %zu parameters, instance has %zu properties\n", 
           component_def->data.component.parameter_count, 
           component_instance->data.element.property_count);
    
    if (!component_def->data.component.body) {
        printf("❌ Component has no body to expand\n");
        return NULL;
    }
    
    // Create parameter context from instance properties
    ParamContext *params = NULL;
    size_t param_count = 0;
    
    // Map instance properties to component parameters
    if (component_instance->data.element.property_count > 0 && component_def->data.component.parameter_count > 0) {
        param_count = component_instance->data.element.property_count;
        params = kryon_alloc(param_count * sizeof(ParamContext));
        if (params) {
            for (size_t i = 0; i < param_count; i++) {
                const KryonASTNode *prop = component_instance->data.element.properties[i];
                if (prop && prop->type == KRYON_AST_PROPERTY && prop->data.property.name) {
                    params[i].param_name = prop->data.property.name;
                    params[i].param_value = prop->data.property.value;
                    printf("🔗 Mapped instance property '%s' to component parameter\n", prop->data.property.name);
                }
            }
        }
    }
    
    // Clone the component body with parameter substitution
    KryonASTNode *expanded = clone_and_substitute_node(component_def->data.component.body, params, param_count);
    
    // Clean up parameter context
    kryon_free(params);
    if (!expanded) {
        printf("❌ Failed to clone component body\n");
        return NULL;
    }
    
    printf("✅ Successfully expanded component: %s\n", component_name);
    return expanded;
}

static KryonASTNode *clone_and_substitute_node(const KryonASTNode *original, const ParamContext *params, size_t param_count) {
    if (!original) return NULL;
    
    printf("🔍 DEBUG: Cloning node type %d\n", original->type);
    
    // Create new node
    KryonASTNode *cloned = create_minimal_ast_node(original->type);
    if (!cloned) {
        printf("🔍 DEBUG: Failed to create cloned node\n");
        return NULL;
    }
    
    // Copy location
    cloned->location = original->location;
    
    // Copy type-specific data
    switch (original->type) {
        case KRYON_AST_ELEMENT: {
            // Clone element type string
            if (original->data.element.element_type) {
                cloned->data.element.element_type = strdup(original->data.element.element_type);
            }
            
            // Clone properties
            cloned->data.element.property_count = original->data.element.property_count;
            if (original->data.element.property_count > 0) {
                cloned->data.element.properties = calloc(original->data.element.property_count, sizeof(KryonASTNode*));
                for (size_t i = 0; i < original->data.element.property_count; i++) {
                    cloned->data.element.properties[i] = clone_and_substitute_node(original->data.element.properties[i], params, param_count);
                }
            }
            
            // Clone children
            cloned->data.element.child_count = original->data.element.child_count;
            if (original->data.element.child_count > 0) {
                cloned->data.element.children = calloc(original->data.element.child_count, sizeof(KryonASTNode*));
                for (size_t i = 0; i < original->data.element.child_count; i++) {
                    cloned->data.element.children[i] = clone_and_substitute_node(original->data.element.children[i], params, param_count);
                }
            }
            break;
        }
        
        case KRYON_AST_PROPERTY: {
            // Clone property name
            if (original->data.property.name) {
                cloned->data.property.name = strdup(original->data.property.name);
            }
            
            // Clone property value
            if (original->data.property.value) {
                cloned->data.property.value = clone_and_substitute_node(original->data.property.value, params, param_count);
            }
            break;
        }
        
        case KRYON_AST_IDENTIFIER: {
            // Check if this identifier is a parameter that needs substitution
            if (params && original->data.identifier.name) {
                printf("🔍 DEBUG: Checking identifier '%s' for substitution (params has %zu entries)\n", 
                       original->data.identifier.name, param_count);
                const KryonASTNode *param_value = find_param_value(params, param_count, original->data.identifier.name);
                if (param_value) {
                    printf("✅ Substituting identifier '%s'\n", original->data.identifier.name);
                    // Return a clone of the parameter value instead
                    kryon_free(cloned);
                    return clone_and_substitute_node(param_value, NULL, 0);
                }
            }
            
            // No substitution, clone identifier name
            if (original->data.identifier.name) {
                cloned->data.identifier.name = strdup(original->data.identifier.name);
            }
            break;
        }
        
        case KRYON_AST_LITERAL: {
            // Clone literal value
            cloned->data.literal.value = original->data.literal.value;
            if (original->data.literal.value.type == KRYON_VALUE_STRING && 
                original->data.literal.value.data.string_value) {
                cloned->data.literal.value.data.string_value = strdup(original->data.literal.value.data.string_value);
            }
            break;
        }
        
        case KRYON_AST_VARIABLE: {
            // Check if this variable is a parameter that needs substitution
            if (params && original->data.variable.name) {
                printf("🔍 DEBUG: Checking variable '$%s' for substitution (params has %zu entries)\n", 
                       original->data.variable.name, param_count);
                const KryonASTNode *param_value = find_param_value(params, param_count, original->data.variable.name);
                if (param_value) {
                    printf("✅ Substituting variable '$%s'\n", original->data.variable.name);
                    // Return a clone of the parameter value instead
                    kryon_free(cloned);
                    return clone_and_substitute_node(param_value, NULL, 0);
                }
            }
            
            // No substitution, clone variable name
            if (original->data.variable.name) {
                cloned->data.variable.name = strdup(original->data.variable.name);
            }
            break;
        }
        
        case KRYON_AST_TEMPLATE: {
            // Template variable substitution (e.g., $value)
            KryonASTNode *expression = original->data.template.expression;
            
            // Check if the template expression is a variable that needs substitution
            if (params && expression && expression->type == KRYON_AST_VARIABLE && expression->data.variable.name) {
                printf("🔍 DEBUG: Checking template variable '$%s' for substitution (params has %zu entries)\n", 
                       expression->data.variable.name, param_count);
                const KryonASTNode *param_value = find_param_value(params, param_count, expression->data.variable.name);
                if (param_value) {
                    printf("✅ Found template variable '$%s' - this is a REACTIVE parameter\n", 
                           expression->data.variable.name);
                    
                    // IMPORTANT: Component parameters are REACTIVE variables, not constants!
                    // They must maintain variable references for runtime updates, not be substituted
                    // at compile time. Create a variable reference for runtime resolution.
                    
                    // Free the cloned template node
                    kryon_free(cloned);
                    
                    // Create a variable reference node that the runtime can resolve dynamically
                    KryonASTNode *var_ref_node = create_minimal_ast_node(KRYON_AST_VARIABLE);
                    if (var_ref_node) {
                        var_ref_node->data.variable.name = strdup(expression->data.variable.name);
                        printf("    Created reactive variable reference for '$%s'\n", expression->data.variable.name);
                        return var_ref_node;
                    }
                }
                printf("⚠️  No substitution found for template variable '$%s'\n", expression->data.variable.name);
            }
            
            // No substitution, clone the template expression
            if (original->data.template.expression) {
                cloned->data.template.expression = clone_and_substitute_node(original->data.template.expression, params, param_count);
            }
            break;
        }
        
        default:
            // For other types, do basic copy
            memcpy(&cloned->data, &original->data, sizeof(original->data));
            break;
    }
    
    printf("🔍 DEBUG: Successfully cloned node type %d\n", original->type);
    return cloned;
}

static const KryonASTNode *find_param_value(const ParamContext *params, size_t param_count, const char *param_name) {
    if (!params || !param_name) return NULL;
    
    for (size_t i = 0; i < param_count; i++) {
        if (params[i].param_name && strcmp(params[i].param_name, param_name) == 0) {
            return params[i].param_value;
        }
    }
    
    return NULL;
}

static KryonASTNode *create_minimal_ast_node(KryonASTNodeType type) {
    KryonASTNode *node = kryon_alloc(sizeof(KryonASTNode));
    if (!node) return NULL;
    
    memset(node, 0, sizeof(KryonASTNode));
    node->type = type;
    
    return node;
}

static bool add_child_to_node(KryonASTNode *parent, KryonASTNode *child) {
    if (!parent || !child) return false;
    
    // Ensure capacity
    if (parent->data.element.child_count >= parent->data.element.child_capacity) {
        size_t new_capacity = parent->data.element.child_capacity == 0 ? 4 : parent->data.element.child_capacity * 2;
        KryonASTNode **new_children = realloc(parent->data.element.children, new_capacity * sizeof(KryonASTNode*));
        if (!new_children) return false;
        
        parent->data.element.children = new_children;
        parent->data.element.child_capacity = new_capacity;
    }
    
    parent->data.element.children[parent->data.element.child_count++] = child;
    child->parent = parent;
    
    return true;
}

static bool add_property_to_node(KryonASTNode *parent, KryonASTNode *property) {
    if (!parent || !property) return false;
    
    // Ensure capacity
    if (parent->data.element.property_count >= parent->data.element.property_capacity) {
        size_t new_capacity = parent->data.element.property_capacity == 0 ? 4 : parent->data.element.property_capacity * 2;
        KryonASTNode **new_properties = realloc(parent->data.element.properties, new_capacity * sizeof(KryonASTNode*));
        if (!new_properties) return false;
        
        parent->data.element.properties = new_properties;
        parent->data.element.property_capacity = new_capacity;
    }
    
    parent->data.element.properties[parent->data.element.property_count++] = property;
    property->parent = parent;
    
    return true;
}

bool kryon_expand_const_for_loop(KryonCodeGenerator *codegen, const KryonASTNode *const_for, const KryonASTNode *ast_root) {
    if (!const_for || const_for->type != KRYON_AST_CONST_FOR_LOOP) {
        return false;
    }
    
    if (const_for->data.const_for_loop.is_range) {
        // Handle range-based loop
        int start = const_for->data.const_for_loop.range_start;
        int end = const_for->data.const_for_loop.range_end;
        
        printf("DEBUG: Expanding range const_for loop: %s in %d..%d\n", 
               const_for->data.const_for_loop.var_name, start, end);
        
        // Expand loop body for each value in range
        for (int i = start; i <= end; i++) {
            printf("DEBUG: Processing range value %d\n", i);
            
            // Create a literal AST node for the current value
            KryonASTNode *value_node = kryon_calloc(1, sizeof(KryonASTNode));
            if (!value_node) {
                printf("DEBUG: ERROR: Failed to allocate value node\n");
                return false;
            }
            
            value_node->type = KRYON_AST_LITERAL;
            value_node->data.literal.value.type = KRYON_VALUE_INTEGER;
            value_node->data.literal.value.data.int_value = i;
            
            // Expand each element in the loop body
            for (size_t j = 0; j < const_for->data.const_for_loop.body_count; j++) {
                const KryonASTNode *body_element = const_for->data.const_for_loop.body[j];
                
                if (body_element && body_element->type == KRYON_AST_ELEMENT) {
                    // Substitute template variables in the element
                    KryonASTNode *substituted_element = kryon_substitute_template_vars(
                        body_element,
                        const_for->data.const_for_loop.var_name,
                        value_node
                    );
                    
                    if (substituted_element) {
                        printf("DEBUG: Writing substituted element for iteration %d\n", i);
                        if (!kryon_write_element_instance(codegen, substituted_element, ast_root)) {
                            printf("DEBUG: ERROR: Failed to write substituted element\n");
                            kryon_free(value_node);
                            return false;
                        }
                        printf("DEBUG: Successfully wrote substituted element\n");
                    } else {
                        printf("DEBUG: ERROR: Template substitution returned NULL\n");
                    }
                }
            }
            
            kryon_free(value_node);
        }
        
    } else {
        // Handle array-based loop (existing logic)
        printf("DEBUG: Expanding array const_for loop: %s in %s\n", 
               const_for->data.const_for_loop.var_name,
               const_for->data.const_for_loop.array_name);
        
        // Find the array constant
        const char *array_name = const_for->data.const_for_loop.array_name;
        const KryonASTNode *array_const = find_constant_value(codegen, array_name);
        
        if (!array_const || array_const->type != KRYON_AST_ARRAY_LITERAL) {
            printf("DEBUG: ERROR: Array constant '%s' not found or not an array\n", array_name);
            return false;
        }
        
        printf("DEBUG: Found array constant '%s' with %zu elements\n", 
               array_name, array_const->data.array_literal.element_count);
        
        // Expand loop body for each array element
        for (size_t i = 0; i < array_const->data.array_literal.element_count; i++) {
            const KryonASTNode *array_element = array_const->data.array_literal.elements[i];
            
            printf("DEBUG: Processing array element %zu\n", i);
            
            // Expand each element in the loop body
            for (size_t j = 0; j < const_for->data.const_for_loop.body_count; j++) {
                const KryonASTNode *body_element = const_for->data.const_for_loop.body[j];
                
                if (body_element && body_element->type == KRYON_AST_ELEMENT) {
                    // Substitute template variables in the element
                    KryonASTNode *substituted_element = kryon_substitute_template_vars(
                        body_element,
                        const_for->data.const_for_loop.var_name,
                        array_element
                    );
                    
                    if (substituted_element) {
                        printf("DEBUG: Writing substituted element for iteration %zu\n", i);
                        if (!kryon_write_element_instance(codegen, substituted_element, ast_root)) {
                            printf("DEBUG: ERROR: Failed to write substituted element\n");
                            return false;
                        }
                        printf("DEBUG: Successfully wrote substituted element\n");
                    } else {
                        printf("DEBUG: ERROR: Template substitution returned NULL\n");
                    }
                }
            }
        }
    }
    
    return true;
}

uint32_t kryon_count_const_for_elements(KryonCodeGenerator *codegen, const KryonASTNode *const_for) {
    if (!const_for || const_for->type != KRYON_AST_CONST_FOR_LOOP) {
        return 0;
    }
    
    uint32_t iteration_count = 0;
    
    if (const_for->data.const_for_loop.is_range) {
        // Calculate range size
        int start = const_for->data.const_for_loop.range_start;
        int end = const_for->data.const_for_loop.range_end;
        iteration_count = (uint32_t)(end - start + 1);
    } else {
        // Find the array constant
        const char *array_name = const_for->data.const_for_loop.array_name;
        const KryonASTNode *array_const = find_constant_value(codegen, array_name);
        
        if (!array_const || array_const->type != KRYON_AST_ARRAY_LITERAL) {
            return 1; // Fallback count
        }
        
        iteration_count = (uint32_t)array_const->data.array_literal.element_count;
    }
    
    // Count elements in the loop body that will be expanded
    uint32_t body_element_count = 0;
    for (size_t i = 0; i < const_for->data.const_for_loop.body_count; i++) {
        if (const_for->data.const_for_loop.body[i] && 
            const_for->data.const_for_loop.body[i]->type == KRYON_AST_ELEMENT) {
            body_element_count++;
        }
    }
    
    // Total elements = iteration count * body elements
    return iteration_count * body_element_count;
}

KryonASTNode *kryon_substitute_template_vars(const KryonASTNode *node, const char *var_name, const KryonASTNode *var_value) {
    if (!node || !var_name || !var_value) {
        return NULL;
    }
    
    printf("🔍 DEBUG: Substituting templates in node type %d\n", node->type);
    
    // Create a deep copy of the node
    KryonASTNode *cloned = create_minimal_ast_node(node->type);
    if (!cloned) return NULL;
    
    // Copy basic properties
    cloned->location = node->location;
    
    switch (node->type) {
        case KRYON_AST_ELEMENT: {
            // Clone element type string
            if (node->data.element.element_type) {
                cloned->data.element.element_type = strdup(node->data.element.element_type);
            }
            
            // Clone and substitute properties
            cloned->data.element.property_count = node->data.element.property_count;
            cloned->data.element.property_capacity = node->data.element.property_count;
            if (node->data.element.property_count > 0) {
                cloned->data.element.properties = calloc(node->data.element.property_count, sizeof(KryonASTNode*));
                for (size_t i = 0; i < node->data.element.property_count; i++) {
                    cloned->data.element.properties[i] = kryon_substitute_template_vars(node->data.element.properties[i], var_name, var_value);
                }
            }
            
            // Clone and substitute children
            cloned->data.element.child_count = node->data.element.child_count;
            cloned->data.element.child_capacity = node->data.element.child_count;
            if (node->data.element.child_count > 0) {
                cloned->data.element.children = calloc(node->data.element.child_count, sizeof(KryonASTNode*));
                for (size_t i = 0; i < node->data.element.child_count; i++) {
                    cloned->data.element.children[i] = kryon_substitute_template_vars(node->data.element.children[i], var_name, var_value);
                }
            }
            break;
        }
        
        case KRYON_AST_PROPERTY: {
            // Clone property name
            if (node->data.property.name) {
                cloned->data.property.name = strdup(node->data.property.name);
            }
            
            // Clone and substitute property value
            if (node->data.property.value) {
                cloned->data.property.value = kryon_substitute_template_vars(node->data.property.value, var_name, var_value);
            }
            break;
        }
        
        case KRYON_AST_LITERAL: {
            // Clone literal value with template processing for strings
            if (node->data.literal.value.type == KRYON_VALUE_STRING && node->data.literal.value.data.string_value) {
                const char *original_string = node->data.literal.value.data.string_value;
                
                // Check if string contains template variables ${...}
                if (strstr(original_string, "${") != NULL) {
                    char *processed = process_string_template(original_string, var_name, var_value);
                    if (processed) {
                        cloned->data.literal.value.type = KRYON_VALUE_STRING;
                        cloned->data.literal.value.data.string_value = processed;
                    } else {
                        cloned->data.literal.value = node->data.literal.value;
                        cloned->data.literal.value.data.string_value = strdup(original_string);
                    }
                } else {
                    cloned->data.literal.value = node->data.literal.value;
                    cloned->data.literal.value.data.string_value = strdup(original_string);
                }
            } else {
                cloned->data.literal.value = node->data.literal.value;
            }
            break;
        }
        
        case KRYON_AST_IDENTIFIER: {
            // Clone identifier name
            if (node->data.identifier.name) {
                cloned->data.identifier.name = strdup(node->data.identifier.name);
            }
            break;
        }
        
        case KRYON_AST_TEMPLATE: {
            // Handle template expressions like ${alignment.gap} and ${alignment.colors[0]}
            if (node->data.template.expression) {
                
                if (node->data.template.expression->type == KRYON_AST_MEMBER_ACCESS) {
                    // Handle ${alignment.property}
                    KryonASTNode *member_access = node->data.template.expression;
                    
                    // Check if this matches our variable (e.g., "alignment.gap")
                    if (member_access->data.member_access.object &&
                        member_access->data.member_access.object->type == KRYON_AST_IDENTIFIER &&
                        member_access->data.member_access.object->data.identifier.name &&
                        strcmp(member_access->data.member_access.object->data.identifier.name, var_name) == 0) {
                        
                        // Look up the property in the object literal
                        const char *property_name = member_access->data.member_access.member;
                        if (property_name && var_value->type == KRYON_AST_OBJECT_LITERAL) {
                            
                            // Find the property in the object literal
                            for (size_t i = 0; i < var_value->data.object_literal.property_count; i++) {
                                const KryonASTNode *prop = var_value->data.object_literal.properties[i];
                                if (prop && prop->type == KRYON_AST_PROPERTY &&
                                    prop->data.property.name &&
                                    strcmp(prop->data.property.name, property_name) == 0) {
                                    
                                    // Found the property - return a clone of its value
                                    kryon_free(cloned);
                                    return kryon_substitute_template_vars(prop->data.property.value, var_name, var_value);
                                }
                            }
                        }
                    }
                } else if (node->data.template.expression->type == KRYON_AST_ARRAY_ACCESS) {
                    // Handle ${alignment.colors[0]}
                    KryonASTNode *array_access = node->data.template.expression;
                    
                    // Check if the array is a member access of our variable
                    if (array_access->data.array_access.array &&
                        array_access->data.array_access.array->type == KRYON_AST_MEMBER_ACCESS) {
                        
                        KryonASTNode *member_access = array_access->data.array_access.array;
                        
                        if (member_access->data.member_access.object &&
                            member_access->data.member_access.object->type == KRYON_AST_IDENTIFIER &&
                            member_access->data.member_access.object->data.identifier.name &&
                            strcmp(member_access->data.member_access.object->data.identifier.name, var_name) == 0) {
                            
                            const char *property_name = member_access->data.member_access.member;
                            if (property_name && var_value->type == KRYON_AST_OBJECT_LITERAL &&
                                array_access->data.array_access.index &&
                                array_access->data.array_access.index->type == KRYON_AST_LITERAL &&
                                array_access->data.array_access.index->data.literal.value.type == KRYON_VALUE_INTEGER) {
                                
                                int index = (int)array_access->data.array_access.index->data.literal.value.data.int_value;
                                
                                // Find the array property in the object literal
                                for (size_t i = 0; i < var_value->data.object_literal.property_count; i++) {
                                    const KryonASTNode *prop = var_value->data.object_literal.properties[i];
                                    if (prop && prop->type == KRYON_AST_PROPERTY &&
                                        prop->data.property.name &&
                                        strcmp(prop->data.property.name, property_name) == 0 &&
                                        prop->data.property.value &&
                                        prop->data.property.value->type == KRYON_AST_ARRAY_LITERAL) {
                                        
                                        const KryonASTNode *array = prop->data.property.value;
                                        if (index >= 0 && (size_t)index < array->data.array_literal.element_count) {
                                            // Found the array element - return a clone of it
                                            kryon_free(cloned);
                                            return kryon_substitute_template_vars(array->data.array_literal.elements[index], var_name, var_value);
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
            
            // Fallback: clone the template expression
            if (node->data.template.expression) {
                cloned->data.template.expression = kryon_substitute_template_vars(node->data.template.expression, var_name, var_value);
            }
            break;
        }
        
        default:
            // For other types, do basic copy
            memcpy(&cloned->data, &node->data, sizeof(node->data));
            break;
    }
    
    return cloned;
}

static char *process_string_template(const char *template_string, const char *var_name, const KryonASTNode *var_value) {
    if (!template_string || !var_name || !var_value) {
        return NULL;
    }
    
    // Simple implementation: replace ${var_name.property} with actual values
    char *result = malloc(strlen(template_string) + 512); // Extra space for substitutions
    if (!result) {
        return NULL;
    }
    
    strcpy(result, template_string);
    
    // Look for ${alignment.name}, ${alignment.value}, ${alignment.gap}, etc.
    char *pos = result;
    while ((pos = strstr(pos, "${")) != NULL) {
        char *end = strstr(pos, "}");
        if (!end) break;
        
        // Extract the variable reference
        size_t ref_len = end - pos - 2; // Length between ${ and }
        char ref[256];
        strncpy(ref, pos + 2, ref_len);
        ref[ref_len] = '\0';
        
        // Check if it matches our variable
        char pattern[256];
        snprintf(pattern, sizeof(pattern), "%s.", var_name);
        
        if (strncmp(ref, pattern, strlen(pattern)) == 0) {
            const char *property = ref + strlen(pattern);
            const char *replacement = NULL;
            static char num_buf[32];
            
            // If var_value is an object literal, look up the property
            if (var_value->type == KRYON_AST_OBJECT_LITERAL) {
                // Check for array access pattern: property[index]
                char *bracket = strchr(property, '[');
                if (bracket) {
                    // Handle array access like "colors[0]"
                    *bracket = '\0'; // Temporarily null-terminate property name
                    char *end_bracket = strchr(bracket + 1, ']');
                    if (end_bracket) {
                        *end_bracket = '\0';
                        int index = atoi(bracket + 1);
                        
                        // Find the array property
                        for (size_t i = 0; i < var_value->data.object_literal.property_count; i++) {
                            const KryonASTNode *prop = var_value->data.object_literal.properties[i];
                            if (prop && prop->type == KRYON_AST_PROPERTY &&
                                prop->data.property.name &&
                                strcmp(prop->data.property.name, property) == 0 &&
                                prop->data.property.value &&
                                prop->data.property.value->type == KRYON_AST_ARRAY_LITERAL) {
                                
                                const KryonASTNode *array = prop->data.property.value;
                                if (index >= 0 && (size_t)index < array->data.array_literal.element_count) {
                                    const KryonASTNode *element = array->data.array_literal.elements[index];
                                    if (element && element->type == KRYON_AST_LITERAL) {
                                        if (element->data.literal.value.type == KRYON_VALUE_STRING) {
                                            replacement = element->data.literal.value.data.string_value;
                                        }
                                    }
                                }
                                break;
                            }
                        }
                        
                        *end_bracket = ']'; // Restore bracket
                    }
                    *bracket = '['; // Restore bracket
                } else {
                    // Handle simple property access
                    for (size_t i = 0; i < var_value->data.object_literal.property_count; i++) {
                        const KryonASTNode *prop = var_value->data.object_literal.properties[i];
                        if (prop && prop->type == KRYON_AST_PROPERTY &&
                            prop->data.property.name &&
                            strcmp(prop->data.property.name, property) == 0) {
                            
                            const KryonASTNode *prop_value = prop->data.property.value;
                            if (prop_value && prop_value->type == KRYON_AST_LITERAL) {
                                if (prop_value->data.literal.value.type == KRYON_VALUE_STRING) {
                                    replacement = prop_value->data.literal.value.data.string_value;
                                } else if (prop_value->data.literal.value.type == KRYON_VALUE_INTEGER) {
                                    snprintf(num_buf, sizeof(num_buf), "%lld", 
                                           (long long)prop_value->data.literal.value.data.int_value);
                                    replacement = num_buf;
                                } else if (prop_value->data.literal.value.type == KRYON_VALUE_FLOAT) {
                                    snprintf(num_buf, sizeof(num_buf), "%.0f", 
                                           prop_value->data.literal.value.data.float_value);
                                    replacement = num_buf;
                                }
                            }
                            break;
                        }
                    }
                }
            }
            
            // If no replacement found, skip this template
            if (!replacement) {
                pos = end + 1;
                continue;
            }
            
            // Replace the template
            size_t template_len = end - pos + 1; // Include the }
            size_t replacement_len = strlen(replacement);
            
            // Shift the remaining string
            memmove(pos + replacement_len, end + 1, strlen(end + 1) + 1);
            
            // Insert the replacement
            memcpy(pos, replacement, replacement_len);
        }
        
        pos = end + 1;
    }
    
    return result;
}