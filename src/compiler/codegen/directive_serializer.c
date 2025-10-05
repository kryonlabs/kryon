/**
 * @file directive_serializer.c
 * @brief Directive serialization for KRB format
 * 
 * Handles serialization of directives like @variables, @function, @component, @event
 * to the KRB binary format. Manages both compile-time and runtime directives.
 */

#include "codegen.h"
#include "memory.h"
#include "krb_format.h"
#include "binary_io.h"
#include "../../shared/krb_schema.h"
#include "krb_sections.h"
#include "string_table.h"
#include "element_serializer.h"
#include <string.h>
#include <stdio.h>

// Forward declarations
bool kryon_write_single_variable(KryonCodeGenerator *codegen, const KryonASTNode *variable);

// Helper to evaluate constant unary operations (e.g., -1, !true)
static bool try_evaluate_constant_unary(const KryonASTNode *node, KryonASTValue *out_value) {
    if (!node || node->type != KRYON_AST_UNARY_OP) {
        return false;
    }

    // Check if operand is a literal
    if (!node->data.unary_op.operand || node->data.unary_op.operand->type != KRYON_AST_LITERAL) {
        return false;
    }

    const KryonASTValue *operand_value = &node->data.unary_op.operand->data.literal.value;

    // Handle negation operator on integers/floats
    if (node->data.unary_op.operator == KRYON_TOKEN_MINUS) {
        if (operand_value->type == KRYON_VALUE_INTEGER) {
            out_value->type = KRYON_VALUE_INTEGER;
            out_value->data.int_value = -operand_value->data.int_value;
            return true;
        } else if (operand_value->type == KRYON_VALUE_FLOAT) {
            out_value->type = KRYON_VALUE_FLOAT;
            out_value->data.float_value = -operand_value->data.float_value;
            return true;
        }
    }

    // Handle logical NOT on booleans
    if (node->data.unary_op.operator == KRYON_TOKEN_LOGICAL_NOT) {
        if (operand_value->type == KRYON_VALUE_BOOLEAN) {
            out_value->type = KRYON_VALUE_BOOLEAN;
            out_value->data.bool_value = !operand_value->data.bool_value;
            return true;
        }
    }

    return false;
}

bool kryon_write_variable_node(KryonCodeGenerator *codegen, const KryonASTNode *variable) {
    if (!variable || (variable->type != KRYON_AST_VARIABLE_DEFINITION && variable->type != KRYON_AST_STATE_DEFINITION)) {
        return false;
    }
    
    // Check if this is a variables block (contains children) or single variable
    if (variable->data.variable_def.name && 
        strcmp(variable->data.variable_def.name, "__variables_block__") == 0) {
        // Handle @variables { ... } block - write all child variables
        for (size_t i = 0; i < variable->data.element.child_count; i++) {
            const KryonASTNode *child_var = variable->data.element.children[i];
            if (child_var && child_var->type == KRYON_AST_VARIABLE_DEFINITION) {
                if (!kryon_write_single_variable(codegen, child_var)) {
                    return false;
                }
            }
        }
        return true;
    } else {
        // Handle single @var
        return kryon_write_single_variable(codegen, variable);
    }
}

bool kryon_write_single_variable(KryonCodeGenerator *codegen, const KryonASTNode *variable) {
    if (!variable || (variable->type != KRYON_AST_VARIABLE_DEFINITION && variable->type != KRYON_AST_STATE_DEFINITION)) {
        return false;
    }
    
    // Variable name (as string reference)
    uint32_t name_ref = add_string_to_table(codegen, variable->data.variable_def.name);
    printf("DEBUG: Variable name '%s' string ref: %u\n", variable->data.variable_def.name, name_ref);
    if (!write_uint32(codegen, name_ref)) {
        return false;
    }
    
    // Determine variable type based on value and whether it's reactive
    KryonVariableType var_type = KRYON_VAR_STATIC_STRING; // Default
    bool is_reactive = (variable->type == KRYON_AST_STATE_DEFINITION); // @state is reactive

    // Try to evaluate constant expressions to determine actual type
    KryonASTValue evaluated_value;
    const KryonASTValue *value_to_use = NULL;

    if (variable->data.variable_def.value) {
        if (variable->data.variable_def.value->type == KRYON_AST_LITERAL) {
            value_to_use = &variable->data.variable_def.value->data.literal.value;
        } else if (try_evaluate_constant_unary(variable->data.variable_def.value, &evaluated_value)) {
            // Successfully evaluated UNARY_OP to a constant
            value_to_use = &evaluated_value;
            printf("DEBUG: Evaluated UNARY_OP for '%s' to type=%d\n",
                   variable->data.variable_def.name, evaluated_value.type);
            if (evaluated_value.type == KRYON_VALUE_INTEGER) {
                printf("DEBUG: Integer value = %lld\n", (long long)evaluated_value.data.int_value);
            }
        }

        if (value_to_use) {
            switch (value_to_use->type) {
                case KRYON_VALUE_INTEGER:
                    var_type = is_reactive ? KRYON_VAR_REACTIVE_INTEGER : KRYON_VAR_STATIC_INTEGER;
                    break;
                case KRYON_VALUE_FLOAT:
                    var_type = is_reactive ? KRYON_VAR_REACTIVE_FLOAT : KRYON_VAR_STATIC_FLOAT;
                    break;
                case KRYON_VALUE_BOOLEAN:
                    var_type = is_reactive ? KRYON_VAR_REACTIVE_BOOLEAN : KRYON_VAR_STATIC_BOOLEAN;
                    break;
                case KRYON_VALUE_STRING:
                default:
                    var_type = is_reactive ? KRYON_VAR_REACTIVE_STRING : KRYON_VAR_STATIC_STRING;
                    break;
            }
        } else {
            // Complex expressions that can't be evaluated at compile time
            var_type = is_reactive ? KRYON_VAR_REACTIVE_STRING : KRYON_VAR_STATIC_STRING;
        }
    }
    
    printf("DEBUG: Writing variable '%s': type=%u, reactive=%d\n",
           variable->data.variable_def.name, (uint8_t)var_type, is_reactive);
    if (!write_uint8(codegen, (uint8_t)var_type)) {
        return false;
    }

    // Variable flags based on directive type
    uint8_t flags = KRYON_VAR_FLAG_NONE;
    if (is_reactive) {
        flags |= KRYON_VAR_FLAG_REACTIVE;
    }
    // TODO: Add more flags based on variable modifiers (@private, @readonly, etc.)

    printf("DEBUG: Writing flags=%u for variable '%s'\n", flags, variable->data.variable_def.name);
    if (!write_uint8(codegen, flags)) {
        return false;
    }
    
    // Variable initial value
    if (variable->data.variable_def.value) {
        printf("DEBUG: Variable '%s' has value node type=%d\n",
               variable->data.variable_def.name ? variable->data.variable_def.name : "unknown",
               variable->data.variable_def.value->type);

        // Use the evaluated value if we have one (from constant UNARY_OP evaluation)
        if (value_to_use) {
            // We have a compile-time constant value (either literal or evaluated UNARY_OP)
            if (value_to_use->type == KRYON_VALUE_STRING) {
                // Write as string reference for consistency with non-literal values
                if (!write_uint8(codegen, KRYON_VALUE_STRING)) {
                    return false;
                }
                const char* str_value = value_to_use->data.string_value ? value_to_use->data.string_value : "";
                uint32_t str_ref = add_string_to_table(codegen, str_value);
                printf("DEBUG: Variable literal string value string ref: %u for value '%s'\n", str_ref, str_value);
                if (!write_uint32(codegen, str_ref)) {
                    return false;
                }
            } else {
                // For non-string literals, use the original write_value_node but add type tag
                if (!write_uint8(codegen, (uint8_t)value_to_use->type)) {
                    return false;
                }
                if (!write_value_node(codegen, value_to_use)) {
                    return false;
                }
            }
        } else if (variable->data.variable_def.value->type == KRYON_AST_LITERAL) {
            // Fallback to original literal handling (shouldn't normally reach here since value_to_use should be set)
            const KryonASTValue* literal_value = &variable->data.variable_def.value->data.literal.value;
            if (literal_value->type == KRYON_VALUE_STRING) {
                if (!write_uint8(codegen, KRYON_VALUE_STRING)) {
                    return false;
                }
                const char* str_value = literal_value->data.string_value ? literal_value->data.string_value : "";
                uint32_t str_ref = add_string_to_table(codegen, str_value);
                printf("DEBUG: Variable literal string value string ref: %u for value '%s'\n", str_ref, str_value);
                if (!write_uint32(codegen, str_ref)) {
                    return false;
                }
            } else {
                if (!write_uint8(codegen, (uint8_t)literal_value->type)) {
                    return false;
                }
                if (!write_value_node(codegen, literal_value)) {
                    return false;
                }
            }
        } else {
            // For non-literal values (like arrays), convert to string representation
            printf("DEBUG: Non-literal variable value, writing as string\n");
            
            // Try to get the raw value as string from the AST
            const char* raw_value = NULL;
            if (variable->data.variable_def.value->type == KRYON_AST_ARRAY_LITERAL) {
                // Convert array AST to JSON string representation
                const KryonASTNode* array_node = variable->data.variable_def.value;
                
                // Build JSON string from actual AST nodes
                size_t buffer_size = 1024;
                char* json_buffer = malloc(buffer_size);
                size_t json_len = 0;
                
                json_buffer[json_len++] = '[';
                
                for (size_t i = 0; i < array_node->data.array_literal.element_count; i++) {
                    if (i > 0) {
                        json_buffer[json_len++] = ',';
                        json_buffer[json_len++] = ' ';
                    }
                    
                    const KryonASTNode* element = array_node->data.array_literal.elements[i];
                    if (element && element->type == KRYON_AST_LITERAL) {
                        if (element->data.literal.value.type == KRYON_VALUE_STRING) {
                            // Add quoted string
                            json_buffer[json_len++] = '"';
                            const char* str = element->data.literal.value.data.string_value;
                            size_t str_len = strlen(str);
                            memcpy(json_buffer + json_len, str, str_len);
                            json_len += str_len;
                            json_buffer[json_len++] = '"';
                        } else if (element->data.literal.value.type == KRYON_VALUE_INTEGER) {
                            // Add integer
                            json_len += snprintf(json_buffer + json_len, buffer_size - json_len, 
                                               "%lld", element->data.literal.value.data.int_value);
                        } else if (element->data.literal.value.type == KRYON_VALUE_FLOAT) {
                            // Add float
                            json_len += snprintf(json_buffer + json_len, buffer_size - json_len, 
                                               "%f", element->data.literal.value.data.float_value);
                        }
                    }
                }
                
                json_buffer[json_len++] = ']';
                json_buffer[json_len] = '\0';
                
                raw_value = json_buffer;
                printf("DEBUG: Converted array AST to JSON: %s\n", raw_value);
            }
            
            if (raw_value) {
                // Write as string value
                if (!write_uint8(codegen, KRYON_VALUE_STRING)) {
                    return false;
                }
                uint32_t str_ref = add_string_to_table(codegen, raw_value);
                printf("DEBUG: Variable value string ref: %u for value '%s'\n", str_ref, raw_value);
                
                // Free the allocated JSON buffer if it was dynamically created
                if (variable->data.variable_def.value->type == KRYON_AST_ARRAY_LITERAL) {
                    free((char*)raw_value);
                }
                
                if (!write_uint32(codegen, str_ref)) {
                    return false;
                }
            } else {
                // Write null value
                if (!write_uint8(codegen, KRYON_VALUE_NULL)) {
                    return false;
                }
            }
        }
    } else {
        // Write null value
        if (!write_uint8(codegen, KRYON_VALUE_NULL)) {
            return false;
        }
    }
    
    return true;
}

bool kryon_write_function_node(KryonCodeGenerator *codegen, const KryonASTNode *function) {
    if (!function || function->type != KRYON_AST_FUNCTION_DEFINITION) {
        return false;
    }
    
    // Prepare function header using schema
    KRBFunctionHeader header = {0};
    header.magic = KRB_MAGIC_FUNC;
    header.lang_ref = add_string_to_table(codegen, function->data.function_def.language);
    header.name_ref = add_string_to_table(codegen, function->data.function_def.name);
    header.param_count = (uint16_t)function->data.function_def.parameter_count;
    header.flags = KRB_FUNC_FLAG_NONE;
    
    // Script engines are disabled; store function source as-is
    uint32_t code_ref = add_string_to_table(codegen, function->data.function_def.code);

    header.code_ref = code_ref;
    
    // Write using schema-defined format
    if (!write_uint32(codegen, header.magic)) return false;
    if (!write_uint32(codegen, header.lang_ref)) return false;
    if (!write_uint32(codegen, header.name_ref)) return false;
    if (!write_uint16(codegen, header.param_count)) return false;
    
    // Write parameter names
    for (size_t i = 0; i < function->data.function_def.parameter_count; i++) {
        uint32_t param_ref = add_string_to_table(codegen, function->data.function_def.parameters[i]);
        if (!write_uint32(codegen, param_ref)) {
            return false;
        }
    }
    
    // Write flags and code reference (schema format)
    if (!write_uint16(codegen, header.flags)) return false;
    if (!write_uint32(codegen, header.code_ref)) return false;
    
    return true;
}

bool kryon_write_component_node(KryonCodeGenerator *codegen, const KryonASTNode *component) {
    if (!component || component->type != KRYON_AST_COMPONENT) {
        return false;
    }
    
    printf("📝 Writing component definition: '%s'\n", component->data.component.name ? component->data.component.name : "unnamed");
    
    // Write component header
    if (!write_uint32(codegen, 0x434F4D50)) { // "COMP"
        return false;
    }
    
    // Component name (as string reference)
    uint32_t name_ref = add_string_to_table(codegen, component->data.component.name);
    if (!write_uint32(codegen, name_ref)) {
        return false;
    }
    
    // Parameter count
    uint16_t param_count = (uint16_t)component->data.component.parameter_count;
    if (!write_uint16(codegen, param_count)) {
        return false;
    }
    
    // Write parameters
    for (size_t i = 0; i < component->data.component.parameter_count; i++) {
        // Parameter name
        uint32_t param_name_ref = add_string_to_table(codegen, component->data.component.parameters[i]);
        if (!write_uint32(codegen, param_name_ref)) {
            return false;
        }
        
        // Parameter default value (if any)
        if (component->data.component.param_defaults && component->data.component.param_defaults[i]) {
            if (!write_uint8(codegen, 1)) { // Has default
                return false;
            }
            uint32_t default_ref = add_string_to_table(codegen, component->data.component.param_defaults[i]);
            if (!write_uint32(codegen, default_ref)) {
                return false;
            }
        } else {
            if (!write_uint8(codegen, 0)) { // No default
                return false;
            }
        }
    }
    
    // State variable count
    uint16_t state_count = (uint16_t)component->data.component.state_count;
    printf("📝 Writing state_count=%u for component '%s'\n",
           state_count, component->data.component.name);
    if (!write_uint16(codegen, state_count)) {
        return false;
    }

    // Write state variables
    for (size_t i = 0; i < component->data.component.state_count; i++) {
        printf("📝 Writing state variable %zu\n", i);
        if (!kryon_write_single_variable(codegen, component->data.component.state_vars[i])) {
            return false;
        }
    }
    
    // Function count
    uint16_t function_count = (uint16_t)component->data.component.function_count;
    if (!write_uint16(codegen, function_count)) {
        return false;
    }
    
    // Write functions metadata (not full FUNC sections - those go in script section)
    for (size_t i = 0; i < component->data.component.function_count; i++) {
        const KryonASTNode *func = component->data.component.functions[i];
        if (!func || func->type != KRYON_AST_FUNCTION_DEFINITION) {
            return false;
        }

        // Write function name reference
        uint32_t func_name_ref = add_string_to_table(codegen, func->data.function_def.name);
        if (!write_uint32(codegen, func_name_ref)) {
            return false;
        }

        // Write function language reference
        uint32_t func_lang_ref = add_string_to_table(codegen, func->data.function_def.language);
        if (!write_uint32(codegen, func_lang_ref)) {
            return false;
        }

        // Write has_default flag (always 0 for now - functions written in script section)
        if (!write_uint8(codegen, 0)) {
            return false;
        }
    }
    
    // Component body (UI definition) - write body elements count and elements
    if (component->data.component.body_count > 0) {
        if (!write_uint8(codegen, 1)) { // Has body
            return false;
        }
        printf("📝 Writing %zu component body elements\n", component->data.component.body_count);
        // Write count of body elements
        if (!write_uint16(codegen, (uint16_t)component->data.component.body_count)) {
            return false;
        }
        // Write each body element
        for (size_t i = 0; i < component->data.component.body_count; i++) {
            if (!kryon_write_element_node(codegen, component->data.component.body_elements[i], NULL)) {
                printf("❌ Failed to write component body element %zu\n", i);
                return false;
            }
        }
        printf("✅ Successfully wrote %zu component body elements\n", component->data.component.body_count);
    } else {
        if (!write_uint8(codegen, 0)) { // No body
            return false;
        }
    }
    
    return true;
}

bool kryon_write_style_node(KryonCodeGenerator *codegen, const KryonASTNode *style) {
    if (!style || style->type != KRYON_AST_STYLE_BLOCK) {
        return false;
    }
    
    // Write style name as string reference
    uint32_t name_ref = add_string_to_table(codegen, style->data.style.name);
    if (!write_uint32(codegen, name_ref)) {
        return false;
    }
    
    // Write parent style name (for inheritance)
    if (style->data.style.parent_name) {
        uint32_t parent_ref = add_string_to_table(codegen, style->data.style.parent_name);
        if (!write_uint32(codegen, parent_ref)) {
            return false;
        }
    } else {
        if (!write_uint32(codegen, 0)) { // No parent
            return false;
        }
    }
    
    // Write property count
    uint16_t prop_count = (uint16_t)style->data.style.property_count;
    if (!write_uint16(codegen, prop_count)) {
        return false;
    }
    
    // Write properties
    for (size_t i = 0; i < style->data.style.property_count; i++) {
        if (!write_property_node(codegen, style->data.style.properties[i])) {
            return false;
        }
    }
    
    return true;
}

bool kryon_write_theme_node(KryonCodeGenerator *codegen, const KryonASTNode *theme) {
    if (!theme || theme->type != KRYON_AST_THEME_DEFINITION) {
        return false;
    }
    
    // Write theme group name
    uint32_t group_ref = add_string_to_table(codegen, theme->data.theme.group_name);
    if (!write_uint32(codegen, group_ref)) {
        return false;
    }
    
    // Write variable count
    uint16_t var_count = (uint16_t)theme->data.theme.variable_count;
    if (!write_uint16(codegen, var_count)) {
        return false;
    }
    
    // Write theme variables
    for (size_t i = 0; i < theme->data.theme.variable_count; i++) {
        if (!kryon_write_single_variable(codegen, theme->data.theme.variables[i])) {
            return false;
        }
    }
    
    return true;
}

bool kryon_write_metadata_node(KryonCodeGenerator *codegen, const KryonASTNode *metadata) {
    if (!metadata || metadata->type != KRYON_AST_METADATA_DIRECTIVE) {
        return false;
    }
    
    // Metadata is merged into App element properties during compilation
    // NO separate metadata section is written to KRB file
    // This function exists for API compatibility but does nothing
    
    return true;  // Success - metadata handled during App element creation
}

bool kryon_write_const_definition(KryonCodeGenerator *codegen, const KryonASTNode *const_def) {
    if (!const_def || const_def->type != KRYON_AST_CONST_DEFINITION) {
        return false;
    }
    
    // Constants are processed at compile time and not written to KRB
    // They are expanded inline during code generation
    // Add to constant table for use during compilation
    return add_constant_to_table(codegen, const_def->data.const_def.name, const_def->data.const_def.value);
}

bool kryon_write_onload_directive(KryonCodeGenerator *codegen, const KryonASTNode *onload) {
    if (!onload || onload->type != KRYON_AST_ONLOAD_DIRECTIVE) {
        return false;
    }
    
    printf("📝 Writing onload directive\n");
    
    // Extract language and code from the script structure
    const char *language = onload->data.script.language ? onload->data.script.language : "";
    const char *code = onload->data.script.code;
    
    if (!code) {
        printf("⚠️  Onload directive has no code content\n");
        return false;
    }
    
    printf("📝 Writing onload directive: lang='%s', code_length=%zu\n", 
           language, strlen(code));
    
    // Write onload function header
    KRBFunctionHeader header = {0};
    header.magic = KRB_MAGIC_FUNC;
    header.lang_ref = add_string_to_table(codegen, language);
    header.name_ref = add_string_to_table(codegen, "__onload__");  // Special name for onload function
    header.param_count = 0;  // onload functions take no parameters
    header.flags = KRB_FUNC_FLAG_ONLOAD;  // Mark as onload function
    
    // Script engines are disabled; store onload source as-is
    uint32_t code_ref = add_string_to_table(codegen, code);
    
    header.code_ref = code_ref;
    
    // Write using schema-defined format
    if (!write_uint32(codegen, header.magic)) return false;
    if (!write_uint32(codegen, header.lang_ref)) return false;
    if (!write_uint32(codegen, header.name_ref)) return false;
    if (!write_uint16(codegen, header.param_count)) return false;
    
    // No parameters for onload
    
    // Write flags and code reference
    if (!write_uint16(codegen, header.flags)) return false;
    if (!write_uint32(codegen, header.code_ref)) return false;
    
    return true;
}
