/**

 * @file ast_expander.c
 * @brief AST expansion for components and const_for loops
 *
 * Handles expansion of custom components and const_for loops during code generation.
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
// Make this function public so codegen.c can use it
KryonASTNode *kryon_substitute_template_vars(const KryonASTNode *node, const char *var_name, const KryonASTNode *var_value, KryonCodeGenerator *codegen);
static char *process_string_template(const char *template_string, const char *var_name, const KryonASTNode *var_value, KryonCodeGenerator *codegen);
static bool is_compile_time_resolvable(const char *var_name, KryonCodeGenerator *codegen);
static KryonASTNode *clone_ast_literal(const KryonASTNode *original);

KryonASTNode *kryon_find_component_definition(const char *component_name, const KryonASTNode *ast_root) {
    if (!component_name || !ast_root) return NULL;
    
    print("🔍 Searching for component '%s' in AST with %zu children\n", 
           component_name, ast_root->data.element.child_count);
    
    // Search through all children in the AST root
    for (size_t i = 0; i < ast_root->data.element.child_count; i++) {
        const KryonASTNode *child = ast_root->data.element.children[i];
        print("🔍 Child %zu: type=%d", i, child ? child->type : -1);
        if (child && child->type == KRYON_AST_COMPONENT) {
            print(" (COMPONENT: name='%s')", child->data.component.name ? child->data.component.name : "NULL");
            if (child->data.component.name && strcmp(child->data.component.name, component_name) == 0) {
                print(" ✅ MATCH!\n");
                return (KryonASTNode*)child; // Found the component definition
            }
        }
        print("\n");
    }
    
    print("❌ Component '%s' not found in AST\n", component_name);
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

    print("🔗 Resolving inheritance: %s extends %s\n",
           component_def->data.component.name,
           component_def->data.component.parent_component);

    // Find parent component definition
    KryonASTNode *parent_def = find_component_definition(component_def->data.component.parent_component, ast_root);
    if (!parent_def) {
        // Check if parent is a built-in element (not a custom component)
        uint16_t element_hex = kryon_codegen_get_element_hex(component_def->data.component.parent_component);
        if (element_hex != 0) {
            print("✅ Parent '%s' is a built-in element (0x%04X), creating synthetic parent\n",
                   component_def->data.component.parent_component, element_hex);

            // Create a synthetic parent element
            KryonASTNode *parent_element = create_minimal_ast_node(KRYON_AST_ELEMENT);
            if (!parent_element) {
                print("❌ Failed to create synthetic parent element\n");
                return (KryonASTNode*)component_def;
            }

            parent_element->data.element.element_type = strdup(component_def->data.component.parent_component);

            // Apply override properties to the parent element
            if (component_def->data.component.override_count > 0) {
                print("🔧 Applying %zu override properties to built-in parent element\n",
                       component_def->data.component.override_count);

                for (size_t i = 0; i < component_def->data.component.override_count; i++) {
                    const KryonASTNode *override_prop = component_def->data.component.override_props[i];
                    if (override_prop && override_prop->type == KRYON_AST_PROPERTY) {
                        print("  🔧 Adding property: %s\n", override_prop->data.property.name);
                        add_property_to_node(parent_element, clone_and_substitute_node(override_prop, NULL, 0));
                    }
                }
            }

            // Make child body elements the children of this parent element
            if (component_def->data.component.body_count > 0) {
                print("🔧 Adding %zu child body elements as children of built-in parent element\n",
                       component_def->data.component.body_count);
                for (size_t i = 0; i < component_def->data.component.body_count; i++) {
                    add_child_to_node(parent_element, component_def->data.component.body_elements[i]);
                }
            }

            // Create merged component with this parent element as the body
            KryonASTNode *merged = create_minimal_ast_node(KRYON_AST_COMPONENT);
            if (!merged) {
                print("❌ Failed to create merged component node\n");
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

            print("✅ Built-in parent inheritance resolved: %s extends %s\n",
                   merged->data.component.name, component_def->data.component.parent_component);

            return merged;
        }

        print("❌ Parent component '%s' not found (not a built-in element or custom component)\n",
               component_def->data.component.parent_component);
        return (KryonASTNode*)component_def;
    }

    // Recursively resolve parent's inheritance first (for custom component parents)
    parent_def = kryon_resolve_component_inheritance(parent_def, ast_root);

    // Create a merged component definition
    KryonASTNode *merged = create_minimal_ast_node(KRYON_AST_COMPONENT);
    if (!merged) {
        print("❌ Failed to create merged component node\n");
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
            print("🔧 Applying %zu override properties to parent body\n", component_def->data.component.override_count);

            KryonASTNode *first_body = merged->data.component.body_elements[0];
            // Add or replace properties in the parent body
            for (size_t i = 0; i < component_def->data.component.override_count; i++) {
                const KryonASTNode *override_prop = component_def->data.component.override_props[i];
                if (!override_prop || override_prop->type != KRYON_AST_PROPERTY) continue;

                const char *prop_name = override_prop->data.property.name;
                print("  🔧 Override property: %s\n", prop_name);

                // Find and replace existing property, or add if not found
                bool found = false;
                for (size_t j = 0; j < first_body->data.element.property_count; j++) {
                    KryonASTNode *existing_prop = first_body->data.element.properties[j];
                    if (existing_prop && existing_prop->type == KRYON_AST_PROPERTY &&
                        existing_prop->data.property.name &&
                        strcmp(existing_prop->data.property.name, prop_name) == 0) {
                        // Replace existing property
                        print("    ✅ Replaced existing property '%s'\n", prop_name);
                        first_body->data.element.properties[j] =
                            clone_and_substitute_node(override_prop, NULL, 0);
                        found = true;
                        break;
                    }
                }

                if (!found) {
                    // Add new property
                    print("    ✅ Added new property '%s'\n", prop_name);
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

    print("✅ Inheritance resolved: %s (merged %zu state vars, %zu functions)\n",
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
    print("🔍 Looking for component definition: %s\n", component_name);
    
    KryonASTNode *component_def = find_component_definition(component_name, ast_root);
    if (!component_def) {
        print("❌ Component definition not found: %s\n", component_name);
        return NULL;
    }

    // Resolve component inheritance if present
    component_def = kryon_resolve_component_inheritance(component_def, ast_root);

    print("✅ Found component definition: %s\n", component_name);
    print("🔍 Component has %zu parameters, instance has %zu properties\n",
           component_def->data.component.parameter_count,
           component_instance->data.element.property_count);

    if (component_def->data.component.body_count == 0) {
        print("❌ Component has no body to expand\n");
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
                    print("🔗 Mapped instance property '%s' to component parameter\n", prop->data.property.name);
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
        print("❌ Failed to clone component body\n");
        return NULL;
    }

    // TODO: Handle multiple body elements - for now we only return the first one
    // This maintains backwards compatibility but doesn't fully support multiple roots yet
    
    print("✅ Successfully expanded component: %s\n", component_name);
    return expanded;
}

static KryonASTNode *clone_and_substitute_node(const KryonASTNode *original, const ParamContext *params, size_t param_count) {
    if (!original) return NULL;
    
    print("🔍 DEBUG: Cloning node type %d\n", original->type);
    
    // Create new node
    KryonASTNode *cloned = create_minimal_ast_node(original->type);
    if (!cloned) {
        print("🔍 DEBUG: Failed to create cloned node\n");
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

            print("🔍 DEBUG: Cloning element '%s' with %zu properties and %zu children\n",
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
                    print("🔍 DEBUG: Cloning child %zu of %zu (type %d)\n", i, original->data.element.child_count,
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
                print("🔍 DEBUG: Checking identifier '%s' for substitution (params has %zu entries)\n", 
                       original->data.identifier.name, param_count);
                const KryonASTNode *param_value = find_param_value(params, param_count, original->data.identifier.name);
                if (param_value) {
                    print("✅ Substituting identifier '%s'\n", original->data.identifier.name);
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
                print("🔍 DEBUG: Checking variable '$%s' for substitution (params has %zu entries)\n", 
                       original->data.variable.name, param_count);
                const KryonASTNode *param_value = find_param_value(params, param_count, original->data.variable.name);
                if (param_value) {
                    print("✅ Substituting variable '$%s'\n", original->data.variable.name);
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
    
    print("🔍 DEBUG: Successfully cloned node type %d\n", original->type);
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

bool kryon_expand_const_for_loop(KryonCodeGenerator *codegen, const KryonASTNode *const_for, const KryonASTNode *ast_root) {
    if (!const_for || const_for->type != KRYON_AST_CONST_FOR_LOOP) {
        return false;
    }
    
    if (const_for->data.const_for_loop.is_range) {
        // Handle range-based loop
        int start = const_for->data.const_for_loop.range_start;
        int end = const_for->data.const_for_loop.range_end;

        if (const_for->data.const_for_loop.index_var_name) {
            print("DEBUG: Expanding range const_for loop: %s, %s in %d..%d\n",
                   const_for->data.const_for_loop.index_var_name,
                   const_for->data.const_for_loop.var_name, start, end);
        } else {
            print("DEBUG: Expanding range const_for loop: %s in %d..%d\n",
                   const_for->data.const_for_loop.var_name, start, end);
        }

        // Expand loop body for each value in range
        int iteration_index = 0;
        for (int i = start; i <= end; i++, iteration_index++) {
            print("DEBUG: Processing range value %d (index %d)\n", i, iteration_index);

            // Create a literal AST node for the current value
            KryonASTNode *value_node = kryon_calloc(1, sizeof(KryonASTNode));
            if (!value_node) {
                print("DEBUG: ERROR: Failed to allocate value node\n");
                return false;
            }

            value_node->type = KRYON_AST_LITERAL;
            value_node->data.literal.value.type = KRYON_VALUE_INTEGER;
            value_node->data.literal.value.data.int_value = i;

            // Create index node if index variable is present
            KryonASTNode *index_node = NULL;
            if (const_for->data.const_for_loop.index_var_name) {
                index_node = kryon_calloc(1, sizeof(KryonASTNode));
                if (!index_node) {
                    print("DEBUG: ERROR: Failed to allocate index node\n");
                    kryon_free(value_node);
                    return false;
                }

                index_node->type = KRYON_AST_LITERAL;
                index_node->data.literal.value.type = KRYON_VALUE_INTEGER;
                index_node->data.literal.value.data.int_value = iteration_index;
            }

            print("DEBUG: Created value_node for i=%d\n", i);

            // Expand each element in the loop body
            for (size_t j = 0; j < const_for->data.const_for_loop.body_count; j++) {
                const KryonASTNode *body_element = const_for->data.const_for_loop.body[j];

                if (body_element && body_element->type == KRYON_AST_ELEMENT) {
                    // First substitute index variable if present
                    KryonASTNode *element_with_index = (KryonASTNode*)body_element;
                    if (index_node) {
                        element_with_index = kryon_substitute_template_vars(
                            body_element,
                            const_for->data.const_for_loop.index_var_name,
                            index_node,
                            codegen
                        );
                        if (!element_with_index) {
                            element_with_index = (KryonASTNode*)body_element;
                        }
                    }

                    // Then substitute value variable
                    KryonASTNode *substituted_element = kryon_substitute_template_vars(
                        element_with_index,
                        const_for->data.const_for_loop.var_name,
                        value_node,
                        codegen
                    );

                    if (substituted_element) {
                        if (!kryon_write_element_instance(codegen, substituted_element, ast_root)) {
                            kryon_free(value_node);
                            if (index_node) kryon_free(index_node);
                            return false;
                        }
                    }
                }
            }

            kryon_free(value_node);
            if (index_node) kryon_free(index_node);
        }
        
    } else {
        // Handle array-based loop (existing logic)
        if (const_for->data.const_for_loop.index_var_name) {
            print("DEBUG: Expanding array const_for loop: %s, %s in %s\n",
                   const_for->data.const_for_loop.index_var_name,
                   const_for->data.const_for_loop.var_name,
                   const_for->data.const_for_loop.array_name);
        } else {
            print("DEBUG: Expanding array const_for loop: %s in %s\n",
                   const_for->data.const_for_loop.var_name,
                   const_for->data.const_for_loop.array_name);
        }

        // Find the array constant
        const char *array_name = const_for->data.const_for_loop.array_name;
        const KryonASTNode *array_const = find_constant_value(codegen, array_name);

        if (!array_const || array_const->type != KRYON_AST_ARRAY_LITERAL) {
            print("DEBUG: ERROR: Array constant '%s' not found or not an array\n", array_name);
            return false;
        }

        print("DEBUG: Found array constant '%s' with %zu elements\n",
               array_name, array_const->data.array_literal.element_count);

        // Expand loop body for each array element
        for (size_t i = 0; i < array_const->data.array_literal.element_count; i++) {
            const KryonASTNode *array_element = array_const->data.array_literal.elements[i];

            print("DEBUG: Processing array element %zu\n", i);

            // Create index node if index variable is present
            KryonASTNode *index_node = NULL;
            if (const_for->data.const_for_loop.index_var_name) {
                index_node = kryon_calloc(1, sizeof(KryonASTNode));
                if (!index_node) {
                    print("DEBUG: ERROR: Failed to allocate index node\n");
                    return false;
                }

                index_node->type = KRYON_AST_LITERAL;
                index_node->data.literal.value.type = KRYON_VALUE_INTEGER;
                index_node->data.literal.value.data.int_value = (int)i;
            }

            // Expand each element in the loop body
            for (size_t j = 0; j < const_for->data.const_for_loop.body_count; j++) {
                const KryonASTNode *body_element = const_for->data.const_for_loop.body[j];

                if (body_element && body_element->type == KRYON_AST_ELEMENT) {
                    print("🔄 DEBUG: About to substitute template vars for element, var_name='%s', iteration=%zu\n", const_for->data.const_for_loop.var_name, i);
                    print("🔄 DEBUG: Array element type: %d, object property count: %zu\n",
                            array_element ? array_element->type : -1,
                            array_element && array_element->type == KRYON_AST_OBJECT_LITERAL ? array_element->data.object_literal.property_count : 0);

                    // First substitute index variable if present
                    KryonASTNode *element_with_index = (KryonASTNode*)body_element;
                    if (index_node) {
                        element_with_index = kryon_substitute_template_vars(
                            body_element,
                            const_for->data.const_for_loop.index_var_name,
                            index_node,
                            codegen
                        );
                        if (!element_with_index) {
                            element_with_index = (KryonASTNode*)body_element;
                        }
                    }

                    // Then substitute value variable
                    KryonASTNode *substituted_element = kryon_substitute_template_vars(
                        element_with_index,
                        const_for->data.const_for_loop.var_name,
                        array_element,
                        codegen
                    );

                    if (substituted_element) {
                        print("✅ DEBUG: Template substitution completed for iteration %zu\n", i);
                        if (!kryon_write_element_instance(codegen, substituted_element, ast_root)) {
                            print("❌ DEBUG: Failed to write substituted element\n");
                            if (index_node) kryon_free(index_node);
                            return false;
                        }
                        print("✅ DEBUG: Successfully wrote substituted element\n");
                    } else {
                        print("❌ DEBUG: Template substitution returned NULL\n");
                    }
                }
            }

            if (index_node) kryon_free(index_node);
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
    
    print("🔧 SUBSTITUTE DEBUG: Processing node type %d for var '%s'\n", node->type, var_name);

    print("🔍 DEBUG: Substituting templates in node type %d, looking for var '%s'\n", node->type, var_name);
    
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
            // This handles expressions like "alignment.value".
            // First, we must determine if the base object is our loop variable.

            if (node->data.member_access.object &&
                node->data.member_access.object->type == KRYON_AST_IDENTIFIER &&
                node->data.member_access.object->data.identifier.name &&
                strcmp(node->data.member_access.object->data.identifier.name, var_name) == 0 &&
                var_value && var_value->type == KRYON_AST_OBJECT_LITERAL)
            {
                // It is! We can resolve this at compile-time.
                const char *property_name = node->data.member_access.member;
                
                // Find the property in the object literal (e.g., find "value" in the alignment object)
                for (size_t i = 0; i < var_value->data.object_literal.property_count; i++) {
                    const KryonASTNode *prop = var_value->data.object_literal.properties[i];
                    if (prop && strcmp(prop->data.property.name, property_name) == 0) {
                        // Found it! The value of this property is our result.
                        // It could be a literal or another structure (like an array).
                        kryon_free(cloned); // Discard the placeholder clone.
                        
                        // Recursively call substitute on the found value. This is the key.
                        // If the value is a literal, it will be cloned.
                        // If it's an array, it will be cloned.
                        return kryon_substitute_template_vars(prop->data.property.value, var_name, var_value, codegen);
                    }
                }
                
                // If the property wasn't found, it's a compile error.
                // Fall through to clone the unresolved expression, which will likely fail later.
            }
            
            // If we are here, it's not our loop variable, so it's a runtime expression.
            // Clone the structure as-is for the runtime to handle.
            cloned->data.member_access.object = kryon_substitute_template_vars(node->data.member_access.object, var_name, var_value, codegen);
            if (node->data.member_access.member) {
            cloned->data.member_access.member = strdup(node->data.member_access.member);
            }
            break;
        }


        case KRYON_AST_BINARY_OP: {
            // Handle binary operations like "i % 6" during @const_for expansion
            print("🔧 BINARY_OP DEBUG: Processing binary operation for var '%s'\n", var_name);
            
            // Recursively substitute both operands
            KryonASTNode *left = kryon_substitute_template_vars(node->data.binary_op.left, var_name, var_value, codegen);
            KryonASTNode *right = kryon_substitute_template_vars(node->data.binary_op.right, var_name, var_value, codegen);
            
            print("🔧 BINARY_OP DEBUG: Left operand type %d, Right operand type %d\n", 
                   left ? left->type : -1, right ? right->type : -1);
            
            // If both operands are literals, we can evaluate at compile time
            if (left && left->type == KRYON_AST_LITERAL && left->data.literal.value.type == KRYON_VALUE_INTEGER &&
                right && right->type == KRYON_AST_LITERAL && right->data.literal.value.type == KRYON_VALUE_INTEGER) {
                
                int64_t left_val = left->data.literal.value.data.int_value;
                int64_t right_val = right->data.literal.value.data.int_value;
                int64_t result = 0;
                
                print("🔧 BINARY_OP DEBUG: Evaluating %lld %s %lld\n", 
                       (long long)left_val, 
                       node->data.binary_op.operator == KRYON_TOKEN_MODULO ? "%" : "?",
                       (long long)right_val);
                
                // Evaluate based on operator type
                switch (node->data.binary_op.operator) {
                    case KRYON_TOKEN_MODULO:
                        if (right_val != 0) {
                            result = left_val % right_val;
                        } else {
                            print("❌ BINARY_OP DEBUG: Division by zero in modulo operation\n");
                            result = 0;
                        }
                        break;
                    case KRYON_TOKEN_PLUS:
                        result = left_val + right_val;
                        break;
                    case KRYON_TOKEN_MINUS:
                        result = left_val - right_val;
                        break;
                    case KRYON_TOKEN_MULTIPLY:
                        result = left_val * right_val;
                        break;
                    case KRYON_TOKEN_DIVIDE:
                        if (right_val != 0) {
                            result = left_val / right_val;
                        } else {
                            print("❌ BINARY_OP DEBUG: Division by zero\n");
                            result = 0;
                        }
                        break;
                    default:
                        print("❌ BINARY_OP DEBUG: Unsupported operator %d\n", node->data.binary_op.operator);
                        // Fall back to preserving the structure
                        cloned->data.binary_op.left = left;
                        cloned->data.binary_op.right = right;
                        cloned->data.binary_op.operator = node->data.binary_op.operator;
                        break;
                }
                
                print("✅ BINARY_OP DEBUG: Result = %lld\n", (long long)result);
                
                // Create a literal node with the result
                kryon_free(cloned);
                KryonASTNode *result_node = create_minimal_ast_node(KRYON_AST_LITERAL);
                if (result_node) {
                    result_node->data.literal.value.type = KRYON_VALUE_INTEGER;
                    result_node->data.literal.value.data.int_value = result;
                }
                
                // Free the intermediate nodes
                if (left) kryon_free(left);
                if (right) kryon_free(right);
                
                return result_node;
            } else {
                print("❌ BINARY_OP DEBUG: Cannot evaluate at compile time - operands are not integer literals\n");
                // Store the substituted operands for runtime evaluation
                cloned->data.binary_op.left = left;
                cloned->data.binary_op.right = right;
                cloned->data.binary_op.operator = node->data.binary_op.operator;
            }
            break;
        }

        case KRYON_AST_ARRAY_ACCESS: {
            // This handles expressions like `colors[i % 6]` in @const_for loops
            
            print("🔍 ARRAY_ACCESS DEBUG: var_name='%s'\n", var_name);
            
            // 1. Resolve the array part - check if it's a constant identifier first
            KryonASTNode *resolved_array_node = NULL;
            if (node->data.array_access.array && node->data.array_access.array->type == KRYON_AST_IDENTIFIER) {
                const char *array_name = node->data.array_access.array->data.identifier.name;
                print("🔍 ARRAY_ACCESS DEBUG: Looking up constant array '%s'\n", array_name);
                
                // Look up the array as a constant
                const KryonASTNode *array_const = find_constant_value(codegen, array_name);
                if (array_const && array_const->type == KRYON_AST_ARRAY_LITERAL) {
                    print("🔍 ARRAY_ACCESS DEBUG: Found constant array '%s' with %zu elements\n", 
                           array_name, array_const->data.array_literal.element_count);
                    resolved_array_node = (KryonASTNode*)array_const; // Use the constant directly
                } else {
                    print("🔍 ARRAY_ACCESS DEBUG: Constant '%s' not found or not an array\n", array_name);
                }
            }
            
            // If not a constant identifier, try recursive substitution
            if (!resolved_array_node) {
                resolved_array_node = kryon_substitute_template_vars(
                    node->data.array_access.array, var_name, var_value, codegen);
                print("🔍 ARRAY_ACCESS DEBUG: Array resolved via substitution to type %d\n", 
                       resolved_array_node ? resolved_array_node->type : -1);
            }
        
            // 2. Recursively resolve the "index" part of the expression.
            KryonASTNode *resolved_index_node = kryon_substitute_template_vars(
                node->data.array_access.index, var_name, var_value, codegen);
            print("🔍 ARRAY_ACCESS DEBUG: Index resolved to type %d\n", resolved_index_node ? resolved_index_node->type : -1);
            if (resolved_index_node && resolved_index_node->type == KRYON_AST_LITERAL) {
                print("🔍 ARRAY_ACCESS DEBUG: Index value = %lld\n", 
                       (long long)resolved_index_node->data.literal.value.data.int_value);
            }
        
            // 3. Check if we can resolve this at COMPILE TIME.
            if (resolved_array_node && resolved_array_node->type == KRYON_AST_ARRAY_LITERAL &&
                resolved_index_node && resolved_index_node->type == KRYON_AST_LITERAL &&
                resolved_index_node->data.literal.value.type == KRYON_VALUE_INTEGER)
            {
                 int64_t index = resolved_index_node->data.literal.value.data.int_value;
                 print("🔍 ARRAY_ACCESS DEBUG: Attempting compile-time resolution: index=%lld, array_size=%zu\n", 
                        (long long)index, resolved_array_node->data.array_literal.element_count);
        
                 if (index >= 0 && (size_t)index < resolved_array_node->data.array_literal.element_count) {
                    const KryonASTNode *target_element = resolved_array_node->data.array_literal.elements[index];
                    print("✅ ARRAY_ACCESS DEBUG: Successfully resolved colors[%lld] to element type %d\n", 
                           (long long)index, target_element ? target_element->type : -1);
                    
                    if (target_element && target_element->type == KRYON_AST_LITERAL) {
                        print("✅ ARRAY_ACCESS DEBUG: Resolved to literal value: '%s'\n", 
                               target_element->data.literal.value.data.string_value);
                    }
                    
                    // Free the cloned node since we're returning a different result
                    kryon_free(cloned);
        
                    // Return a clone of the final, resolved literal value.
                    return clone_ast_literal(target_element);
                 } else {
                    print("❌ ARRAY_ACCESS DEBUG: Index %lld out of bounds (array size: %zu)!\n", 
                           (long long)index, resolved_array_node->data.array_literal.element_count);
                 }
            } else {
                print("❌ ARRAY_ACCESS DEBUG: Cannot resolve at compile time\n");
                print("  - Array type: %d (need %d)\n", resolved_array_node ? resolved_array_node->type : -1, KRYON_AST_ARRAY_LITERAL);
                print("  - Index type: %d (need %d)\n", resolved_index_node ? resolved_index_node->type : -1, KRYON_AST_LITERAL);
                if (resolved_index_node && resolved_index_node->type == KRYON_AST_LITERAL) {
                    print("  - Index value type: %d (need %d)\n", resolved_index_node->data.literal.value.type, KRYON_VALUE_INTEGER);
                }
            }
            
            // 4. If not resolved, this is a RUNTIME expression. Preserve its structure.
            // Only assign if they were created by substitution (not constants)
            if (resolved_array_node != find_constant_value(codegen, node->data.array_access.array->data.identifier.name)) {
                cloned->data.array_access.array = resolved_array_node;
            } else {
                // Clone the original array node since we can't use the constant directly
                cloned->data.array_access.array = kryon_substitute_template_vars(
                    node->data.array_access.array, var_name, var_value, codegen);
            }
            cloned->data.array_access.index = resolved_index_node;
            break;
        }

        case KRYON_AST_TEMPLATE: {
            // This is the logic for text: "mainAxisAlignment: ${alignment.name}"
            
            bool all_resolved_to_literals = true;
            char final_string[1024] = {0}; // Buffer for the final concatenated string
        
            for (size_t i = 0; i < node->data.template.segment_count; i++) {
                const KryonASTNode *segment = node->data.template.segments[i];
                
                // IMPORTANT: Recursively substitute on the segment itself!
                KryonASTNode *resolved_segment = kryon_substitute_template_vars(segment, var_name, var_value, codegen);
        
                if (resolved_segment && resolved_segment->type == KRYON_AST_LITERAL) {
                    // Append the literal string to our buffer
                    const char* literal_text = resolved_segment->data.literal.value.data.string_value;
                    if (literal_text) {
                        strcat(final_string, literal_text);
                    }
                } else {
                    // If any segment does not resolve to a literal, we can't form a final string.
                    all_resolved_to_literals = false;
                    // TODO: Free resolved_segment if it was cloned
                    break;
                }
                // TODO: Free resolved_segment if it was cloned
            }
        
            if (all_resolved_to_literals) {
                // SUCCESS! We can replace the entire template node with a single string literal node.
                kryon_free(cloned); // Discard the placeholder template clone
                
                KryonASTNode* final_literal = create_minimal_ast_node(KRYON_AST_LITERAL);
                final_literal->data.literal.value.type = KRYON_VALUE_STRING;
                final_literal->data.literal.value.data.string_value = strdup(final_string);
                return final_literal;
            }
        
            // If we are here, the template has runtime variables.
            // Fall back to the original cloning logic.
            // (Your original logic for cloning the template segments was here)
            cloned->data.template.segment_count = node->data.template.segment_count;
            if (node->data.template.segment_count > 0) {
                cloned->data.template.segments = kryon_alloc(node->data.template.segment_count * sizeof(KryonASTNode*));
                for (size_t i = 0; i < node->data.template.segment_count; i++) {
                    cloned->data.template.segments[i] = kryon_substitute_template_vars(node->data.template.segments[i], var_name, var_value, codegen);
                }
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
                print("🔍 DEBUG: Substituting property value for '%s', original node type: %d\n",
                       node->data.property.name, node->data.property.value->type);

        
                cloned->data.property.value = kryon_substitute_template_vars(
                    node->data.property.value, var_name, var_value, codegen);
                if (cloned->data.property.value) {
                    print("🔍 DEBUG: After substitution, property '%s' has node type: %d\n",
                           node->data.property.name, cloned->data.property.value->type);
                } else {
                    print("❌ DEBUG: Property value substitution returned NULL for '%s'\n",
                           node->data.property.name);
                }
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
                    snprint(num_buf, sizeof(num_buf), "%d", val);
                    replacement = num_buf;
                } else if (var_value->data.literal.value.type == KRYON_VALUE_FLOAT) {
                    snprint(num_buf, sizeof(num_buf), "%.0f", 
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
                        // Array constant not found or not an array - mark as unresolved template
                    }
                }
            }
        }
        
        // Check if it matches our variable for property access
        char pattern[256];
        snprint(pattern, sizeof(pattern), "%s.", var_name);
        
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
                                    snprint(num_buf, sizeof(num_buf), "%lld", 
                                           (long long)prop_value->data.literal.value.data.int_value);
                                    replacement = num_buf;
                                } else if (prop_value->data.literal.value.type == KRYON_VALUE_FLOAT) {
                                    snprint(num_buf, sizeof(num_buf), "%.0f", 
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
// =============================================================================
// CONDITIONAL DIRECTIVE EXPANSION
// =============================================================================

/**
 * @brief Evaluate a condition expression at compile time
 * @param condition The condition AST node
 * @param codegen Code generator context
 * @return true if condition evaluates to true, false otherwise
 */
static bool evaluate_condition_compile_time(const KryonASTNode *condition, KryonCodeGenerator *codegen) {
    if (!condition) {
        return false;
    }

    // Handle literal boolean values
    if (condition->type == KRYON_AST_LITERAL) {
        if (condition->data.literal.value.type == KRYON_VALUE_BOOLEAN) {
            return condition->data.literal.value.data.bool_value;
        }
        // Non-zero numbers are truthy
        if (condition->data.literal.value.type == KRYON_VALUE_INTEGER) {
            return condition->data.literal.value.data.int_value != 0;
        }
        return true; // Non-empty strings/values are truthy
    }

    // Handle variable references - look up in constant table
    if (condition->type == KRYON_AST_VARIABLE) {
        const KryonASTNode *const_value = find_constant_value(codegen, condition->data.variable.name);
        if (const_value) {
            return evaluate_condition_compile_time(const_value, codegen);
        }
        print("WARNING: Variable '%s' not found in constant table, assuming false\n",
               condition->data.variable.name);
        return false;
    }

    // Handle binary operations
    if (condition->type == KRYON_AST_BINARY_OP) {
        bool left = evaluate_condition_compile_time(condition->data.binary_op.left, codegen);
        bool right = evaluate_condition_compile_time(condition->data.binary_op.right, codegen);

        switch (condition->data.binary_op.operator) {
            case KRYON_TOKEN_LOGICAL_AND:
                return left && right;
            case KRYON_TOKEN_LOGICAL_OR:
                return left || right;
            case KRYON_TOKEN_EQUALS:
                return left == right;
            case KRYON_TOKEN_NOT_EQUALS:
                return left != right;
            default:
                print("WARNING: Unsupported operator in compile-time condition\n");
                return false;
        }
    }

    // Handle unary operations
    if (condition->type == KRYON_AST_UNARY_OP) {
        bool operand = evaluate_condition_compile_time(condition->data.unary_op.operand, codegen);
        if (condition->data.unary_op.operator == KRYON_TOKEN_LOGICAL_NOT) {
            return !operand;
        }
    }

    print("WARNING: Unsupported condition type for compile-time evaluation\n");
    return false;
}

/**
 * @brief Expand @const_if directive by evaluating condition at compile time
 * @param codegen Code generator context
 * @param const_if The const_if AST node
 * @param ast_root Root AST node
 * @return true on success, false on error
 */
bool kryon_expand_const_if(KryonCodeGenerator *codegen, const KryonASTNode *const_if, const KryonASTNode *ast_root) {
    if (!const_if || (const_if->type != KRYON_AST_CONST_IF_DIRECTIVE && const_if->type != KRYON_AST_IF_DIRECTIVE)) {
        return false;
    }

    print("DEBUG: Expanding @const_if directive\n");

    // Evaluate the main condition
    bool condition_result = evaluate_condition_compile_time(const_if->data.conditional.condition, codegen);
    print("DEBUG: Main condition evaluated to %s\n", condition_result ? "true" : "false");

    // If main condition is true, emit then body
    if (condition_result) {
        print("DEBUG: Emitting then body (%zu elements)\n", const_if->data.conditional.then_count);
        for (size_t i = 0; i < const_if->data.conditional.then_count; i++) {
            if (!kryon_write_element_instance(codegen, const_if->data.conditional.then_body[i], ast_root)) {
                return false;
            }
        }
        return true;
    }

    // Check elif conditions
    for (size_t i = 0; i < const_if->data.conditional.elif_count; i++) {
        bool elif_result = evaluate_condition_compile_time(const_if->data.conditional.elif_conditions[i], codegen);
        print("DEBUG: Elif %zu condition evaluated to %s\n", i, elif_result ? "true" : "false");

        if (elif_result) {
            print("DEBUG: Emitting elif %zu body (%zu elements)\n", i, const_if->data.conditional.elif_counts[i]);
            for (size_t j = 0; j < const_if->data.conditional.elif_counts[i]; j++) {
                if (!kryon_write_element_instance(codegen, const_if->data.conditional.elif_bodies[i][j], ast_root)) {
                    return false;
                }
            }
            return true;
        }
    }

    // If no conditions matched, emit else body
    if (const_if->data.conditional.else_count > 0) {
        print("DEBUG: Emitting else body (%zu elements)\n", const_if->data.conditional.else_count);
        for (size_t i = 0; i < const_if->data.conditional.else_count; i++) {
            if (!kryon_write_element_instance(codegen, const_if->data.conditional.else_body[i], ast_root)) {
                return false;
            }
        }
    }

    return true;
}

// Helper function to serialize condition expression to string
char *serialize_condition_expression(const KryonASTNode *condition) {
    if (!condition) {
        return NULL;
    }

    print("DEBUG: serialize_condition_expression called with node type=%d\n", condition->type);

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
    print("WARNING: Unsupported condition type %d for serialization\n", condition->type);
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

    print("DEBUG: Writing @if directive as runtime conditional element\n");
    print("DEBUG: @if condition pointer: %p\n", (void*)if_directive->data.conditional.condition);
    if (if_directive->data.conditional.condition) {
        print("DEBUG: @if condition node type: %d\n", if_directive->data.conditional.condition->type);
    }

    // Serialize the condition expression to string
    char *condition_str = serialize_condition_expression(if_directive->data.conditional.condition);
    if (!condition_str) {
        print("ERROR: Failed to serialize @if condition expression (condition was %s)\n",
               if_directive->data.conditional.condition ? "non-NULL" : "NULL");
        return false;
    }

    print("DEBUG: @if condition serialized as: '%s'\n", condition_str);

    // Get element hex for "if" syntax directive
    uint16_t if_element_hex = kryon_get_syntax_hex("if");
    print("DEBUG: @if element hex code: 0x%04X\n", if_element_hex);

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
        print("ERROR: Failed to write @if element header\n");
        kryon_free(condition_str);
        return false;
    }

    // Write condition property using schema-based property header
    uint16_t condition_prop_hex = 0x9100; // Custom property for "condition"

    KRBPropertyHeader prop_header = {
        .property_id = condition_prop_hex,
        .value_type = KRYON_RUNTIME_PROP_STRING
    };

    print("DEBUG: Writing condition property with hex 0x%04X, value='%s'\n", condition_prop_hex, condition_str);

    // Write property header using schema writer
    if (!krb_write_property_header(schema_writer, codegen, &prop_header)) {
        print("ERROR: Failed to write condition property header\n");
        kryon_free(condition_str);
        return false;
    }

    // Write string value (length + data)
    uint16_t condition_len = (uint16_t)strlen(condition_str);
    if (!write_uint16(codegen, condition_len) ||
        !write_binary_data(codegen, condition_str, condition_len)) {
        print("ERROR: Failed to write condition property value\n");
        kryon_free(condition_str);
        return false;
    }

    print("DEBUG: Successfully wrote condition property (length=%u)\n", condition_len);
    kryon_free(condition_str);

    // Write then_count property to mark where "then" ends and "else" begins
    // Property 0x9101 = then_count (helps runtime split children correctly)
    uint16_t then_count_prop_id = 0x9101;
    uint8_t then_count_type = KRYON_RUNTIME_PROP_INTEGER;
    uint32_t then_count_value = (uint32_t)if_directive->data.conditional.then_count;

    write_binary_data(codegen, (const char*)&then_count_prop_id, sizeof(uint16_t));
    write_binary_data(codegen, (const char*)&then_count_type, sizeof(uint8_t));
    if (!write_uint32(codegen, then_count_value)) {
        print("ERROR: Failed to write then_count value\n");
        return false;
    }

    print("DEBUG: Wrote then_count property: %u\n", then_count_value);

    // Write then body children recursively
    print("DEBUG: Writing %zu children for @if then body\n", if_directive->data.conditional.then_count);
    for (size_t i = 0; i < if_directive->data.conditional.then_count; i++) {
        if (!kryon_write_element_instance(codegen, if_directive->data.conditional.then_body[i], ast_root)) {
            print("ERROR: Failed to write @if then child %zu\n", i);
            return false;
        }
    }

    // Write else body children (if any)
    if (if_directive->data.conditional.else_count > 0) {
        print("DEBUG: Writing %zu children for @else body\n", if_directive->data.conditional.else_count);
        for (size_t i = 0; i < if_directive->data.conditional.else_count; i++) {
            if (!kryon_write_element_instance(codegen, if_directive->data.conditional.else_body[i], ast_root)) {
                print("ERROR: Failed to write @if else child %zu\n", i);
                return false;
            }
        }
    }

    // TODO: Support elif branches
    if (if_directive->data.conditional.elif_count > 0) {
        print("WARNING: @elif branches not yet supported at runtime\n");
    }

    print("DEBUG: @if directive written successfully\n");
    return true;
}
