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
#include "../../shared/krb_schema.h"
#include "../../include/types.h"
#include <math.h>
#include "string_table.h"
#include "ast_expression_serializer.h"
#include "ast_expander.h"
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <math.h>

// Forward declarations for internal helpers
static bool write_property_value(KryonCodeGenerator *codegen, const KryonASTValue *value, uint16_t property_hex);
static bool write_variable_reference(KryonCodeGenerator *codegen, const char *variable_name, uint16_t property_hex);
static KryonValueTypeHint get_property_type_hint(uint16_t property_hex);
static bool write_array_literal_property(KryonCodeGenerator *codegen, const KryonASTNode *array_node);
static bool write_template_property(KryonCodeGenerator *codegen, const KryonASTNode *template_node, uint16_t property_hex);
static bool serialize_for_template(KryonCodeGenerator* codegen, const KryonASTNode* element);
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
    
    // Handle const_for loops by expanding them
    if (element->type == KRYON_AST_CONST_FOR_LOOP) {
        return kryon_expand_const_for_loop(codegen, element, ast_root);
    }
    
    if (element->type != KRYON_AST_ELEMENT && element->type != KRYON_AST_FOR_DIRECTIVE) {
        return false;
    }
    
    // Check if this is a custom component instance that needs expansion
    uint16_t element_hex;
    const char* element_type_name;
    
    if (element->type == KRYON_AST_FOR_DIRECTIVE) {
        element_hex = kryon_get_syntax_hex("for");
        element_type_name = "@for";
    } else {
        element_hex = kryon_codegen_get_element_hex(element->data.element.element_type);
        element_type_name = element->data.element.element_type;
    }
    
    printf("DEBUG: write_element_instance - Element '%s' has hex 0x%04X\n", 
           element_type_name, element_hex);
           
    if (kryon_element_in_range(element_hex, KRYON_ELEMENT_RANGE_CUSTOM_START, KRYON_ELEMENT_RANGE_CUSTOM_END)) {
        // This is a custom component - expand it (but not syntax elements)
        printf("🔧 Expanding custom component in write_element_instance: %s\n", element_type_name);
        
        KryonASTNode *expanded = expand_component_instance(codegen, element, ast_root);
        if (expanded) {
            // Write the expanded component body
            bool result = kryon_write_element_instance(codegen, expanded, ast_root);
            // TODO: Free expanded node memory if we implement proper cloning
            return result;
        } else {
            printf("❌ Failed to expand component in write_element_instance: %s\n", element_type_name);
            return false;
        }
    }
    
    // Use centralized schema-based element header writing
    uint32_t instance_id = codegen->next_element_id++;
    uint16_t child_count = count_expanded_children(codegen, element);
    
    // Create element header using schema format
    KRBElementHeader header = {
        .instance_id = instance_id,
        .element_type = (uint32_t)element_hex,
        .parent_id = 0,  // Root elements for now
        .style_ref = 0,  // No styles for now
        .property_count = (uint16_t)element->data.element.property_count,
        .child_count = child_count,
        .event_count = 0,  // No events for now
        .flags = 0  // No flags for now
    };
    
    printf("🏗️  Writing element header: id=%u type=0x%x props=%u children=%u\n", 
           instance_id, element_hex, header.property_count, header.child_count);
    
    // Write header using centralized schema validation
    if (!krb_write_element_header(schema_writer, codegen, &header)) {
        codegen_error(codegen, "Failed to write element header using schema");
        return false;
    }
    
    // Write properties
    for (size_t i = 0; i < element->data.element.property_count; i++) {
        if (!write_property_node(codegen, element->data.element.properties[i])) {
            return false;
        }
    }
    
    // Write child instances recursively (elements, const_for loops, and @for directives)
    for (size_t i = 0; i < element->data.element.child_count; i++) {
        const KryonASTNode *child = element->data.element.children[i];
        if (child && (child->type == KRYON_AST_ELEMENT || child->type == KRYON_AST_CONST_FOR_LOOP)) {
            if (!kryon_write_element_instance(codegen, child, ast_root)) {
                return false;
            }
        } else if (child && child->type == KRYON_AST_FOR_DIRECTIVE) {
            // @for directives need to be written as element instances for consistent loading
            if (!kryon_write_element_instance(codegen, child, ast_root)) {
                return false;
            }
        }
        // Skip other directive nodes - they are handled in their respective sections
    }
    
    // Event handlers (none for now)
    
    return true;
}

bool kryon_write_element_node(KryonCodeGenerator *codegen, const KryonASTNode *element, const KryonASTNode *ast_root) {
    if (!element || (element->type != KRYON_AST_ELEMENT && element->type != KRYON_AST_EVENT_DIRECTIVE && element->type != KRYON_AST_FOR_DIRECTIVE)) {
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
    // Handle @for directives as templates, not elements
    if (element->type == KRYON_AST_FOR_DIRECTIVE) {
        printf("🔄 Skipping @for directive serialization for now\n");
        // TODO: Implement proper @for template storage
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

bool write_property_node(KryonCodeGenerator *codegen, const KryonASTNode *property) {
    if (!property || property->type != KRYON_AST_PROPERTY) {
        return false;
    }
    
    // Get property hex code using kryon mappings
    uint16_t property_hex = kryon_get_property_hex(property->data.property.name);
    if (property_hex == 0) {
        // Unknown property - assign custom property marker for consistency
        // Use a value that won't conflict with mapped properties
        property_hex = 0xFFFF; // TODO: Define KRYON_PROPERTY_CUSTOM constant
    }
    
    // Debug output
    printf("🔍 DEBUG: About to process property '%s' for element '%s'\n", 
           property->data.property.name, 
           property->parent && property->parent->type == KRYON_AST_ELEMENT ? 
           property->parent->data.element.element_type : "Unknown");
    
    printf("DEBUG: Processing property: '%s'\n", property->data.property.name);
    
    // Determine property value type
    uint8_t value_type = KRYON_RUNTIME_PROP_STRING; // Default
    if (property->data.property.value) {
        if (property->data.property.value->type == KRYON_AST_LITERAL) {
            // Get the property's expected type hint first
            KryonValueTypeHint hint = get_property_type_hint(property_hex);
            
            switch (property->data.property.value->data.literal.value.type) {
                case KRYON_VALUE_STRING: 
                    // Check if this string should be treated as a different type based on property hint
                    if (hint == KRYON_TYPE_HINT_COLOR) {
                        value_type = KRYON_RUNTIME_PROP_COLOR;
                    } else {
                        value_type = KRYON_RUNTIME_PROP_STRING;
                    }
                    break;
                case KRYON_VALUE_INTEGER: 
                    // Check if the target property expects a string (e.g., text property)
                    if (hint == KRYON_TYPE_HINT_STRING) {
                        value_type = KRYON_RUNTIME_PROP_STRING;
                    } else {
                        value_type = KRYON_RUNTIME_PROP_INTEGER;
                    }
                    break;
                case KRYON_VALUE_FLOAT: 
                    // Check if the target property expects a string
                    if (hint == KRYON_TYPE_HINT_STRING) {
                        value_type = KRYON_RUNTIME_PROP_STRING;
                    } else {
                        value_type = KRYON_RUNTIME_PROP_FLOAT;
                    }
                    break;
                case KRYON_VALUE_BOOLEAN: 
                    // Check if the target property expects a string
                    if (hint == KRYON_TYPE_HINT_STRING) {
                        value_type = KRYON_RUNTIME_PROP_STRING;
                    } else {
                        value_type = KRYON_RUNTIME_PROP_BOOLEAN;
                    }
                    break;
                case KRYON_VALUE_COLOR: value_type = KRYON_RUNTIME_PROP_COLOR; break;
                default: value_type = KRYON_RUNTIME_PROP_STRING; break;
            }
        } else if (property->data.property.value->type == KRYON_AST_VARIABLE) {
            value_type = KRYON_RUNTIME_PROP_REFERENCE;
        } else if (property->data.property.value->type == KRYON_AST_TEMPLATE) {
            // Check if template can be resolved to literals
            const KryonASTNode *template_node = property->data.property.value;
            bool all_literals = true;
            for (size_t i = 0; i < template_node->data.template.segment_count; i++) {
                const KryonASTNode *segment = template_node->data.template.segments[i];
                if (!segment || segment->type != KRYON_AST_LITERAL) {
                    all_literals = false;
                    break;
                }
            }
            
            if (all_literals) {
                // Template resolves to literals - treat as STRING
                value_type = KRYON_RUNTIME_PROP_STRING;
            } else {
                // Template has runtime variables - treat as TEMPLATE
                value_type = KRYON_RUNTIME_PROP_TEMPLATE;
            }
        } else if (property->data.property.value->type == KRYON_AST_ARRAY_LITERAL) {
            value_type = KRYON_RUNTIME_PROP_ARRAY;
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
    
    // If custom property, write the name as a string
    if (property_hex == 0xFFFF) {
        uint32_t name_ref = add_string_to_table(codegen, property->data.property.name);
        if (!write_uint32(codegen, name_ref)) {
            return false;
        }
    }
    
    // Write property value
    if (property->data.property.value) {
        printf("🔍 DEBUG: Property '%s' has value node type %d\n", property->data.property.name, property->data.property.value->type);
        if (property->data.property.value->type == KRYON_AST_LITERAL) {
            printf("🔍 DEBUG: Writing literal value with type %d\n", property->data.property.value->data.literal.value.type);
            // Write literal value with type hint
            return write_property_value(codegen, &property->data.property.value->data.literal.value, property_hex);
        } else if (property->data.property.value->type == KRYON_AST_VARIABLE) {
            // Write reactive variable reference for runtime resolution
            printf("❌ ERROR: Found unresolved variable reference: '%s' for property '%s' (0x%04X)\n", 
                   property->data.property.value->data.variable.name, 
                   property->data.property.name, property_hex);
            printf("❌ This variable should have been substituted during const evaluation!\n");
            printf("❌ Property location: element at current write position\n");
            return write_variable_reference(codegen, property->data.property.value->data.variable.name, property_hex);
        } else if (property->data.property.value->type == KRYON_AST_TEMPLATE) {
            // Serialize template with structured segments
            printf("🔄 Writing structured template with segments\n");
            return write_template_property(codegen, property->data.property.value, property_hex);
        } else if (property->data.property.value->type == KRYON_AST_ARRAY_LITERAL) {
            KryonValueTypeHint type_hint = get_property_type_hint(property_hex);
            if (type_hint != KRYON_TYPE_HINT_ARRAY) {
                codegen_error(codegen, "Property does not support array values");
                return false;
            }
            return write_array_literal_property(codegen, property->data.property.value);
        } else {
            // Complex expression - convert to string representation 
            printf("🔄 Converting complex expression (type %d) to string for runtime evaluation\n", property->data.property.value->type);
            char* expression_str = kryon_ast_expression_to_string(property->data.property.value, codegen);
            if (!expression_str) {
                printf("❌ Failed to serialize expression to string\n");
                codegen_error(codegen, "Failed to serialize complex expression");
                return false;
            }
            
            // Check if the expression was fully evaluated to a literal value
            char *end_ptr;
            double numeric_value = strtod(expression_str, &end_ptr);
            
            if (*end_ptr == '\0') {
                // Expression resolved to a pure number - write as literal
                printf("✅ Expression resolved to literal number: %s - writing as literal\n", expression_str);
                
                // Create a literal AST value and use existing write_property_value
                KryonASTValue literal_value = {0};
                
                // Check if it's a whole number vs decimal  
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
                // Expression resolved to a boolean - write as literal
                printf("✅ Expression resolved to literal boolean: %s - writing as literal\n", expression_str);
                
                KryonASTValue literal_value = {0};
                literal_value.type = KRYON_VALUE_BOOLEAN;
                literal_value.data.bool_value = strcmp(expression_str, "true") == 0;
                
                kryon_free(expression_str);
                return write_property_value(codegen, &literal_value, property_hex);
                
            } else if (expression_str[0] == '"' && expression_str[strlen(expression_str)-1] == '"') {
                // Expression resolved to a string literal - write as literal
                printf("✅ Expression resolved to literal string: %s - writing as literal\n", expression_str);
                
                // Remove quotes and create string literal
                size_t len = strlen(expression_str);
                char* unquoted = malloc(len - 1); // -2 for quotes, +1 for null terminator
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
                // Expression resolved to a color hex string - write as literal
                printf("✅ Expression resolved to literal color: %s - writing as literal\n", expression_str);
                
                KryonASTValue literal_value = {0};
                literal_value.type = KRYON_VALUE_STRING;
                literal_value.data.string_value = expression_str;
                
                bool result = write_property_value(codegen, &literal_value, property_hex);
                kryon_free(expression_str);
                return result;
                
            } else {
                // Still a complex expression - write as variable reference
                printf("🔄 Writing complex expression as reactive reference: '%s'\n", expression_str);
                bool result = write_variable_reference(codegen, expression_str, property_hex);
                kryon_free(expression_str);
                return result;
            }
        }
    } else {
        printf("❌ DEBUG: Property '%s' has no value node\n", property->data.property.name);
        // Write empty value for properties without values
        if (property_hex == 0xFFFF) {
            // For custom properties, write empty string
            if (!write_uint16(codegen, 0)) return false; // Empty string length
        } else {
            // For known properties, write appropriate empty value based on type hint
            KryonValueTypeHint type_hint = get_property_type_hint(property_hex);
            if (type_hint == KRYON_TYPE_HINT_STRING || type_hint == KRYON_TYPE_HINT_REFERENCE) {
                if (!write_uint16(codegen, 0)) return false; // Empty string length
            } else {
                if (!write_uint32(codegen, 0)) return false; // Zero value
            }
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
        char *allocated_str = NULL;
        
        if (value->type == KRYON_VALUE_STRING) {
            str = value->data.string_value;
            printf("🔍 DEBUG: Writing string value: '%s' (length=%zu)\n", str ? str : "(null)", str ? strlen(str) : 0);
        } else if (value->type == KRYON_VALUE_INTEGER) {
            // Convert integer to string for properties that expect strings (like text)
            allocated_str = malloc(32);
            if (!allocated_str) {
                codegen_error(codegen, "Failed to allocate memory for integer-to-string conversion");
                return false;
            }
            snprintf(allocated_str, 32, "%lld", (long long)value->data.int_value);
            str = allocated_str;
            printf("🔍 DEBUG: Converting integer %lld to string '%s' for string property\n", 
                   (long long)value->data.int_value, str);
        } else if (value->type == KRYON_VALUE_FLOAT) {
            // Convert float to string for properties that expect strings
            allocated_str = malloc(32);
            if (!allocated_str) {
                codegen_error(codegen, "Failed to allocate memory for float-to-string conversion");
                return false;
            }
            snprintf(allocated_str, 32, "%.6g", value->data.float_value);
            str = allocated_str;
            printf("🔍 DEBUG: Converting float %.6g to string '%s' for string property\n", 
                   value->data.float_value, str);
        } else if (value->type == KRYON_VALUE_BOOLEAN) {
            // Convert boolean to string for properties that expect strings
            str = value->data.bool_value ? "true" : "false";
            printf("🔍 DEBUG: Converting boolean %s to string for string property\n", str);
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
        printf("❌ ERROR: Found alignment variable reference that should have been substituted: '%s'\n", variable_name);
        printf("❌ ERROR: This indicates a template substitution bug - alignment references should be resolved at compile time!\n");
        
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
        printf("❌ ERROR: Found unresolved const property access: '%s'\n", variable_name);
        printf("❌ ERROR: This should have been resolved during @const evaluation\n");
        printf("❌ ERROR: Check that template substitution is working for MEMBER_ACCESS and ARRAY_ACCESS nodes\n");
        codegen_error(codegen, "Unresolved const property access found");
        return false;
    }
    
    // Log successful component instance variable detection
    if (is_component_instance_variable) {
        printf("✅ Allowing component instance variable: '%s'\n", variable_name);
    }
    
    // Validate variable name format
    size_t name_len = strlen(variable_name);
    if (name_len == 0 || name_len > 1024) {
        printf("❌ ERROR: Invalid variable name length: %zu (name='%s')\n", name_len, variable_name);
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
        printf("❌ ERROR: String table reference is suspiciously large: %u for variable '%s'\n", 
               variable_name_ref, variable_name);
        codegen_error(codegen, "String table reference out of range");
        return false;
    }
    
    // Write property type as REFERENCE
    if (!write_uint8(codegen, (uint8_t)KRYON_RUNTIME_PROP_REFERENCE)) {
        codegen_error(codegen, "Failed to write REFERENCE property type");
        return false;
    }
    
    // Write variable name reference (points to string table)
    if (!write_uint32(codegen, variable_name_ref)) {
        codegen_error(codegen, "Failed to write variable name reference");
        return false;
    }
    
    printf("    Wrote variable reference: name='%s' (string_ref=%u, type=REFERENCE)\n", 
           variable_name, variable_name_ref);
    
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
    if (!element || (element->type != KRYON_AST_ELEMENT && element->type != KRYON_AST_FOR_DIRECTIVE)) {
        return 0;
    }
    
    uint16_t expanded_count = 0;
    
    for (size_t i = 0; i < element->data.element.child_count; i++) {
        const KryonASTNode *child = element->data.element.children[i];
        if (!child) continue;
        
        if (child->type == KRYON_AST_CONST_FOR_LOOP) {
            // Count how many elements this const_for will expand to
            uint32_t iteration_count = 0;
            
            if (child->data.const_for_loop.is_range) {
                // Handle range-based loop
                int start = child->data.const_for_loop.range_start;
                int end = child->data.const_for_loop.range_end;
                iteration_count = (uint32_t)(end - start + 1);
            } else {
                // Handle array-based loop
                const char *array_name = child->data.const_for_loop.array_name;
                const KryonASTNode *array_const = find_constant_value(codegen, array_name);
                
                if (array_const && array_const->type == KRYON_AST_ARRAY_LITERAL) {
                    iteration_count = (uint32_t)array_const->data.array_literal.element_count;
                } else {
                    // Fallback: count as 1 if we can't find the array
                    iteration_count = 1;
                }
            }
            
            // Each iteration creates elements from the loop body
            expanded_count += (uint16_t)iteration_count;
        } else if (child->type == KRYON_AST_ELEMENT) {
            // Regular element counts as 1
            expanded_count += 1;
        } else if (child->type == KRYON_AST_FOR_DIRECTIVE) {
            // @for directive counts as 1 element (will be expanded at runtime)
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
    bool all_literals = true;
    size_t total_length = 0;
    
    for (size_t i = 0; i < template_node->data.template.segment_count; i++) {
        const KryonASTNode *segment = template_node->data.template.segments[i];
        if (!segment || segment->type != KRYON_AST_LITERAL) {
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
        printf("✅ Template fully resolved to literals - writing as string property\n");
        
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
        
        printf("📝 Concatenated template to: '%s'\n", concatenated);
        
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
    printf("🔄 Template has runtime variables - writing as complex template\n");
    
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
    
    printf("📝 Writing template with %u segments\n", segment_count);
    
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
            
            printf("  📝 Literal segment: '%s' (%u bytes)\n", text ? text : "", text_len);
            
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
            
            uint32_t var_name_ref = add_string_to_table(codegen, var_name);
            if (var_name_ref == UINT32_MAX) {
                codegen_error(codegen, "Failed to add variable name to string table");
                return false;
            }
            
            if (!write_uint32(codegen, var_name_ref)) {
                codegen_error(codegen, "Failed to write variable name reference");
                return false;
            }
            
            printf("  🔗 Variable segment: '%s' (string_ref=%u)\n", var_name, var_name_ref);
            
        } else {
            printf("❌ Error: Unsupported template segment type: %d\n", segment->type);
            codegen_error(codegen, "Unsupported template segment type");
            return false;
        }
    }
    
    printf("✅ Template property written successfully\n");
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
    
    printf("DEBUG: Serializing @for template with %zu children\n", element->data.element.child_count);
    
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
    
    printf("DEBUG: @for template - variable: %s, array: %s\n", 
           var_name ? var_name : "NULL", 
           array_name ? array_name : "NULL");
    
    // For now, just return true to skip serializing as element
    // The actual template expansion will happen at runtime
    return true;
}
