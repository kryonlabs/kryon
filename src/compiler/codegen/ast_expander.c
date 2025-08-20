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
KryonASTNode *kryon_substitute_template_vars(const KryonASTNode *node, const char *var_name, const KryonASTNode *var_value, KryonCodeGenerator *codegen);
static char *process_string_template(const char *template_string, const char *var_name, const KryonASTNode *var_value, KryonCodeGenerator *codegen);
static bool is_compile_time_resolvable(const char *var_name, KryonCodeGenerator *codegen);

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
            // Template cloning - copy the segments structure
            cloned->data.template.segment_count = original->data.template.segment_count;
            cloned->data.template.segment_capacity = original->data.template.segment_capacity;
            
            if (original->data.template.segment_count > 0 && original->data.template.segments) {
                cloned->data.template.segments = kryon_alloc(original->data.template.segment_count * sizeof(KryonASTNode*));
                for (size_t i = 0; i < original->data.template.segment_count; i++) {
                    if (original->data.template.segments[i]) {
                        cloned->data.template.segments[i] = clone_and_substitute_node(original->data.template.segments[i], params, param_count);
                    } else {
                        cloned->data.template.segments[i] = NULL;
                    }
                }
            } else {
                cloned->data.template.segments = NULL;
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
            
            printf("DEBUG: Created value_node for i=%d\n", i);
            
            // Expand each element in the loop body
            for (size_t j = 0; j < const_for->data.const_for_loop.body_count; j++) {
                const KryonASTNode *body_element = const_for->data.const_for_loop.body[j];
                
                if (body_element && body_element->type == KRYON_AST_ELEMENT) {
                    // Substitute template variables in the element
                    KryonASTNode *substituted_element = kryon_substitute_template_vars(
                        body_element,
                        const_for->data.const_for_loop.var_name,
                        value_node,
                        codegen
                    );
                    
                    if (substituted_element) {
                        if (!kryon_write_element_instance(codegen, substituted_element, ast_root)) {
                            kryon_free(value_node);
                            return false;
                        }
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
                        array_element,
                        codegen
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

KryonASTNode *kryon_substitute_template_vars(const KryonASTNode *node, const char *var_name, const KryonASTNode *var_value, KryonCodeGenerator *codegen) {
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
        
        
        case KRYON_AST_LITERAL: {
            // Clone literal value with template processing for strings
            if (node->data.literal.value.type == KRYON_VALUE_STRING && node->data.literal.value.data.string_value) {
                const char *original_string = node->data.literal.value.data.string_value;
                
                // Check if string contains template variables ${...}
                if (strstr(original_string, "${") != NULL) {
                    char *processed = process_string_template(original_string, var_name, var_value, codegen);
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
            // Check if this identifier matches our loop variable and should be substituted
            if (node->data.identifier.name && strcmp(node->data.identifier.name, var_name) == 0) {
                // This is a direct reference to our loop variable
                // In const_for expansion context (we have var_value), always substitute
                if (var_value) {
                    // Substitute with the actual value
                    kryon_free(cloned);
                    return kryon_substitute_template_vars(var_value, var_name, var_value, codegen);
                }
            }
            
            // Clone identifier name (either different variable or runtime variable)
            if (node->data.identifier.name) {
                cloned->data.identifier.name = strdup(node->data.identifier.name);
            }
            break;
        }
        
        case KRYON_AST_MEMBER_ACCESS: {
            // Handle member access like "alignment.value"
            printf("🔧 DEBUG: Processing MEMBER_ACCESS: %s.%s\n", 
                   node->data.member_access.object && node->data.member_access.object->data.identifier.name 
                       ? node->data.member_access.object->data.identifier.name : "unknown",
                   node->data.member_access.member ? node->data.member_access.member : "unknown");
                   
            if (node->data.member_access.object &&
                node->data.member_access.object->type == KRYON_AST_IDENTIFIER &&
                node->data.member_access.object->data.identifier.name &&
                strcmp(node->data.member_access.object->data.identifier.name, var_name) == 0) {
                
                printf("✅ DEBUG: Found matching MEMBER_ACCESS for var '%s', substituting...\n", var_name);
                // This is accessing a property of our loop variable
                // In const_for expansion context (we have var_value), always substitute
                if (var_value) {
                    const char *property_name = node->data.member_access.member;
                    if (property_name && var_value->type == KRYON_AST_OBJECT_LITERAL) {
                        
                        // Find the property in the object literal
                        for (size_t i = 0; i < var_value->data.object_literal.property_count; i++) {
                            const KryonASTNode *prop = var_value->data.object_literal.properties[i];
                            if (prop && prop->type == KRYON_AST_PROPERTY &&
                                prop->data.property.name &&
                                strcmp(prop->data.property.name, property_name) == 0) {
                                
                                // Found the property - return a clone of its value
                                kryon_free(cloned);
                                return kryon_substitute_template_vars(prop->data.property.value, var_name, var_value, codegen);
                            }
                        }
                    }
                }
            }
            
            // Either not our variable or runtime variable - clone as-is
            if (node->data.member_access.object) {
                cloned->data.member_access.object = kryon_substitute_template_vars(node->data.member_access.object, var_name, var_value, codegen);
            }
            if (node->data.member_access.member) {
                cloned->data.member_access.member = strdup(node->data.member_access.member);
            }
            break;
        }
        
        case KRYON_AST_ARRAY_ACCESS: {
            // Handle array access like "alignment.colors[0]"
            printf("🔧 DEBUG: Processing ARRAY_ACCESS\n");
            
            if (node->data.array_access.array &&
                node->data.array_access.array->type == KRYON_AST_MEMBER_ACCESS) {
                
                KryonASTNode *member_access = node->data.array_access.array;
                
                printf("🔧 DEBUG: ARRAY_ACCESS on MEMBER_ACCESS: %s.%s[...]\n", 
                       member_access->data.member_access.object && member_access->data.member_access.object->data.identifier.name 
                           ? member_access->data.member_access.object->data.identifier.name : "unknown",
                       member_access->data.member_access.member ? member_access->data.member_access.member : "unknown");
                
                if (member_access->data.member_access.object &&
                    member_access->data.member_access.object->type == KRYON_AST_IDENTIFIER &&
                    member_access->data.member_access.object->data.identifier.name &&
                    strcmp(member_access->data.member_access.object->data.identifier.name, var_name) == 0) {
                    
                    printf("✅ DEBUG: Found matching ARRAY_ACCESS for var '%s', substituting...\n", var_name);
                    
                    // This is accessing an array property of our loop variable
                    // In const_for expansion context (we have var_value), always substitute
                    if (var_value) {
                        const char *property_name = member_access->data.member_access.member;
                        if (property_name && var_value->type == KRYON_AST_OBJECT_LITERAL &&
                            node->data.array_access.index &&
                            node->data.array_access.index->type == KRYON_AST_LITERAL &&
                            node->data.array_access.index->data.literal.value.type == KRYON_VALUE_INTEGER) {
                            
                            int index = (int)node->data.array_access.index->data.literal.value.data.int_value;
                            
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
                                        printf("✅ DEBUG: ARRAY_ACCESS resolved: %s[%d] -> found element\n", property_name, index);
                                        
                                        // Ensure we have a valid element to substitute
                                        const KryonASTNode *element = array->data.array_literal.elements[index];
                                        if (!element) {
                                            printf("❌ DEBUG: ARRAY_ACCESS element is NULL at index %d\n", index);
                                            break; // Fall through to clone logic
                                        }
                                        
                                        kryon_free(cloned);
                                        KryonASTNode *substituted = kryon_substitute_template_vars(element, var_name, var_value, codegen);
                                        if (!substituted) {
                                            printf("❌ DEBUG: ARRAY_ACCESS substitution returned NULL for %s[%d]\n", property_name, index);
                                            // Create a safe fallback
                                            substituted = create_minimal_ast_node(KRYON_AST_LITERAL);
                                            if (substituted) {
                                                substituted->data.literal.value.type = KRYON_VALUE_STRING;
                                                substituted->data.literal.value.data.string_value = strdup("ERROR");
                                            }
                                        } else {
                                            printf("✅ DEBUG: ARRAY_ACCESS substitution successful for %s[%d], node type=%d\n", 
                                                   property_name, index, substituted->type);
                                        }
                                        return substituted;
                                    } else {
                                        printf("❌ DEBUG: ARRAY_ACCESS index out of bounds: %s[%d] (array size: %zu)\n", 
                                               property_name, index, array->data.array_literal.element_count);
                                    }
                                }
                            }
                        }
                    }
                }
            }
            
            // Either not our variable or runtime variable - clone as-is
            printf("⚠️ DEBUG: ARRAY_ACCESS not substituted - cloning as-is (var_name=%s)\n", var_name ? var_name : "NULL");
            
            // For debugging: check what we're cloning
            if (node->data.array_access.array && node->data.array_access.array->type == KRYON_AST_MEMBER_ACCESS) {
                KryonASTNode *member_access = node->data.array_access.array;
                if (member_access->data.member_access.object && 
                    member_access->data.member_access.object->type == KRYON_AST_IDENTIFIER) {
                    printf("⚠️ DEBUG: Cloning ARRAY_ACCESS: %s.%s[...] (looking for var '%s')\n",
                           member_access->data.member_access.object->data.identifier.name ? member_access->data.member_access.object->data.identifier.name : "NULL",
                           member_access->data.member_access.member ? member_access->data.member_access.member : "NULL",
                           var_name ? var_name : "NULL");
                }
            }
            
            if (node->data.array_access.array) {
                cloned->data.array_access.array = kryon_substitute_template_vars(node->data.array_access.array, var_name, var_value, codegen);
            }
            if (node->data.array_access.index) {
                cloned->data.array_access.index = kryon_substitute_template_vars(node->data.array_access.index, var_name, var_value, codegen);
            }
            break;
        }
        
        case KRYON_AST_TEMPLATE: {
            // Implement template variable substitution for the segments structure
            printf("🔄 DEBUG: Processing TEMPLATE with %zu segments for variable substitution\n", node->data.template.segment_count);
            
            // Copy template structure
            cloned->data.template = node->data.template;
            
            // Substitute variables in each segment - TEMPLATE uses ASTNode segments, not KryonTemplateSegment
            if (node->data.template.segments && node->data.template.segment_count > 0) {
                // Allocate new segments array
                cloned->data.template.segments = kryon_malloc(node->data.template.segment_count * sizeof(KryonASTNode*));
                if (!cloned->data.template.segments) {
                    kryon_free(cloned);
                    return NULL;
                }
                
                // Process each segment
                for (size_t i = 0; i < node->data.template.segment_count; i++) {
                    const KryonASTNode *orig_seg = node->data.template.segments[i];
                    
                    if (orig_seg->type == KRYON_AST_VARIABLE) {
                        // Variable segment - check if it needs substitution
                        printf("🔍 DEBUG: Processing variable segment: '%s'\n", orig_seg->data.variable.name);
                        
                        // Check for MEMBER_ACCESS pattern like "alignment.name"
                        if (strncmp(orig_seg->data.variable.name, var_name, strlen(var_name)) == 0 && 
                            orig_seg->data.variable.name[strlen(var_name)] == '.') {
                            
                            const char *property_name = orig_seg->data.variable.name + strlen(var_name) + 1;  // Skip "alignment."
                            printf("🔧 DEBUG: Found template variable '%s' with property '%s'\n", var_name, property_name);
                            
                            if (var_value && var_value->type == KRYON_AST_OBJECT_LITERAL) {
                                // Find the property in the object literal
                                bool found = false;
                                for (size_t j = 0; j < var_value->data.object_literal.property_count; j++) {
                                    const KryonASTNode *prop = var_value->data.object_literal.properties[j];
                                    if (prop && prop->type == KRYON_AST_PROPERTY && 
                                        strcmp(prop->data.property.name, property_name) == 0) {
                                        
                                        if (prop->data.property.value && prop->data.property.value->type == KRYON_AST_LITERAL) {
                                            const char *resolved_value = prop->data.property.value->data.literal.value.data.string_value;
                                            printf("✅ DEBUG: Resolved template variable '%s' to '%s'\n", orig_seg->data.variable.name, resolved_value);
                                            
                                            // Create new literal node with resolved value
                                            KryonASTNode *literal_node = kryon_malloc(sizeof(KryonASTNode));
                                            literal_node->type = KRYON_AST_LITERAL;
                                            literal_node->data.literal.value = kryon_ast_value_string(resolved_value);
                                            literal_node->parent = cloned;
                                            cloned->data.template.segments[i] = literal_node;
                                            found = true;
                                            break;
                                        }
                                    }
                                }
                                
                                if (!found) {
                                    printf("⚠️  DEBUG: Could not resolve template variable '%s'\n", orig_seg->data.variable.name);
                                    // Clone original node
                                    cloned->data.template.segments[i] = kryon_substitute_template_vars(orig_seg, var_name, var_value, codegen);
                                }
                            } else {
                                printf("⚠️  DEBUG: Variable value not an object literal, keeping original\n");
                                // Clone original node
                                cloned->data.template.segments[i] = kryon_substitute_template_vars(orig_seg, var_name, var_value, codegen);
                            }
                        } else {
                            // Not our target variable, recursively process
                            cloned->data.template.segments[i] = kryon_substitute_template_vars(orig_seg, var_name, var_value, codegen);
                        }
                    } else {
                        // Non-variable segment - recursively process
                        cloned->data.template.segments[i] = kryon_substitute_template_vars(orig_seg, var_name, var_value, codegen);
                    }
                }
                cloned->data.template.segment_capacity = node->data.template.segment_capacity;
            } else {
                cloned->data.template.segments = NULL;
            }
            break;
        }
        
        case KRYON_AST_ELEMENT: {
            // Copy all element data first to avoid corruption
            cloned->data.element = node->data.element;
            
            // Copy element type string
            cloned->data.element.element_type = NULL;
            if (node->data.element.element_type) {
                cloned->data.element.element_type = strdup(node->data.element.element_type);
            }
            
            // Clear pointers to prevent double-free, then process recursively
            cloned->data.element.properties = NULL;
            cloned->data.element.children = NULL;
            
            // Process element properties recursively
            if (node->data.element.properties && node->data.element.property_count > 0) {
                cloned->data.element.properties = kryon_calloc(node->data.element.property_count, sizeof(KryonASTNode*));
                cloned->data.element.property_count = node->data.element.property_count;
                cloned->data.element.property_capacity = node->data.element.property_count;
                
                for (size_t i = 0; i < node->data.element.property_count; i++) {
                    if (node->data.element.properties[i]) {
                        cloned->data.element.properties[i] = kryon_substitute_template_vars(
                            node->data.element.properties[i], var_name, var_value, codegen);
                    }
                }
            }
            
            // Process element children recursively
            if (node->data.element.children && node->data.element.child_count > 0) {
                cloned->data.element.children = kryon_calloc(node->data.element.child_count, sizeof(KryonASTNode*));
                cloned->data.element.child_count = node->data.element.child_count;
                cloned->data.element.child_capacity = node->data.element.child_count;
                
                for (size_t i = 0; i < node->data.element.child_count; i++) {
                    if (node->data.element.children[i]) {
                        cloned->data.element.children[i] = kryon_substitute_template_vars(
                            node->data.element.children[i], var_name, var_value, codegen);
                    }
                }
            }
            break;
        }
        
        case KRYON_AST_PROPERTY: {
            // Copy all property data first
            cloned->data.property = node->data.property;
            
            // Clear pointers to prevent double-free
            cloned->data.property.name = NULL;
            cloned->data.property.value = NULL;
            
            // Copy property name
            if (node->data.property.name) {
                cloned->data.property.name = strdup(node->data.property.name);
            }
            
            // Process property value recursively
            if (node->data.property.value) {
                cloned->data.property.value = kryon_substitute_template_vars(
                    node->data.property.value, var_name, var_value, codegen);
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

static char *process_string_template(const char *template_string, const char *var_name, const KryonASTNode *var_value, KryonCodeGenerator *codegen) {
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
        
        // Check for simple variable match first (e.g., ${i})
        if (strcmp(ref, var_name) == 0) {
            // Simple variable substitution - convert var_value to string
            const char *replacement = NULL;
            static char num_buf[32];
            
            if (var_value->type == KRYON_AST_LITERAL) {
                if (var_value->data.literal.value.type == KRYON_VALUE_INTEGER) {
                    int val = (int)var_value->data.literal.value.data.int_value;
                    snprintf(num_buf, sizeof(num_buf), "%d", val);
                    replacement = num_buf;
                } else if (var_value->data.literal.value.type == KRYON_VALUE_FLOAT) {
                    snprintf(num_buf, sizeof(num_buf), "%.0f", 
                           var_value->data.literal.value.data.float_value);
                    replacement = num_buf;
                } else if (var_value->data.literal.value.type == KRYON_VALUE_STRING) {
                    replacement = var_value->data.literal.value.data.string_value;
                }
            }
            
            if (replacement) {
                // Replace the template
                size_t template_len = end - pos + 1; // Include the }
                size_t replacement_len = strlen(replacement);
                
                // Shift the remaining string
                memmove(pos + replacement_len, end + 1, strlen(end + 1) + 1);
                
                // Insert the replacement
                memcpy(pos, replacement, replacement_len);
                
                pos += replacement_len; // Move past the replacement
                continue;
            }
        }
        
        // Check if it matches an array access pattern (e.g., colors[i % 6])
        char *bracket = strchr(ref, '[');
        if (bracket) {
            // Extract array name
            size_t array_name_len = bracket - ref;
            char array_name[256];
            strncpy(array_name, ref, array_name_len);
            array_name[array_name_len] = '\0';
            
            // Extract index expression
            char *end_bracket = strchr(bracket + 1, ']');
            if (end_bracket) {
                *end_bracket = '\0';
                const char *index_expr = bracket + 1;
                
                // Handle expression evaluation (i % 6, i, etc.)
                int index = -1;
                if (strstr(index_expr, "%")) {
                    // Handle modulo expression like "i % 6"
                    char *percent = strstr(index_expr, "%");
                    *percent = '\0';
                    const char *left_operand = index_expr;
                    const char *right_operand = percent + 1;
                    
                    // Trim leading whitespace
                    while (*left_operand == ' ') left_operand++;
                    while (*right_operand == ' ') right_operand++;
                    
                    // Trim trailing whitespace
                    char left_trimmed[64], right_trimmed[64];
                    strcpy(left_trimmed, left_operand);
                    strcpy(right_trimmed, right_operand);
                    
                    // Remove trailing spaces from left operand
                    int len = strlen(left_trimmed);
                    while (len > 0 && left_trimmed[len-1] == ' ') {
                        left_trimmed[--len] = '\0';
                    }
                    
                    // Remove trailing spaces from right operand  
                    len = strlen(right_trimmed);
                    while (len > 0 && right_trimmed[len-1] == ' ') {
                        right_trimmed[--len] = '\0';
                    }
                    
                    left_operand = left_trimmed;
                    right_operand = right_trimmed;
                    
                    // Evaluate left operand (should be our variable)
                    int left_val = 0;
                    if (strcmp(left_operand, var_name) == 0 && var_value->type == KRYON_AST_LITERAL) {
                        left_val = (int)var_value->data.literal.value.data.int_value;
                    }
                    
                    // Evaluate right operand (should be a number)
                    int right_val = atoi(right_operand);
                    
                    
                    if (right_val > 0) {
                        index = left_val % right_val;
                    }
                    
                    *percent = '%'; // Restore
                } else if (strcmp(index_expr, var_name) == 0) {
                    // Simple variable reference
                    if (var_value->type == KRYON_AST_LITERAL) {
                        index = (int)var_value->data.literal.value.data.int_value;
                    }
                } else {
                    // Literal number
                    index = atoi(index_expr);
                }
                
                *end_bracket = ']'; // Restore
                
                // Now find the array constant and get the element
                if (index >= 0) {
                    // Use proper constant lookup now that we have codegen access
                    const KryonASTNode *array_const = find_constant_value(codegen, array_name);
                    
                    if (array_const && array_const->type == KRYON_AST_ARRAY_LITERAL) {
                        if ((size_t)index < array_const->data.array_literal.element_count) {
                            const KryonASTNode *element = array_const->data.array_literal.elements[index];
                            if (element && element->type == KRYON_AST_LITERAL && 
                                element->data.literal.value.type == KRYON_VALUE_STRING) {
                                
                                const char *replacement = element->data.literal.value.data.string_value;
                                
                                // Replace the template
                                size_t template_len = end - pos + 1;
                                size_t replacement_len = strlen(replacement);
                                
                                memmove(pos + replacement_len, end + 1, strlen(end + 1) + 1);
                                memcpy(pos, replacement, replacement_len);
                                
                                pos += replacement_len;
                                continue;
                            }
                        }
                    } else {
                    }
                }
            }
        }
        
        // Check if it matches our variable for property access
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
            
            pos += replacement_len; // Move past the replacement
        } else {
            pos = end + 1; // No replacement, move past the template
        }
    }
    
    return result;
}

static bool is_compile_time_resolvable(const char *var_name, KryonCodeGenerator *codegen) {
    if (!var_name || !codegen) {
        return false;
    }
    
    // Check if the variable is in the constant table (compile-time resolvable)
    const KryonASTNode *const_value = find_constant_value(codegen, var_name);
    return const_value != NULL;
}