/**

 * @file element_serializer.c
 * @brief Element serialization for KRB format
 * 
 * Handles serialization of UI elements and their properties to the KRB binary format.
 * Includes element instance writing, property serialization, and type management.
 */
#include "lib9.h"


#include "codegen.h"
#include "memory.h"
#include "krb_format.h"
#include "binary_io.h"
#include "color_utils.h"
#include "../../shared/kryon_mappings.h"
#include "../../shared/krb_schema.h"
#include "types.h"
#include <math.h>
#include "string_table.h"
#include "ast_expression_serializer.h"
#include "ast_expander.h"
#include <stdint.h>
#include <math.h>

// Forward declarations for internal helpers
static bool write_property_value(KryonCodeGenerator *codegen, const KryonASTValue *value, uint16_t property_hex);
static bool write_variable_reference(KryonCodeGenerator *codegen, const char *variable_name, uint16_t property_hex);
static KryonValueTypeHint get_property_type_hint(uint16_t property_hex);
static bool write_array_literal_property(KryonCodeGenerator *codegen, const KryonASTNode *array_node);
static bool write_template_property(KryonCodeGenerator *codegen, const KryonASTNode *template_node, uint16_t property_hex);
static bool serialize_for_template(KryonCodeGenerator* codegen, const KryonASTNode* element);
static uint8_t determine_property_value_type(const KryonASTNode *value_node, KryonValueTypeHint type_hint);
static uint8_t determine_literal_value_type(const KryonASTValue *literal_value, KryonValueTypeHint type_hint);
static uint16_t get_or_create_custom_property_hex(const char *property_name);
static bool write_enhanced_property_value(KryonCodeGenerator *codegen, const KryonASTNode *value_node, uint16_t property_hex, KryonValueTypeHint type_hint);
static bool write_empty_property_value(KryonCodeGenerator *codegen, uint16_t property_hex, KryonValueTypeHint type_hint);
bool kryon_write_element_node(KryonCodeGenerator *codegen, const KryonASTNode *element, const KryonASTNode *ast_root);

// Schema writer wrapper
static bool schema_writer(void *context, const void *data, size_t size) {
    KryonCodeGenerator *codegen = (KryonCodeGenerator *)context;
    return write_binary_data(codegen, data, size);
}

bool kryon_write_element_instance(KryonCodeGenerator *codegen, const KryonASTNode *element, const KryonASTNode *ast_root) {
    if (!element) {
        return false;
    }

    // Handle @if as runtime conditional (emit all branches with condition metadata)
    if (element->type == KRYON_AST_IF_DIRECTIVE) {
        return kryon_write_if_directive(codegen, element, ast_root);
    }

    if (element->type != KRYON_AST_ELEMENT && element->type != KRYON_AST_FOR_DIRECTIVE && element->type != KRYON_AST_IF_DIRECTIVE) {
        return false;
    }

    // Check if this is a custom component instance that needs expansion
    uint16_t element_hex;
    const char* element_type_name;

    if (element->type == KRYON_AST_FOR_DIRECTIVE) {
        element_hex = kryon_get_syntax_hex("for");
        element_type_name = "@for";
    } else if (element->type == KRYON_AST_IF_DIRECTIVE) {
        element_hex = kryon_get_syntax_hex("if");
        element_type_name = "@if";
    } else {
        element_hex = kryon_codegen_get_element_hex(element->data.element.element_type);
        element_type_name = element->data.element.element_type;
    }
    
    fprintf(stderr, "DEBUG: write_element_instance - Element '%s' has hex 0x%04X\n", 
           element_type_name, element_hex);
           
    if (kryon_element_in_range(element_hex, KRYON_ELEMENT_RANGE_CUSTOM_START, KRYON_ELEMENT_RANGE_CUSTOM_END)) {
        // This is a custom component - expand it (but not syntax elements)
        fprintf(stderr, "🔧 Expanding custom component in write_element_instance: %s\n", element_type_name);
        
        KryonASTNode *expanded = expand_component_instance(codegen, element, ast_root);
        if (expanded) {
            // Write the expanded component body directly (it's the actual element like Row, not a wrapper)
            // The expanded node IS the component body element (e.g., Row), so write it as a normal element
            return kryon_write_element_instance(codegen, expanded, ast_root);
        } else {
            fprintf(stderr, "❌ Failed to expand component in write_element_instance: %s\n", element_type_name);
            return false;
        }
    }
    
    // Use centralized schema-based element header writing
    uint32_t instance_id = codegen->next_element_id++;
    uint16_t child_count = count_expanded_children(codegen, element);
    
    // Create element header using schema format
    // Get property count based on node type
    uint16_t property_count = 0;
    if (element->type == KRYON_AST_ELEMENT) {
        property_count = (uint16_t)element->data.element.property_count;
    } else if (element->type == KRYON_AST_FOR_DIRECTIVE || element->type == KRYON_AST_IF_DIRECTIVE) {
        property_count = 1; // Directives have 1 property (the condition/array)
    }

    KRBElementHeader header = {
        .instance_id = instance_id,
        .element_type = (uint32_t)element_hex,
        .parent_id = 0,  // Root elements for now
        .style_ref = 0,  // No styles for now
        .property_count = property_count,
        .child_count = child_count,
        .event_count = 0,  // No events for now
        .flags = 0  // No flags for now
    };

    fprintf(stderr, "🏗️  Writing element header: id=%u type=0x%x props=%u children=%u\n",
           instance_id, element_hex, header.property_count, header.child_count);

    // Write header using centralized schema validation
    if (!krb_write_element_header(schema_writer, codegen, &header)) {
        codegen_error(codegen, "Failed to write element header using schema");
        return false;
    }

    // Write properties based on node type
    if (element->type == KRYON_AST_ELEMENT) {
        for (size_t i = 0; i < element->data.element.property_count; i++) {
            if (!write_property_node(codegen, element->data.element.properties[i])) {
                return false;
            }
        }
    } else if (element->type == KRYON_AST_FOR_DIRECTIVE) {
        // Write @for metadata: variable name and array name as a single property
        // Format: "variable|array" (e.g., "habit|habits")
        // Or: "index,variable|array" (e.g., "i,habit|habits") when index is present
        const char *index_var_name = element->data.for_loop.index_var_name;
        const char *var_name = element->data.for_loop.var_name;
        const char *array_name = element->data.for_loop.array_name;

        fprintf(stderr, "🔍 DEBUG: Writing @for metadata: index='%s', var='%s', array='%s'\n",
               index_var_name ? index_var_name : "NULL",
               var_name ? var_name : "NULL",
               array_name ? array_name : "NULL");

        if (var_name && array_name) {
            // Create combined string
            size_t buffer_len = strlen(var_name) + strlen(array_name) + 2; // Base: var|array\0
            if (index_var_name) {
                buffer_len += strlen(index_var_name) + 1; // +1 for comma
            }

            char *combined = kryon_malloc(buffer_len);
            if (index_var_name) {
                snprint(combined, buffer_len, "%s,%s|%s", index_var_name, var_name, array_name);
            } else {
                snprint(combined, buffer_len, "%s|%s", var_name, array_name);
            }

            fprintf(stderr, "🔍 DEBUG: Combined string = '%s'\n", combined);

            // Write property header in native byte order (same as other properties)
            uint16_t prop_id = 0x9000;
            uint8_t prop_type = KRYON_RUNTIME_PROP_STRING;
            write_binary_data(codegen, (const char*)&prop_id, sizeof(uint16_t));
            write_binary_data(codegen, (const char*)&prop_type, sizeof(uint8_t));

            // Write the combined string (length + data)
            uint16_t str_len = (uint16_t)strlen(combined);
            write_binary_data(codegen, (const char*)&str_len, sizeof(uint16_t));
            write_binary_data(codegen, combined, str_len);
            kryon_free(combined);
        } else {
            fprintf(stderr, "❌ DEBUG: var_name or array_name is NULL!\n");
        }
    } else if (element->type == KRYON_AST_IF_DIRECTIVE) {
        // Write @if condition as property (hex 0x9100)
        if (element->data.conditional.condition) {
            // For simple identifier conditions, just write the variable name
            const char *condition_str = NULL;
            if (element->data.conditional.condition->type == KRYON_AST_IDENTIFIER) {
                condition_str = element->data.conditional.condition->data.identifier.name;
            } else if (element->data.conditional.condition->type == KRYON_AST_VARIABLE) {
                condition_str = element->data.conditional.condition->data.variable.name;
            } else {
                // For complex expressions, we'd need to serialize them
                // For now, just use a placeholder
                condition_str = "true";
            }

            // Write property header in native byte order (same as other properties)
            uint16_t prop_id = 0x9100;
            uint8_t prop_type = KRYON_RUNTIME_PROP_STRING;
            write_binary_data(codegen, (const char*)&prop_id, sizeof(uint16_t));
            write_binary_data(codegen, (const char*)&prop_type, sizeof(uint8_t));

            // Write condition string (length + data)
            uint16_t cond_len = (uint16_t)strlen(condition_str);
            write_binary_data(codegen, (const char*)&cond_len, sizeof(uint16_t));
            write_binary_data(codegen, condition_str, cond_len);
        }
    }

    // Write child instances recursively using unified helper
    size_t children_count = 0;
    KryonASTNode **children = kryon_ast_get_children(element, &children_count);

    if (children) {
        for (size_t i = 0; i < children_count; i++) {
            const KryonASTNode *child = children[i];
            if (child && (child->type == KRYON_AST_ELEMENT ||
                         child->type == KRYON_AST_FOR_DIRECTIVE ||
                         child->type == KRYON_AST_IF_DIRECTIVE)) {
                if (!kryon_write_element_instance(codegen, child, ast_root)) {
                    return false;
                }
            }
            // Skip other directive nodes - they are handled in their respective sections
        }
    }
    
    // Event handlers (none for now)
    
    return true;
}

bool kryon_write_element_node(KryonCodeGenerator *codegen, const KryonASTNode *element, const KryonASTNode *ast_root) {
    if (!element || (element->type != KRYON_AST_ELEMENT && element->type != KRYON_AST_EVENT_DIRECTIVE && element->type != KRYON_AST_FOR_DIRECTIVE && element->type != KRYON_AST_IF_DIRECTIVE)) {
        return false;
    }

    // Check if this is a custom component instance that needs expansion
    if (element->type == KRYON_AST_ELEMENT) {
        uint16_t element_hex = kryon_codegen_get_element_hex(element->data.element.element_type);
        fprintf(stderr, "🔍 DEBUG: Element '%s' has hex 0x%04X\n", element->data.element.element_type, element_hex);
        if (element_hex >= 0x2000) {
            // This is a custom component - expand it
            fprintf(stderr, "🔧 Expanding custom component in recursive call: %s\n", element->data.element.element_type);
            KryonASTNode *expanded = expand_component_instance(codegen, element, ast_root);
            if (expanded) {
                bool result = kryon_write_element_node(codegen, expanded, ast_root);
                // TODO: Free expanded node memory
                return result;
            } else {
                fprintf(stderr, "❌ Failed to expand component in recursive call: %s\n", element->data.element.element_type);
                return false;
            }
        }
    }

    // Get element type hex code for standard elements
    // Handle @for directives as templates, not elements
    if (element->type == KRYON_AST_FOR_DIRECTIVE) {
        fprintf(stderr, "🔄 Skipping @for directive serialization for now\n");
        // TODO: Implement proper @for template storage
        return true;
    }

    // Handle @if directives as conditional structures in component templates
    if (element->type == KRYON_AST_IF_DIRECTIVE) {
        fprintf(stderr, "🔄 Writing @if directive in component definition\n");

        // Get @if element hex
        uint16_t if_hex = kryon_get_syntax_hex("if");
        if (!write_uint16(codegen, if_hex)) {
            return false;
        }

        // For component definitions, we need to store the structure differently
        // Write a simplified representation: condition string + then branch + else branch

        // Write condition as serialized expression string
        char *condition_str = serialize_condition_expression(element->data.conditional.condition);
        if (!condition_str) {
            fprintf(stderr, "ERROR: Failed to serialize @if condition in component definition\n");
            return false;
        }

        fprintf(stderr, "DEBUG: Serialized @if condition: '%s'\n", condition_str);

        // Write condition as a property
        uint32_t condition_ref = add_string_to_table(codegen, condition_str);

        // Free the condition string (add_string_to_table makes its own copy)
        if (condition_str) {
            kryon_free(condition_str);
            condition_str = NULL;
        }

        // Property count = 1 (the condition)
        if (!write_uint16(codegen, 1)) {
            return false;
        }

        // Write condition property (using a special property ID for @if condition)
        if (!write_uint16(codegen, 0x9100)) { // Special property ID for @if condition
            return false;
        }
        if (!write_uint32(codegen, condition_ref)) {
            return false;
        }

        // Write child count (then branch + else branch elements)
        uint16_t child_count = (uint16_t)(element->data.conditional.then_count + element->data.conditional.else_count);
        if (!write_uint16(codegen, child_count)) {
            return false;
        }

        // Write then branch elements
        for (size_t i = 0; i < element->data.conditional.then_count; i++) {
            if (!kryon_write_element_node(codegen, element->data.conditional.then_body[i], ast_root)) {
                return false;
            }
        }

        // Write else branch elements
        for (size_t i = 0; i < element->data.conditional.else_count; i++) {
            if (!kryon_write_element_node(codegen, element->data.conditional.else_body[i], ast_root)) {
                return false;
            }
        }

        fprintf(stderr, "✅ @if directive written to component definition\n");
        return true;
    }
    
    uint16_t element_hex;
    if (element->type == KRYON_AST_EVENT_DIRECTIVE) {
        element_hex = kryon_get_syntax_hex("event");
    } else {
        element_hex = kryon_codegen_get_element_hex(element->data.element.element_type);
        if (element_hex == 0) {
            codegen_error(codegen, "Unknown element type");
            return false;
        }
    }
    
    // Write element header
    if (!write_uint16(codegen, element_hex)) {
        return false;
    }
    
    // Write property count
    uint16_t prop_count = (uint16_t)element->data.element.property_count;
    if (!write_uint16(codegen, prop_count)) {
        return false;
    }
    
    // Write properties
    for (size_t i = 0; i < element->data.element.property_count; i++) {
        if (!write_property_node(codegen, element->data.element.properties[i])) {
            return false;
        }
    }
    
    // Write child count
    uint16_t child_count = (uint16_t)element->data.element.child_count;
    if (!write_uint16(codegen, child_count)) {
        return false;
    }
    
    // Write children recursively
    for (size_t i = 0; i < element->data.element.child_count; i++) {
        if (!kryon_write_element_node(codegen, element->data.element.children[i], ast_root)) {
            return false;
        }
    }
    
    return true;
}

// Custom property registry for dynamic property types
static struct {
    char *name;
    uint16_t hex;
} custom_property_registry[512]; // Support up to 512 custom properties
static size_t custom_property_count = 0;
static uint16_t next_custom_property_hex = 0x0A00; // Start custom properties at 0x0A00

// Forward declaration for custom property handling
static uint16_t get_or_create_custom_property_hex(const char *property_name);

bool write_property_node(KryonCodeGenerator *codegen, const KryonASTNode *property) {
    if (!property || property->type != KRYON_AST_PROPERTY) {
        return false;
    }
    
    // Get property hex code using new kryon mappings system
    uint16_t property_hex = kryon_get_property_hex(property->data.property.name);
    bool is_custom_property = false;
    
    if (property_hex == 0) {
        // Unknown property - assign custom property hex automatically
        property_hex = get_or_create_custom_property_hex(property->data.property.name);
        is_custom_property = true;
    }
    
    // Validate property against element if parent element is available
    const KryonASTNode *parent_element = property->parent;
    uint16_t element_hex = 0;
    
    if (parent_element && parent_element->type == KRYON_AST_ELEMENT) {
        element_hex = kryon_codegen_get_element_hex(parent_element->data.element.element_type);
        
        // Only validate against known elements (custom elements are allowed all properties)
        if (element_hex != 0 && element_hex < 0x2000 && !is_custom_property) {
            if (!kryon_is_valid_property_for_element(element_hex, property_hex)) {
                fprintf(stderr, "⚠️  WARNING: Property '%s' (0x%04X) may not be valid for element '%s' (0x%04X)\n",
                       property->data.property.name, property_hex,
                       parent_element->data.element.element_type, element_hex);
                fprintf(stderr, "⚠️  Allowed categories for this element:\n");
                
                // Show allowed categories for debugging
                const KryonPropertyCategoryIndex* allowed = kryon_get_element_allowed_categories(element_hex);
                if (allowed) {
                    for (int i = 0; allowed[i] != KRYON_CATEGORY_COUNT; i++) {
                        fprintf(stderr, "    - %s\n", kryon_property_categories[allowed[i]].name);
                    }
                }
                // Continue anyway for flexibility during development
            }
        }
    }
    
    fprintf(stderr, "🔍 Processing property: '%s' -> 0x%04X (%s)\n", 
           property->data.property.name, property_hex,
           is_custom_property ? "custom" : "built-in");
    
    // Determine property value type using enhanced type system (AFTER const resolution)
    uint8_t value_type = KRYON_RUNTIME_PROP_STRING; // Default
    KryonValueTypeHint type_hint = kryon_get_property_type_hint(property_hex);
    
    if (property->data.property.value) {
        // For IDENTIFIER nodes, check if they're const definitions first
        if (property->data.property.value->type == KRYON_AST_IDENTIFIER) {
            const char *identifier_name = property->data.property.value->data.identifier.name;
            const char *const_value = lookup_constant_value(codegen, identifier_name);
            
            if (const_value) {
                // This identifier resolves to a const - determine type based on const value
                char *endptr;
                double num_value = strtod(const_value, &endptr);
                if (*endptr == '\0') {
                    // Const value is numeric - determine appropriate numeric type based on type hint
                    if (type_hint == KRYON_TYPE_HINT_DIMENSION || 
                        type_hint == KRYON_TYPE_HINT_SPACING || 
                        type_hint == KRYON_TYPE_HINT_UNIT ||
                        type_hint == KRYON_TYPE_HINT_FLOAT) {
                        value_type = KRYON_RUNTIME_PROP_FLOAT;
                    } else {
                        value_type = KRYON_RUNTIME_PROP_INTEGER;
                    }
                    } else {
                    // Const value is string
                        value_type = KRYON_RUNTIME_PROP_STRING;
                }
            } else {
                // Not a const - treat as variable reference
            value_type = KRYON_RUNTIME_PROP_REFERENCE;
            }
        } else if (property->data.property.value->type == KRYON_AST_BINARY_OP ||
                   property->data.property.value->type == KRYON_AST_UNARY_OP ||
                   property->data.property.value->type == KRYON_AST_TERNARY_OP ||
                   property->data.property.value->type == KRYON_AST_FUNCTION_CALL ||
                   property->data.property.value->type == KRYON_AST_MEMBER_ACCESS ||
                   property->data.property.value->type == KRYON_AST_ARRAY_ACCESS) {
            // For complex expressions, check if they can be resolved to literals
            // This matches the logic in write_enhanced_property_value for expressions
            char *expression_str = kryon_ast_expression_to_string(property->data.property.value, codegen);
            if (expression_str) {
                // Check if expression resolved to a number
                char *endptr;
                double num_value = strtod(expression_str, &endptr);
                if (*endptr == '\0') {
                    // Expression resolved to a number
                    if (type_hint == KRYON_TYPE_HINT_DIMENSION || 
                        type_hint == KRYON_TYPE_HINT_SPACING || 
                        type_hint == KRYON_TYPE_HINT_UNIT ||
                        type_hint == KRYON_TYPE_HINT_FLOAT) {
                        value_type = KRYON_RUNTIME_PROP_FLOAT;
                    } else {
                        value_type = KRYON_RUNTIME_PROP_INTEGER;
                    }
                } else if (strcmp(expression_str, "true") == 0 || strcmp(expression_str, "false") == 0) {
                    // Expression resolved to a boolean
                    value_type = KRYON_RUNTIME_PROP_BOOLEAN;
                } else if (expression_str[0] == '"' && expression_str[strlen(expression_str)-1] == '"') {
                    // Expression resolved to a quoted string literal - check for color inside quotes
                    size_t len = strlen(expression_str);
                    if (len == 9 && expression_str[1] == '#') {
                        // Quoted color string like "#90EE90"
                        value_type = KRYON_RUNTIME_PROP_COLOR;
                    } else {
                        value_type = KRYON_RUNTIME_PROP_STRING;
                    }
                } else if (expression_str[0] == '#' && strlen(expression_str) == 7) {
                    // Expression resolved to a color hex string
                    value_type = KRYON_RUNTIME_PROP_COLOR;
                } else {
                    // Still a complex expression - treat as reference
                    value_type = KRYON_RUNTIME_PROP_REFERENCE;
                }
                kryon_free(expression_str);
            } else {
                // Expression evaluation failed - treat as reference
                value_type = KRYON_RUNTIME_PROP_REFERENCE;
            }
        } else {
            // For other node types (literals, templates, etc.), use standard type determination
            value_type = determine_property_value_type(property->data.property.value, type_hint);
        }
    }
    
    // Create property header using schema format
    KRBPropertyHeader prop_header = {
        .property_id = property_hex,
        .value_type = value_type
    };
    
    // Write property header using centralized schema validation
    if (!krb_write_property_header(schema_writer, codegen, &prop_header)) {
        codegen_error(codegen, "Failed to write property header using schema");
        return false;
    }
    
    // If custom property, write the name as a string reference
    if (is_custom_property) {
        uint32_t name_ref = add_string_to_table(codegen, property->data.property.name);
        if (!write_uint32(codegen, name_ref)) {
            return false;
        }
    }
    
    // Write property value using enhanced processing
    if (property->data.property.value) {
        fprintf(stderr, "🔍 Processing property value: '%s' (type=%d, hint=%d)\n", 
               property->data.property.name, property->data.property.value->type, type_hint);
               
        return write_enhanced_property_value(codegen, property->data.property.value, property_hex, type_hint);
                } else {
        fprintf(stderr, "⚠️  Property '%s' has no value - writing empty value\n", property->data.property.name);
        return write_empty_property_value(codegen, property_hex, type_hint);
    }
}

static bool write_property_value(KryonCodeGenerator *codegen, const KryonASTValue *value, uint16_t property_hex) {
    // Get runtime property type for this property to match loader expectations
    KryonValueTypeHint type_hint = get_property_type_hint(property_hex);
    
    // Debug output
    fprintf(stderr, "DEBUG: Property 0x%04X has type hint %d, value type %d\n", 
           property_hex, type_hint, value->type);
    
    // Write value based on property type hint, not AST value type
    if (type_hint == KRYON_TYPE_HINT_COLOR) {
        // This property should be written as a color (u32), regardless of AST value type
        uint32_t color_value;
        
        if (value->type == KRYON_VALUE_COLOR) {
            color_value = value->data.color_value;
        } else if (value->type == KRYON_VALUE_STRING) {
            // Parse string color to u32
            color_value = kryon_color_parse_string(value->data.string_value);
            fprintf(stderr, "DEBUG: Parsed color '%s' -> 0x%08X\n", value->data.string_value, color_value);
        } else {
            codegen_error(codegen, "Cannot convert value type to color");
            return false;
        }
        
        if (!write_uint32(codegen, color_value)) return false;
        
    } else if (type_hint == KRYON_TYPE_HINT_FLOAT || type_hint == KRYON_TYPE_HINT_DIMENSION || 
               type_hint == KRYON_TYPE_HINT_UNIT || type_hint == KRYON_TYPE_HINT_SPACING) {
        // This property should be written as a float (u32 bits)
        float f;
        
        if (value->type == KRYON_VALUE_FLOAT) {
            f = (float)value->data.float_value;
        } else if (value->type == KRYON_VALUE_INTEGER) {
            f = (float)value->data.int_value;
        } else if (value->type == KRYON_VALUE_UNIT) {
            f = (float)value->data.unit_value.value;
        } else {
            codegen_error(codegen, "Cannot convert value type to float");
            return false;
        }
        
        uint32_t bits;
        memcpy(&bits, &f, sizeof(uint32_t));
        if (!write_uint32(codegen, bits)) return false;
        
    } else if (type_hint == KRYON_TYPE_HINT_INTEGER) {
        // This property should be written as an integer (u32)
        uint32_t int_val;
        
        if (value->type == KRYON_VALUE_INTEGER) {
            int_val = (uint32_t)value->data.int_value;
        } else if (value->type == KRYON_VALUE_FLOAT) {
            int_val = (uint32_t)value->data.float_value;
        } else {
            codegen_error(codegen, "Cannot convert value type to integer");
            return false;
        }
        
        if (!write_uint32(codegen, int_val)) return false;
        
    } else if (type_hint == KRYON_TYPE_HINT_BOOLEAN) {
        // This property should be written as a boolean (u8)
        uint8_t bool_val;
        
        if (value->type == KRYON_VALUE_BOOLEAN) {
            bool_val = value->data.bool_value ? 1 : 0;
        } else {
            codegen_error(codegen, "Cannot convert value type to boolean");
            return false;
        }
        
        if (!write_uint8(codegen, bool_val)) return false;
        
    } else if (type_hint == KRYON_TYPE_HINT_REFERENCE) {
        // This property is a function reference - write as string but mark as function type
        const char *str = NULL;
        
        if (value->type == KRYON_VALUE_STRING) {
            str = value->data.string_value;
        } else {
            codegen_error(codegen, "Function reference must be a string");
            return false;
        }
        
        uint16_t len = str ? (uint16_t)strlen(str) : 0;
        if (!write_uint16(codegen, len)) return false;
        if (len > 0 && !write_binary_data(codegen, str, len)) return false;
        
    } else {
        // Default: write as string (length + data)
        const char *str = NULL;
        char *allocated_str = NULL;
        
        if (value->type == KRYON_VALUE_STRING) {
            str = value->data.string_value;
            fprintf(stderr, "🔍 DEBUG: Writing string value: '%s' (length=%zu)\n", str ? str : "(null)", str ? strlen(str) : 0);
        } else if (value->type == KRYON_VALUE_INTEGER) {
            // Convert integer to string for properties that expect strings (like text)
            allocated_str = malloc(32);
            if (!allocated_str) {
                codegen_error(codegen, "Failed to allocate memory for integer-to-string conversion");
                return false;
            }
            snprint(allocated_str, 32, "%lld", (long long)value->data.int_value);
            str = allocated_str;
            fprintf(stderr, "🔍 DEBUG: Converting integer %lld to string '%s' for string property\n", 
                   (long long)value->data.int_value, str);
        } else if (value->type == KRYON_VALUE_FLOAT) {
            // Convert float to string for properties that expect strings
            allocated_str = malloc(32);
            if (!allocated_str) {
                codegen_error(codegen, "Failed to allocate memory for float-to-string conversion");
                return false;
            }
            snprint(allocated_str, 32, "%.6g", value->data.float_value);
            str = allocated_str;
            fprintf(stderr, "🔍 DEBUG: Converting float %.6g to string '%s' for string property\n", 
                   value->data.float_value, str);
        } else if (value->type == KRYON_VALUE_BOOLEAN) {
            // Convert boolean to string for properties that expect strings
            str = value->data.bool_value ? "true" : "false";
            fprintf(stderr, "🔍 DEBUG: Converting boolean %s to string for string property\n", str);
        } else {
            codegen_error(codegen, "Cannot convert value type to string");
            return false;
        }
        
        uint16_t len = str ? (uint16_t)strlen(str) : 0;
        bool success = write_uint16(codegen, len);
        if (success && len > 0) {
            success = write_binary_data(codegen, str, len);
        }
        
        // Clean up allocated string
        if (allocated_str) {
            free(allocated_str);
        }
        
        if (!success) return false;
    }
    
    return true;
}

static bool write_variable_reference(KryonCodeGenerator *codegen, const char *variable_name, uint16_t property_hex) {
    // Write a variable reference as proper REFERENCE type (no $ prefix!)
    
    if (!variable_name) {
        codegen_error(codegen, "Variable reference has null name");
        return false;
    }
    
    // Enhanced validation - check for unresolved const variable patterns that should have been substituted
    if (strstr(variable_name, "alignment.") == variable_name) {
        fprintf(stderr, "❌ ERROR: Found alignment variable reference that should have been substituted: '%s'\n", variable_name);
        fprintf(stderr, "❌ ERROR: This indicates a template substitution bug - alignment references should be resolved at compile time!\n");
        
        // For safety, reject writing this corrupted reference
        codegen_error(codegen, "Alignment variable reference found - should have been substituted");
        return false;
    }
    
    // Additional validation for other const variable patterns
    // Allow component instance variables (comp_N.xxx) but block other property access patterns
    bool is_component_instance_variable = (strncmp(variable_name, "comp_", 5) == 0);
    
    if (!is_component_instance_variable && 
        (strstr(variable_name, ".colors[") != NULL || strstr(variable_name, ".value") != NULL || 
         strstr(variable_name, ".gap") != NULL || strstr(variable_name, ".name") != NULL)) {
        fprintf(stderr, "❌ ERROR: Found unresolved const property access: '%s'\n", variable_name);
        fprintf(stderr, "❌ ERROR: This should have been resolved during @const evaluation\n");
        fprintf(stderr, "❌ ERROR: Check that template substitution is working for MEMBER_ACCESS and ARRAY_ACCESS nodes\n");
        codegen_error(codegen, "Unresolved const property access found");
        return false;
    }
    
    // Log successful component instance variable detection
    if (is_component_instance_variable) {
        fprintf(stderr, "✅ Allowing component instance variable: '%s'\n", variable_name);
    }
    
    // Validate variable name format
    size_t name_len = strlen(variable_name);
    if (name_len == 0 || name_len > 1024) {
        fprintf(stderr, "❌ ERROR: Invalid variable name length: %zu (name='%s')\n", name_len, variable_name);
        codegen_error(codegen, "Invalid variable name length");
        return false;
    }
    
    // Add variable name to string table and get reference
    uint32_t variable_name_ref = add_string_to_table(codegen, variable_name);
    if (variable_name_ref == UINT32_MAX) {
        codegen_error(codegen, "Failed to add variable name to string table");
        return false;
    }
    
    // Validate the string table reference is reasonable
    if (variable_name_ref > 10000) {
        fprintf(stderr, "❌ ERROR: String table reference is suspiciously large: %u for variable '%s'\n", 
               variable_name_ref, variable_name);
        codegen_error(codegen, "String table reference out of range");
        return false;
    }
    
    // Write property type prefix first (required by runtime loader)
    if (!write_uint8(codegen, KRYON_RUNTIME_PROP_REFERENCE)) {
        codegen_error(codegen, "Failed to write REFERENCE property type prefix");
        return false;
    }
    
    // Write variable name reference (points to string table)
    if (!write_uint32(codegen, variable_name_ref)) {
        codegen_error(codegen, "Failed to write variable name reference");
        return false;
    }
    
    fprintf(stderr, "    Wrote variable reference: name='%s' (string_ref=%u, type=REFERENCE)\n", 
           variable_name, variable_name_ref);
    
    return true;
}

static KryonValueTypeHint get_property_type_hint(uint16_t property_hex) {
    // Use centralized property type hints
    return kryon_get_property_type_hint(property_hex);
}

// Helper function to get or create custom property hex codes
static uint16_t get_or_create_custom_property_hex(const char *property_name) {
    if (!property_name) {
        return 0;
    }
    
    // Check if this custom property already exists
    for (size_t i = 0; i < custom_property_count; i++) {
        if (custom_property_registry[i].name && 
            strcmp(custom_property_registry[i].name, property_name) == 0) {
            return custom_property_registry[i].hex;
        }
    }
    
    // Create new custom property entry
    if (custom_property_count >= 512) {
        // Registry full
        fprintf(stderr, "❌ Custom property registry full, cannot register: %s\n", property_name);
        return 0;
    }
    
    // Allocate memory for the property name
    custom_property_registry[custom_property_count].name = malloc(strlen(property_name) + 1);
    if (!custom_property_registry[custom_property_count].name) {
        return 0;
    }
    
    strcpy(custom_property_registry[custom_property_count].name, property_name);
    custom_property_registry[custom_property_count].hex = next_custom_property_hex++;
    
    fprintf(stderr, "🔧 Registered custom property: %s -> 0x%04X\n", 
           property_name, custom_property_registry[custom_property_count].hex);
    
    return custom_property_registry[custom_property_count++].hex;
}

// Enhanced property value type determination
static uint8_t determine_property_value_type(const KryonASTNode *value_node, KryonValueTypeHint type_hint) {
    if (!value_node) {
        return KRYON_RUNTIME_PROP_STRING;
    }
    
    switch (value_node->type) {
        case KRYON_AST_LITERAL:
            return determine_literal_value_type(&value_node->data.literal.value, type_hint);
            
        case KRYON_AST_VARIABLE:
            return KRYON_RUNTIME_PROP_REFERENCE;
            
        case KRYON_AST_TEMPLATE: {
            // Check if template can be resolved to literals
            bool all_literals = true;
            for (size_t i = 0; i < value_node->data.template.segment_count; i++) {
                const KryonASTNode *segment = value_node->data.template.segments[i];
                if (!segment || segment->type != KRYON_AST_LITERAL) {
                    all_literals = false;
                    break;
                }
            }
            
            if (all_literals) {
                // Template resolves to literals - treat as STRING
                return KRYON_RUNTIME_PROP_STRING;
            } else {
                // Template has runtime variables - treat as TEMPLATE
                return KRYON_RUNTIME_PROP_TEMPLATE;
            }
        }
        
        case KRYON_AST_ARRAY_LITERAL:
            return KRYON_RUNTIME_PROP_ARRAY;
            
        case KRYON_AST_IDENTIFIER: {
            // Check if identifier is a const or a declared variable
            const char *identifier_name = value_node->data.identifier.name;
            
            if (!identifier_name || strlen(identifier_name) == 0) {
                return KRYON_RUNTIME_PROP_STRING;
            }
            
            // Check if this identifier is a const definition (these should be resolved to literals)
            // We need access to the codegen context to check for consts, but this function doesn't have it.
            // For now, assume any identifier that would reach this point is a variable reference.
            // The actual const resolution happens in write_enhanced_property_value, not here.
            
            // NOTE: If a const was properly resolved, we should never reach this case
            // because the AST node would have been replaced with a literal during const evaluation.
            // So any IDENTIFIER that reaches here is likely a runtime variable reference.
            
            return KRYON_RUNTIME_PROP_REFERENCE;
        }
        
        default:
            // Complex expressions default to string representation
            return KRYON_RUNTIME_PROP_STRING;
    }
}

// Helper for literal value type determination
static uint8_t determine_literal_value_type(const KryonASTValue *literal_value, KryonValueTypeHint type_hint) {
    if (!literal_value) {
        return KRYON_RUNTIME_PROP_STRING;
    }
    
    // Use type hint to override default behavior when necessary
    switch (literal_value->type) {
        case KRYON_VALUE_STRING:
            // Check if this string should be treated as a different type based on property hint
            if (type_hint == KRYON_TYPE_HINT_COLOR) {
                return KRYON_RUNTIME_PROP_COLOR;
            } else {
                return KRYON_RUNTIME_PROP_STRING;
            }
            
        case KRYON_VALUE_INTEGER:
            // Check if the target property expects a different type based on hint
            if (type_hint == KRYON_TYPE_HINT_STRING) {
                return KRYON_RUNTIME_PROP_STRING;
            } else if (type_hint == KRYON_TYPE_HINT_DIMENSION || 
                      type_hint == KRYON_TYPE_HINT_SPACING || 
                      type_hint == KRYON_TYPE_HINT_UNIT ||
                      type_hint == KRYON_TYPE_HINT_FLOAT) {
                // Integer values for dimension/spacing/unit properties are written as floats
                return KRYON_RUNTIME_PROP_FLOAT;
            } else {
                return KRYON_RUNTIME_PROP_INTEGER;
            }
            
        case KRYON_VALUE_FLOAT:
            // Check if the target property expects a string
            if (type_hint == KRYON_TYPE_HINT_STRING) {
                return KRYON_RUNTIME_PROP_STRING;
            } else {
                return KRYON_RUNTIME_PROP_FLOAT;
            }
            
        case KRYON_VALUE_BOOLEAN:
            // Check if the target property expects a string
            if (type_hint == KRYON_TYPE_HINT_STRING) {
                return KRYON_RUNTIME_PROP_STRING;
            } else {
                return KRYON_RUNTIME_PROP_BOOLEAN;
            }
            
        case KRYON_VALUE_COLOR:
            return KRYON_RUNTIME_PROP_COLOR;
            
        case KRYON_VALUE_UNIT:
            // Unit values are always written as floats regardless of the property hint
            return KRYON_RUNTIME_PROP_FLOAT;
            
        default:
            return KRYON_RUNTIME_PROP_STRING;
    }
}

// Enhanced property value writer using new mapping system
static bool write_enhanced_property_value(KryonCodeGenerator *codegen, const KryonASTNode *value_node, uint16_t property_hex, KryonValueTypeHint type_hint) {
    if (!value_node) {
        return false;
    }
    
    switch (value_node->type) {
        case KRYON_AST_LITERAL:
            fprintf(stderr, "🔍 Writing literal value with type hint %d\n", type_hint);
            return write_property_value(codegen, &value_node->data.literal.value, property_hex);
            
        case KRYON_AST_VARIABLE:
            // Check if this is a runtime variable reference (e.g., @for array property)
            if (property_hex == 0x8202) { // @for array property - runtime variable expected
                fprintf(stderr, "🔗 Property 'array' bound to variable '%s'\n", value_node->data.variable.name);
                return write_variable_reference(codegen, value_node->data.variable.name, property_hex);
            } else {
                // For other properties, this might be an unresolved compile-time constant
                fprintf(stderr, "❌ Found unresolved variable reference: '%s' for property (0x%04X)\n", 
                       value_node->data.variable.name, property_hex);
                fprintf(stderr, "❌ This variable should have been substituted during const evaluation!\n");
                return write_variable_reference(codegen, value_node->data.variable.name, property_hex);
            }
            
        case KRYON_AST_TEMPLATE:
            fprintf(stderr, "🔄 Writing structured template with segments\n");
            return write_template_property(codegen, value_node, property_hex);
            
        case KRYON_AST_ARRAY_LITERAL:
            if (type_hint != KRYON_TYPE_HINT_ARRAY) {
                codegen_error(codegen, "Property does not support array values");
                return false;
            }
            return write_array_literal_property(codegen, value_node);
            
        case KRYON_AST_IDENTIFIER: {
            const char *identifier_name = value_node->data.identifier.name;
            
            fprintf(stderr, "🔍 Processing IDENTIFIER node '%s' for property (0x%04X)\n", 
                   identifier_name, property_hex);
            
            if (!identifier_name || strlen(identifier_name) == 0) {
                codegen_error(codegen, "Empty identifier name");
                return false;
            }
            
            // Check if this identifier is a const definition
            const char *const_value = lookup_constant_value(codegen, identifier_name);
            if (const_value) {
                fprintf(stderr, "✅ Const resolved: %s = %s - writing as literal\n", identifier_name, const_value);
                
                // Create a temporary literal value and write it directly
                KryonASTValue literal_value = {0};
                
                // Try to parse as number first, then fallback to string
                char *endptr;
                double num_value = strtod(const_value, &endptr);
                if (*endptr == '\0') {
                    // Successfully parsed as number
                    literal_value.type = KRYON_VALUE_FLOAT;
                    literal_value.data.float_value = (float)num_value;
                } else {
                    // Treat as string - make a copy since const_value is const
                    literal_value.type = KRYON_VALUE_STRING;
                    literal_value.data.string_value = (char*)const_value; // Cast away const for temporary use
                }
                
                return write_property_value(codegen, &literal_value, property_hex);
            } else {
                // Not a const, treat as runtime variable reference  
                fprintf(stderr, "✅ Writing identifier '%s' as variable reference\n", identifier_name);
                return write_variable_reference(codegen, identifier_name, property_hex);
            }
        }
        
        default: {
            // Complex expression - convert to string representation 
            fprintf(stderr, "🔄 Converting complex expression (type %d) to string for runtime evaluation\n", value_node->type);
            char* expression_str = kryon_ast_expression_to_string(value_node, codegen);
            if (!expression_str) {
                fprintf(stderr, "❌ Failed to serialize expression to string\n");
                codegen_error(codegen, "Failed to serialize complex expression");
                return false;
            }
            
            // Check if the expression was fully evaluated to a literal value
            char *end_ptr;
            double numeric_value = strtod(expression_str, &end_ptr);
            
            if (*end_ptr == '\0') {
                // Expression resolved to a pure number - write as literal
                fprintf(stderr, "✅ Expression resolved to literal number: %s - writing as literal\n", expression_str);
                
                KryonASTValue literal_value = {0};
                
                if (numeric_value == floor(numeric_value)) {
                    literal_value.type = KRYON_VALUE_INTEGER;
                    literal_value.data.int_value = (int64_t)numeric_value;
                } else {
                    literal_value.type = KRYON_VALUE_FLOAT;
                    literal_value.data.float_value = numeric_value;
                }
                
                kryon_free(expression_str);
                return write_property_value(codegen, &literal_value, property_hex);
                
            } else if (strcmp(expression_str, "true") == 0 || strcmp(expression_str, "false") == 0) {
                // Expression resolved to a boolean
                fprintf(stderr, "✅ Expression resolved to literal boolean: %s - writing as literal\n", expression_str);
                
                KryonASTValue literal_value = {0};
                literal_value.type = KRYON_VALUE_BOOLEAN;
                literal_value.data.bool_value = strcmp(expression_str, "true") == 0;
                
                kryon_free(expression_str);
                return write_property_value(codegen, &literal_value, property_hex);
                
            } else if (expression_str[0] == '"' && expression_str[strlen(expression_str)-1] == '"') {
                // Expression resolved to a string literal
                fprintf(stderr, "✅ Expression resolved to literal string: %s - writing as literal\n", expression_str);
                
                size_t len = strlen(expression_str);
                char* unquoted = malloc(len - 1);
                if (unquoted) {
                    strncpy(unquoted, expression_str + 1, len - 2);
                    unquoted[len - 2] = '\0';
                }
                
                KryonASTValue literal_value = {0};
                literal_value.type = KRYON_VALUE_STRING;
                literal_value.data.string_value = unquoted;
                
                kryon_free(expression_str);
                bool result = write_property_value(codegen, &literal_value, property_hex);
                free(unquoted);
                return result;
                
            } else if (expression_str[0] == '#' && strlen(expression_str) == 7) {
                // Expression resolved to a color hex string
                fprintf(stderr, "✅ Expression resolved to literal color: %s - writing as literal\n", expression_str);
                
                KryonASTValue literal_value = {0};
                literal_value.type = KRYON_VALUE_STRING;
                literal_value.data.string_value = expression_str;
                
                bool result = write_property_value(codegen, &literal_value, property_hex);
                kryon_free(expression_str);
                return result;
                
            } else {
                // Still a complex expression - write as variable reference
                fprintf(stderr, "🔄 Writing complex expression as reactive reference: '%s'\n", expression_str);
                bool result = write_variable_reference(codegen, expression_str, property_hex);
                kryon_free(expression_str);
                return result;
            }
        }
    }
}

// Write empty property value based on type hint
static bool write_empty_property_value(KryonCodeGenerator *codegen, uint16_t property_hex, KryonValueTypeHint type_hint) {
    // For custom properties or string properties, write empty string
    if (property_hex >= 0x0A00 || type_hint == KRYON_TYPE_HINT_STRING || type_hint == KRYON_TYPE_HINT_REFERENCE) {
        return write_uint16(codegen, 0); // Empty string length
    } else {
        // For other properties, write zero value
        return write_uint32(codegen, 0);
    }
}

static bool write_array_literal_property(KryonCodeGenerator *codegen, const KryonASTNode *array_node) {
    if (!array_node || array_node->type != KRYON_AST_ARRAY_LITERAL) {
        return false;
    }

    // Write number of elements as u16
    uint16_t count = (uint16_t)array_node->data.array_literal.element_count;
    if (!write_uint16(codegen, count)) {
        return false;
    }

    // Write each element
    for (size_t i = 0; i < count; i++) {
        const KryonASTNode *element_node = array_node->data.array_literal.elements[i];
        if (!element_node || element_node->type != KRYON_AST_LITERAL) {
            codegen_error(codegen, "Array elements must be literals for now");
            return false;
        }

        const KryonASTValue *value = &element_node->data.literal.value;

        if (value->type != KRYON_VALUE_STRING) {
            codegen_error(codegen, "Array elements must be strings for 'options' property");
            return false;
        }

        // Write string element
        uint32_t string_ref = add_string_to_table(codegen, value->data.string_value);
        if (!write_uint32(codegen, string_ref)) {
            return false;
        }
    }

    return true;
}

uint16_t count_expanded_children(KryonCodeGenerator *codegen, const KryonASTNode *element) {
    if (!element) {
        return 0;
    }

    uint16_t expanded_count = 0;

    // Get children using unified helper function
    size_t child_count = 0;
    KryonASTNode **children = kryon_ast_get_children(element, &child_count);

    if (!children) {
        return 0;
    }

    for (size_t i = 0; i < child_count; i++) {
        const KryonASTNode *child = children[i];
        if (!child) continue;
        
        if (child->type == KRYON_AST_ELEMENT) {
            // Regular element counts as 1
            expanded_count += 1;
        } else if (child->type == KRYON_AST_FOR_DIRECTIVE) {
            // @for directive counts as 1 element (will be expanded at runtime)
            expanded_count += 1;
        } else if (child->type == KRYON_AST_IF_DIRECTIVE) {
            // @if directive counts as 1 element
            expanded_count += 1;
        }
        // Skip other types (constants, etc.)
    }
    
    return expanded_count;
}

uint32_t kryon_count_elements_recursive(const KryonASTNode *node) {
    if (!node) return 0;

    uint32_t count = 0;

    // Count this node if it's an element
    if (node->type == KRYON_AST_ELEMENT) {
        count = 1;

        // Count children recursively
        for (size_t i = 0; i < node->data.element.child_count; i++) {
            count += kryon_count_elements_recursive(node->data.element.children[i]);
        }
    } else if (node->type == KRYON_AST_ROOT) {
        // For root nodes, just count children
        for (size_t i = 0; i < node->data.element.child_count; i++) {
            count += kryon_count_elements_recursive(node->data.element.children[i]);
        }
    } else if (node->type == KRYON_AST_FOR_DIRECTIVE) {
        // Count @for directive as 1 element, plus its body
        count = 1;
        for (size_t i = 0; i < node->data.for_loop.body_count; i++) {
            count += kryon_count_elements_recursive(node->data.for_loop.body[i]);
        }
    } else if (node->type == KRYON_AST_IF_DIRECTIVE) {
        // Count @if directive as 1 element, plus its branches
        count = 1;
        for (size_t i = 0; i < node->data.conditional.then_count; i++) {
            count += kryon_count_elements_recursive(node->data.conditional.then_body[i]);
        }
        for (size_t i = 0; i < node->data.conditional.elif_count; i++) {
            for (size_t j = 0; j < node->data.conditional.elif_counts[i]; j++) {
                count += kryon_count_elements_recursive(node->data.conditional.elif_bodies[i][j]);
            }
        }
        for (size_t i = 0; i < node->data.conditional.else_count; i++) {
            count += kryon_count_elements_recursive(node->data.conditional.else_body[i]);
        }
    }

    return count;
}

uint32_t kryon_count_properties_recursive(const KryonASTNode *node) {
    if (!node) return 0;

    uint32_t count = 0;

    if (node->type == KRYON_AST_ELEMENT) {
        // Count this element's properties
        count = (uint32_t)node->data.element.property_count;

        // Count children's properties recursively
        for (size_t i = 0; i < node->data.element.child_count; i++) {
            count += kryon_count_properties_recursive(node->data.element.children[i]);
        }
    } else if (node->type == KRYON_AST_ROOT) {
        // For root nodes, just count children's properties
        for (size_t i = 0; i < node->data.element.child_count; i++) {
            count += kryon_count_properties_recursive(node->data.element.children[i]);
        }
    } else if (node->type == KRYON_AST_FOR_DIRECTIVE) {
        // @for directive has 1 property (condition), plus body properties
        count = 1;
        for (size_t i = 0; i < node->data.for_loop.body_count; i++) {
            count += kryon_count_properties_recursive(node->data.for_loop.body[i]);
        }
    } else if (node->type == KRYON_AST_IF_DIRECTIVE) {
        // @if directive has 1 property (condition), plus body properties
        count = 1;
        for (size_t i = 0; i < node->data.conditional.then_count; i++) {
            count += kryon_count_properties_recursive(node->data.conditional.then_body[i]);
        }
        for (size_t i = 0; i < node->data.conditional.elif_count; i++) {
            for (size_t j = 0; j < node->data.conditional.elif_counts[i]; j++) {
                count += kryon_count_properties_recursive(node->data.conditional.elif_bodies[i][j]);
            }
        }
        for (size_t i = 0; i < node->data.conditional.else_count; i++) {
            count += kryon_count_properties_recursive(node->data.conditional.else_body[i]);
        }
    }

    return count;
}

// =============================================================================
// TEMPLATE PROPERTY SERIALIZATION  
// =============================================================================

/**
 * @brief Write template property with structured segments
 * @param codegen Code generator context
 * @param template_node Template AST node with segments
 * @param property_hex Property hex code 
 * @return true on success, false on failure
 */
static bool write_template_property(KryonCodeGenerator *codegen, const KryonASTNode *template_node, uint16_t property_hex) {
    if (!template_node || template_node->type != KRYON_AST_TEMPLATE) {
        codegen_error(codegen, "Invalid template node");
        return false;
    }
    
    // Check if all segments are literals (fully resolved template)
    // Templates with VARIABLE segments need to be kept as templates for runtime resolution
    bool all_literals = true;
    bool has_variables = false;
    size_t total_length = 0;

    for (size_t i = 0; i < template_node->data.template.segment_count; i++) {
        const KryonASTNode *segment = template_node->data.template.segments[i];
        if (!segment) {
            all_literals = false;
            break;
        }

        if (segment->type == KRYON_AST_VARIABLE || segment->type == KRYON_AST_IDENTIFIER) {
            // Variable or identifier segment - must keep as template for runtime resolution
            all_literals = false;
            has_variables = true;
            break;
        }

        if (segment->type != KRYON_AST_LITERAL) {
            all_literals = false;
            break;
        }

        const char *text = segment->data.literal.value.data.string_value;
        if (text) {
            total_length += strlen(text);
        }
    }
    
    // If all segments are literals, concatenate them and write as a simple string
    if (all_literals) {
        fprintf(stderr, "✅ Template fully resolved to literals - writing as string property\n");
        
        // Allocate buffer for concatenated string
        char *concatenated = kryon_alloc(total_length + 1);
        if (!concatenated) {
            codegen_error(codegen, "Failed to allocate memory for template concatenation");
            return false;
        }
        
        concatenated[0] = '\0';  // Start with empty string
        
        // Concatenate all literal segments
        for (size_t i = 0; i < template_node->data.template.segment_count; i++) {
            const KryonASTNode *segment = template_node->data.template.segments[i];
            const char *text = segment->data.literal.value.data.string_value;
            if (text) {
                strcat(concatenated, text);
            }
        }
        
        fprintf(stderr, "📝 Concatenated template to: '%s'\n", concatenated);
        
        // Create a literal value and write as string property
        KryonASTValue string_value = {
            .type = KRYON_VALUE_STRING,
            .data.string_value = concatenated
        };
        
        bool result = write_property_value(codegen, &string_value, property_hex);
        
        kryon_free(concatenated);
        return result;
    }
    
    // Template has runtime variables - write as complex TEMPLATE property
    fprintf(stderr, "🔄 Template has runtime variables - writing as complex template\n");
    
    // Write property type as TEMPLATE
    if (!write_uint8(codegen, (uint8_t)KRYON_RUNTIME_PROP_TEMPLATE)) {
        codegen_error(codegen, "Failed to write TEMPLATE property type");
        return false;
    }
    
    // Write segment count
    uint16_t segment_count = (uint16_t)template_node->data.template.segment_count;
    if (!write_uint16(codegen, segment_count)) {
        codegen_error(codegen, "Failed to write template segment count");
        return false;
    }
    
    fprintf(stderr, "📝 Writing template with %u segments\n", segment_count);
    
    // Write each segment
    for (size_t i = 0; i < segment_count; i++) {
        const KryonASTNode *segment = template_node->data.template.segments[i];
        if (!segment) {
            codegen_error(codegen, "Null template segment");
            return false;
        }
        
        if (segment->type == KRYON_AST_LITERAL) {
            // Write LITERAL segment type
            if (!write_uint8(codegen, (uint8_t)KRYON_TEMPLATE_SEGMENT_LITERAL)) {
                codegen_error(codegen, "Failed to write LITERAL segment type");
                return false;
            }
            
            // Write literal text (length + data)
            const char *text = segment->data.literal.value.data.string_value;
            uint16_t text_len = text ? (uint16_t)strlen(text) : 0;
            
            if (!write_uint16(codegen, text_len)) {
                codegen_error(codegen, "Failed to write literal text length");
                return false;
            }
            
            if (text_len > 0 && !write_binary_data(codegen, text, text_len)) {
                codegen_error(codegen, "Failed to write literal text data");
                return false;
            }
            
            fprintf(stderr, "  📝 Literal segment: '%s' (%u bytes)\n", text ? text : "", text_len);
            
        } else if (segment->type == KRYON_AST_VARIABLE) {
            // Write VARIABLE segment type
            if (!write_uint8(codegen, (uint8_t)KRYON_TEMPLATE_SEGMENT_VARIABLE)) {
                codegen_error(codegen, "Failed to write VARIABLE segment type");
                return false;
            }

            // Add variable name to string table and write reference
            const char *var_name = segment->data.variable.name;
            if (!var_name) {
                codegen_error(codegen, "Variable segment has null name");
                return false;
            }

            fprintf(stderr, "  🔗 DEBUG: VARIABLE segment has name='%s' (ptr=%p)\n", var_name, (void*)var_name);

            uint32_t var_name_ref = add_string_to_table(codegen, var_name);
            if (var_name_ref == UINT32_MAX) {
                codegen_error(codegen, "Failed to add variable name to string table");
                return false;
            }

            if (!write_uint32(codegen, var_name_ref)) {
                codegen_error(codegen, "Failed to write variable name reference");
                return false;
            }

            fprintf(stderr, "  🔗 Variable segment: '%s' (string_ref=%u)\n", var_name, var_name_ref);
            
        } else {
            fprintf(stderr, "❌ Error: Unsupported template segment type: %d\n", segment->type);
            codegen_error(codegen, "Unsupported template segment type");
            return false;
        }
    }
    
    fprintf(stderr, "✅ Template property written successfully\n");
    return true;
}

// =============================================================================
// @FOR TEMPLATE SERIALIZATION
// =============================================================================

/**
 * @brief Serialize @for directive as template metadata
 * @param codegen Code generator context
 * @param element @for directive AST node
 * @return true on success, false on failure
 */
bool serialize_for_template(KryonCodeGenerator* codegen, const KryonASTNode* element) {
    if (!codegen || !element || element->type != KRYON_AST_FOR_DIRECTIVE) {
        return false;
    }
    
    fprintf(stderr, "DEBUG: Serializing @for template with %zu children\n", element->data.element.child_count);
    
    // Store template metadata instead of writing as element
    // For now, just store in a special templates array in the codegen context
    // TODO: Implement proper template storage in KRB format
    
    // Extract @for properties (variable name, array name)
    const char* var_name = NULL;
    const char* array_name = NULL;
    
    for (size_t i = 0; i < element->data.element.property_count; i++) {
        const KryonASTNode* prop = element->data.element.properties[i];
        if (prop && strcmp(prop->data.property.name, "variable") == 0) {
            if (prop->data.property.value && prop->data.property.value->type == KRYON_AST_LITERAL) {
                var_name = prop->data.property.value->data.literal.value.data.string_value;
            }
        } else if (prop && strcmp(prop->data.property.name, "array") == 0) {
            if (prop->data.property.value && prop->data.property.value->type == KRYON_AST_LITERAL) {
                array_name = prop->data.property.value->data.literal.value.data.string_value;
            }
        }
    }
    
    fprintf(stderr, "DEBUG: @for template - variable: %s, array: %s\n", 
           var_name ? var_name : "NULL", 
           array_name ? array_name : "NULL");
    
    // For now, just return true to skip serializing as element
    // The actual template expansion will happen at runtime
    return true;
}
