/**

 * @file ast_expander.c
 * @brief AST expansion for components
 *
 * Handles expansion of custom components during code generation.
 * Provides parameter substitution and template processing.
 */
#include "lib9.h"


#include "codegen.h"
#include "memory.h"
#include "parser.h"
#include "types.h"
#include "../../shared/kryon_mappings.h"
#include "../../shared/krb_schema.h"

// Schema writer wrapper
static bool schema_writer(void *context, const void *data, size_t size) {
    KryonCodeGenerator *codegen = (KryonCodeGenerator *)context;
    return write_binary_data(codegen, data, size);
}

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
static char *process_string_template(const char *template_string, const char *var_name, const KryonASTNode *var_value, KryonCodeGenerator *codegen);
static bool is_compile_time_resolvable(const char *var_name, KryonCodeGenerator *codegen);
static KryonASTNode *clone_ast_literal(const KryonASTNode *original);

KryonASTNode *kryon_find_component_definition(const char *component_name, const KryonASTNode *ast_root) {
    if (!component_name || !ast_root) return NULL;
    
    fprintf(stderr, "🔍 Searching for component '%s' in AST with %zu children\n", 
           component_name, ast_root->data.element.child_count);
    
    // Search through all children in the AST root
    for (size_t i = 0; i < ast_root->data.element.child_count; i++) {
        const KryonASTNode *child = ast_root->data.element.children[i];
        fprintf(stderr, "🔍 Child %zu: type=%d", i, child ? child->type : -1);
        if (child && child->type == KRYON_AST_COMPONENT) {
            fprintf(stderr, " (COMPONENT: name='%s')", child->data.component.name ? child->data.component.name : "NULL");
            if (child->data.component.name && strcmp(child->data.component.name, component_name) == 0) {
                fprintf(stderr, " ✅ MATCH!\n");
                return (KryonASTNode*)child; // Found the component definition
            }
        }
        fprintf(stderr, "\n");
    }
    
    fprintf(stderr, "❌ Component '%s' not found in AST\n", component_name);
    return NULL; // Component not found
}

static KryonASTNode *find_component_definition(const char *component_name, const KryonASTNode *ast_root) {
    return kryon_find_component_definition(component_name, ast_root);
}

/**
 * @brief Resolve component inheritance by merging parent and child components
 *
 * Creates a new component definition that combines:
 * - Parent component's body (as the base element)
 * - Override properties from the @extends block
 * - Child component's state variables, functions, and body
 *
 * @param component_def The child component definition (with parent_component set)
 * @param ast_root The AST root to search for parent definitions
 * @return A new merged component definition, or the original if no parent
 */
KryonASTNode *kryon_resolve_component_inheritance(const KryonASTNode *component_def, const KryonASTNode *ast_root) {
    if (!component_def || component_def->type != KRYON_AST_COMPONENT) {
        return (KryonASTNode*)component_def;
    }

    // If no parent component, return as-is
    if (!component_def->data.component.parent_component) {
        return (KryonASTNode*)component_def;
    }

    fprintf(stderr, "🔗 Resolving inheritance: %s extends %s\n",
           component_def->data.component.name,
           component_def->data.component.parent_component);

    // Find parent component definition
    KryonASTNode *parent_def = find_component_definition(component_def->data.component.parent_component, ast_root);
    if (!parent_def) {
        // Check if parent is a built-in element (not a custom component)
        uint16_t element_hex = kryon_codegen_get_element_hex(component_def->data.component.parent_component);
        if (element_hex != 0) {
            fprintf(stderr, "✅ Parent '%s' is a built-in element (0x%04X), creating synthetic parent\n",
                   component_def->data.component.parent_component, element_hex);

            // Create a synthetic parent element
            KryonASTNode *parent_element = create_minimal_ast_node(KRYON_AST_ELEMENT);
            if (!parent_element) {
                fprintf(stderr, "❌ Failed to create synthetic parent element\n");
                return (KryonASTNode*)component_def;
            }

            parent_element->data.element.element_type = strdup(component_def->data.component.parent_component);

            // Apply override properties to the parent element
            if (component_def->data.component.override_count > 0) {
                fprintf(stderr, "🔧 Applying %zu override properties to built-in parent element\n",
                       component_def->data.component.override_count);

                for (size_t i = 0; i < component_def->data.component.override_count; i++) {
                    const KryonASTNode *override_prop = component_def->data.component.override_props[i];
                    if (override_prop && override_prop->type == KRYON_AST_PROPERTY) {
                        fprintf(stderr, "  🔧 Adding property: %s\n", override_prop->data.property.name);
                        add_property_to_node(parent_element, clone_and_substitute_node(override_prop, NULL, 0));
                    }
                }
            }

            // Make child body elements the children of this parent element
            if (component_def->data.component.body_count > 0) {
                fprintf(stderr, "🔧 Adding %zu child body elements as children of built-in parent element\n",
                       component_def->data.component.body_count);
                for (size_t i = 0; i < component_def->data.component.body_count; i++) {
                    add_child_to_node(parent_element, component_def->data.component.body_elements[i]);
                }
            }

            // Create merged component with this parent element as the body
            KryonASTNode *merged = create_minimal_ast_node(KRYON_AST_COMPONENT);
            if (!merged) {
                fprintf(stderr, "❌ Failed to create merged component node\n");
                return (KryonASTNode*)component_def;
            }

            merged->location = component_def->location;
            merged->data.component.name = component_def->data.component.name ? strdup(component_def->data.component.name) : NULL;

            // Copy parameters
            merged->data.component.parameter_count = component_def->data.component.parameter_count;
            if (component_def->data.component.parameter_count > 0) {
                merged->data.component.parameters = calloc(component_def->data.component.parameter_count, sizeof(char*));
                merged->data.component.param_defaults = calloc(component_def->data.component.parameter_count, sizeof(char*));
                for (size_t i = 0; i < component_def->data.component.parameter_count; i++) {
                    merged->data.component.parameters[i] = component_def->data.component.parameters[i] ?
                        strdup(component_def->data.component.parameters[i]) : NULL;
                    merged->data.component.param_defaults[i] = component_def->data.component.param_defaults[i] ?
                        strdup(component_def->data.component.param_defaults[i]) : NULL;
                }
            }

            // Copy state variables
            merged->data.component.state_count = component_def->data.component.state_count;
            if (component_def->data.component.state_count > 0) {
                merged->data.component.state_vars = calloc(component_def->data.component.state_count, sizeof(KryonASTNode*));
                for (size_t i = 0; i < component_def->data.component.state_count; i++) {
                    merged->data.component.state_vars[i] = component_def->data.component.state_vars[i];
                }
            }

            // Copy functions
            merged->data.component.function_count = component_def->data.component.function_count;
            if (component_def->data.component.function_count > 0) {
                merged->data.component.functions = calloc(component_def->data.component.function_count, sizeof(KryonASTNode*));
                for (size_t i = 0; i < component_def->data.component.function_count; i++) {
                    merged->data.component.functions[i] = component_def->data.component.functions[i];
                }
            }

            // Copy lifecycle hooks
            merged->data.component.on_create = component_def->data.component.on_create;
            merged->data.component.on_mount = component_def->data.component.on_mount;
            merged->data.component.on_unmount = component_def->data.component.on_unmount;

            // Set the parent element as the single body element
            merged->data.component.body_elements = calloc(1, sizeof(KryonASTNode*));
            merged->data.component.body_elements[0] = parent_element;
            merged->data.component.body_count = 1;
            merged->data.component.body_capacity = 1;

            // No parent for merged component (inheritance is resolved)
            merged->data.component.parent_component = NULL;
            merged->data.component.override_props = NULL;
            merged->data.component.override_count = 0;

            fprintf(stderr, "✅ Built-in parent inheritance resolved: %s extends %s\n",
                   merged->data.component.name, component_def->data.component.parent_component);

            return merged;
        }

        fprintf(stderr, "❌ Parent component '%s' not found (not a built-in element or custom component)\n",
               component_def->data.component.parent_component);
        return (KryonASTNode*)component_def;
    }

    // Recursively resolve parent's inheritance first (for custom component parents)
    parent_def = kryon_resolve_component_inheritance(parent_def, ast_root);

    // Create a merged component definition
    KryonASTNode *merged = create_minimal_ast_node(KRYON_AST_COMPONENT);
    if (!merged) {
        fprintf(stderr, "❌ Failed to create merged component node\n");
        return (KryonASTNode*)component_def;
    }

    // Copy child component metadata
    merged->location = component_def->location;
    merged->data.component.name = component_def->data.component.name ? strdup(component_def->data.component.name) : NULL;

    // Copy parameters from child (child's parameters take precedence)
    merged->data.component.parameter_count = component_def->data.component.parameter_count;
    if (component_def->data.component.parameter_count > 0) {
        merged->data.component.parameters = calloc(component_def->data.component.parameter_count, sizeof(char*));
        merged->data.component.param_defaults = calloc(component_def->data.component.parameter_count, sizeof(char*));
        for (size_t i = 0; i < component_def->data.component.parameter_count; i++) {
            merged->data.component.parameters[i] = component_def->data.component.parameters[i] ?
                strdup(component_def->data.component.parameters[i]) : NULL;
            merged->data.component.param_defaults[i] = component_def->data.component.param_defaults[i] ?
                strdup(component_def->data.component.param_defaults[i]) : NULL;
        }
    }

    // No parent for merged component (inheritance is resolved)
    merged->data.component.parent_component = NULL;
    merged->data.component.override_props = NULL;
    merged->data.component.override_count = 0;

    // Merge state variables (parent + child)
    size_t total_state_count = (parent_def->data.component.state_count + component_def->data.component.state_count);
    if (total_state_count > 0) {
        merged->data.component.state_vars = calloc(total_state_count, sizeof(KryonASTNode*));
        merged->data.component.state_count = 0;

        // Copy parent state vars
        for (size_t i = 0; i < parent_def->data.component.state_count; i++) {
            merged->data.component.state_vars[merged->data.component.state_count++] = parent_def->data.component.state_vars[i];
        }

        // Copy child state vars
        for (size_t i = 0; i < component_def->data.component.state_count; i++) {
            merged->data.component.state_vars[merged->data.component.state_count++] = component_def->data.component.state_vars[i];
        }
    }

    // Merge functions (parent + child)
    size_t total_func_count = (parent_def->data.component.function_count + component_def->data.component.function_count);
    if (total_func_count > 0) {
        merged->data.component.functions = calloc(total_func_count, sizeof(KryonASTNode*));
        merged->data.component.function_count = 0;

        // Copy parent functions
        for (size_t i = 0; i < parent_def->data.component.function_count; i++) {
            merged->data.component.functions[merged->data.component.function_count++] = parent_def->data.component.functions[i];
        }

        // Copy child functions
        for (size_t i = 0; i < component_def->data.component.function_count; i++) {
            merged->data.component.functions[merged->data.component.function_count++] = component_def->data.component.functions[i];
        }
    }

    // Copy lifecycle hooks (child overrides parent)
    merged->data.component.on_create = component_def->data.component.on_create ?
        component_def->data.component.on_create : parent_def->data.component.on_create;
    merged->data.component.on_mount = component_def->data.component.on_mount ?
        component_def->data.component.on_mount : parent_def->data.component.on_mount;
    merged->data.component.on_unmount = component_def->data.component.on_unmount ?
        component_def->data.component.on_unmount : parent_def->data.component.on_unmount;

    // Clone parent body elements as the base
    if (parent_def->data.component.body_count > 0) {
        merged->data.component.body_count = parent_def->data.component.body_count;
        merged->data.component.body_capacity = parent_def->data.component.body_count;
        merged->data.component.body_elements = calloc(merged->data.component.body_count, sizeof(KryonASTNode*));

        for (size_t i = 0; i < parent_def->data.component.body_count; i++) {
            merged->data.component.body_elements[i] = clone_and_substitute_node(parent_def->data.component.body_elements[i], NULL, 0);
        }

        // Apply override properties to the first cloned parent body element
        if (component_def->data.component.override_count > 0 && merged->data.component.body_count > 0) {
            fprintf(stderr, "🔧 Applying %zu override properties to parent body\n", component_def->data.component.override_count);

            KryonASTNode *first_body = merged->data.component.body_elements[0];
            // Add or replace properties in the parent body
            for (size_t i = 0; i < component_def->data.component.override_count; i++) {
                const KryonASTNode *override_prop = component_def->data.component.override_props[i];
                if (!override_prop || override_prop->type != KRYON_AST_PROPERTY) continue;

                const char *prop_name = override_prop->data.property.name;
                fprintf(stderr, "  🔧 Override property: %s\n", prop_name);

                // Find and replace existing property, or add if not found
                bool found = false;
                for (size_t j = 0; j < first_body->data.element.property_count; j++) {
                    KryonASTNode *existing_prop = first_body->data.element.properties[j];
                    if (existing_prop && existing_prop->type == KRYON_AST_PROPERTY &&
                        existing_prop->data.property.name &&
                        strcmp(existing_prop->data.property.name, prop_name) == 0) {
                        // Replace existing property
                        fprintf(stderr, "    ✅ Replaced existing property '%s'\n", prop_name);
                        first_body->data.element.properties[j] =
                            clone_and_substitute_node(override_prop, NULL, 0);
                        found = true;
                        break;
                    }
                }

                if (!found) {
                    // Add new property
                    fprintf(stderr, "    ✅ Added new property '%s'\n", prop_name);
                    add_property_to_node(first_body,
                                        clone_and_substitute_node(override_prop, NULL, 0));
                }
            }
        }
    }

    // If child has its own body elements, replace the merged body with child's body
    // (child body takes full precedence over parent body)
    if (component_def->data.component.body_count > 0) {
        merged->data.component.body_elements = component_def->data.component.body_elements;
        merged->data.component.body_count = component_def->data.component.body_count;
        merged->data.component.body_capacity = component_def->data.component.body_capacity;
    }

    fprintf(stderr, "✅ Inheritance resolved: %s (merged %zu state vars, %zu functions)\n",
           merged->data.component.name,
           merged->data.component.state_count,
           merged->data.component.function_count);

    return merged;
}

KryonASTNode *kryon_expand_component_instance(const KryonASTNode *component_instance, const KryonASTNode *ast_root) {
    if (!component_instance || component_instance->type != KRYON_AST_ELEMENT) {
        return NULL;
    }
    
    const char *component_name = component_instance->data.element.element_type;
    fprintf(stderr, "🔍 Looking for component definition: %s\n", component_name);
    
    KryonASTNode *component_def = find_component_definition(component_name, ast_root);
    if (!component_def) {
        fprintf(stderr, "❌ Component definition not found: %s\n", component_name);
        return NULL;
    }

    // Resolve component inheritance if present
    component_def = kryon_resolve_component_inheritance(component_def, ast_root);

    fprintf(stderr, "✅ Found component definition: %s\n", component_name);
    fprintf(stderr, "🔍 Component has %zu parameters, instance has %zu properties\n",
           component_def->data.component.parameter_count,
           component_instance->data.element.property_count);

    if (component_def->data.component.body_count == 0) {
        fprintf(stderr, "❌ Component has no body to expand\n");
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
                    fprintf(stderr, "🔗 Mapped instance property '%s' to component parameter\n", prop->data.property.name);
                }
            }
        }
    }
    
    // Clone the component body elements with parameter substitution
    // For components with multiple body elements, we need to return the first one
    // and the caller will handle the rest
    KryonASTNode *expanded = clone_and_substitute_node(component_def->data.component.body_elements[0], params, param_count);

    // Clean up parameter context
    kryon_free(params);
    if (!expanded) {
        fprintf(stderr, "❌ Failed to clone component body\n");
        return NULL;
    }

    // TODO: Handle multiple body elements - for now we only return the first one
    // This maintains backwards compatibility but doesn't fully support multiple roots yet
    
    fprintf(stderr, "✅ Successfully expanded component: %s\n", component_name);
    return expanded;
}

static KryonASTNode *clone_and_substitute_node(const KryonASTNode *original, const ParamContext *params, size_t param_count) {
    if (!original) return NULL;
    
    fprintf(stderr, "🔍 DEBUG: Cloning node type %d\n", original->type);
    
    // Create new node
    KryonASTNode *cloned = create_minimal_ast_node(original->type);
    if (!cloned) {
        fprintf(stderr, "🔍 DEBUG: Failed to create cloned node\n");
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

            fprintf(stderr, "🔍 DEBUG: Cloning element '%s' with %zu properties and %zu children\n",
                   original->data.element.element_type ? original->data.element.element_type : "(null)",
                   original->data.element.property_count,
                   original->data.element.child_count);

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
                    fprintf(stderr, "🔍 DEBUG: Cloning child %zu of %zu (type %d)\n", i, original->data.element.child_count,
                           original->data.element.children[i] ? original->data.element.children[i]->type : -1);
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
                fprintf(stderr, "🔍 DEBUG: Checking identifier '%s' for substitution (params has %zu entries)\n", 
                       original->data.identifier.name, param_count);
                const KryonASTNode *param_value = find_param_value(params, param_count, original->data.identifier.name);
                if (param_value) {
                    fprintf(stderr, "✅ Substituting identifier '%s'\n", original->data.identifier.name);
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
                fprintf(stderr, "🔍 DEBUG: Checking variable '$%s' for substitution (params has %zu entries)\n", 
                       original->data.variable.name, param_count);
                const KryonASTNode *param_value = find_param_value(params, param_count, original->data.variable.name);
                if (param_value) {
                    fprintf(stderr, "✅ Substituting variable '$%s'\n", original->data.variable.name);
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

        case KRYON_AST_IF_DIRECTIVE: {
            // Clone conditional structure
            cloned->data.conditional.is_const = original->data.conditional.is_const;

            // Clone condition expression
            if (original->data.conditional.condition) {
                cloned->data.conditional.condition = clone_and_substitute_node(original->data.conditional.condition, params, param_count);
            } else {
                cloned->data.conditional.condition = NULL;
            }

            // Clone then body
            cloned->data.conditional.then_count = original->data.conditional.then_count;
            cloned->data.conditional.then_capacity = original->data.conditional.then_capacity;
            if (original->data.conditional.then_count > 0 && original->data.conditional.then_body) {
                cloned->data.conditional.then_body = calloc(original->data.conditional.then_count, sizeof(KryonASTNode*));
                for (size_t i = 0; i < original->data.conditional.then_count; i++) {
                    cloned->data.conditional.then_body[i] = clone_and_substitute_node(original->data.conditional.then_body[i], params, param_count);
                }
            } else {
                cloned->data.conditional.then_body = NULL;
            }

            // Clone elif conditions and bodies
            cloned->data.conditional.elif_count = original->data.conditional.elif_count;
            cloned->data.conditional.elif_capacity = original->data.conditional.elif_capacity;
            if (original->data.conditional.elif_count > 0) {
                cloned->data.conditional.elif_conditions = calloc(original->data.conditional.elif_count, sizeof(KryonASTNode*));
                cloned->data.conditional.elif_bodies = calloc(original->data.conditional.elif_count, sizeof(KryonASTNode**));
                cloned->data.conditional.elif_counts = calloc(original->data.conditional.elif_count, sizeof(size_t));

                for (size_t i = 0; i < original->data.conditional.elif_count; i++) {
                    // Clone elif condition
                    cloned->data.conditional.elif_conditions[i] = clone_and_substitute_node(original->data.conditional.elif_conditions[i], params, param_count);

                    // Clone elif body
                    cloned->data.conditional.elif_counts[i] = original->data.conditional.elif_counts[i];
                    if (original->data.conditional.elif_counts[i] > 0) {
                        cloned->data.conditional.elif_bodies[i] = calloc(original->data.conditional.elif_counts[i], sizeof(KryonASTNode*));
                        for (size_t j = 0; j < original->data.conditional.elif_counts[i]; j++) {
                            cloned->data.conditional.elif_bodies[i][j] = clone_and_substitute_node(original->data.conditional.elif_bodies[i][j], params, param_count);
                        }
                    } else {
                        cloned->data.conditional.elif_bodies[i] = NULL;
                    }
                }
            } else {
                cloned->data.conditional.elif_conditions = NULL;
                cloned->data.conditional.elif_bodies = NULL;
                cloned->data.conditional.elif_counts = NULL;
            }

            // Clone else body
            cloned->data.conditional.else_count = original->data.conditional.else_count;
            cloned->data.conditional.else_capacity = original->data.conditional.else_capacity;
            if (original->data.conditional.else_count > 0 && original->data.conditional.else_body) {
                cloned->data.conditional.else_body = calloc(original->data.conditional.else_count, sizeof(KryonASTNode*));
                for (size_t i = 0; i < original->data.conditional.else_count; i++) {
                    cloned->data.conditional.else_body[i] = clone_and_substitute_node(original->data.conditional.else_body[i], params, param_count);
                }
            } else {
                cloned->data.conditional.else_body = NULL;
            }
            break;
        }

        default:
            // For other types, do basic copy
            memcpy(&cloned->data, &original->data, sizeof(original->data));
            break;
    }
    
    fprintf(stderr, "🔍 DEBUG: Successfully cloned node type %d\n", original->type);
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

static KryonASTNode *clone_ast_literal(const KryonASTNode *original) {
    if (!original) return NULL;
    
    KryonASTNode *clone = create_minimal_ast_node(original->type);
    if (!clone) return NULL;
    
    // Copy the node data based on type
    switch (original->type) {
        case KRYON_AST_LITERAL:
            clone->data.literal.value = original->data.literal.value;
            // Deep copy string values
            if (original->data.literal.value.type == KRYON_VALUE_STRING && 
                original->data.literal.value.data.string_value) {
                clone->data.literal.value.data.string_value = strdup(original->data.literal.value.data.string_value);
            }
            break;
            
        default:
            // For other types, just copy the data structure
            memcpy(&clone->data, &original->data, sizeof(original->data));
            break;
    }
    
    // Copy location info
    clone->location = original->location;
    
    return clone;
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


// Helper function to serialize condition expression to string
char *serialize_condition_expression(const KryonASTNode *condition) {
    if (!condition) {
        return NULL;
    }

    fprintf(stderr, "DEBUG: serialize_condition_expression called with node type=%d\n", condition->type);

    // Handle literal boolean values
    if (condition->type == KRYON_AST_LITERAL) {
        if (condition->data.literal.value.type == KRYON_VALUE_BOOLEAN) {
            return kryon_strdup(condition->data.literal.value.data.bool_value ? "true" : "false");
        }
        if (condition->data.literal.value.type == KRYON_VALUE_INTEGER) {
            char buf[64];
            snprint(buf, sizeof(buf), "%lld", (long long)condition->data.literal.value.data.int_value);
            return kryon_strdup(buf);
        }
        if (condition->data.literal.value.type == KRYON_VALUE_STRING) {
            // Return quoted string
            size_t len = strlen(condition->data.literal.value.data.string_value);
            char *quoted = kryon_alloc(len + 3); // quotes + null
            if (quoted) {
                snprint(quoted, len + 3, "\"%s\"", condition->data.literal.value.data.string_value);
            }
            return quoted;
        }
    }

    // Handle variable references (most common case for @if with @var)
    if (condition->type == KRYON_AST_VARIABLE) {
        return kryon_strdup(condition->data.variable.name);
    }

    // Handle identifier references (variable names without $ prefix in expressions)
    if (condition->type == KRYON_AST_IDENTIFIER) {
        return kryon_strdup(condition->data.identifier.name);
    }

    // Handle unary operations (e.g., !showMessage)
    if (condition->type == KRYON_AST_UNARY_OP) {
        char *operand_str = serialize_condition_expression(condition->data.unary_op.operand);
        if (!operand_str) return NULL;

        const char *op_str = "!"; // Only logical NOT is common
        size_t len = strlen(op_str) + strlen(operand_str) + 1;
        char *result = kryon_alloc(len);
        if (result) {
            snprint(result, len, "%s%s", op_str, operand_str);
        }
        kryon_free(operand_str);
        return result;
    }

    // Handle binary operations (e.g., count > 0, isEnabled && isVisible)
    if (condition->type == KRYON_AST_BINARY_OP) {
        char *left_str = serialize_condition_expression(condition->data.binary_op.left);
        char *right_str = serialize_condition_expression(condition->data.binary_op.right);
        if (!left_str || !right_str) {
            kryon_free(left_str);
            kryon_free(right_str);
            return NULL;
        }

        const char *op_str = "";
        switch (condition->data.binary_op.operator) {
            case KRYON_TOKEN_LOGICAL_AND: op_str = " && "; break;
            case KRYON_TOKEN_LOGICAL_OR: op_str = " || "; break;
            case KRYON_TOKEN_EQUALS: op_str = " == "; break;
            case KRYON_TOKEN_NOT_EQUALS: op_str = " != "; break;
            case KRYON_TOKEN_LESS_THAN: op_str = " < "; break;
            case KRYON_TOKEN_LESS_EQUAL: op_str = " <= "; break;
            case KRYON_TOKEN_GREATER_THAN: op_str = " > "; break;
            case KRYON_TOKEN_GREATER_EQUAL: op_str = " >= "; break;
            default: op_str = " ??? "; break;
        }

        size_t len = strlen(left_str) + strlen(op_str) + strlen(right_str) + 1;
        char *result = kryon_alloc(len);
        if (result) {
            snprint(result, len, "%s%s%s", left_str, op_str, right_str);
        }
        kryon_free(left_str);
        kryon_free(right_str);
        return result;
    }

    // Unsupported condition type
    fprintf(stderr, "WARNING: Unsupported condition type %d for serialization\n", condition->type);
    return kryon_strdup("false");
}

/**
 * @brief Write @if directive as runtime conditional element
 * @param codegen Code generator context
 * @param if_directive The if AST node
 * @param ast_root Root AST node
 * @return true on success, false on error
 */
bool kryon_write_if_directive(KryonCodeGenerator *codegen, const KryonASTNode *if_directive, const KryonASTNode *ast_root) {
    if (!if_directive || if_directive->type != KRYON_AST_IF_DIRECTIVE) {
        return false;
    }

    fprintf(stderr, "DEBUG: Writing @if directive as runtime conditional element\n");
    fprintf(stderr, "DEBUG: @if condition pointer: %p\n", (void*)if_directive->data.conditional.condition);
    if (if_directive->data.conditional.condition) {
        fprintf(stderr, "DEBUG: @if condition node type: %d\n", if_directive->data.conditional.condition->type);
    }

    // Serialize the condition expression to string
    char *condition_str = serialize_condition_expression(if_directive->data.conditional.condition);
    if (!condition_str) {
        fprintf(stderr, "ERROR: Failed to serialize @if condition expression (condition was %s)\n",
               if_directive->data.conditional.condition ? "non-NULL" : "NULL");
        return false;
    }

    fprintf(stderr, "DEBUG: @if condition serialized as: '%s'\n", condition_str);

    // Get element hex for "if" syntax directive
    uint16_t if_element_hex = kryon_get_syntax_hex("if");
    fprintf(stderr, "DEBUG: @if element hex code: 0x%04X\n", if_element_hex);

    // Create element header using schema format
    uint32_t instance_id = codegen->next_element_id++;
    uint16_t child_count = (uint16_t)(if_directive->data.conditional.then_count + if_directive->data.conditional.else_count);
    uint16_t property_count = 2; // condition property + then_count property

    KRBElementHeader header = {
        .instance_id = instance_id,
        .element_type = (uint32_t)if_element_hex,
        .parent_id = 0,
        .style_ref = 0,
        .property_count = property_count,
        .child_count = child_count,
        .event_count = 0,
    };

    // Write element header
    if (!krb_write_element_header(schema_writer, codegen, &header)) {
        fprintf(stderr, "ERROR: Failed to write @if element header\n");
        kryon_free(condition_str);
        return false;
    }

    // Write condition property using schema-based property header
    uint16_t condition_prop_hex = 0x9100; // Custom property for "condition"

    KRBPropertyHeader prop_header = {
        .property_id = condition_prop_hex,
        .value_type = KRYON_RUNTIME_PROP_STRING
    };

    fprintf(stderr, "DEBUG: Writing condition property with hex 0x%04X, value='%s'\n", condition_prop_hex, condition_str);

    // Write property header using schema writer
    if (!krb_write_property_header(schema_writer, codegen, &prop_header)) {
        fprintf(stderr, "ERROR: Failed to write condition property header\n");
        kryon_free(condition_str);
        return false;
    }

    // Write string value (length + data)
    uint16_t condition_len = (uint16_t)strlen(condition_str);
    if (!write_uint16(codegen, condition_len) ||
        !write_binary_data(codegen, condition_str, condition_len)) {
        fprintf(stderr, "ERROR: Failed to write condition property value\n");
        kryon_free(condition_str);
        return false;
    }

    fprintf(stderr, "DEBUG: Successfully wrote condition property (length=%u)\n", condition_len);
    kryon_free(condition_str);

    // Write then_count property to mark where "then" ends and "else" begins
    // Property 0x9101 = then_count (helps runtime split children correctly)
    uint16_t then_count_prop_id = 0x9101;
    uint8_t then_count_type = KRYON_RUNTIME_PROP_INTEGER;
    uint32_t then_count_value = (uint32_t)if_directive->data.conditional.then_count;

    write_binary_data(codegen, (const char*)&then_count_prop_id, sizeof(uint16_t));
    write_binary_data(codegen, (const char*)&then_count_type, sizeof(uint8_t));
    if (!write_uint32(codegen, then_count_value)) {
        fprintf(stderr, "ERROR: Failed to write then_count value\n");
        return false;
    }

    fprintf(stderr, "DEBUG: Wrote then_count property: %u\n", then_count_value);

    // Write then body children recursively
    fprintf(stderr, "DEBUG: Writing %zu children for @if then body\n", if_directive->data.conditional.then_count);
    for (size_t i = 0; i < if_directive->data.conditional.then_count; i++) {
        if (!kryon_write_element_instance(codegen, if_directive->data.conditional.then_body[i], ast_root)) {
            fprintf(stderr, "ERROR: Failed to write @if then child %zu\n", i);
            return false;
        }
    }

    // Write else body children (if any)
    if (if_directive->data.conditional.else_count > 0) {
        fprintf(stderr, "DEBUG: Writing %zu children for @else body\n", if_directive->data.conditional.else_count);
        for (size_t i = 0; i < if_directive->data.conditional.else_count; i++) {
            if (!kryon_write_element_instance(codegen, if_directive->data.conditional.else_body[i], ast_root)) {
                fprintf(stderr, "ERROR: Failed to write @if else child %zu\n", i);
                return false;
            }
        }
    }

    // TODO: Support elif branches
    if (if_directive->data.conditional.elif_count > 0) {
        fprintf(stderr, "WARNING: @elif branches not yet supported at runtime\n");
    }

    fprintf(stderr, "DEBUG: @if directive written successfully\n");
    return true;
}
