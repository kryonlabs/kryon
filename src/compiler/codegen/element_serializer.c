/**
 * @file element_serializer.c
 * @brief Element serialization for KRB format
 * 
 * Handles serialization of UI elements and their properties to the KRB binary format.
 * Includes element instance writing, property serialization, and type management.
 */

#include "codegen.h"
#include "memory.h"
#include "krb_format.h"
#include "binary_io.h"
#include "color_utils.h"
#include "../../shared/kryon_mappings.h"
#include "string_table.h"
#include "ast_expression_serializer.h"
#include "ast_expander.h"
#include <string.h>
#include <stdio.h>
#include <math.h>

// Forward declarations for internal helpers
static bool write_property_value(KryonCodeGenerator *codegen, const KryonASTValue *value, uint16_t property_hex);
static bool write_variable_reference(KryonCodeGenerator *codegen, const char *variable_name, uint16_t property_hex);
static KryonValueTypeHint get_property_type_hint(uint16_t property_hex);
static bool write_array_literal_property(KryonCodeGenerator *codegen, const KryonASTNode *array_node);

bool kryon_write_element_instance(KryonCodeGenerator *codegen, const KryonASTNode *element, const KryonASTNode *ast_root) {
    if (!element) {
        return false;
    }
    
    // Handle const_for loops by expanding them
    if (element->type == KRYON_AST_CONST_FOR_LOOP) {
        return kryon_expand_const_for_loop(codegen, element, ast_root);
    }
    
    if (element->type != KRYON_AST_ELEMENT) {
        return false;
    }
    
    // Check if this is a custom component instance that needs expansion
    uint16_t element_hex = kryon_codegen_get_element_hex(element->data.element.element_type);
    printf("DEBUG: write_element_instance - Element '%s' has hex 0x%04X\n", 
           element->data.element.element_type, element_hex);
           
    if (element_hex >= 0x2000) {
        // This is a custom component - expand it
        printf("🔧 Expanding custom component in write_element_instance: %s\n", element->data.element.element_type);
        
        KryonASTNode *expanded = expand_component_instance(codegen, element, ast_root);
        if (expanded) {
            // Write the expanded component body
            bool result = kryon_write_element_instance(codegen, expanded, ast_root);
            // TODO: Free expanded node memory if we implement proper cloning
            return result;
        } else {
            printf("❌ Failed to expand component in write_element_instance: %s\n", element->data.element.element_type);
            return false;
        }
    }
    
    // Element Instance Structure as per KRB v0.1 spec:
    // [Instance ID: u32]
    // [Element Type ID: u32]
    // [Parent Instance ID: u32]
    // [Style Reference ID: u32]
    // [Property Count: u16]
    // [Child Count: u16]
    // [Event Handler Count: u16]
    // [Element Flags: u32]
    // [Properties]
    // [Child Instance IDs]
    // [Event Handlers]
    
    // Instance ID
    uint32_t instance_id = codegen->next_element_id++;
    if (!write_uint32(codegen, instance_id)) return false;
    
    // Element Type ID (use our element mappings)
    uint16_t element_type = kryon_codegen_get_element_hex(element->data.element.element_type);
    if (!write_uint32(codegen, (uint32_t)element_type)) return false;
    
    // Parent Instance ID (0 for root elements)
    if (!write_uint32(codegen, 0)) return false;
    
    // Style Reference ID (0 = no style)
    if (!write_uint32(codegen, 0)) return false;
    
    // Count expanded children (const_for loops expand to multiple elements)
    uint16_t child_count = count_expanded_children(codegen, element);
    
    // Property Count
    if (!write_uint16(codegen, (uint16_t)element->data.element.property_count)) return false;
    
    // Child Count
    printf("DEBUG: Element has %zu AST children, expanded to %u runtime children\n",
           element->data.element.child_count, child_count);
    if (!write_uint16(codegen, child_count)) return false;
    
    // Event Handler Count (0 for now)
    if (!write_uint16(codegen, 0)) return false;
    
    // Element Flags (0 for now)
    if (!write_uint32(codegen, 0)) return false;
    
    // Write properties
    for (size_t i = 0; i < element->data.element.property_count; i++) {
        if (!write_property_node(codegen, element->data.element.properties[i])) {
            return false;
        }
    }
    
    // Write child instances recursively (elements and const_for loops)
    for (size_t i = 0; i < element->data.element.child_count; i++) {
        const KryonASTNode *child = element->data.element.children[i];
        if (child && (child->type == KRYON_AST_ELEMENT || child->type == KRYON_AST_CONST_FOR_LOOP)) {
            if (!kryon_write_element_instance(codegen, child, ast_root)) {
                return false;
            }
        }
        // Skip directive nodes - they are handled in their respective sections
    }
    
    // Event handlers (none for now)
    
    return true;
}

bool kryon_write_element_node(KryonCodeGenerator *codegen, const KryonASTNode *element, const KryonASTNode *ast_root) {
    if (!element || (element->type != KRYON_AST_ELEMENT && element->type != KRYON_AST_EVENT_DIRECTIVE)) {
        return false;
    }
    
    // Check if this is a custom component instance that needs expansion
    if (element->type == KRYON_AST_ELEMENT) {
        uint16_t element_hex = kryon_codegen_get_element_hex(element->data.element.element_type);
        printf("🔍 DEBUG: Element '%s' has hex 0x%04X\n", element->data.element.element_type, element_hex);
        if (element_hex >= 0x2000) {
            // This is a custom component - expand it
            printf("🔧 Expanding custom component in recursive call: %s\n", element->data.element.element_type);
            KryonASTNode *expanded = expand_component_instance(codegen, element, ast_root);
            if (expanded) {
                bool result = kryon_write_element_node(codegen, expanded, ast_root);
                // TODO: Free expanded node memory
                return result;
            } else {
                printf("❌ Failed to expand component in recursive call: %s\n", element->data.element.element_type);
                return false;
            }
        }
    }
    
    // Get element type hex code for standard elements
    uint16_t element_hex;
    if (element->type == KRYON_AST_EVENT_DIRECTIVE) {
        element_hex = KRYON_ELEMENT_EVENT_DIRECTIVE;
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

bool write_property_node(KryonCodeGenerator *codegen, const KryonASTNode *property) {
    if (!property || property->type != KRYON_AST_PROPERTY) {
        return false;
    }
    
    // Get property hex code
    uint16_t property_hex = kryon_get_property_hex(property->data.property.name);
    if (property_hex == 0) {
        // Unknown property - write as custom property with string key
        property_hex = 0xFFFF; // Custom property marker
    }
    
    // Debug output
    printf("🔍 DEBUG: About to process property '%s' for element '%s'\n", 
           property->data.property.name, 
           property->parent && property->parent->type == KRYON_AST_ELEMENT ? 
           property->parent->data.element.element_type : "Unknown");
    
    printf("DEBUG: Processing property: '%s'\n", property->data.property.name);
    
    // Write property identifier
    if (!write_uint16(codegen, property_hex)) {
        return false;
    }
    
    // If custom property, write the name as a string
    if (property_hex == 0xFFFF) {
        uint32_t name_ref = add_string_to_table(codegen, property->data.property.name);
        if (!write_uint32(codegen, name_ref)) {
            return false;
        }
    }
    
    // Write property value
    if (property->data.property.value) {
        if (property->data.property.value->type == KRYON_AST_LITERAL) {
            // Write literal value with type hint
            return write_property_value(codegen, &property->data.property.value->data.literal.value, property_hex);
        } else if (property->data.property.value->type == KRYON_AST_VARIABLE) {
            // Write reactive variable reference for runtime resolution
            printf("🔄 Writing reactive variable reference: '%s'\n", property->data.property.value->data.variable.name);
            return write_variable_reference(codegen, property->data.property.value->data.variable.name, property_hex);
        } else if (property->data.property.value->type == KRYON_AST_TEMPLATE) {
            // Template expression should have been resolved during const_for expansion
            printf("❌ Unresolved template expression in property value\n");
            codegen_error(codegen, "Template expressions should be resolved during const_for expansion");
            return false;
        } else if (property->data.property.value->type == KRYON_AST_ARRAY_LITERAL) {
            KryonValueTypeHint type_hint = get_property_type_hint(property_hex);
            if (type_hint != KRYON_TYPE_HINT_ARRAY) {
                codegen_error(codegen, "Property does not support array values");
                return false;
            }
            return write_array_literal_property(codegen, property->data.property.value);
        } else {
            // Complex expression - convert to string representation for runtime evaluation
            printf("🔄 Converting complex expression (type %d) to string for runtime evaluation\n", property->data.property.value->type);
            char* expression_str = kryon_ast_expression_to_string(property->data.property.value);
            if (!expression_str) {
                printf("❌ Failed to serialize expression to string\n");
                codegen_error(codegen, "Failed to serialize complex expression");
                return false;
            }
            
            printf("🔄 Writing complex expression as reactive reference: '%s'\n", expression_str);
            bool result = write_variable_reference(codegen, expression_str, property_hex);
            kryon_free(expression_str);
            return result;
        }
    }
    
    return true;
}

static bool write_property_value(KryonCodeGenerator *codegen, const KryonASTValue *value, uint16_t property_hex) {
    // Get runtime property type for this property to match loader expectations
    KryonValueTypeHint type_hint = get_property_type_hint(property_hex);
    
    // Debug output
    printf("DEBUG: Property 0x%04X has type hint %d, value type %d\n", 
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
            printf("DEBUG: Parsed color '%s' -> 0x%08X\n", value->data.string_value, color_value);
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
        
        if (value->type == KRYON_VALUE_STRING) {
            str = value->data.string_value;
        } else {
            codegen_error(codegen, "Property requires string value");
            return false;
        }
        
        uint16_t len = str ? (uint16_t)strlen(str) : 0;
        if (!write_uint16(codegen, len)) return false;
        if (len > 0 && !write_binary_data(codegen, str, len)) return false;
    }
    
    return true;
}

static bool write_variable_reference(KryonCodeGenerator *codegen, const char *variable_name, uint16_t property_hex) {
    // Write a reactive variable reference for runtime resolution
    
    if (!variable_name) {
        codegen_error(codegen, "Variable reference has null name");
        return false;
    }
    
    // Get the expected property type to write in the correct format
    KryonValueTypeHint type_hint = get_property_type_hint(property_hex);
    
    if (type_hint == KRYON_TYPE_HINT_STRING || type_hint == KRYON_TYPE_HINT_REFERENCE) {
        // For string properties, write as string (length + data) - this is the existing working case
        uint16_t len = (uint16_t)strlen(variable_name);
        if (!write_uint16(codegen, len)) return false;
        if (len > 0 && !write_binary_data(codegen, variable_name, len)) return false;
        
        printf("    Wrote reactive variable reference as string: '%s' (length=%u)\n", variable_name, len);
    } else if (type_hint == KRYON_TYPE_HINT_FLOAT || type_hint == KRYON_TYPE_HINT_DIMENSION || 
               type_hint == KRYON_TYPE_HINT_UNIT || type_hint == KRYON_TYPE_HINT_SPACING) {
        // For float properties, write a special sentinel value to indicate reactive binding
        // Use -99999.0f as a sentinel value that indicates "resolve this at runtime"
        float sentinel_value = -99999.0f;
        uint32_t bits;
        memcpy(&bits, &sentinel_value, sizeof(uint32_t));
        if (!write_uint32(codegen, bits)) return false;
        
        printf("    Wrote reactive variable reference as float sentinel (-99999.0): '%s'\n", variable_name);
        
        // TODO: Store variable name in element metadata for runtime resolution
        // For now, this will require the runtime to handle NaN values specially
    } else {
        // For other property types, default to string format and hope the runtime can handle it
        printf("⚠️  Unknown property type %d for reactive variable '%s', using string format\n", type_hint, variable_name);
        uint16_t len = (uint16_t)strlen(variable_name);
        if (!write_uint16(codegen, len)) return false;
        if (len > 0 && !write_binary_data(codegen, variable_name, len)) return false;
    }
    
    return true;
}

static KryonValueTypeHint get_property_type_hint(uint16_t property_hex) {
    // Use centralized property type hints
    return kryon_get_property_type_hint(property_hex);
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
    if (!element || element->type != KRYON_AST_ELEMENT) {
        return 0;
    }
    
    uint16_t expanded_count = 0;
    
    for (size_t i = 0; i < element->data.element.child_count; i++) {
        const KryonASTNode *child = element->data.element.children[i];
        if (!child) continue;
        
        if (child->type == KRYON_AST_CONST_FOR_LOOP) {
            // Count how many elements this const_for will expand to
            const char *array_name = child->data.const_for_loop.array_name;
            const KryonASTNode *array_const = find_constant_value(codegen, array_name);
            
            if (array_const && array_const->type == KRYON_AST_ARRAY_LITERAL) {
                // Each iteration creates elements from the loop body
                expanded_count += (uint16_t)array_const->data.array_literal.element_count;
            } else {
                // Fallback: count as 1 if we can't find the array
                expanded_count += 1;
            }
        } else if (child->type == KRYON_AST_ELEMENT) {
            // Regular element counts as 1
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
    }
    
    return count;
}